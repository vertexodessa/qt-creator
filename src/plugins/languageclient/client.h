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

#pragma once

#include "diagnosticmanager.h"
#include "documentsymbolcache.h"
#include "dynamiccapabilities.h"
#include "languageclient_global.h"
#include "languageclientcompletionassist.h"
#include "languageclientformatter.h"
#include "languageclientfunctionhint.h"
#include "languageclienthoverhandler.h"
#include "languageclientquickfix.h"
#include "languageclientsettings.h"
#include "languageclientsymbolsupport.h"
#include "progressmanager.h"
#include "semantichighlightsupport.h"

#include <coreplugin/messagemanager.h>

#include <utils/id.h>
#include <utils/link.h>

#include <languageserverprotocol/client.h>
#include <languageserverprotocol/diagnostics.h>
#include <languageserverprotocol/initializemessages.h>
#include <languageserverprotocol/languagefeatures.h>
#include <languageserverprotocol/messages.h>
#include <languageserverprotocol/progresssupport.h>
#include <languageserverprotocol/semantictokens.h>
#include <languageserverprotocol/shutdownmessages.h>
#include <languageserverprotocol/textsynchronization.h>

#include <texteditor/semantichighlighter.h>

#include <QBuffer>
#include <QHash>
#include <QJsonDocument>
#include <QTextCursor>

#include <unordered_map>
#include <utility>

namespace Core { class IDocument; }
namespace ProjectExplorer { class Project; }
namespace TextEditor
{
class IAssistProcessor;
class TextDocument;
class TextEditorWidget;
}

QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE

namespace LanguageClient {

class BaseClientInterface;

class LANGUAGECLIENT_EXPORT Client : public QObject
{
    Q_OBJECT

public:
    explicit Client(BaseClientInterface *clientInterface); // takes ownership
     ~Client() override;

    Client(const Client &) = delete;
    Client(Client &&) = delete;
    Client &operator=(const Client &) = delete;
    Client &operator=(Client &&) = delete;

    // basic properties
    Utils::Id id() const { return m_id; }
    void setName(const QString &name) { m_displayName = name; }
    QString name() const;

    enum class SendDocUpdates { Send, Ignore };
    void sendContent(const LanguageServerProtocol::IContent &content,
                     SendDocUpdates sendUpdates = SendDocUpdates::Send);

    void cancelRequest(const LanguageServerProtocol::MessageId &id);

    // server state handling
    void start();
    void setInitializationOptions(const QJsonObject& initializationOptions);
    void initialize();
    bool reset();
    void shutdown();
    enum State {
        Uninitialized,
        InitializeRequested,
        Initialized,
        ShutdownRequested,
        Shutdown,
        Error
    };
    State state() const;
    QString stateString() const;
    bool reachable() const { return m_state == Initialized; }

    void setClientInfo(const LanguageServerProtocol::ClientInfo &clientInfo);
    // capabilities
    static LanguageServerProtocol::ClientCapabilities defaultClientCapabilities();
    void setClientCapabilities(const LanguageServerProtocol::ClientCapabilities &caps);
    const LanguageServerProtocol::ServerCapabilities &capabilities() const;
    QString serverName() const { return m_serverName; }
    QString serverVersion() const { return m_serverVersion; }
    const DynamicCapabilities &dynamicCapabilities() const;
    void registerCapabilities(const QList<LanguageServerProtocol::Registration> &registrations);
    void unregisterCapabilities(const QList<LanguageServerProtocol::Unregistration> &unregistrations);

    void setLocatorsEnabled(bool enabled) { m_locatorsEnabled = enabled; }
    bool locatorsEnabled() const { return m_locatorsEnabled; }
    void setAutoRequestCodeActions(bool enabled) { m_autoRequestCodeActions = enabled; }

    // document synchronization
    void setSupportedLanguage(const LanguageFilter &filter);
    void setActivateDocumentAutomatically(bool enabled);
    bool isSupportedDocument(const TextEditor::TextDocument *document) const;
    bool isSupportedFile(const Utils::FilePath &filePath, const QString &mimeType) const;
    bool isSupportedUri(const LanguageServerProtocol::DocumentUri &uri) const;
    virtual void openDocument(TextEditor::TextDocument *document);
    void closeDocument(TextEditor::TextDocument *document);
    void activateDocument(TextEditor::TextDocument *document);
    void deactivateDocument(TextEditor::TextDocument *document);
    bool documentOpen(const TextEditor::TextDocument *document) const;
    TextEditor::TextDocument *documentForFilePath(const Utils::FilePath &file) const;
    void documentContentsSaved(TextEditor::TextDocument *document);
    void documentWillSave(Core::IDocument *document);
    void documentContentsChanged(TextEditor::TextDocument *document,
                                 int position,
                                 int charsRemoved,
                                 int charsAdded);
    void cursorPositionChanged(TextEditor::TextEditorWidget *widget);
    bool documentUpdatePostponed(const Utils::FilePath &fileName) const;
    int documentVersion(const Utils::FilePath &filePath) const;
    void setDocumentChangeUpdateThreshold(int msecs);

