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

#pragma once

#include "sftpdefs.h"
#include "ssh_global.h"

#include <utils/filepath.h>

#include <QFlags>
#include <QHostAddress>
#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

namespace QSsh {
class SshRemoteProcess;

enum SshHostKeyCheckingMode {
    SshHostKeyCheckingNone,
    SshHostKeyCheckingStrict,
    SshHostKeyCheckingAllowNoMatch,
};

class QSSH_EXPORT SshConnectionParameters
{
public:
    enum AuthenticationType {
        AuthenticationTypeAll,
        AuthenticationTypeSpecificKey,
    };

    SshConnectionParameters();

    QString host() const { return url.host(); }
    quint16 port() const { return url.port(); }
    QString userName() const { return url.userName(); }
    QString userAtHost() const { return userName().isEmpty() ? host() : userName() + '@' + host(); }
    void setHost(const QString &host) { url.setHost(host); }
    void setPort(int port) { url.setPort(port); }
    void setUserName(const QString &name) { url.setUserName(name); }

    QStringList connectionOptions(const Utils::FilePath &binary) const;

    QUrl url;
    Utils::FilePath privateKeyFile;
    QString x11DisplayName;
    int timeout = 0; // In seconds.
    AuthenticationType authenticationType = AuthenticationTypeAll;
    SshHostKeyCheckingMode hostKeyCheckingMode = SshHostKeyCheckingAllowNoMatch;
};

QSSH_EXPORT bool operator==(const SshConnectionParameters &p1, const SshConnectionParameters &p2);
QSSH_EXPORT bool operator!=(const SshConnectionParameters &p1, const SshConnectionParameters &p2);

using SshRemoteProcessPtr = std::unique_ptr<SshRemoteProcess>;

class QSSH_EXPORT SshConnection : public QObject
{
    Q_OBJECT

public:
    enum State { Unconnected, Connecting, Connected, Disconnecting };

    explicit SshConnection(const SshConnectionParameters &serverInfo, QObject *parent = nullptr);

    void connectToHost();
    void disconnectFromHost();
    State state() const;
    QString errorString() const;
    SshConnectionParameters connectionParameters() const;
    QStringList connectionOptions(const Utils::FilePath &binary) const;
    bool sharingEnabled() const;
    ~SshConnection();

    SshRemoteProcessPtr createRemoteProcess(const QString &command);
    SshRemoteProcessPtr createRemoteShell();
    SftpTransferPtr createUpload(const FilesToTransfer &files,
                                 FileTransferErrorHandling errorHandlingMode);
    SftpTransferPtr createDownload(const FilesToTransfer &files,
                                   FileTransferErrorHandling errorHandlingMode);

signals:
    void connected();
    void disconnected();
    void errorOccurred();

private:
    void doConnectToHost();
    void emitError(const QString &reason);
    void emitConnected();
    void emitDisconnected();
    SftpTransferPtr setupTransfer(const FilesToTransfer &files, Internal::FileTransferType type,
                                  FileTransferErrorHandling errorHandlingMode);

    struct SshConnectionPrivate;
    SshConnectionPrivate * const d;
};

#ifdef WITH_TESTS
namespace SshTest {
const QString QSSH_EXPORT getHostFromEnvironment();
quint16 QSSH_EXPORT getPortFromEnvironment();
const QString QSSH_EXPORT getUserFromEnvironment();
const QString QSSH_EXPORT getKeyFileFromEnvironment();
const QSSH_EXPORT QString userAtHost();
SshConnectionParameters QSSH_EXPORT getParameters();
bool QSSH_EXPORT checkParameters(const SshConnectionParameters &params);
void QSSH_EXPORT printSetupHelp();
} // namespace SshTest
#endif

} // namespace QSsh

Q_DECLARE_METATYPE(QSsh::SshConnectionParameters::AuthenticationType)
