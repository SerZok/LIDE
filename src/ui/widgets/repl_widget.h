#pragma once
#include <QTextEdit>

#include "settings.h"
#include "util/repl_console/repl_message.h"
#include "util/repl_console/repl_history.h"

// Пока пусть виджет будет содержать контроллер
#include "util/repl_console/repl_controller.h"

class ReplWidget : public QTextEdit
{
	Q_OBJECT

public:
	explicit ReplWidget(QWidget* parent = nullptr);
	~ReplWidget();

	// Отправка кода из консоли (Enter)
	void sendCode(const QString& code);

	// Отправка файла
	void sendFile(const QString& path);

	void restart();
	void clear();
	void stop();

signals:
	void commandEntered(const QString& command);  // команда от пользователя
	void processRestartRequested();                // запрос на перезапуск
	void processStopRequested();                   // запрос на остановку
	void errorLocationAvailable(const QString& file, const QString& message, int line, int column);

public slots:
	void displayMessage(const ReplMessage& msg);   // отобразить сообщение
	void onControllerStarted();
	void onControllerFinished();

protected:
	void keyPressEvent(QKeyEvent* event) override;
	void contextMenuEvent(QContextMenuEvent* event) override;

private:
	ReplHistory m_history;              // история команд
	ReplController* m_controller;        // контроллер с основной логикой
	Settings* m_settings;

	QString m_savedInput;
	bool m_historyBrowsing = false;

	int m_editableStart = 0;            // позиция начала ввода
	bool m_waitingForInput = true;      // ожидание ввода
	QString m_prompt = "* ";     // текущий промпт
	QString m_last_usage_file = "";     // последний использующийся файл

	size_t MAX_LINES;

	void appendOutput(const QString& text, ReplMessageType type = ReplMessageType::Result);
	void appendPrompt();
	bool isFormComplete(const QString& text);
	void sendCurrentLine();
	QString currentLine() const;
	QString currentInput() const;
	void insertFromHistory(int direction);
	void ensureCursorInEditable();
	void setupConnections();
	void onErrorLocationAvailable(const QString& message, int line, int column);
};