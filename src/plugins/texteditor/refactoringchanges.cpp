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

#include "refactoringchanges.h"
#include "texteditor.h"
#include "textdocument.h"

#include <coreplugin/icore.h>
#include <coreplugin/dialogs/readonlyfilesdialog.h>
#include <coreplugin/documentmanager.h>
#include <coreplugin/editormanager/editormanager.h>
#include <utils/algorithm.h>
#include <utils/fileutils.h>
#include <utils/qtcassert.h>

#include <QFile>
#include <QFileInfo>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QDebug>
#include <QApplication>

using namespace Core;
using namespace Utils;

namespace TextEditor {

RefactoringChanges::RefactoringChanges(RefactoringChangesData *data)
    : m_data(data ? data : new RefactoringChangesData)
{}

RefactoringChanges::~RefactoringChanges() = default;

RefactoringSelections RefactoringChanges::rangesToSelections(QTextDocument *document,
                                                             const QList<Range> &ranges)
{
    RefactoringSelections selections;

    foreach (const Range &range, ranges) {
        QTextCursor start(document);
        start.setPosition(range.start);
        start.setKeepPositionOnInsert(true);
        QTextCursor end(document);
        end.setPosition(qMin(range.end, document->characterCount() - 1));

        selections.append(qMakePair(start, end));
    }

    return selections;
}

bool RefactoringChanges::createFile(const FilePath &filePath,
                                    const QString &contents,
                                    bool reindent,
                                    bool openEditor) const
{
    if (filePath.exists())
        return false;

    // Create a text document for the new file:
    auto document = new QTextDocument;
    QTextCursor cursor(document);
    cursor.beginEditBlock();
    cursor.insertText(contents);

    // Reindent the contents:
    if (reindent) {
        cursor.select(QTextCursor::Document);
        m_data->indentSelection(cursor, filePath, nullptr);
    }
    cursor.endEditBlock();

    // Write the file to disk:
    TextFileFormat format;
    format.codec = EditorManager::defaultTextCodec();
    QString error;
    bool saveOk = format.writeFile(filePath, document->toPlainText(), &error);
    delete document;
    if (!saveOk)
        return false;

    m_data->fileChanged(filePath);

    if (openEditor)
        RefactoringChanges::openEditor(filePath, /*bool activate =*/ false, -1, -1);

    return true;
}

bool RefactoringChanges::removeFile(const FilePath &filePath) const
{
    if (!filePath.exists())
        return false;

    // ### implement!
    qWarning() << "RefactoringChanges::removeFile is not implemented";
    return true;
}

TextEditorWidget *RefactoringChanges::openEditor(const FilePath &filePath,
                                                 bool activate,
                                                 int line,
                                                 int column)
{
    EditorManager::OpenEditorFlags flags = EditorManager::IgnoreNavigationHistory;
    if (activate)
        flags |= EditorManager::SwitchSplitIfAlreadyVisible;
    else
        flags |= EditorManager::DoNotChangeCurrentEditor;
    if (line != -1) {
        // openEditorAt uses a 1-based line and a 0-based column!
        column -= 1;
    }
    IEditor *editor = EditorManager::openEditorAt(Link{filePath, line, column}, Id(), flags);

    if (editor)
        return TextEditorWidget::fromEditor(editor);
    else
        return nullptr;
}

RefactoringFilePtr RefactoringChanges::file(TextEditorWidget *editor)
{
    return RefactoringFilePtr(new RefactoringFile(editor));
}

RefactoringFilePtr RefactoringChanges::file(const FilePath &filePath) const
{
    return RefactoringFilePtr(new RefactoringFile(filePath, m_data));
}

RefactoringFile::RefactoringFile(QTextDocument *document, const FilePath &filePath)
    : m_filePath(filePath)
    , m_document(document)
{ }

RefactoringFile::RefactoringFile(TextEditorWidget *editor)
    : m_filePath(editor->textDocument()->filePath())
    , m_editor(editor)
{ }

RefactoringFile::RefactoringFile(const FilePath &filePath,
                                 const QSharedPointer<RefactoringChangesData> &data)
    : m_filePath(filePath)
    , m_data(data)
{
    QList<IEditor *> editors = DocumentModel::editorsForFilePath(filePath);
    if (!editors.isEmpty()) {
        auto editorWidget = TextEditorWidget::fromEditor(editors.first());
        if (editorWidget && !editorWidget->isReadOnly())
            m_editor = editorWidget;
    }
}

RefactoringFile::~RefactoringFile()
{
    delete m_document;
}

bool RefactoringFile::isValid() const
{
    if (m_filePath.isEmpty())
        return false;
    return document();
}

const QTextDocument *RefactoringFile::document() const
{
    return mutableDocument();
}

QTextDocument *RefactoringFile::mutableDocument() const
{
    if (m_editor)
        return m_editor->document();
    if (!m_document) {
        QString fileContents;
        if (!m_filePath.isEmpty()) {
            QString error;
            QTextCodec *defaultCodec = EditorManager::defaultTextCodec();
            TextFileFormat::ReadResult result = TextFileFormat::readFile(m_filePath,
                                                                         defaultCodec,
                                                                         &fileContents,
                                                                         &m_textFileFormat,
                                                                         &error);
            if (result != TextFileFormat::ReadSuccess) {
                qWarning() << "Could not read " << m_filePath << ". Error: " << error;
                m_textFileFormat.codec = nullptr;
            }
        }
        // always make a QTextDocument to avoid excessive null checks
        m_document = new QTextDocument(fileContents);
    }
    return m_document;
}

const QTextCursor RefactoringFile::cursor() const
{
    if (m_editor)
        return m_editor->textCursor();
    if (!m_filePath.isEmpty()) {
        if (QTextDocument *doc = mutableDocument())
            return QTextCursor(doc);
    }

    return QTextCursor();
}

FilePath RefactoringFile::filePath() const
{
    return m_filePath;
}

TextEditorWidget *RefactoringFile::editor() const
{
    return m_editor;
}

int RefactoringFile::position(int line, int column) const
{
    QTC_ASSERT(line != 0, return -1);
    QTC_ASSERT(column != 0, return -1);
    if (const QTextDocument *doc = document())
        return doc->findBlockByNumber(line - 1).position() + column - 1;
    return -1;
}

void RefactoringFile::lineAndColumn(int offset, int *line, int *column) const
{
    QTC_ASSERT(line, return);
    QTC_ASSERT(column, return);
    QTC_ASSERT(offset >= 0, return);
    QTextCursor c(cursor());
    c.setPosition(offset);
    *line = c.blockNumber() + 1;
    *column = c.positionInBlock() + 1;
}

QChar RefactoringFile::charAt(int pos) const
{
    if (const QTextDocument *doc = document())
        return doc->characterAt(pos);
    return QChar();
}

QString RefactoringFile::textOf(int start, int end) const
{
    QTextCursor c = cursor();
    c.setPosition(start);
    c.setPosition(end, QTextCursor::KeepAnchor);
    return c.selectedText();
}

QString RefactoringFile::textOf(const Range &range) const
{
    return textOf(range.start, range.end);
}

ChangeSet RefactoringFile::changeSet() const
{
    return m_changes;
}

void RefactoringFile::setChangeSet(const ChangeSet &changeSet)
{
    if (m_filePath.isEmpty())
        return;

    m_changes = changeSet;
}

void RefactoringFile::appendIndentRange(const Range &range)
{
    if (m_filePath.isEmpty())
        return;

    m_indentRanges.append(range);
}

void RefactoringFile::appendReindentRange(const Range &range)
{
    if (m_filePath.isEmpty())
        return;

    m_reindentRanges.append(range);
}

void RefactoringFile::setOpenEditor(bool activate, int pos)
{
    m_openEditor = true;
    m_activateEditor = activate;
    m_editorCursorPosition = pos;
}

bool RefactoringFile::apply()
{
    // test file permissions
    if (!m_filePath.toFileInfo().isWritable()) {
        ReadOnlyFilesDialog roDialog(m_filePath, ICore::dialogParent());
        const QString &failDetailText = QApplication::translate("RefactoringFile::apply",
                                                                "Refactoring cannot be applied.");
        roDialog.setShowFailWarning(true, failDetailText);
        if (roDialog.exec() == ReadOnlyFilesDialog::RO_Cancel)
            return false;
    }

    // open / activate / goto position
    bool ensureCursorVisible = false;
    if (m_openEditor && !m_filePath.isEmpty()) {
        int line = -1, column = -1;
        if (m_editorCursorPosition != -1) {
            lineAndColumn(m_editorCursorPosition, &line, &column);
            ensureCursorVisible = true;
        }
        m_editor = RefactoringChanges::openEditor(m_filePath, m_activateEditor, line, column);
        m_openEditor = false;
        m_activateEditor = false;
        m_editorCursorPosition = -1;
    }

    const bool withUnmodifiedEditor = m_editor && !m_editor->textDocument()->isModified();
    bool result = true;

    // apply changes, if any
    if (m_data && !(m_indentRanges.isEmpty() && m_changes.isEmpty())) {
        QTextDocument *doc = mutableDocument();
        if (doc) {
            QTextCursor c = cursor();
            if (m_appliedOnce)
                c.joinPreviousEditBlock();
            else
                c.beginEditBlock();

            sort(m_indentRanges);
            sort(m_reindentRanges);

            // build indent selections now, applying the changeset will change locations
            const RefactoringSelections &indentSelections =
                    RefactoringChanges::rangesToSelections(doc, m_indentRanges);
            m_indentRanges.clear();
            const RefactoringSelections &reindentSelections =
                    RefactoringChanges::rangesToSelections(doc, m_reindentRanges);
            m_reindentRanges.clear();

            // apply changes and reindent
            m_changes.apply(&c);
            m_changes.clear();

            indentOrReindent(indentSelections, Indent);
            indentOrReindent(reindentSelections, Reindent);

            c.endEditBlock();

            // if this document doesn't have an editor, write the result to a file
            if (!m_editor && m_textFileFormat.codec) {
                QTC_ASSERT(!m_filePath.isEmpty(), return false);
                QString error;
                // suppress "file has changed" warnings if the file is open in a read-only editor
                Core::FileChangeBlocker block(m_filePath);
                if (!m_textFileFormat.writeFile(m_filePath,
                                                doc->toPlainText(),
                                                &error)) {
                    qWarning() << "Could not apply changes to" << m_filePath
                               << ". Error: " << error;
                    result = false;
                }
            }

            fileChanged();
            if (withUnmodifiedEditor && EditorManager::autoSaveAfterRefactoring())
                m_editor->textDocument()->save(nullptr, m_filePath, false);
        }
    }

    if (m_editor && ensureCursorVisible)
        m_editor->ensureCursorVisible();

    m_appliedOnce = true;
    return result;
}

void RefactoringFile::indentOrReindent(const RefactoringSelections &ranges,
                                       RefactoringFile::IndentType indent)
{
    TextDocument * document = m_editor ? m_editor->textDocument() : nullptr;
    for (const auto &[position, anchor]: ranges) {
        QTextCursor selection(anchor);
        selection.setPosition(position.position(), QTextCursor::KeepAnchor);
        if (indent == Indent)
            m_data->indentSelection(selection, m_filePath, document);
        else
            m_data->reindentSelection(selection, m_filePath, document);
    }
}

void RefactoringFile::fileChanged()
{
    if (!m_filePath.isEmpty())
        m_data->fileChanged(m_filePath);
}

RefactoringChangesData::~RefactoringChangesData() = default;

void RefactoringChangesData::indentSelection(const QTextCursor &,
                                             const FilePath &,
                                             const TextDocument *) const
{
    qWarning() << Q_FUNC_INFO << "not implemented";
}

void RefactoringChangesData::reindentSelection(const QTextCursor &,
                                               const FilePath &,
                                               const TextDocument *) const
{
    qWarning() << Q_FUNC_INFO << "not implemented";
}

void RefactoringChangesData::fileChanged(const FilePath &)
{
}

} // namespace TextEditor
