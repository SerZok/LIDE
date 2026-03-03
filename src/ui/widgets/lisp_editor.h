#pragma once

#include <QPlainTextEdit>
#include <QWidget>

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
    bool saveFile(const QString& fileName);
    QString currentFile() const { return m_currentFile; }
    bool isModified() const { return document()->isModified(); }

protected:
    // Переопределяем для нумерации строк
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private slots:
    void updateLineNumberAreaWidth(int newBlockCount);
    void highlightCurrentLine();
    void updateLineNumberArea(const QRect& rect, int dy);

private:
    class LineNumberArea;
    LineNumberArea* m_lineNumberArea;
    QString m_currentFile;

    int lineNumberAreaWidth() const;
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