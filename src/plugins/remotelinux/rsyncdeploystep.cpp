/****************************************************************************
**
** Copyright (C) 2018 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of Qt Creator.
**
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 3 as published by the Free Software
** Foundation with exceptions as appearing in the file LICENSE.GPL3-EXCEPT
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-3.0.html.
**
****************************************************************************/

#include "rsyncdeploystep.h"

#include "abstractremotelinuxdeployservice.h"
#include "remotelinux_constants.h"

#include <projectexplorer/deploymentdata.h>
#include <projectexplorer/devicesupport/idevice.h>
#include <projectexplorer/runconfigurationaspects.h>
#include <projectexplorer/target.h>
#include <ssh/sshconnection.h>
#include <ssh/sshremoteprocess.h>
#include <ssh/sshsettings.h>
#include <utils/algorithm.h>
#include <utils/qtcprocess.h>

using namespace ProjectExplorer;
using namespace QSsh;
using namespace Utils;

namespace RemoteLinux {
namespace Internal {

class RsyncDeployService : public AbstractRemoteLinuxDeployService
{
    Q_OBJECT
public:
    RsyncDeployService(QObject *parent = nullptr) : AbstractRemoteLinuxDeployService(parent)
        { SshRemoteProcess::setupSshEnvironment(&m_rsync); }

    void setDeployableFiles(const QList<DeployableFile> &files) { m_deployableFiles = files; }
    void setIgnoreMissingFiles(bool ignore) { m_ignoreMissingFiles = ignore; }
    void setFlags(const QString &flags) { m_flags = flags; }

private:
    bool isDeploymentNecessary() const override;

    void doDeploy() override;
    void stopDeployment() override { setFinished(); };

    void filterDeployableFiles() const;
    void createRemoteDirectories();
    void deployFiles();
    void deployNextFile();
    void setFinished();

