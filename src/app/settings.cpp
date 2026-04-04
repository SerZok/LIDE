#include "settings.h"

#include <QDir>
#include <QFile>
#include <QDialog>
#include <QMainWindow>
#include <QTranslator>
#include <QLibraryInfo>
#include <QApplication>
#include <QFontDatabase>
#include <QStandardPaths>

Settings* Settings::m_instance = nullptr;
static QTranslator* s_qtTranslator = nullptr;
static QTranslator* s_appTranslator = nullptr;

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

QString Settings::currentLang() const
{
    return value<QString>(Key::APP_LANG, QLocale::system().name());
}

void Settings::setCurrentLang(const QString& locale)
{
    // Сохраняем
    setValue(Key::APP_LANG, locale);

    // Удаляем старые переводчики
    if (s_qtTranslator) {
        QCoreApplication::removeTranslator(s_qtTranslator);
        delete s_qtTranslator;
        s_qtTranslator = nullptr;
    }
    if (s_appTranslator) {
        QCoreApplication::removeTranslator(s_appTranslator);
        delete s_appTranslator;
        s_appTranslator = nullptr;
    }

    // Qt переводы
    s_qtTranslator = new QTranslator(qApp);
    QString qtLocale = "qt_" + locale;
    QString qtTranslationsDir = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (s_qtTranslator->load(qtLocale, qtTranslationsDir)) {
        QCoreApplication::installTranslator(s_qtTranslator);
        qDebug() << qtLocale << " loaded from" << qtTranslationsDir;
    }
    else {
        qDebug() << qtLocale << "NOT loaded from" << qtTranslationsDir;
        delete s_qtTranslator;
        s_qtTranslator = nullptr;
    }

    // App переводы
    s_appTranslator = new QTranslator(qApp);
    QString appDir = QCoreApplication::applicationDirPath();
    QString appTranslationsDir = QDir(appDir).filePath("translations");
    QString appLocale = "LIDE_" + locale;

    if (s_appTranslator->load(appLocale, appTranslationsDir)) {
        qDebug() << appLocale << " loaded from" << appTranslationsDir;
        if (QCoreApplication::installTranslator(s_appTranslator)) {
            qDebug() << appLocale << "sucess installed!";
        }
        else {
            qDebug() << appLocale << "install error!!!";
        }
    }
    else {
        qDebug() << appLocale << "NOT loaded from" << appTranslationsDir;
        delete s_appTranslator;
        s_appTranslator = nullptr;
    }

    emit settingsChanged();

    qDebug() << "=== Language changed to:" << locale << "===";
    qDebug() << "Top-level widgets:" << QApplication::topLevelWidgets().size();
    retranslateAllWindows();
}

void Settings::retranslateAllWindows()
{
    for (QWidget* widget : QApplication::topLevelWidgets()) {
        QEvent langEvent(QEvent::LanguageChange);
        QCoreApplication::sendEvent(widget, &langEvent);

        if (auto* mw = qobject_cast<QMainWindow*>(widget)) {
            mw->update();
        }
    }
}


// Theme settings
QString Settings::currentTheme() const
{
    return value<QString>(Key::APP_THEME, "dark");
}

