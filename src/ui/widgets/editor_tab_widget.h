#pragma once

#include <QTabWidget>
#include <QMap>
#include "lisp_editor.h"

class EditorTabWidget : public QTabWidget
{
    Q_OBJECT

public:
    explicit EditorTabWidget(QWidget* parent = nullptr);

    // Открыть файл в новой или существующей вкладке
    bool openFile(const QString& path);

    // Получить текущий редактор
    LispEditor* currentEditor() const;

    // Получить редактор по индексу
    LispEditor* editorAt(int index) const;

    // Закрыть текущую вкладку
    void closeCurrentTab();

    // Сохранить все файлы
    void saveAll();

signals:
    void fileOpened(const QString& path);
    void fileClosed(const QString& path);
    void currentFileChanged(const QString& path);
    void fileModifiedChanged(const QString& path, bool modified);

private slots:
    void onTabCloseRequested(int index);
    void onCurrentChanged(int index);
    void onEditorModifiedChanged(const QString& path, bool modified);
    void onEditorSaved(const QString& path);

private:
    QMap<QString, int> m_pathToIndex;  // Путь -> индекс вкладки
    QMap<int, QString> m_indexToPath;  // Индекс -> путь

    void updateTabText(int index, const QString& path, bool modified);
    int findTabByPath(const QString& path) const;
    void addEditorTab(const QString& path, LispEditor* editor);
};