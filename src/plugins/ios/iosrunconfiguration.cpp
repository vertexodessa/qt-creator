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

#include "iosrunconfiguration.h"
#include "iosconstants.h"
#include "iosdevice.h"
#include "simulatorcontrol.h"

#include <projectexplorer/buildconfiguration.h>
#include <projectexplorer/buildstep.h>
#include <projectexplorer/buildsteplist.h>
#include <projectexplorer/deployconfiguration.h>
#include <projectexplorer/devicesupport/devicemanager.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/project.h>
#include <projectexplorer/projectnodes.h>
#include <projectexplorer/runconfigurationaspects.h>
#include <projectexplorer/target.h>

#include <utils/algorithm.h>
#include <utils/fileutils.h>
#include <utils/qtcprocess.h>
#include <utils/qtcassert.h>
#include <utils/layoutbuilder.h>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QVariant>
#include <QWidget>

using namespace ProjectExplorer;
using namespace Utils;

namespace Ios {
namespace Internal {

static const QLatin1String deviceTypeKey("Ios.device_type");

static QString displayName(const SimulatorInfo &device)
{
    return QString("%1, %2").arg(device.name).arg(device.runtimeName);
}

static IosDeviceType toIosDeviceType(const SimulatorInfo &device)
{
    IosDeviceType iosDeviceType(IosDeviceType::SimulatedDevice,
                                device.identifier,
                                displayName(device));
    return iosDeviceType;
}

IosRunConfiguration::IosRunConfiguration(Target *target, Utils::Id id)
    : RunConfiguration(target, id)
{
    auto executableAspect = addAspect<ExecutableAspect>();
    executableAspect->setDisplayStyle(StringAspect::LabelDisplay);

    addAspect<ArgumentsAspect>();

    m_deviceTypeAspect = addAspect<IosDeviceTypeAspect>(this);

    setUpdater([this, target, executableAspect] {
        IDevice::ConstPtr dev = DeviceKitAspect::device(target->kit());
        const QString devName = dev.isNull() ? IosDevice::name() : dev->displayName();
        setDefaultDisplayName(tr("Run on %1").arg(devName));
        setDisplayName(tr("Run %1 on %2").arg(applicationName()).arg(devName));

        executableAspect->setExecutable(localExecutable());

        m_deviceTypeAspect->updateDeviceType();
    });
}

void IosDeviceTypeAspect::deviceChanges()
{
    m_runConfiguration->update();
}

void IosDeviceTypeAspect::updateDeviceType()
{
    if (DeviceTypeKitAspect::deviceTypeId(m_runConfiguration->kit())
            == Constants::IOS_DEVICE_TYPE)
        m_deviceType = IosDeviceType(IosDeviceType::IosDevice);
    else if (m_deviceType.type == IosDeviceType::IosDevice)
        m_deviceType = IosDeviceType(IosDeviceType::SimulatedDevice);
}

bool IosRunConfiguration::isEnabled() const
{
    Utils::Id devType = DeviceTypeKitAspect::deviceTypeId(kit());
    if (devType != Constants::IOS_DEVICE_TYPE && devType != Constants::IOS_SIMULATOR_TYPE)
        return false;

    IDevice::ConstPtr dev = DeviceKitAspect::device(kit());
    if (dev.isNull() || dev->deviceState() != IDevice::DeviceReadyToUse)
        return false;

    return true;
}

QString IosRunConfiguration::applicationName() const
{
    if (ProjectNode *node = project()->findNodeForBuildKey(buildKey()))
        return node->data(Constants::IosTarget).toString();

    return QString();
}

FilePath IosRunConfiguration::bundleDirectory() const
{
    Utils::Id devType = DeviceTypeKitAspect::deviceTypeId(kit());
    bool isDevice = (devType == Constants::IOS_DEVICE_TYPE);
    if (!isDevice && devType != Constants::IOS_SIMULATOR_TYPE) {
        qCWarning(iosLog) << "unexpected device type in bundleDirForTarget: " << devType.toString();
        return {};
    }
    FilePath res;
    bool shouldAppendBuildTypeAndPlatform = true;
    if (BuildConfiguration *bc = target()->activeBuildConfiguration()) {
        Project *project = target()->project();
        if (ProjectNode *node = project->findNodeForBuildKey(buildKey())) {
            QString pathStr = node->data(Constants::IosBuildDir).toString();
            const QString cmakeGenerator = node->data(Constants::IosCmakeGenerator).toString();

            if (cmakeGenerator.isEmpty()) {
                // qmake node gives absolute IosBuildDir
                res = FilePath::fromString(pathStr);
            } else {
                // CMake node gives IosBuildDir relative to root build directory

                bool useCmakePath = true;

                if (pathStr.isEmpty())
                    useCmakePath = false;

                if (useCmakePath && cmakeGenerator == "Xcode") {
                    // When generating Xcode project, CMake may put a "${EFFECTIVE_PLATFORM_NAME}" macro,
                    // which is expanded by Xcode at build time.
                    // To get an actual executable path at configure time, replace this macro here
                    // depending on the device type.

                    const QString before = "${EFFECTIVE_PLATFORM_NAME}";

                    int idx = pathStr.indexOf(before);

                    if (idx == -1) {
                        useCmakePath = false;
                    } else {
                        QString after;
                        if (isDevice)
                            after = "-iphoneos";
                        else
                            after = "-iphonesimulator";

                        pathStr.replace(idx, before.length(), after);
                    }
                }

                if (useCmakePath) {
                    // With Ninja generator IosBuildDir may be just "." when executable is in the root directory,
                    // so use canonical path to ensure that redundand dot is removed.
                    res = bc->buildDirectory().pathAppended(pathStr).canonicalPath();
                    // All done with path provided by CMake
                    shouldAppendBuildTypeAndPlatform = false;
                } else {
                    res = bc->buildDirectory();
                }
            }
        }

        if (res.isEmpty()) {
            // Fallback
            res = bc->buildDirectory();
            shouldAppendBuildTypeAndPlatform = true;
        }

        if (shouldAppendBuildTypeAndPlatform) {
            switch (bc->buildType()) {
            case BuildConfiguration::Debug :
            case BuildConfiguration::Unknown :
                if (isDevice)
                    res = res / "Debug-iphoneos";
                else
                    res = res.pathAppended("Debug-iphonesimulator");
                break;
            case BuildConfiguration::Profile :
            case BuildConfiguration::Release :
                if (isDevice)
                    res = res.pathAppended("Release-iphoneos");
                else
                    res = res.pathAppended("Release-iphonesimulator");
                break;
            default:
                qCWarning(iosLog) << "IosBuildStep had an unknown buildType "
                         << target()->activeBuildConfiguration()->buildType();
            }
        }
    }
    return res.pathAppended(applicationName() + ".app");
}

FilePath IosRunConfiguration::localExecutable() const
{
    return bundleDirectory().pathAppended(applicationName());
}

void IosDeviceTypeAspect::fromMap(const QVariantMap &map)
{
    bool deviceTypeIsInt;
    map.value(deviceTypeKey).toInt(&deviceTypeIsInt);
    if (deviceTypeIsInt || !m_deviceType.fromMap(map.value(deviceTypeKey).toMap()))
        updateDeviceType();

    m_runConfiguration->update();
}

void IosDeviceTypeAspect::toMap(QVariantMap &map) const
{
    map.insert(deviceTypeKey, deviceType().toMap());
}

QString IosRunConfiguration::disabledReason() const
{
    Utils::Id devType = DeviceTypeKitAspect::deviceTypeId(kit());
    if (devType != Constants::IOS_DEVICE_TYPE && devType != Constants::IOS_SIMULATOR_TYPE)
        return tr("Kit has incorrect device type for running on iOS devices.");
    IDevice::ConstPtr dev = DeviceKitAspect::device(kit());
    QString validDevName;
    bool hasConncetedDev = false;
    if (devType == Constants::IOS_DEVICE_TYPE) {
        DeviceManager *dm = DeviceManager::instance();
        for (int idev = 0; idev < dm->deviceCount(); ++idev) {
            IDevice::ConstPtr availDev = dm->deviceAt(idev);
            if (!availDev.isNull() && availDev->type() == Constants::IOS_DEVICE_TYPE) {
                if (availDev->deviceState() == IDevice::DeviceReadyToUse) {
                    validDevName += QLatin1Char(' ');
                    validDevName += availDev->displayName();
                } else if (availDev->deviceState() == IDevice::DeviceConnected) {
                    hasConncetedDev = true;
                }
            }
        }
    }

    if (dev.isNull()) {
        if (!validDevName.isEmpty())
            return tr("No device chosen. Select %1.").arg(validDevName); // should not happen
        else if (hasConncetedDev)
            return tr("No device chosen. Enable developer mode on a device."); // should not happen
        else
            return tr("No device available.");
    } else {
        switch (dev->deviceState()) {
        case IDevice::DeviceReadyToUse:
            break;
        case IDevice::DeviceConnected:
            return tr("To use this device you need to enable developer mode on it.");
        case IDevice::DeviceDisconnected:
        case IDevice::DeviceStateUnknown:
            if (!validDevName.isEmpty())
                return tr("%1 is not connected. Select %2?")
                        .arg(dev->displayName(), validDevName);
            else if (hasConncetedDev)
                return tr("%1 is not connected. Enable developer mode on a device?")
                        .arg(dev->displayName());
            else
                return tr("%1 is not connected.").arg(dev->displayName());
        }
    }
    return RunConfiguration::disabledReason();
}

IosDeviceType IosRunConfiguration::deviceType() const
{
    return m_deviceTypeAspect->deviceType();
}

IosDeviceType IosDeviceTypeAspect::deviceType() const
{
    if (m_deviceType.type == IosDeviceType::SimulatedDevice) {
        QList<SimulatorInfo> availableSimulators = SimulatorControl::availableSimulators();
        if (availableSimulators.isEmpty())
            return m_deviceType;
        if (Utils::contains(availableSimulators,
                            Utils::equal(&SimulatorInfo::identifier, m_deviceType.identifier))) {
                 return m_deviceType;
        }
        const QStringList parts = m_deviceType.displayName.split(QLatin1Char(','));
        if (parts.count() < 2)
            return toIosDeviceType(availableSimulators.last());

        QList<SimulatorInfo> eligibleDevices;
        eligibleDevices = Utils::filtered(availableSimulators, [parts](const SimulatorInfo &info) {
            return info.name == parts.at(0) && info.runtimeName == parts.at(1);
        });
        return toIosDeviceType(eligibleDevices.isEmpty() ? availableSimulators.last()
                                                         : eligibleDevices.last());
    }
    return m_deviceType;
}

void IosDeviceTypeAspect::setDeviceType(const IosDeviceType &deviceType)
{
    m_deviceType = deviceType;
}

IosDeviceTypeAspect::IosDeviceTypeAspect(IosRunConfiguration *runConfiguration)
    : m_runConfiguration(runConfiguration)
{
    addDataExtractor(this, &IosDeviceTypeAspect::deviceType, &Data::deviceType);
    addDataExtractor(this, &IosDeviceTypeAspect::bundleDirectory, &Data::bundleDirectory);
    addDataExtractor(this, &IosDeviceTypeAspect::applicationName, &Data::applicationName);
    addDataExtractor(this, &IosDeviceTypeAspect::localExecutable, &Data::localExecutable);

    connect(DeviceManager::instance(), &DeviceManager::updated,
            this, &IosDeviceTypeAspect::deviceChanges);
    connect(KitManager::instance(), &KitManager::kitsChanged,
            this, &IosDeviceTypeAspect::deviceChanges);
}

void IosDeviceTypeAspect::addToLayout(LayoutBuilder &builder)
{
    m_deviceTypeComboBox = new QComboBox;
    m_deviceTypeComboBox->setModel(&m_deviceTypeModel);

    m_deviceTypeLabel = new QLabel(IosRunConfiguration::tr("Device type:"));

    builder.addItems({m_deviceTypeLabel, m_deviceTypeComboBox});

    updateValues();

    connect(m_deviceTypeComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &IosDeviceTypeAspect::setDeviceTypeIndex);
}

void IosDeviceTypeAspect::setDeviceTypeIndex(int devIndex)
{
    QVariant selectedDev = m_deviceTypeModel.data(m_deviceTypeModel.index(devIndex, 0), Qt::UserRole + 1);
    if (selectedDev.isValid())
        setDeviceType(toIosDeviceType(selectedDev.value<SimulatorInfo>()));
}


void IosDeviceTypeAspect::updateValues()
{
    bool showDeviceSelector = deviceType().type != IosDeviceType::IosDevice;
    m_deviceTypeLabel->setVisible(showDeviceSelector);
    m_deviceTypeComboBox->setVisible(showDeviceSelector);
    if (showDeviceSelector && m_deviceTypeModel.rowCount() == 0) {
        foreach (const SimulatorInfo &device, SimulatorControl::availableSimulators()) {
            QStandardItem *item = new QStandardItem(Internal::displayName(device));
            QVariant v;
            v.setValue(device);
            item->setData(v);
            m_deviceTypeModel.appendRow(item);
        }
    }

    IosDeviceType currentDType = deviceType();
    QVariant currentData = m_deviceTypeComboBox->currentData();
    if (currentDType.type == IosDeviceType::SimulatedDevice && !currentDType.identifier.isEmpty()
            && (!currentData.isValid()
                || currentDType != toIosDeviceType(currentData.value<SimulatorInfo>())))
    {
        bool didSet = false;
        for (int i = 0; m_deviceTypeModel.hasIndex(i, 0); ++i) {
            QVariant vData = m_deviceTypeModel.data(m_deviceTypeModel.index(i, 0), Qt::UserRole + 1);
            SimulatorInfo dType = vData.value<SimulatorInfo>();
            if (dType.identifier == currentDType.identifier) {
                m_deviceTypeComboBox->setCurrentIndex(i);
                didSet = true;
                break;
            }
        }
        if (!didSet) {
            qCWarning(iosLog) << "could not set " << currentDType << " as it is not in model";
        }
    }
}

FilePath IosDeviceTypeAspect::bundleDirectory() const
{
    return m_runConfiguration->bundleDirectory();
}

QString IosDeviceTypeAspect::applicationName() const
{
    return m_runConfiguration->applicationName();
}

FilePath IosDeviceTypeAspect::localExecutable() const
{
    return m_runConfiguration->localExecutable();
}

// IosRunConfigurationFactory

IosRunConfigurationFactory::IosRunConfigurationFactory()
{
    registerRunConfiguration<IosRunConfiguration>("Qt4ProjectManager.IosRunConfiguration:");
    addSupportedTargetDeviceType(Constants::IOS_DEVICE_TYPE);
    addSupportedTargetDeviceType(Constants::IOS_SIMULATOR_TYPE);
}

} // namespace Internal
} // namespace Ios
