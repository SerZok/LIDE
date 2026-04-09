#pragma once
#include <QStringList>

class ReplHistory
{
public:
    void add(const QString& command);
    QString previous();
    QString next();
    void clear();
    bool isEmpty() const;
    int size() const;
    QStringList getAll() const;  // для сохранения

private:
    QStringList m_commands;
    int m_index = -1;
};