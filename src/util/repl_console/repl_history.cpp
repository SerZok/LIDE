#include "repl_history.h"

void ReplHistory::add(const QString& command)
{
    if (command.isEmpty()) return;
    m_commands.append(command);
    m_index = m_commands.size();
}

QString ReplHistory::previous()
{
    if (m_commands.isEmpty()) return {};
    if (m_index > 0) m_index--;
    return m_commands[m_index];
}

QString ReplHistory::next()
{
    if (m_index < m_commands.size())
        m_index++;
    if (m_index == m_commands.size())
        return "";
    return m_commands[m_index];
}

void ReplHistory::clear()
{
    m_commands.clear();
    m_index = -1;
}

int ReplHistory::size() const
{
    return m_commands.size();
}

bool ReplHistory::isEmpty() const
{
    return m_commands.isEmpty();
}

QStringList ReplHistory::getAll() const
{
    return m_commands;
}