void Settings::setCurrentTheme(const QString& theme)
{
    setValue(Key::APP_THEME, theme);

    QFile file(QString(":styles/%1.css").arg(theme));
    if (file.open(QFile::ReadOnly)) {
        QString styleSheet = QLatin1String(file.readAll());
        qApp->setStyleSheet(styleSheet);
        file.close();

        emit themeChanged(theme);
    }
    else {
        qDebug() << "Ошибка загрузки темы: " << theme;
        return;
    }

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

//TODO: сохранять несколько проектов
// 
//QStringList Settings::recentProjects() const
//{
//    return value<QStringList>(Key::PROJECT_RECENT, QStringList());
//}
//
//void Settings::addRecentProject(const QString& path)
//{
//    QStringList recent = recentProjects();
//    recent.removeAll(path);
//    recent.prepend(path);
//    while (recent.size() > 10) {
//        recent.removeLast();
//    }
//    setValue(Key::PROJECT_RECENT, recent);
//}
//
//void Settings::clearRecentProjects()
//{
//    m_settings.remove(Key::PROJECT_RECENT);
//}

// SBCL
QString Settings::sbclPath() const
{
    if (m_settings.contains(Key::SBCL_PATH)) {
        QString saved = m_settings.value(Key::SBCL_PATH).toString();
        if (!saved.isEmpty()) return saved;
    }

    // Пробуем найти SBCL относительно исполняемого файла
    QString appDir = QCoreApplication::applicationDirPath();

#if defined(Q_OS_WIN)
    QString bundledSbcl = QDir(appDir).filePath("SBCL/sbcl.exe");
#else
    QString bundledSbcl = QDir(appDir).filePath("SBCL/sbcl");
#endif

    if (QFileInfo::exists(bundledSbcl)) {
        return bundledSbcl;
    }

    // 2. Fallback: ищем в PATH (просто "sbcl")
    return "sbcl";
}

void Settings::setSbclPath(const QString& path)
{
    setValue(Key::SBCL_PATH, path);
    emit settingsChanged();
}

QStringList Settings::sbclArgs() const
{
    return value<QStringList>(Key::SBCL_ARGS,
        QStringList() << "--noinform" << "--disable-debugger");
}

void Settings::setSbclArgs(const QStringList& args)
{
    setValue(Key::SBCL_ARGS, args);
    emit settingsChanged();
}

bool Settings::sbclLoadVerbose() const {
    return value<bool>(Key::SBCL_LOAD_VERBOSE, true);
}

void Settings::setSbclLoadVerbose(bool v) {
    setValue(Key::SBCL_LOAD_VERBOSE, v);
    emit settingsChanged();
}

// SBCL load

bool Settings::sbclLoadPrint() const {
    return value<bool>(Key::SBCL_LOAD_PRINT, true);
}

void Settings::setSbclLoadPrint(bool v) {
    setValue(Key::SBCL_LOAD_PRINT, v);
    emit settingsChanged();
}

bool Settings::sbclLoadIfNotExists() const {
    return value<bool>(Key::SBCL_LOAD_IF_NOT_EXISTS, true);
}

void Settings::setSbclLoadIfNotExists(bool v) {
    setValue(Key::SBCL_LOAD_IF_NOT_EXISTS, v);
    emit settingsChanged();
}

QString Settings::sbclLoadExternalFormat() const {
    return value<QString>(Key::SBCL_LOAD_VERBOSE, ":default");
}

void Settings::setSbclLoadExternalFormat(const QString& format) {
    setValue(Key::SBCL_LOAD_VERBOSE, format);
    emit settingsChanged();
}

// REPL settings
int Settings::replMaxLines() const {
    return value<int>(Key::REPL_MAX_LINES, 100);
}

void Settings::setReplMaxLines(int n) {
    setValue(Key::REPL_MAX_LINES, n);
    emit settingsChanged();
}

int Settings::replParseMode() const {
    return value<int>(Key::REPL_PARSE_MODE, 2);
}

void Settings::setReplParseMode(int mode) {
    setValue(Key::REPL_PARSE_MODE, mode);
    emit settingsChanged();
}

int Settings::replChunkSize() const {
    return value<int>(Key::REPL_CHUNK_SIZE, 100);
}

void Settings::setReplChunkSize(int n) {
    setValue(Key::REPL_CHUNK_SIZE, n);
    emit settingsChanged();
}

bool Settings::replAutoRestart() const {
    return value<bool>(Key::REPL_AUTO_RESTART, true);
}

void Settings::setReplAutoRestart(bool v) {
    setValue(Key::REPL_AUTO_RESTART, v);
    emit settingsChanged();
}

int Settings::replOutputDelay() const {
    return value<int>(Key::REPL_OUTPUT_DELAY, 25);
}

void Settings::setReplOutputDelay(int ms) {
    setValue(Key::REPL_OUTPUT_DELAY, ms);
    emit settingsChanged();
}

// Project
QString Settings::projectDefaultPath() const {
    return value<QString>(Key::PROJECT_DEFAULT_PATH, QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation));
}

void Settings::setProjectDefaultPath(const QString& path) {
    setValue(Key::PROJECT_DEFAULT_PATH, path);
    emit settingsChanged();
}

QString Settings::projectExcludeFilters() const {
    return value<QString>(Key::PROJECT_EXCLUDE_FILTERS, "*.lisp *.lsp");
}

void Settings::setProjectExcludeFilters(const QString& filters) {
    setValue(Key::PROJECT_EXCLUDE_FILTERS, filters);
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