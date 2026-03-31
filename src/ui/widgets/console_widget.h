#pragma once
// Общение с пользователем
#include <QTextEdit>
#include <QKeyEvent>
#include <QDebug>
#include <QTextBlock>
#include "components/console/console_process.h"

class Console : public QTextEdit
{
    Q_OBJECT

public:
    explicit Console(QWidget* parent = nullptr);
    void sendCode(const QString& code); // Отправить код 'code'
    void sendFile(const QString& path); // Отправить файл 'path'
	void restartSbclProcess(); // Перезагрузить процесс SBCL

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void sendCurrentCommandLine();  // Отправить текущую строку


private:
    QString m_currentCommand;
    QStringList m_commandHistory;
    int m_historyIndex;
    QString m_prompt;
    QString m_console_output_mark;
    bool m_waitingForInput;
    QString m_last_output;
	ConsoleProcess* m_console_process;


    // Позиция, от которой можно редактировать (после prompt)
    int m_editableStart;

    void setupConsole();
    void appendOutput(const QString& text, bool isError = false, bool isNotice = false);
    void appendPrompt( bool force = false);
    QString getCurrentCommandLineText() const;
    void insertFromHistory(int direction); // -1 назад, +1 вперёд

    // Вспомогательные
    void ensureCursorInEditableArea();
    void clear();

signals:
    // Сигнал для передачи ошибок с координатами (для MainWindow)
    void errorOccurred(const QString& file, int filePosition, const QString& message, int line, int column);
};