#include "lisp_highlighter.h"

LispHighlighter::LispHighlighter(QTextDocument* parent): QSyntaxHighlighter(parent)
{
    auto* m_settings = Settings::instance();
    setupFormat(ThemeColors::fromString(m_settings->currentTheme()));

    connect(m_settings, &Settings::themeChanged, this, &LispHighlighter::onThemeChanged);
}

void LispHighlighter::onThemeChanged(const QString& theme) {
    setupFormat(ThemeColors::fromString(theme));
    rehighlight();
}

void LispHighlighter::highlightBlock(const QString& text)
{
    for (const HighlightingRule& rule : qAsConst(highlightingRules)) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
    setCurrentBlockState(0);
}


void LispHighlighter::setupFormat(ThemeColors themeColors) {
    highlightingRules.clear();

    // Ключевые слова
    keywordFormat.setForeground(themeColors.keyword);
    keywordFormat.setFontWeight(QFont::Bold);

    // Функции
    functionFormat.setForeground(themeColors.function);

    // Комментарии
    commentFormat.setForeground(themeColors.comment);
    commentFormat.setFontItalic(true);

    // Строки
    stringFormat.setForeground(themeColors.string);

    // Числа
    numberFormat.setForeground(themeColors.number);

    // Скобки
    parenthesisFormat.setForeground(themeColors.parenthesis);
    parenthesisFormat.setFontWeight(QFont::Bold);

    // Цитирование
    quoteFormat.setForeground(themeColors.quote);

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