    // workspace control
    virtual void setCurrentProject(ProjectExplorer::Project *project);
    ProjectExplorer::Project *project() const;
    virtual void projectOpened(ProjectExplorer::Project *project);
    virtual void projectClosed(ProjectExplorer::Project *project);
    void updateConfiguration(const QJsonValue &configuration);

    // commands
    void requestCodeActions(const LanguageServerProtocol::DocumentUri &uri,
                            const LanguageServerProtocol::Diagnostic &diagnostic);
    void requestCodeActions(const LanguageServerProtocol::DocumentUri &uri,
                            const QList<LanguageServerProtocol::Diagnostic> &diagnostics);
    void requestCodeActions(const LanguageServerProtocol::CodeActionRequest &request);
    void handleCodeActionResponse(const LanguageServerProtocol::CodeActionRequest::Response &response,
                                  const LanguageServerProtocol::DocumentUri &uri);
    virtual void executeCommand(const LanguageServerProtocol::Command &command);

    // language support
    void addAssistProcessor(TextEditor::IAssistProcessor *processor);
    void removeAssistProcessor(TextEditor::IAssistProcessor *processor);
    SymbolSupport &symbolSupport();
    DocumentSymbolCache *documentSymbolCache();
    HoverHandler *hoverHandler();
    QList<LanguageServerProtocol::Diagnostic> diagnosticsAt(
        const LanguageServerProtocol::DocumentUri &uri,
        const QTextCursor &cursor) const;
    bool hasDiagnostic(const LanguageServerProtocol::DocumentUri &uri,
                       const LanguageServerProtocol::Diagnostic &diag) const;
    void setSemanticTokensHandler(const SemanticTokensHandler &handler);
    void setSymbolStringifier(const LanguageServerProtocol::SymbolStringifier &stringifier);
    LanguageServerProtocol::SymbolStringifier symbolStringifier() const;
    void setSnippetsGroup(const QString &group);
    void setCompletionAssistProvider(LanguageClientCompletionAssistProvider *provider);
    void setQuickFixAssistProvider(LanguageClientQuickFixProvider *provider);
    virtual bool supportsDocumentSymbols(const TextEditor::TextDocument *doc) const;

    // logging
    enum class LogTarget { Console, Ui };
    void setLogTarget(LogTarget target) { m_logTarget = target; }
    void log(const QString &message) const;
    template<typename Error>
    void log(const LanguageServerProtocol::ResponseError<Error> &responseError) const
    { log(responseError.toString()); }

    // Caller takes ownership.
    using CustomInspectorTab = std::pair<QWidget *, QString>;
    using CustomInspectorTabs = QList<CustomInspectorTab>;
    virtual const CustomInspectorTabs createCustomInspectorTabs() { return {}; }

    // Caller takes ownership
    virtual TextEditor::RefactoringChangesData *createRefactoringChangesBackend() const;

signals:
    void initialized(const LanguageServerProtocol::ServerCapabilities &capabilities);
    void capabilitiesChanged(const DynamicCapabilities &capabilities);
    void documentUpdated(TextEditor::TextDocument *document);
    void workDone(const LanguageServerProtocol::ProgressToken &token);
    void finished();

protected:
    void setError(const QString &message);
    void setProgressTitleForToken(const LanguageServerProtocol::ProgressToken &token,
                                  const QString &message);
    void handleMessage(const LanguageServerProtocol::BaseMessage &message);
    virtual void handleDiagnostics(const LanguageServerProtocol::PublishDiagnosticsParams &params);
    virtual DiagnosticManager *createDiagnosticManager();

private:
    void sendMessage(const LanguageServerProtocol::BaseMessage &message);
    void handleResponse(const LanguageServerProtocol::MessageId &id,
                        const LanguageServerProtocol::IContent &content);
    void handleMethod(const QString &method,
                      const LanguageServerProtocol::MessageId &id,
                      const LanguageServerProtocol::IContent &content);

    void initializeCallback(const LanguageServerProtocol::InitializeRequest::Response &initResponse);
    void shutDownCallback(const LanguageServerProtocol::ShutdownRequest::Response &shutdownResponse);
    bool sendWorkspceFolderChanges() const;
    void log(const LanguageServerProtocol::ShowMessageParams &message);

