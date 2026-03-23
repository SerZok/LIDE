#pragma once

#include "settings.h"

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

class LispHighlighter : public QSyntaxHighlighter
{
	Q_OBJECT

public:
	explicit LispHighlighter(QTextDocument* parent = nullptr);

public slots:
    void onThemeChanged(const QString& theme);

protected:
	void highlightBlock(const QString& text) override;

private:
    struct HighlightingRule
    {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    QVector<HighlightingRule> highlightingRules;

    struct ThemeColors
    {
        // Основные цвета
        QColor keyword;
        QColor function;
        QColor comment;
        QColor string;
        QColor number;
        QColor parenthesis;
        QColor quote;
        QColor trueVal;
        QColor nilVal;

        // Статические методы для получения цветов темы
        static ThemeColors darkTheme() {
            ThemeColors colors;
            colors.keyword = QColor(255, 160, 0);    // Оранжевый
            colors.function = QColor(0, 150, 200);    // Голубой
            colors.comment = QColor(0, 128, 0);       // Зелёный
            colors.string = QColor(255, 120, 120);    // Красноватый
            colors.number = QColor(180, 120, 255);    // Фиолетовый
            colors.parenthesis = QColor(255, 255, 255); // Белый
            colors.quote = QColor(255, 200, 100);     // Золотой
            colors.trueVal = QColor(30, 200, 0); // true
            colors.nilVal = QColor(255, 50, 50); // false
            return colors;
        }

        static ThemeColors lightTheme() {
            ThemeColors colors;
            colors.keyword = QColor(0, 0, 255);        // Синий
            colors.function = QColor(0, 100, 200);      // Тёмно-синий
            colors.comment = QColor(0, 128, 0);         // Зелёный
            colors.string = QColor(153, 0, 0);          // Красный
            colors.number = QColor(180, 0, 255);        // Фиолетовый
            colors.parenthesis = QColor(0, 0, 0);       // Чёрный
            colors.quote = QColor(200, 150, 0);     // Типо коричневого
            colors.trueVal = QColor(30, 200, 0); // true
            colors.nilVal = QColor(255, 50, 50); // false
            return colors;
        }

        static ThemeColors fromString(const QString& theme) {
            if (theme == "light") {
                return lightTheme();
            }
            return darkTheme(); // по умолчанию
        }
    };

    void setupFormat(ThemeColors themeColors);

    QTextCharFormat keywordFormat;
    QTextCharFormat functionFormat;
    QTextCharFormat commentFormat;
    QTextCharFormat stringFormat;
    QTextCharFormat numberFormat;
    QTextCharFormat parenthesisFormat;
    QTextCharFormat quoteFormat;
    QTextCharFormat quasiquoteFormat;
    QTextCharFormat tFormat;
    QTextCharFormat nilFormat;
};