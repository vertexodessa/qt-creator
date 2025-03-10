/****************************************************************************
**
** Copyright (C) 2016 Hugues Delorme
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

#include "bazaarclient.h"
#include "constants.h"

#include <vcsbase/vcsbaseplugin.h>
#include <vcsbase/vcsoutputwindow.h>
#include <vcsbase/vcsbaseeditorconfig.h>

#include <utils/hostosinfo.h>
#include <utils/qtcprocess.h>

#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>
#include <QDebug>

using namespace Utils;
using namespace VcsBase;

namespace Bazaar {
namespace Internal {

// Parameter widget controlling whitespace diff mode, associated with a parameter
class BazaarDiffConfig : public VcsBaseEditorConfig
{
    Q_OBJECT
public:
    BazaarDiffConfig(BazaarSettings &settings, QToolBar *toolBar) :
        VcsBaseEditorConfig(toolBar)
    {
        mapSetting(addToggleButton("-w", tr("Ignore Whitespace")),
                   &settings.diffIgnoreWhiteSpace);
        mapSetting(addToggleButton("-B", tr("Ignore Blank Lines")),
                   &settings.diffIgnoreBlankLines);
    }

    QStringList arguments() const override
    {
        QStringList args;
        // Bazaar wants "--diff-options=-w -B.."
        const QStringList formatArguments = VcsBaseEditorConfig::arguments();
        if (!formatArguments.isEmpty()) {
            const QString a = "--diff-options=" + formatArguments.join(' ');
            args.append(a);
        }
        return args;
    }
};

class BazaarLogConfig : public VcsBaseEditorConfig
{
    Q_OBJECT
public:
    BazaarLogConfig(BazaarSettings &settings, QToolBar *toolBar) :
        VcsBaseEditorConfig(toolBar)
    {
        mapSetting(addToggleButton("--verbose", tr("Verbose"),
                                   tr("Show files changed in each revision.")),
                   &settings.logVerbose);
        mapSetting(addToggleButton("--forward", tr("Forward"),
                                   tr("Show from oldest to newest.")),
                   &settings.logForward);
        mapSetting(addToggleButton("--include-merges", tr("Include Merges"),
                                   tr("Show merged revisions.")),
                   &settings.logIncludeMerges);

        const QList<ChoiceItem> logChoices = {
            {tr("Detailed"), "long"},
            {tr("Moderately Short"), "short"},
            {tr("One Line"), "line"},
            {tr("GNU Change Log"), "gnu-changelog"}
        };
        mapSetting(addChoices(tr("Format"), { "--log-format=%1" }, logChoices),
                   &settings.logFormat);
    }
};

BazaarClient::BazaarClient(BazaarSettings *settings) : VcsBaseClient(settings)
{
    setDiffConfigCreator([settings](QToolBar *toolBar) {
        return new BazaarDiffConfig(*settings, toolBar);
    });
    setLogConfigCreator([settings](QToolBar *toolBar) {
        return new BazaarLogConfig(*settings, toolBar);
    });
}

BranchInfo BazaarClient::synchronousBranchQuery(const FilePath &repositoryRoot) const
{
    QFile branchConfFile(repositoryRoot.toString() + QLatin1Char('/') +
                         QLatin1String(Constants::BAZAARREPO) +
                         QLatin1String("/branch/branch.conf"));
    if (!branchConfFile.open(QIODevice::ReadOnly))
        return BranchInfo(QString(), false);

    QTextStream ts(&branchConfFile);
    QString branchLocation;
    QString isBranchBound;
    QRegularExpression branchLocationRx("bound_location\\s*=\\s*(.+)$");
    QRegularExpression isBranchBoundRx("bound\\s*=\\s*(.+)$");
    while (!ts.atEnd() && (branchLocation.isEmpty() || isBranchBound.isEmpty())) {
        const QString line = ts.readLine();
        QRegularExpressionMatch match = branchLocationRx.match(line);
        if (match.hasMatch()) {
            branchLocation = match.captured(1);
        } else {
            QRegularExpressionMatch match = isBranchBoundRx.match(line);
            if (match.hasMatch())
                isBranchBound = match.captured(1);
        }
    }
    if (isBranchBound.simplified().toLower() == QLatin1String("true"))
        return BranchInfo(branchLocation, true);
    return BranchInfo(repositoryRoot.toString(), false);
}

//! Removes the last committed revision(s)
bool BazaarClient::synchronousUncommit(const FilePath &workingDir,
                                       const QString &revision,
                                       const QStringList &extraOptions)
{
    QStringList args;
    args << QLatin1String("uncommit")
         << QLatin1String("--force")   // Say yes to all questions
         << QLatin1String("--verbose") // Will print out what is being removed
         << revisionSpec(revision)
         << extraOptions;

    QtcProcess proc;
    vcsFullySynchronousExec(proc, workingDir, args);
    VcsOutputWindow::append(proc.stdOut());
    return proc.result() == ProcessResult::FinishedWithSuccess;
}

void BazaarClient::commit(const FilePath &repositoryRoot, const QStringList &files,
                          const QString &commitMessageFile, const QStringList &extraOptions)
{
    VcsBaseClient::commit(repositoryRoot, files, commitMessageFile,
                          QStringList(extraOptions) << QLatin1String("-F") << commitMessageFile);
}

VcsBaseEditorWidget *BazaarClient::annotate(
        const FilePath &workingDir, const QString &file, const QString &revision,
        int lineNumber, const QStringList &extraOptions)
{
    return VcsBaseClient::annotate(workingDir, file, revision, lineNumber,
                                   QStringList(extraOptions) << QLatin1String("--long"));
}

bool BazaarClient::isVcsDirectory(const FilePath &filePath) const
{
    return filePath.isDir()
            && !filePath.fileName().compare(Constants::BAZAARREPO, HostOsInfo::fileNameCaseSensitivity());
}

FilePath BazaarClient::findTopLevelForFile(const FilePath &file) const
{
    const QString repositoryCheckFile =
            QLatin1String(Constants::BAZAARREPO) + QLatin1String("/branch-format");
    return VcsBase::findRepositoryForFile(file, repositoryCheckFile);
}

bool BazaarClient::managesFile(const FilePath &workingDirectory, const QString &fileName) const
{
    QStringList args(QLatin1String("status"));
    args << fileName;

    QtcProcess proc;
    vcsFullySynchronousExec(proc, workingDirectory, args);
    if (proc.result() != ProcessResult::FinishedWithSuccess)
        return false;
    return proc.rawStdOut().startsWith("unknown");
}

void BazaarClient::view(const QString &source, const QString &id, const QStringList &extraOptions)
{
    QStringList args(QLatin1String("log"));
    args << QLatin1String("-p") << QLatin1String("-v") << extraOptions;
    VcsBaseClient::view(source, id, args);
}

Utils::Id BazaarClient::vcsEditorKind(VcsCommandTag cmd) const
{
    switch (cmd) {
    case AnnotateCommand:
        return Constants::ANNOTATELOG_ID;
    case DiffCommand:
        return Constants::DIFFLOG_ID;
    case LogCommand:
        return Constants::FILELOG_ID;
    default:
        return Utils::Id();
    }
}

QString BazaarClient::vcsCommandString(VcsCommandTag cmd) const
{
    switch (cmd) {
    case CloneCommand:
        return QLatin1String("branch");
    default:
        return VcsBaseClient::vcsCommandString(cmd);
    }
}

ExitCodeInterpreter BazaarClient::exitCodeInterpreter(VcsCommandTag cmd) const
{
    if (cmd == DiffCommand) {
        return [](int code) {
            return (code < 0 || code > 2) ? ProcessResult::FinishedWithError
                                          : ProcessResult::FinishedWithSuccess;
        };
    }
    return {};
}

QStringList BazaarClient::revisionSpec(const QString &revision) const
{
    QStringList args;
    if (!revision.isEmpty())
        args << QLatin1String("-r") << revision;
    return args;
}

BazaarClient::StatusItem BazaarClient::parseStatusLine(const QString &line) const
{
    StatusItem item;
    if (!line.isEmpty()) {
        const QChar flagVersion = line[0];
        if (flagVersion == QLatin1Char('+'))
            item.flags = QLatin1String("Versioned");
        else if (flagVersion == QLatin1Char('-'))
            item.flags = QLatin1String("Unversioned");
        else if (flagVersion == QLatin1Char('R'))
            item.flags = QLatin1String(Constants::FSTATUS_RENAMED);
        else if (flagVersion == QLatin1Char('?'))
            item.flags = QLatin1String("Unknown");
        else if (flagVersion == QLatin1Char('X'))
            item.flags = QLatin1String("Nonexistent");
        else if (flagVersion == QLatin1Char('C'))
            item.flags = QLatin1String("Conflict");
        else if (flagVersion == QLatin1Char('P'))
            item.flags = QLatin1String("PendingMerge");

        const int lineLength = line.length();
        if (lineLength >= 2) {
            const QChar flagContents = line[1];
            if (flagContents == QLatin1Char('N'))
                item.flags = QLatin1String(Constants::FSTATUS_CREATED);
            else if (flagContents == QLatin1Char('D'))
                item.flags = QLatin1String(Constants::FSTATUS_DELETED);
            else if (flagContents == QLatin1Char('K'))
                item.flags = QLatin1String("KindChanged");
            else if (flagContents == QLatin1Char('M'))
                item.flags = QLatin1String(Constants::FSTATUS_MODIFIED);
        }
        if (lineLength >= 3) {
            const QChar flagExec = line[2];
            if (flagExec == QLatin1Char('*'))
                item.flags = QLatin1String("ExecuteBitChanged");
        }
        // The status string should be similar to "xxx file_with_changes"
        // so just should take the file name part and store it
        item.file = line.mid(4);
    }
    return item;
}

} // namespace Internal
} // namespace Bazaar

#include "bazaarclient.moc"
