/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
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

#include "perforcechecker.h"

#include <utils/qtcassert.h>

#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTimer>

using namespace Utils;

namespace Perforce {
namespace Internal {

PerforceChecker::PerforceChecker(QObject *parent) : QObject(parent)
{
    connect(&m_process, &QtcProcess::done, this, &PerforceChecker::slotDone);
}

PerforceChecker::~PerforceChecker()
{
    m_process.kill();
    m_process.waitForFinished();
    resetOverrideCursor();
}

bool PerforceChecker::isRunning() const
{
    return m_process.state() == QProcess::Running;
}

bool PerforceChecker::waitForFinished()
{
    return m_process.waitForFinished() || m_process.state() == QProcess::NotRunning;
}

void PerforceChecker::resetOverrideCursor()
{
    if (m_isOverrideCursor) {
        QApplication::restoreOverrideCursor();
        m_isOverrideCursor = false;
    }
}

void PerforceChecker::start(const FilePath &binary, const FilePath &workingDirectory,
                            const QStringList &basicArgs,
                            int timeoutMS)
{
    if (isRunning()) {
        emitFailed(QLatin1String("Internal error: process still running"));
        return;
    }
    if (binary.isEmpty()) {
        emitFailed(tr("No executable specified"));
        return;
    }
    m_binary = binary;
    QStringList args = basicArgs;
    args << QLatin1String("client") << QLatin1String("-o");

    if (!workingDirectory.isEmpty())
        m_process.setWorkingDirectory(workingDirectory);

    m_process.setCommand({m_binary, args});
    m_process.start();
    // Timeout handling
    m_timeOutMS = timeoutMS;
    m_timedOut = false;
    if (timeoutMS > 0)
        QTimer::singleShot(m_timeOutMS, this, &PerforceChecker::slotTimeOut);
    // Cursor
    if (m_useOverideCursor) {
        m_isOverrideCursor = true;
        QApplication::setOverrideCursor(QCursor(Qt::BusyCursor));
    }
}

void PerforceChecker::slotTimeOut()
{
    if (!isRunning())
        return;
    m_timedOut = true;
    m_process.stopProcess();
    emitFailed(tr("\"%1\" timed out after %2 ms.").arg(m_binary.toUserOutput()).arg(m_timeOutMS));
}

void PerforceChecker::slotDone()
{
    if (m_timedOut)
        return;
    if (m_process.error() == QProcess::FailedToStart) {
        emitFailed(tr("Unable to launch \"%1\": %2").
                   arg(m_binary.toUserOutput(), m_process.errorString()));
        return;
    }
    switch (m_process.exitStatus()) {
    case QProcess::CrashExit:
        emitFailed(tr("\"%1\" crashed.").arg(m_binary.toUserOutput()));
        break;
    case QProcess::NormalExit:
        if (m_process.exitCode()) {
            const QString stdErr = m_process.stdErr();
            emitFailed(tr("\"%1\" terminated with exit code %2: %3").
                   arg(m_binary.toUserOutput()).arg(m_process.exitCode()).arg(stdErr));
        } else {
            parseOutput(m_process.stdOut());
        }
        break;
    }
}

static inline QString findTerm(const QString& in, const QLatin1String& term)
{
    QRegularExpression regExp(QString("(\\n|\\r\\n|\\r)%1\\s*(.*)(\\n|\\r\\n|\\r)").arg(term));
    QTC_ASSERT(regExp.isValid(), return QString());
    QRegularExpressionMatch match = regExp.match(in);
    if (match.hasMatch())
        return match.captured(2).trimmed();
    return QString();
}

// Parse p4 client output for the top level
static inline QString clientRootFromOutput(const QString &in)
{
    QString root = findTerm(in, QLatin1String("Root:"));
    if (!root.isNull()) {
        // Normalize slashes and capitalization of Windows drive letters for caching.
        return QFileInfo(root).absoluteFilePath();
    }
    return QString();
}

// When p4 port and p4 user is set a preconfigured Root: is given, which doesn't relate with
// the current mapped project. In this case "Client:" has the same value as "Host:", which is an
// invalid case.
static inline bool clientAndHostAreEqual(const QString &in)
{
    QString client = findTerm(in, QLatin1String("Client:"));
    QString host = findTerm(in, QLatin1String("Host:"));

    return client == host;
}

void PerforceChecker::parseOutput(const QString &response)
{
    if (!response.contains(QLatin1String("View:")) && !response.contains(QLatin1String("//depot/"))) {
        emitFailed(tr("The client does not seem to contain any mapped files."));
        return;
    }

    if (clientAndHostAreEqual(response)) {
        // Is an invalid case. But not an error. QtC checks cmake install directories for
        // p4 repositories, or the %temp% directory.
        return;
    }

    const QString repositoryRoot = clientRootFromOutput(response);
    if (repositoryRoot.isEmpty()) {
        //: Unable to determine root of the p4 client installation
        emitFailed(tr("Unable to determine the client root."));
        return;
    }
    // Check existence. No precise check here, might be a symlink
    if (QFileInfo::exists(repositoryRoot)) {
        emitSucceeded(repositoryRoot);
    } else {
        emitFailed(tr("The repository \"%1\" does not exist.").
                   arg(QDir::toNativeSeparators(repositoryRoot)));
    }
}

void PerforceChecker::emitFailed(const QString &m)
{
    resetOverrideCursor();
    emit failed(m);
}

void PerforceChecker::emitSucceeded(const QString &m)
{
    resetOverrideCursor();
    emit succeeded(FilePath::fromString(m));
}

bool PerforceChecker::useOverideCursor() const
{
    return m_useOverideCursor;
}

void PerforceChecker::setUseOverideCursor(bool v)
{
    m_useOverideCursor = v;
}

}
}

