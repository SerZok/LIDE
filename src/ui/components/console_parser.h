#pragma once
#include <string>
#include <optional>
#include <QString>

enum class SBCLMessageType {
    Success,
    ReaderError,
    RuntimeError,
    Unknown
};

struct SBCLMessage {
    SBCLMessageType type;
    QString message;         // короткая читаемая строка
    std::optional<int> line;     // если известна позиция
    std::optional<int> column;   // если известна позиция
    std::optional<int> filePosition;  // позиция в файле
    QString file;                     // имя файла
};

class ConsoleParser {
public:
    // Парсит строку вывода SBCL, возвращает нормализованное сообщение
    static SBCLMessage parse(const QString& sbclOutput);
};