#pragma once
#include <QString>
#include <optional>

enum class ReplMessageType
{
    Prompt,   // приглашение к вводу
    Result,   // результат вычисления
    Output,   // обычный вывод
    Error,    // ошибка
    Warning   // предупреждение
};

struct ReplMessage
{
    ReplMessageType type = ReplMessageType::Output;
    QString text;

    std::optional<int> line;
    std::optional<int> column;
    std::optional<QString> file;
};