    LanguageServerProtocol::LanguageClientValue<LanguageServerProtocol::MessageActionItem>
    showMessageBox(const LanguageServerProtocol::ShowMessageRequestParams &message);

    void removeDiagnostics(const LanguageServerProtocol::DocumentUri &uri);
    void resetAssistProviders(TextEditor::TextDocument *document);

    void sendPostponedDocumentUpdates(Schedule semanticTokensSchedule);

    void updateCompletionProvider(TextEditor::TextDocument *document);
    void updateFunctionHintProvider(TextEditor::TextDocument *document);

    void requestDocumentHighlights(TextEditor::TextEditorWidget *widget);
    void requestDocumentHighlightsNow(TextEditor::TextEditorWidget *widget);
    LanguageServerProtocol::SemanticRequestTypes supportedSemanticRequests(TextEditor::TextDocument *document) const;
    void handleSemanticTokens(const LanguageServerProtocol::SemanticTokens &tokens);
    void requestCodeActions(const LanguageServerProtocol::DocumentUri &uri,
                            const LanguageServerProtocol::Range &range,
                            const QList<LanguageServerProtocol::Diagnostic> &diagnostics);
    void documentClosed(Core::IDocument *document);

    virtual void handleDocumentClosed(TextEditor::TextDocument *) {}
    virtual void handleDocumentOpened(TextEditor::TextDocument *) {}
    virtual QTextCursor adjustedCursorForHighlighting(const QTextCursor &cursor,
                                                      TextEditor::TextDocument *doc);

    using ContentHandler = std::function<void(const QByteArray &, QTextCodec *, QString &,
                                              LanguageServerProtocol::ResponseHandlers,
                                              LanguageServerProtocol::MethodHandler)>;

    State m_state = Uninitialized;
    QHash<LanguageServerProtocol::MessageId,
          LanguageServerProtocol::ResponseHandler::Callback> m_responseHandlers;
    QHash<QByteArray, ContentHandler> m_contentHandler;
    QString m_displayName;
    LanguageFilter m_languagFilter;
    QJsonObject m_initializationOptions;
    QMap<TextEditor::TextDocument *, QString> m_openedDocument;
    QSet<TextEditor::TextDocument *> m_postponedDocuments;
    QMap<Utils::FilePath, int> m_documentVersions;
    std::unordered_map<TextEditor::TextDocument *,
         QList<LanguageServerProtocol::DidChangeTextDocumentParams::TextDocumentContentChangeEvent>>
        m_documentsToUpdate;
    QMap<TextEditor::TextEditorWidget *, QTimer *> m_documentHighlightsTimer;
    QTimer m_documentUpdateTimer;
    Utils::Id m_id;
    LanguageServerProtocol::ClientCapabilities m_clientCapabilities = defaultClientCapabilities();
    LanguageServerProtocol::ServerCapabilities m_serverCapabilities;
    DynamicCapabilities m_dynamicCapabilities;
    struct AssistProviders
    {
        QPointer<TextEditor::CompletionAssistProvider> completionAssistProvider;
        QPointer<TextEditor::CompletionAssistProvider> functionHintProvider;
        QPointer<TextEditor::IAssistProvider> quickFixAssistProvider;
    };

    AssistProviders m_clientProviders;
    QMap<TextEditor::TextDocument *, AssistProviders> m_resetAssistProvider;
    QHash<TextEditor::TextEditorWidget *, LanguageServerProtocol::MessageId> m_highlightRequests;
    int m_restartsLeft = 5;
    QScopedPointer<BaseClientInterface> m_clientInterface;
    DiagnosticManager *m_diagnosticManager = nullptr;
    DocumentSymbolCache m_documentSymbolCache;
    HoverHandler m_hoverHandler;
    QHash<LanguageServerProtocol::DocumentUri, TextEditor::HighlightingResults> m_highlights;
    ProjectExplorer::Project *m_project = nullptr;
    QSet<TextEditor::IAssistProcessor *> m_runningAssistProcessors;
    SymbolSupport m_symbolSupport;
    ProgressManager m_progressManager;
    bool m_activateDocAutomatically = false;
    SemanticTokenSupport m_tokenSupport;
    QString m_serverName;
    QString m_serverVersion;
    LanguageServerProtocol::SymbolStringifier m_symbolStringifier;
    LogTarget m_logTarget = LogTarget::Ui;
    bool m_locatorsEnabled = true;
    bool m_autoRequestCodeActions = true;
    QTimer m_shutdownTimer;
    LanguageServerProtocol::ClientInfo m_clientInfo;
};

} // namespace LanguageClient
