#include "editor_tab_widget.h"
#include <QMessageBox>
#include <QFileInfo>

EditorTabWidget::EditorTabWidget(QWidget* parent)
    : QTabWidget(parent)
{
    setTabsClosable(true);
    setMovable(true);

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
    // Проверяем, не открыт ли уже файл
    int existingIndex = findTabByPath(path);
    if (existingIndex != -1) {
        setCurrentIndex(existingIndex);
        return true;
    }

    // Создаём новый редактор
    auto* editor = new LispEditor(this);

    // Подключаем сигналы редактора
    connect(editor, &LispEditor::fileModifiedChanged, this, &EditorTabWidget::onEditorModifiedChanged);
    connect(editor, &LispEditor::fileSaved, this, &EditorTabWidget::onEditorSaved);
    connect(editor, &LispEditor::fileClosed, this, &EditorTabWidget::onFileClosed);

    if (!editor->loadFile(path)) {
        delete editor;
        return false;
    }

    // Добавляем вкладку
    addEditorTab(path, editor);

    emit fileOpened(path);
    return true;
}

void EditorTabWidget::addEditorTab(const QString& path, LispEditor* editor)
{
    QFileInfo info(path);
    QString filename = info.fileName();

    bool wasBlocked = signalsBlocked();
    this->blockSignals(true);

    int index = addTab(editor, filename);
    setTabToolTip(index, path);

    m_pathToIndex[path] = index;
    m_indexToPath[index] = path;
    setCurrentIndex(index);

    this->blockSignals(wasBlocked);

    emit currentChanged(index);
}

int EditorTabWidget::findTabByPath(const QString& path) const
{
    return m_pathToIndex.value(path, -1);
}

void EditorTabWidget::onTabCloseRequested(int index)
{
    auto* editor = editorAt(index);
    if (!editor) return;

    // Проверяем, сохранён ли файл
    if (editor->isModified()) {
        QString path = m_indexToPath.value(index);
        QFileInfo info(path);

        QMessageBox::StandardButton reply = QMessageBox::question(this,
            tr("Закрыть файл"),
            tr("Файл %1 не сохранён.\nСохранить перед закрытием?")
            .arg(info.fileName()),
            QMessageBox::Save | QMessageBox::Abort | QMessageBox::Cancel);

        if (reply == QMessageBox::Save) {
            editor->saveFile();
        }
        else if (reply == QMessageBox::Cancel) {
            return;
        }
    }

    QString path = m_indexToPath.value(index);
    emit fileClosed(path);

    m_pathToIndex.remove(path);
    m_indexToPath.remove(index);

    removeTab(index);
}

void EditorTabWidget::onCurrentChanged(int index)
{
    if (index >= 0 && m_indexToPath.contains(index)) {
        emit currentFileChanged(m_indexToPath[index]);
        emit currentEditorChanged(editorAt(index));
    }
    else {
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
        m_pathToIndex.remove(path);
        m_indexToPath.remove(index);
        removeTab(index);
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
        files.emplace_back(editorAt(i)->currentFile());
    }
    return files;
}