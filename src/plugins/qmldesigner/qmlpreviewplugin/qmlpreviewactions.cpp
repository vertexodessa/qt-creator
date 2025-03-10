/****************************************************************************
**
** Copyright (C) 2019 The Qt Company Ltd.
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

#include "qmlpreviewplugin.h"
#include "qmlpreviewactions.h"

#include <zoomaction.h>
#include <designersettings.h>

#include <utils/utilsicons.h>
#include <projectexplorer/kitinformation.h>
#include <projectexplorer/projectexplorer.h>
#include <projectexplorer/session.h>
#include <projectexplorer/project.h>
#include <projectexplorer/target.h>

#include <android/androidconstants.h>

#include <QLabel>
#include <QComboBox>
#include <QPointer>

namespace QmlDesigner {

using namespace ProjectExplorer;

const Utils::Icon previewIcon({
        {":/qmlpreviewplugin/images/live_preview.png", Utils::Theme::IconsBaseColor}});
const QByteArray livePreviewId = "LivePreview";

static void handleAction(const SelectionContext &context)
{
    if (context.view()->isAttached()) {
        if (context.toggled()) {
            bool skipDeploy = false;
            if (const Target *startupTarget = SessionManager::startupTarget()) {
                const Kit *kit = startupTarget->kit();
                if (kit
                    && (kit->supportedPlatforms().contains(Android::Constants::ANDROID_DEVICE_TYPE)
                        || DeviceTypeKitAspect::deviceTypeId(kit)
                               == Android::Constants::ANDROID_DEVICE_TYPE)) {
                    skipDeploy = true;
                    // In case of an android kit we don't want the live preview button to be toggled
                    // when the emulator is started as we don't have control over its run status.
                    DesignerActionManager &designerActionManager = QmlDesignerPlugin::instance()
                                                                       ->designerActionManager();
                    if (const ActionInterface *interface = designerActionManager.actionByMenuId(
                            livePreviewId))
                        interface->action()->setChecked(false);
                }
            }
            ProjectExplorerPlugin::runStartupProject(Constants::QML_PREVIEW_RUN_MODE, skipDeploy);
        } else {
            QmlPreviewWidgetPlugin::stopAllRunControls();
        }
    }
}

QmlPreviewAction::QmlPreviewAction() : ModelNodeAction(livePreviewId,
                                                       "Live Preview",
                                                       previewIcon.icon(),
                                                       QmlPreviewWidgetPlugin::tr("Show Live Preview"),
                                                       ComponentCoreConstants::qmlPreviewCategory,
                                                       QKeySequence("Alt+p"),
                                                       20,
                                                       &handleAction,
                                                       &SelectionContextFunctors::always)
{
    if (!QmlPreviewWidgetPlugin::getPreviewPlugin())
        defaultAction()->setVisible(false);

    defaultAction()->setCheckable(true);
}

void QmlPreviewAction::updateContext()
{
    if (selectionContext().view()->isAttached())
        QmlPreviewWidgetPlugin::setQmlFile();

    defaultAction()->setSelectionContext(selectionContext());
}

ActionInterface::Type QmlPreviewAction::type() const
{
    return ToolBarAction;
}

ZoomPreviewAction::ZoomPreviewAction()
    : m_zoomAction(new ZoomAction(nullptr))
{
    QObject::connect(m_zoomAction.get(), &ZoomAction::zoomLevelChanged, [=](float d) {
        QmlPreviewWidgetPlugin::setZoomFactor(d);
    });
    if (!QmlPreviewWidgetPlugin::getPreviewPlugin())
        m_zoomAction->setVisible(false);
}

ZoomPreviewAction::~ZoomPreviewAction()
= default;

QAction *ZoomPreviewAction::action() const
{
    return m_zoomAction.get();
}

QByteArray ZoomPreviewAction::category() const
{
    return ComponentCoreConstants::qmlPreviewCategory;
}

QByteArray ZoomPreviewAction::menuId() const
{
    return QByteArray();
}

int ZoomPreviewAction::priority() const
{
    return 19;
}

ActionInterface::Type ZoomPreviewAction::type() const
{
    return ToolBarAction;
}

void ZoomPreviewAction::currentContextChanged(const SelectionContext &)
{}

quint16 FpsLabelAction::lastValidFrames = 0;
QList<QPointer<QLabel>> FpsLabelAction::fpsHandlerLabelList;

FpsLabelAction::FpsLabelAction(QObject *parent)
    : QWidgetAction(parent)
{
}

void FpsLabelAction::fpsHandler(quint16 fpsValues[8])
{
    quint16 frames = fpsValues[0];
    if (frames != 0)
        lastValidFrames = frames;
    QString fpsText("%1 FPS");
    if (lastValidFrames == 0 || (frames == 0 && lastValidFrames < 2))
        fpsText = fpsText.arg("--");
    else
        fpsText = fpsText.arg(lastValidFrames);
    for (const QPointer<QLabel> &label : qAsConst(fpsHandlerLabelList)) {
        if (label)
            label->setText(fpsText);
    }
}

void FpsLabelAction::cleanFpsCounter()
{
    lastValidFrames = 0;
    quint16 nullInitialized[8] = {0};
    fpsHandler(nullInitialized);
}

QWidget *FpsLabelAction::createWidget(QWidget *parent)
{
    auto label = new QLabel(parent);
    auto originList = fpsHandlerLabelList;
    fpsHandlerLabelList.clear();
    fpsHandlerLabelList.append(label);
    for (const auto &labelPointer : originList) {
        if (labelPointer)
            fpsHandlerLabelList.append(labelPointer);
    }

    return label;
}

void FpsLabelAction::refreshFpsLabel(quint16 frames)
{
    for (const auto &labelPointer : qAsConst(fpsHandlerLabelList)) {
        if (labelPointer)
            labelPointer->setText(QString("%1 FPS").arg(frames));
    }
}

FpsAction::FpsAction() : m_fpsLabelAction(new FpsLabelAction(nullptr))
{}

QAction *FpsAction::action() const
{
    return m_fpsLabelAction.get();
}

QByteArray FpsAction::category() const
{
    return ComponentCoreConstants::qmlPreviewCategory;
}

QByteArray FpsAction::menuId() const
{
    return QByteArray();
}

int FpsAction::priority() const
{
    return 19;
}

ActionInterface::Type FpsAction::type() const
{
    return ToolBarAction;
}

void FpsAction::currentContextChanged(const SelectionContext &)
{}

SwitchLanguageComboboxAction::SwitchLanguageComboboxAction(QObject *parent)
    : QWidgetAction(parent)
{
}

QWidget *SwitchLanguageComboboxAction::createWidget(QWidget *parent)
{
    QPointer<QComboBox> comboBox = new QComboBox(parent);
    const QString toolTip(tr("Switch the language used by preview."));
    comboBox->setToolTip(toolTip);
    comboBox->addItem(tr("Default"));

    auto refreshComboBoxFunction = [this, comboBox, toolTip] (ProjectExplorer::Project *project) {
        if (comboBox && project) {
            comboBox->setDisabled(true);
            QString errorMessage;
            auto locales = project->availableQmlPreviewTranslations(&errorMessage);
            if (!errorMessage.isEmpty())
                comboBox->setToolTip(QString("%1<br/>(%2)").arg(toolTip, errorMessage));
            if (m_previousLocales != locales) {
                comboBox->clear();
                comboBox->addItem(tr("Default"));
                comboBox->addItems(locales);
                m_previousLocales = locales;
                comboBox->setEnabled(true);
            }
        }
    };
    connect(ProjectExplorer::SessionManager::instance(),  &ProjectExplorer::SessionManager::startupProjectChanged,
        comboBox, refreshComboBoxFunction);

    if (auto project = SessionManager::startupProject())
        refreshComboBoxFunction(project);

    // do this after refreshComboBoxFunction so we do not get currentLocaleChanged signals at initialization
    connect(comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), [this, comboBox](int index) {
        if (index == 0) // == Default
            emit currentLocaleChanged("");
        else
            emit currentLocaleChanged(comboBox->currentText());
    });

    return comboBox;
}

SwitchLanguageAction::SwitchLanguageAction()
    : m_switchLanguageAction(new SwitchLanguageComboboxAction(nullptr))
{
    QObject::connect(m_switchLanguageAction.get(), &SwitchLanguageComboboxAction::currentLocaleChanged,
                     &QmlPreviewWidgetPlugin::setLanguageLocale);
}

QAction *SwitchLanguageAction::action() const
{
    return m_switchLanguageAction.get();
}

QByteArray SwitchLanguageAction::category() const
{
    return ComponentCoreConstants::qmlPreviewCategory;
}

QByteArray SwitchLanguageAction::menuId() const
{
    return QByteArray();
}

int SwitchLanguageAction::priority() const
{
    return 10;
}

ActionInterface::Type SwitchLanguageAction::type() const
{
    return ToolBarAction;
}

void SwitchLanguageAction::currentContextChanged(const SelectionContext &)
{}


} // namespace QmlDesigner
