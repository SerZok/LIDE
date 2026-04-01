#pragma once

#include <QFileSystemWatcher>
#include <QPlainTextEdit>
#include <QTimer>
#include <QWidget>

#include "ui/components/lisp_highlighter.h"

class QPaintEvent;
class QResizeEvent;

class LispEditor : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit LispEditor(QWidget* parent = nullptr);
    ~LispEditor();

    // Методы для работы с файлами
    bool loadFile(const QString& fileName);
    bool saveFile();
    bool saveFileAs(const QString& fileName);

    QString currentFile() const { return m_currentFile; }
    bool isModified() const { return document()->isModified(); }
    
    void highlightErrorAtPosition(const QString& message, int line, int column);
    void clearErrorHighlight();

signals:
    void fileChangedExternally(const QString& path);
    void fileSaved(const QString& path);
    void fileLoaded(const QString& path);
    void fileModifiedChanged(const QString& path, bool modified);
    void fileClosed(const QString& path);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    QList<QTextEdit::ExtraSelection> highlightMatchingBrackets();
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect& rect, int dy);

    void onFileChanged(const QString& path);
    void onTextChanged();
    void askForReload();

private:
    class LineNumberArea;

    QString m_currentFile;
    QFileSystemWatcher* m_fileWatcher;
    LineNumberArea* m_lineNumberArea;
    QTimer* m_reloadTimer;
    bool m_ignoreChanges;
    LispHighlighter* m_highlighter;
    QTextCharFormat m_matchedBracketFormat;
    QTextCharFormat m_mismatchedBracketFormat;

    QTextEdit::ExtraSelection m_errorSelection;
    QList<QTextEdit::ExtraSelection> m_errorSelections;
    QList<QTextEdit::ExtraSelection> m_extraSelections;

    void setupSyntaxHighlighting();
    void updateErrorHighlight();

    int lineNumberAreaWidth() const;

    void setupWatcher();
    void updateWatcher();
    bool reloadFile();

    int findMatchingBracket(int startPos, bool forward) const;
    void matchBrackets(const QTextCursor& cursor);
    bool isPositionInComment(int position) const;

};

// Нумерация строк
class LispEditor::LineNumberArea : public QWidget
{
public:
    LineNumberArea(LispEditor* editor) : QWidget(editor), m_editor(editor) {}
    QSize sizeHint() const override { return QSize(m_editor->lineNumberAreaWidth(), 0); }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    LispEditor* m_editor;
};