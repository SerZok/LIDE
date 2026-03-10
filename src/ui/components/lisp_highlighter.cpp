#include "lisp_highlighter.h"

LispHighlighter::LispHighlighter(QTextDocument* parent): QSyntaxHighlighter(parent)
{
    // Ключевые слова
    keywordFormat.setForeground(QColor(255, 160, 0)); // Оранжевый
    keywordFormat.setFontWeight(QFont::Bold);

    // Функции
    functionFormat.setForeground(QColor(0, 180, 255)); // Голубой

    // Комментарии (точка с запятой)
    commentFormat.setForeground(QColor(0, 128, 0)); // Серый
    commentFormat.setFontItalic(true);

    // Строки
    stringFormat.setForeground(QColor(255, 120, 120)); // Красноватый

    // Числа
    numberFormat.setForeground(QColor(180, 120, 255)); // Фиолетовый

    // Скобки
    parenthesisFormat.setForeground(QColor(255, 255, 255)); // Белый
    parenthesisFormat.setFontWeight(QFont::Bold);

    // Цитирование '
    quoteFormat.setForeground(QColor(255, 200, 100)); // Золотой

    // --- Правила подсветки ---

    // Ключевые слова Lisp
    QStringList keywordPatterns;
    keywordPatterns << "\\bdefun\\b" << "\\bdefvar\\b" << "\\bdefparameter\\b"
        << "\\bdefmacro\\b" << "\\blambda\\b" << "\\blet\\b"
        << "\\blet\\*\\b" << "\\bif\\b" << "\\bcond\\b"
        << "\\bcase\\b" << "\\bwhen\\b" << "\\bunless\\b"
        << "\\bprogn\\b" << "\\bquote\\b" << "\\bsetq\\b"
        << "\\bsetf\\b" << "\\bloop\\b" << "\\breturn\\b"
        << "\\bformat\\b" << "\\bprint\\b" << "\\bcar\\b"
        << "\\bcdr\\b" << "\\bcons\\b" << "\\blist\\b"
        << "\\bappend\\b" << "\\beq\\b" << "\\beql\\b"
        << "\\bequal\\b" << "\\bnull\\b" << "\\batom\\b";

    for (const QString& pattern : keywordPatterns) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    // Скобки
    HighlightingRule rule;
    rule.pattern = QRegularExpression("[()]");
    rule.format = parenthesisFormat;
    highlightingRules.append(rule);

    // Цитирование (одинарная кавычка перед выражением)
    rule.pattern = QRegularExpression("'[^'\\s)]+");
    rule.format = quoteFormat;
    highlightingRules.append(rule);

    // Строки в двойных кавычках
    rule.pattern = QRegularExpression("\".*\"");
    rule.format = stringFormat;
    highlightingRules.append(rule);

    // Строки с escape последовательностями
    rule.pattern = QRegularExpression("\"(?:[^\"\\\\]|\\\\.)*\"");
    rule.format = stringFormat;
    highlightingRules.append(rule);

    // Числа
    rule.pattern = QRegularExpression("\\b-?[0-9]+\\b");
    rule.format = numberFormat;
    highlightingRules.append(rule);

    rule.pattern = QRegularExpression("\\b-?[0-9]+\\.[0-9]+\\b");
    rule.format = numberFormat;
    highlightingRules.append(rule);

    // Комментарии (от ; до конца строки)
    rule.pattern = QRegularExpression(";[^\n]*");
    rule.format = commentFormat;
    highlightingRules.append(rule);
}

void LispHighlighter::highlightBlock(const QString& text)
{
    // Применяем все правила
    for (const HighlightingRule& rule : qAsConst(highlightingRules)) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }

    // Дополнительно: подсветка многострочных комментариев
    // (если нужны, хотя в Lisp обычно только однострочные)
    setCurrentBlockState(0);
}