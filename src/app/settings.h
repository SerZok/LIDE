#pragma once

#include <QFont>
#include <QColor>
#include <QString>
#include <QSettings>
#include <QApplication>

class Settings : public QObject
{
    Q_OBJECT

public:
    struct Key {
        // App
        static constexpr const char* WINDOW_STATE = "app/state";
        static constexpr const char* WINDOW_GEOMETRY = "app/geometry";
        static constexpr const char* APP_LANG = "app/language";
        static constexpr const char* APP_THEME = "app/theme";
        static constexpr const char* SAVE_TIME = "app/autosaveTime";
        static constexpr const char* SAVE_EXIT = "app/exitSave";

        // Editor
        static constexpr const char* EDITOR_FONT = "editor/font";
        static constexpr const char* EDITOR_TAB_SIZE = "editor/tabSize";
        static constexpr const char* EDITOR_OPENED_FILES = "editor/openedFiles";
        static constexpr const char* EDITOR_SHOW_LINE_NUMBERS = "editor/showLineNumbers";


        // Project widget
        //static constexpr const char* PROJECT_RECENT = "project/recent"; TODO: сохранять несколько проектов
        static constexpr const char* PROJECT_DEFAULT_PATH = "project/defaultPath";
        static constexpr const char* PROJECT_LAST_PATH = "project/lastPath";
        static constexpr const char* PROJECT_EXCLUDE_FILTERS = "project/excludeFilters";

        // SBCL settings
        static constexpr const char* SBCL_PATH = "sbcl/sbclPath";
        static constexpr const char* SBCL_ARGS = "sbcl/sbclArgs";

        // SBCL LOAD settings
        static constexpr const char* SBCL_LOAD_PRINT = "sbcl/loadPrint";
        static constexpr const char* SBCL_LOAD_VERBOSE = "sbcl/loadVerbose";
        static constexpr const char* SBCL_LOAD_IF_NOT_EXISTS = "sbcl/loadIfDoesNotExist";
        static constexpr const char* SBCL_LOAD_EXTERNAL_FORMAT = "sbcl/loadExternalFormat";

        // REPL
        static constexpr const char* REPL_MAX_LINES = "repl/maxLines";
        static constexpr const char* REPL_PARSE_MODE = "repl/parseMode"; // 0 - none; 1- simple; 2 - full
        static constexpr const char* REPL_CHUNK_SIZE = "repl/chunkSize";
        static constexpr const char* REPL_AUTO_RESTART = "repl/autoRestart";
        static constexpr const char* REPL_OUTPUT_DELAY = "repl/outputDelayMs";

        // Syntax highlighting colors
        static constexpr const char* SYNTAX_QUOTE = "syntax/quote";
        static constexpr const char* SYNTAX_STRING = "syntax/string";
        static constexpr const char* SYNTAX_NUMBER = "syntax/number";
        static constexpr const char* SYNTAX_KEYWORD = "syntax/keyword";
        static constexpr const char* SYNTAX_COMMENT = "syntax/comment";
        static constexpr const char* SYNTAX_FUNCTION = "syntax/function";
        static constexpr const char* SYNTAX_PARENTHESIS = "syntax/parenthesis";
    };

    enum class ParseMode {
        Minimal = 0,
        Simple = 1,
        Full = 2
    };
    Q_ENUM(ParseMode);

    static Settings* instance();

    template<typename T>
    T value(const QString& key, const T& defaultValue) const;

    template<typename T>
    void setValue(const QString& key, const T& value);

    static void retranslateAllWindows();

    // App
    QByteArray mainWindowGeometry() const;
    void setMainWindowGeometry(const QByteArray& geometry);

    QByteArray mainWindowState() const;
    void setMainWindowState(const QByteArray& state);

    QString currentLang() const;
    void setCurrentLang(const QString& locale);

    QString currentTheme() const;
    void setCurrentTheme(const QString& stylePath);

    int saveTime() const;
    void setSaveTime(int saveTime);

    bool saveExit() const;
    void setSaveExit(bool saveExit);

    // Editor settings
    QFont editorFont() const;
    void setEditorFont(const QFont& font);

    int tabSize() const;
    void setTabSize(int size);

    bool showLineNumbers() const;
    void setShowLineNumbers(bool show);

    QStringList openedFiles() const;
    void setOpenedFiles(const QStringList& files);

    // SBCL
    QString sbclPath() const;
    void setSbclPath(const QString& path);

    QStringList sbclArgs() const;
    void setSbclArgs(const QStringList& args);

    // SBCL LOAD settings
    QString sbclLoadArgsString() const;

    bool sbclLoadVerbose() const;
    void setSbclLoadVerbose(bool v);

    bool sbclLoadPrint() const;
    void setSbclLoadPrint(bool v);

    bool sbclLoadIfNotExists() const;
    void setSbclLoadIfNotExists(bool v);

    QString sbclLoadExternalFormat() const;
    void setSbclLoadExternalFormat(const QString& format);

    // REPL settings
    int replMaxLines() const;
    void setReplMaxLines(int n);

    ParseMode replParseMode() const;
    void setReplParseMode(ParseMode mode);

    int replChunkSize() const;
    void setReplChunkSize(int n);

    bool replAutoRestart() const;
    void setReplAutoRestart(bool v);

    int replOutputDelay() const;
    void setReplOutputDelay(int ms);

    // Project
    QString projectDefaultPath() const;
    void setProjectDefaultPath(const QString& path);

    QString projectExcludeFilters() const;
    QStringList projectExcludeFiltersList() const;
    void setProjectExcludeFilters(const QString& filters);


    QString lastProjectPath() const;
    void setLastProjectPath(const QString& path);

    //QStringList recentProjects() const;
    //void addRecentProject(const QString& path);
    //void clearRecentProjects();

    // Syntax highlighting colors
    QColor keywordColor() const;
    void setKeywordColor(const QColor& color);

    QColor functionColor() const;
    void setFunctionColor(const QColor& color);

    QColor commentColor() const;
    void setCommentColor(const QColor& color);

    QColor stringColor() const;
    void setStringColor(const QColor& color);

    QColor numberColor() const;
    void setNumberColor(const QColor& color);

    QColor parenthesisColor() const;
    void setParenthesisColor(const QColor& color);

    QColor quoteColor() const;
    void setQuoteColor(const QColor& color);

    // Utility methods
    void resetToDefaults();
    bool contains(const QString& key) const;
    void remove(const QString& key);

signals:
    void settingsChanged();

    void editorFontChanged(const QFont& font);
    void themeChanged(const QString& theme);
    void projectFilterChanged();

    void replMaxLineChanged();
    void replChunkSizeChanged();
    void replParseModeChanged();
    void replAutoRestartChanged();
    void replOutputTimeChanged();

    void sbclPathChanged();
    void sbclArgsChanged();
    void sbclLoadArgsChanged();

private:
    explicit Settings(QObject* parent = nullptr);
    ~Settings();

    static Settings* m_instance;
    QSettings m_settings;

    // Default values
    QFont m_defaultFont;
};