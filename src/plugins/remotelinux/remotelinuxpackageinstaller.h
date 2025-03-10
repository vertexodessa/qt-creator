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

#include "remotelinux_export.h"

#include <projectexplorer/devicesupport/idevicefwd.h>

#include <QObject>

#include <memory>

namespace RemoteLinux {

namespace Internal { class AbstractRemoteLinuxPackageInstallerPrivate; }

class REMOTELINUX_EXPORT AbstractRemoteLinuxPackageInstaller : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY(AbstractRemoteLinuxPackageInstaller)
public:
    ~AbstractRemoteLinuxPackageInstaller() override;

    void installPackage(const ProjectExplorer::IDeviceConstPtr &deviceConfig,
                        const QString &packageFilePath, bool removePackageFile);
    void cancelInstallation();

signals:
    void stdoutData(const QString &output);
    void stderrData(const QString &output);
    void finished(const QString &errorMsg = QString());

protected:
    explicit AbstractRemoteLinuxPackageInstaller(QObject *parent = nullptr);

private:
    virtual QString installCommandLine(const QString &packageFilePath) const = 0;
    virtual QString cancelInstallationCommandLine() const = 0;

    std::unique_ptr<Internal::AbstractRemoteLinuxPackageInstallerPrivate> d;
};


class REMOTELINUX_EXPORT RemoteLinuxTarPackageInstaller : public AbstractRemoteLinuxPackageInstaller
{
    Q_OBJECT
public:
    RemoteLinuxTarPackageInstaller(QObject *parent = nullptr);

private:
    QString installCommandLine(const QString &packageFilePath) const override;
    QString cancelInstallationCommandLine() const override;
};


} // namespace RemoteLinux
