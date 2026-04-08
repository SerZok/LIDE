#pragma once

#include <QObject>
#include <QTimer>

#include "settings.h"
#include "repl_message.h"

class ReplParser : public QObject
{
	Q_OBJECT

public:
	explicit ReplParser(QObject* parent = nullptr);
	void pushData(const QString& chunk);   // добавить порцию данных
	void reset();                          // сбросить состояние
	void setPrompt(const QString& prompt); // установить Prompt
	void setParserMode(int mode);

signals:
	void messageReady(const ReplMessage& msg);
	void errorLocationAvailable(const QString& message, int line, int column);
	void sbclTerminated(); //  SBCL завершил работу с --disable-debugger

private slots:
	void processBuffer();

private:
	Settings* m_settings;
	size_t m_maxLinesPerTick;
	Settings::ParseOutputMode m_parseMode;
	QTimer* m_timer;
	bool m_pendingSbclTerminated = false;

	QString m_inputBuffer;              // Сырые данные, ещё не разбитые на строки
	QStringList m_blockBuffer;          // Строки текущего логического блока
	QString m_currentPrompt;

	enum class ParseState {
		Idle,               // Ожидаем начало нового блока
		CollectingError,    // Собираем многострочную ошибку
		CollectingBacktrace // Собираем стектрейс
	};
	ParseState m_state;

	void tryClassifyBlock();           // Пытается классифицировать собранный блок
	void flushBlock();                 // Парсит, фильтрует и эмитит блок
	bool isBlockComplete() const;      // Блок можно обрабатывать?

	// Классификация
	enum class BlockType {
		Unknown,
		Prompt,
		Error,
		Warning,
		Result,
		TechnicalNoise
	};

	BlockType classifyBlock(const QStringList& lines) const;
	bool isErrorStart(const QString& line) const;
	bool isBacktraceStart(const QString& line) const;
	bool isPromptLine(const QString& line) const;
	bool isTechnicalNoise(const QString& line) const;

	// Парсинг
	ReplMessage parseErrorBlock(const QStringList& lines);
	ReplMessage parseWarningBlock(const QStringList& lines);
	ReplMessage parseResultBlock(const QStringList& lines);

	bool tryExtractPosition(const QString& text, int& line, int& column) const;
	void extractErrorPosition(const QString& joined, ReplMessage& msg) const;
	void extractErrorFile(const QString& joined, ReplMessage& msg) const;
	QString formatErrorText(const QStringList& lines, Settings::ParseOutputMode mode) const;
	QString shortenBacktraceFrame(const QString& frame) const;

	// Фильтрация
	bool shouldEmitBlock(BlockType type, const QStringList& lines) const;
	bool shouldFilterComment(const QString& line) const;
};