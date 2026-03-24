#include "editor_tab_widget.h"
#include <QMessageBox>
#include <QFileInfo>
#include <QShortCut>

EditorTabWidget::EditorTabWidget(QWidget* parent)
    : QTabWidget(parent)
{
    setTabsClosable(true);
    setMovable(true);

    QShortcut* closeShortcut = new QShortcut(QKeySequence::Close, this);
    connect(closeShortcut, &QShortcut::activated, this, &EditorTabWidget::closeCurrentTab);
    connect(this, &QTabWidget::tabCloseRequested, this, &EditorTabWidget::onTabCloseRequested);
    connect(this, &QTabWidget::currentChanged, this, &EditorTabWidget::onCurrentChanged);
}

LispEditor* EditorTabWidget::currentEditor() const
{
    return qobject_cast<LispEditor*>(currentWidget());
}

LispEditor* EditorTabWidget::editorAt(int index) const
{
    return qobject_cast<LispEditor*>(widget(index));
}

bool EditorTabWidget::openFile(const QString& path)
{
    int existingIndex = findTabByPath(path);
    if (existingIndex != -1) {
        setCurrentIndex(existingIndex);
        return true;
    }

    auto* editor = new LispEditor(this);
    connect(editor, &LispEditor::fileModifiedChanged, this, &EditorTabWidget::onEditorModifiedChanged);
    connect(editor, &LispEditor::fileSaved, this, &EditorTabWidget::onEditorSaved);
    connect(editor, &LispEditor::fileClosed, this, &EditorTabWidget::onFileClosed);

    if (!editor->loadFile(path)) {
        delete editor;
        return false;
    }
    addEditorTab(path, editor);

    emit fileOpened(path);
    return true;
}

void EditorTabWidget::addEditorTab(const QString& path, LispEditor* editor)
{
    QFileInfo info(path);
    QString filename = info.fileName();

    int index = addTab(editor, filename);
    setTabToolTip(index, path);
    setCurrentIndex(index);

    updateMappings();

    emit currentFileChanged(path);
    emit currentEditorChanged(editor);
}

int EditorTabWidget::findTabByPath(const QString& path) const
{
    return m_pathToIndex.value(path, -1);
}

void EditorTabWidget::onTabCloseRequested(int index)
{
    auto* editor = editorAt(index);
    if (!editor) return;

    if (editor->isModified()) {
        QString path = m_indexToPath.value(index);
        QFileInfo info(path);

        QMessageBox::StandardButton reply = QMessageBox::question(this,
            tr("Закрыть файл"),
            tr("Файл %1 не сохранён.\nСохранить перед закрытием?")
            .arg(info.fileName()),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);

        if (reply == QMessageBox::Save) {
            if (!editor->saveFile()) {
                return;
            }
        }
        else if (reply == QMessageBox::Cancel) {
            return;
        }
    }

    QString path = m_indexToPath.value(index);

    int currentTabBeforeClose = currentIndex();
    m_pathToIndex.remove(path);
    m_indexToPath.remove(index);

    removeTab(index);
    updateMappings();
    emit fileClosed(path);

    if (currentTabBeforeClose != index) {
        int newIndex = currentTabBeforeClose;
        if (currentTabBeforeClose > index && newIndex > 0) {
            newIndex = currentTabBeforeClose - 1;
        }
        if (newIndex >= 0 && newIndex < count()) {
            setCurrentIndex(newIndex);
        }
    }
}

void EditorTabWidget::updateMappings()
{
    m_pathToIndex.clear();
    m_indexToPath.clear();

    for (int i = 0; i < count(); ++i) {
        LispEditor* editor = editorAt(i);
        if (editor) {
            QString path = editor->currentFile();
            if (!path.isEmpty()) {
                m_pathToIndex[path] = i;
                m_indexToPath[i] = path;
            }
        }
    }
}

void EditorTabWidget::onCurrentChanged(int index)
{
    if (index >= 0 && index < count() && m_indexToPath.contains(index)) {
        QString path = m_indexToPath[index];
        emit currentFileChanged(path);
        emit currentEditorChanged(editorAt(index));
    }
    else if (index == -1 || count() == 0) {
        emit currentFileChanged(QString());
        emit currentEditorChanged(nullptr);
    }
}

void EditorTabWidget::onEditorModifiedChanged(const QString& path, bool modified)
{
    int index = findTabByPath(path);
    if (index != -1) {
        updateTabText(index, path, modified);
    }
    emit fileModifiedChanged(path, modified);
}

void EditorTabWidget::onEditorSaved(const QString& path)
{
    int index = findTabByPath(path);
    if (index != -1) {
        updateTabText(index, path, false);
    }
}

void EditorTabWidget::onFileClosed(const QString& path)
{
    int index = findTabByPath(path);
    if (index != -1) {
        LispEditor* editor = editorAt(index);
        if (editor && editor->currentFile() == path) {
            qDebug() << "Removing tab at index:" << index;
            m_pathToIndex.remove(path);
            m_indexToPath.remove(index);
            removeTab(index);
            updateMappings();
        }
    }
}

void EditorTabWidget::updateTabText(int index, const QString& path, bool modified)
{
    QFileInfo info(path);
    QString filename = info.fileName();
    if (modified) {
        filename += " *";
    }
    setTabText(index, filename);
}

void EditorTabWidget::closeCurrentTab()
{
    int index = currentIndex();
    if (index != -1) {
        onTabCloseRequested(index);
    }
}

void EditorTabWidget::saveAll()
{
    for (int i = 0; i < count(); ++i) {
        auto* editor = editorAt(i);
        if (editor && editor->isModified()) {
            editor->saveFile();
        }
    }
}

QStringList EditorTabWidget::openedFiles() {
    QStringList files;
    for (int i = 0; i < count(); ++i) {
        LispEditor* editor = editorAt(i);
        if (editor) {
            QString file = editor->currentFile();
            if (!file.isEmpty()) {
                files.append(file);
            }
        }
    }
    return files;
}