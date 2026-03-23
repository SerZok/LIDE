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

void LispHighlighter::setupFormat(ThemeColors themeColors) {
    highlightingRules.clear();

    // --- Настройка форматов ---

    // Ключевые слова (специальные операторы)
    keywordFormat.setForeground(themeColors.keyword);
    keywordFormat.setFontWeight(QFont::Bold);

    // Встроенные функции
    functionFormat.setForeground(themeColors.function);

    // Комментарии (только однострочные)
    commentFormat.setForeground(themeColors.comment);
    commentFormat.setFontItalic(true);

    // Строки
    stringFormat.setForeground(themeColors.string);

    // Числа
    numberFormat.setForeground(themeColors.number);

    // Скобки
    parenthesisFormat.setForeground(themeColors.parenthesis);
    parenthesisFormat.setFontWeight(QFont::Bold);

    // Цитирование (')
    quoteFormat.setForeground(themeColors.quote);

    // Квази-цитирование (`, ,@)
    quasiquoteFormat.setForeground(themeColors.quote);
    quasiquoteFormat.setFontWeight(QFont::Bold);

    // t значения
    tFormat.setForeground(themeColors.trueVal);
    tFormat.setFontWeight(QFont::Bold);

    // nil значения
    nilFormat.setForeground(themeColors.nilVal);
    nilFormat.setFontWeight(QFont::Bold);

    // --- 1. Специальные операторы ---
    QStringList specialForms;
    specialForms << "\\bquote\\b" << "\\bif\\b" << "\\blet\\b" << "\\blet\\*\\b"
        << "\\blambda\\b" << "\\bprogn\\b" << "\\bsetq\\b" << "\\bsetf\\b"
        << "\\bdefun\\b" << "\\bdefmacro\\b" << "\\bdefvar\\b"
        << "\\bdefparameter\\b" << "\\bdefconstant\\b" << "\\bblock\\b"
        << "\\breturn-from\\b" << "\\btagbody\\b" << "\\bgo\\b"
        << "\\bcatch\\b" << "\\bthrow\\b" << "\\bunwind-protect\\b"
        << "\\bmacrolet\\b" << "\\bsymbol-macrolet\\b" << "\\blabels\\b"
        << "\\bflet\\b" << "\\blocally\\b" << "\\bdeclare\\b"
        << "\\bthe\\b" << "\\bmultiple-value-bind\\b" << "\\bmultiple-value-call\\b";

    for (const QString& pattern : specialForms) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat;
        highlightingRules.append(rule);
    }

    // --- 2. Встроенные функции и макросы ---
    QStringList builtinFunctions;
    builtinFunctions << "\\bcar\\b" << "\\bcdr\\b" << "\\bcons\\b" << "\\blist\\b"
        << "\\bappend\\b" << "\\breverse\\b" << "\\blength\\b"
        << "\\bnth\\b" << "\\bfirst\\b" << "\\bsecond\\b" << "\\brest\\b"
        << "\\blast\\b" << "\\bmember\\b" << "\\bassoc\\b"
        << "\\b\\+\\b" << "\\b-\\b" << "\\b\\*\\b" << "\\b/\\b"
        << "\\b=\\b" << "\\b/=\\b" << "\\b<\\b" << "\\b>\\b" << "\\b<=\\b" << "\\b>=\\b"
        << "\\bmod\\b" << "\\brem\\b" << "\\bfloor\\b" << "\\bceiling\\b"
        << "\\btruncate\\b" << "\\bround\\b" << "\\bsin\\b" << "\\bcos\\b"
        << "\\btan\\b" << "\\bexp\\b" << "\\blog\\b" << "\\bsqrt\\b"
        << "\\babs\\b" << "\\brandom\\b" << "\\bmin\\b" << "\\bmax\\b"
        << "\\band\\b" << "\\bor\\b" << "\\bnot\\b" << "\\beq\\b"
        << "\\beql\\b" << "\\bequal\\b" << "\\bequalp\\b"
        << "\\bcond\\b" << "\\bcase\\b" << "\\btypecase\\b"
        << "\\bwhen\\b" << "\\bunless\\b" << "\\bloop\\b"
        << "\\bdotimes\\b" << "\\bdolist\\b" << "\\bdo\\b" << "\\bdo\\*\\b"
        << "\\bformat\\b" << "\\bprinc\\b" << "\\bprint\\b"
        << "\\bprin1\\b" << "\\bterpri\\b" << "\\bread\\b"
        << "\\bread-line\\b" << "\\bread-char\\b" << "\\bopen\\b"
        << "\\bclose\\b" << "\\bwith-open-file\\b"
        << "\\bstring\\b" << "\\bsymbol-name\\b" << "\\bsymbol-value\\b"
        << "\\bsymbol-function\\b" << "\\bmake-symbol\\b" << "\\bgensym\\b"
        << "\\bintern\\b" << "\\bchar\\b" << "\\bstring=\\b"
        << "\\bstring-equal\\b" << "\\bstring<\\b" << "\\bstring>\\b"
        << "\\bconcatenate\\b" << "\\bsubseq\\b" << "\\bsearch\\b"
        << "\\bmake-array\\b" << "\\baref\\b" << "\\bvector\\b"
        << "\\bsvref\\b" << "\\barray-dimensions\\b" << "\\barray-rank\\b"
        << "\\bmake-hash-table\\b" << "\\bgethash\\b" << "\\bremhash\\b"
        << "\\bclrhash\\b" << "\\bmaphash\\b" << "\\btypep\\b"
        << "\\btype-of\\b" << "\\bcoerce\\b" << "\\bsubtypep\\b"
        << "\\beval\\b" << "\\bapply\\b" << "\\bfuncall\\b"
        << "\\bcompile\\b" << "\\bload\\b" << "\\bprovide\\b" << "\\brequire\\b";

    for (const QString& pattern : builtinFunctions) {
        HighlightingRule rule;
        rule.pattern = QRegularExpression(pattern);
        rule.format = functionFormat;
        highlightingRules.append(rule);
    }

    // --- 3. Константы t и nil ---
    HighlightingRule tRule;
    tRule.pattern = QRegularExpression("\\bt\\b");
    tRule.format = tFormat;
    highlightingRules.append(tRule);

    HighlightingRule nilRule;
    nilRule.pattern = QRegularExpression("\\bnil\\b");
    nilRule.format = nilFormat;
    highlightingRules.append(nilRule);

    // --- 4. Скобки ---
    HighlightingRule bracketRule;
    bracketRule.pattern = QRegularExpression("[()]");
    bracketRule.format = parenthesisFormat;
    highlightingRules.append(bracketRule);

    // --- 5. Цитирование и квази-цитирование ---
    // Простое цитирование 'symbol
    HighlightingRule quoteRule;
    quoteRule.pattern = QRegularExpression("'[^'\\s)()]+");
    quoteRule.format = quoteFormat;
    highlightingRules.append(quoteRule);

    // Цитирование списка '(a b c)
    quoteRule.pattern = QRegularExpression("'\\([^)]*\\)");
    quoteRule.format = quoteFormat;
    highlightingRules.append(quoteRule);

    // Квази-цитирование `(...)
    HighlightingRule quasiquoteRule;
    quasiquoteRule.pattern = QRegularExpression("`[^`\\s)()]+|`\\([^)]*\\)");
    quasiquoteRule.format = quasiquoteFormat;
    highlightingRules.append(quasiquoteRule);

    // Распаковка ,@ и ,
    HighlightingRule unquoteRule;
    unquoteRule.pattern = QRegularExpression(",[@]?");
    unquoteRule.format = quasiquoteFormat;
    highlightingRules.append(unquoteRule);

    // --- 6. Строки ---
    HighlightingRule stringRule;
    // Строки в двойных кавычках с поддержкой escape-последовательностей
    stringRule.pattern = QRegularExpression("\"(?:[^\"\\\\]|\\\\.)*\"");
    stringRule.format = stringFormat;
    highlightingRules.append(stringRule);

    // --- 7. Числа ---
    HighlightingRule numberRule;
    // Целые числа
    numberRule.pattern = QRegularExpression("\\b-?[0-9]+\\b");
    numberRule.format = numberFormat;
    highlightingRules.append(numberRule);

    // Числа с плавающей точкой
    numberRule.pattern = QRegularExpression("\\b-?[0-9]+\\.[0-9]+\\b");
    numberRule.format = numberFormat;
    highlightingRules.append(numberRule);

    // Шестнадцатеричные числа #x...
    numberRule.pattern = QRegularExpression("#x[0-9a-fA-F]+");
    numberRule.format = numberFormat;
    highlightingRules.append(numberRule);

    // Восьмеричные числа #o...
    numberRule.pattern = QRegularExpression("#o[0-7]+");
    numberRule.format = numberFormat;
    highlightingRules.append(numberRule);

    // Двоичные числа #b...
    numberRule.pattern = QRegularExpression("#b[01]+");
    numberRule.format = numberFormat;
    highlightingRules.append(numberRule);

    // Рациональные числа 1/2
    numberRule.pattern = QRegularExpression("\\b[0-9]+/[0-9]+\\b");
    numberRule.format = numberFormat;
    highlightingRules.append(numberRule);

    // --- 8. Однострочные комментарии ---
    HighlightingRule commentRule;
    commentRule.pattern = QRegularExpression(";[^\n]*");
    commentRule.format = commentFormat;
    highlightingRules.append(commentRule);
}

void LispHighlighter::highlightBlock(const QString& text)
{
    // Просто применяем все правила
    for (const HighlightingRule& rule : qAsConst(highlightingRules)) {
        QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
        while (matchIterator.hasNext()) {
            QRegularExpressionMatch match = matchIterator.next();
            setFormat(match.capturedStart(), match.capturedLength(), rule.format);
        }
    }
}