    mutable QList<DeployableFile> m_deployableFiles;
    bool m_ignoreMissingFiles = false;
    QString m_flags;
    QtcProcess m_rsync;
    std::unique_ptr<QtcProcess> m_mkdir;
};

bool RsyncDeployService::isDeploymentNecessary() const
{
    filterDeployableFiles();
    return !m_deployableFiles.empty();
}

void RsyncDeployService::doDeploy()
{
    createRemoteDirectories();
}

void RsyncDeployService::filterDeployableFiles() const
{
    if (m_ignoreMissingFiles) {
        Utils::erase(m_deployableFiles, [](const DeployableFile &f) {
            return !f.localFilePath().exists();
        });
    }
}

void RsyncDeployService::createRemoteDirectories()
{
    QStringList remoteDirs;
    for (const DeployableFile &f : qAsConst(m_deployableFiles))
        remoteDirs << f.remoteDirectory();
    remoteDirs.sort();
    remoteDirs.removeDuplicates();
    m_mkdir.reset(new QtcProcess);
    const CommandLine command {deviceConfiguration()->mapToGlobalPath("mkdir"),
        {"-p", ProcessArgs::createUnixArgs(remoteDirs).toString()}};
    m_mkdir->setCommand(command);
    connect(m_mkdir.get(), &QtcProcess::done, this, [this] {
        if (m_mkdir->result() != ProcessResult::FinishedWithSuccess) {
            emit errorMessage(tr("Failed to create remote directories: %1")
                              .arg(QString::fromUtf8(m_mkdir->readAllStandardError())));
            setFinished();
            return;
        }
        deployFiles();
        m_mkdir.release()->deleteLater();
    });
    m_mkdir->start();
}

void RsyncDeployService::deployFiles()
{
    connect(&m_rsync, &QtcProcess::readyReadStandardOutput, this, [this] {
        emit progressMessage(QString::fromLocal8Bit(m_rsync.readAllStandardOutput()));
    });
    connect(&m_rsync, &QtcProcess::readyReadStandardError, this, [this] {
        emit warningMessage(QString::fromLocal8Bit(m_rsync.readAllStandardError()));
    });
    connect(&m_rsync, &QtcProcess::done, this, [this] {
        auto notifyError = [this](const QString &message) {
            emit errorMessage(message);
            setFinished();
        };
        if (m_rsync.error() == QProcess::FailedToStart)
            notifyError(tr("rsync failed to start: %1").arg(m_rsync.errorString()));
        else if (m_rsync.exitStatus() == QProcess::CrashExit)
            notifyError(tr("rsync crashed."));
        else if (m_rsync.exitCode() != 0)
            notifyError(tr("rsync failed with exit code %1.").arg(m_rsync.exitCode()));
        else
            deployNextFile();
    });
    deployNextFile();
}

void RsyncDeployService::deployNextFile()
{
    if (m_deployableFiles.empty()) {
        setFinished();
        return;
    }
    const DeployableFile file = m_deployableFiles.takeFirst();
    const RsyncCommandLine cmdLine = RsyncDeployStep::rsyncCommand(*connection(), m_flags);
    QString localFilePath = file.localFilePath().toString();

    // On Windows, rsync is either from msys or cygwin. Neither work with the other's ssh.exe.
    if (HostOsInfo::isWindowsHost()) {
        localFilePath = '/' + localFilePath.at(0) + localFilePath.mid(2);
        if (anyOf(cmdLine.options, [](const QString &opt) {
                return opt.contains("cygwin", Qt::CaseInsensitive); })) {
            localFilePath.prepend("/cygdrive");
        }
    }

    const QStringList args = QStringList(cmdLine.options)
            << (localFilePath + (file.localFilePath().isDir() ? "/" : QString()))
            << (cmdLine.remoteHostSpec + ':' + file.remoteFilePath());
    m_rsync.setCommand(CommandLine("rsync", args));
    m_rsync.start(); // TODO: Get rsync location from settings?
}

void RsyncDeployService::setFinished()
{
    if (m_mkdir)
        m_mkdir.release()->deleteLater();
    m_rsync.close();
    handleDeploymentDone();
}

} // namespace Internal

RsyncDeployStep::RsyncDeployStep(BuildStepList *bsl, Utils::Id id)
    : AbstractRemoteLinuxDeployStep(bsl, id)
{
    auto service = createDeployService<Internal::RsyncDeployService>();

    auto flags = addAspect<StringAspect>();
    flags->setDisplayStyle(StringAspect::LineEditDisplay);
    flags->setSettingsKey("RemoteLinux.RsyncDeployStep.Flags");
    flags->setLabelText(tr("Flags:"));
    flags->setValue(defaultFlags());

    auto ignoreMissingFiles = addAspect<BoolAspect>();
    ignoreMissingFiles->setSettingsKey("RemoteLinux.RsyncDeployStep.IgnoreMissingFiles");
    ignoreMissingFiles->setLabel(tr("Ignore missing files:"),
                                 BoolAspect::LabelPlacement::InExtraLabel);
    ignoreMissingFiles->setValue(false);

    setInternalInitializer([service, flags, ignoreMissingFiles] {
        service->setIgnoreMissingFiles(ignoreMissingFiles->value());
        service->setFlags(flags->value());
        return service->isDeploymentPossible();
    });

    setRunPreparer([this, service] {
        service->setDeployableFiles(target()->deploymentData().allFiles());
    });
}

RsyncDeployStep::~RsyncDeployStep() = default;

Utils::Id RsyncDeployStep::stepId()
{
    return Constants::RsyncDeployStepId;
}

QString RsyncDeployStep::displayName()
{
    return tr("Deploy files via rsync");
}

QString RsyncDeployStep::defaultFlags()
{
    return QString("-av");
}

RsyncCommandLine RsyncDeployStep::rsyncCommand(const SshConnection &sshConnection,
                                               const QString &flags)
{
    const QString sshCmdLine = ProcessArgs::joinArgs(
                QStringList{SshSettings::sshFilePath().toUserOutput()}
                << sshConnection.connectionOptions(SshSettings::sshFilePath()), OsTypeLinux);
    const SshConnectionParameters sshParams = sshConnection.connectionParameters();
    return RsyncCommandLine(QStringList{"-e", sshCmdLine, flags},
                            sshParams.userName() + '@' + sshParams.host());
}

} //namespace RemoteLinux

#include <rsyncdeploystep.moc>
