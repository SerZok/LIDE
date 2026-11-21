#pragma once

#include <QColor>
#include <QString>

namespace ContextColor
{
inline int colorIndex(const QString &fileName)
{
    quint32 hash = 2166136261u;
    for (QChar character : fileName.toCaseFolded())
    {
        hash ^= character.unicode();
        hash *= 16777619u;
    }
    return static_cast<int>(hash % 360u);
}

inline int colorHue(const QString &fileName)
{
    return colorIndex(fileName);
}

inline QColor background(const QString &fileName, bool darkTheme)
{
    const int hue = colorHue(fileName);
    return darkTheme ? QColor::fromHsv(hue, 42, 72)
                     : QColor::fromHsv(hue, 24, 238);
}

inline QColor border(const QString &fileName, bool darkTheme)
{
    const int hue = colorHue(fileName);
    return darkTheme ? QColor::fromHsv(hue, 44, 118)
                     : QColor::fromHsv(hue, 34, 205);
}

inline QColor foreground(const QString &fileName, bool darkTheme)
{
    const int hue = colorHue(fileName);
    return darkTheme ? QColor::fromHsv(hue, 115, 255)
                     : QColor::fromHsv(hue, 210, 150);
}

inline QColor chipText(bool darkTheme)
{
    return darkTheme ? QColor("#e2e8f0") : QColor("#25364b");
}

inline QColor removeHover(const QString &fileName, bool darkTheme)
{
    const int hue = colorHue(fileName);
    return darkTheme ? QColor::fromHsv(hue, 48, 105)
                     : QColor::fromHsv(hue, 30, 220);
}
}
