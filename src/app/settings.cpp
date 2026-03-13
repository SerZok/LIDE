#include "settings.h"
#include <QApplication>
#include <QFontDatabase>
#include <QDir>

Settings* Settings::m_instance = nullptr;

Settings* Settings::instance()
{
    if (!m_instance) {
        m_instance = new Settings(qApp);
    }
    return m_instance;
}

Settings::Settings(QObject* parent) : 
    QObject(parent)
    , m_settings(QSettings::Format::IniFormat, QSettings::UserScope, "LIDE", "LIDE")
{
    QSettings::Format format = m_settings.format();
    QString path = m_settings.fileName();
    qDebug() << "Файл настроек:" << path;

    m_defaultFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    m_defaultFont.setPointSize(11);
}

Settings::~Settings() = default;

template<typename T>
T Settings::value(const QString& key, const T& defaultValue) const
{
    return m_settings.value(key, defaultValue).template value<T>();
}

template<typename T>
void Settings::setValue(const QString& key, const T& value)
{
    m_settings.setValue(key, value);
}

// Editor settings
QFont Settings::editorFont() const
{
    return value<QFont>(Key::EDITOR_FONT, m_defaultFont);
}

void Settings::setEditorFont(const QFont& font)
{
    setValue(Key::EDITOR_FONT, font);
    emit editorFontChanged(font);
    emit settingsChanged();
}

int Settings::tabSize() const
{
    return value<int>(Key::EDITOR_TAB_SIZE, 4);
}

void Settings::setTabSize(int size)
{
    setValue(Key::EDITOR_TAB_SIZE, size);
    emit settingsChanged();
}

bool Settings::showLineNumbers() const
{
    return value<bool>(Key::EDITOR_SHOW_LINE_NUMBERS, true);
}

void Settings::setShowLineNumbers(bool show)
{
    setValue(Key::EDITOR_SHOW_LINE_NUMBERS, show);
    emit settingsChanged();
}

bool Settings::wordWrap() const
{
    return value<bool>(Key::EDITOR_WORD_WRAP, false);
}

void Settings::setWordWrap(bool wrap)
{
    setValue(Key::EDITOR_WORD_WRAP, wrap);
    emit settingsChanged();
}

QStringList Settings::openedFiles() const
{
    return m_settings.value(Key::EDITOR_OPENED_FILES).toStringList();
}

void Settings::setOpenedFiles(const QStringList& openedFiles)
{
    m_settings.setValue(Key::EDITOR_OPENED_FILES, openedFiles);
    emit settingsChanged();
}

// Theme settings
QString Settings::currentTheme() const
{
    return value<QString>(Key::THEME_CURRENT, "dark");
}

void Settings::setCurrentTheme(const QString& theme)
{
    setValue(Key::THEME_CURRENT, theme);
    emit themeChanged(theme);
    emit settingsChanged();
}

// Project settings
QString Settings::lastProjectPath() const
{
    return value<QString>(Key::PROJECT_LAST_PATH, QDir::homePath());
}

void Settings::setLastProjectPath(const QString& path)
{
    setValue(Key::PROJECT_LAST_PATH, path);
}

QStringList Settings::recentProjects() const
{
    return value<QStringList>(Key::PROJECT_RECENT, QStringList());
}

void Settings::addRecentProject(const QString& path)
{
    QStringList recent = recentProjects();
    recent.removeAll(path);
    recent.prepend(path);
    while (recent.size() > 10) {
        recent.removeLast();
    }
    setValue(Key::PROJECT_RECENT, recent);
}

void Settings::clearRecentProjects()
{
    m_settings.remove(Key::PROJECT_RECENT);
}

// Lisp interpreter settings
QString Settings::lispInterpreterPath() const
{
    return value<QString>(Key::LISP_INTERPRETER_PATH, "sbcl");
}

void Settings::setLispInterpreterPath(const QString& path)
{
    setValue(Key::LISP_INTERPRETER_PATH, path);
    emit settingsChanged();
}

QStringList Settings::lispInterpreterArgs() const
{
    return value<QStringList>(Key::LISP_INTERPRETER_ARGS,
        QStringList() << "--noinform" << "--disable-debugger");
}

void Settings::setLispInterpreterArgs(const QStringList& args)
{
    setValue(Key::LISP_INTERPRETER_ARGS, args);
    emit settingsChanged();
}

// Window settings
QByteArray Settings::mainWindowGeometry() const
{
    return value<QByteArray>(Key::WINDOW_GEOMETRY, QByteArray());
}

void Settings::setMainWindowGeometry(const QByteArray& geometry)
{
    setValue(Key::WINDOW_GEOMETRY, geometry);
}

QByteArray Settings::mainWindowState() const
{
    return value<QByteArray>(Key::WINDOW_STATE, QByteArray());
}

void Settings::setMainWindowState(const QByteArray& state)
{
    setValue(Key::WINDOW_STATE, state);
}

// Syntax highlighting colors
QColor Settings::keywordColor() const
{
    return value<QColor>(Key::SYNTAX_KEYWORD, QColor(255, 160, 0));
}

void Settings::setKeywordColor(const QColor& color)
{
    setValue(Key::SYNTAX_KEYWORD, color);
    emit settingsChanged();
}

// ... аналогично для других цветов ...

bool Settings::contains(const QString& key) const
{
    return m_settings.contains(key);
}

void Settings::remove(const QString& key)
{
    m_settings.remove(key);
    emit settingsChanged();
}

void Settings::resetToDefaults()
{
    m_settings.clear();
    emit settingsChanged();
}