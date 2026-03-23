#pragma once

#include <QSettings>
#include <QString>
#include <QFont>
#include <QColor>

class Settings : public QObject
{
    Q_OBJECT

public:
    struct Key {
        // Editor settings
        static constexpr const char* EDITOR_FONT = "editor/font";
        static constexpr const char* EDITOR_TAB_SIZE = "editor/tabSize";
        static constexpr const char* EDITOR_SHOW_LINE_NUMBERS = "editor/showLineNumbers";
        static constexpr const char* EDITOR_WORD_WRAP = "editor/wordWrap";
        static constexpr const char* EDITOR_OPENED_FILES = "editor/openedFiles";

        // Theme settings
        static constexpr const char* THEME_CURRENT = "theme/current";

        // Project settings
        static constexpr const char* PROJECT_LAST_PATH = "project/lastPath";
        static constexpr const char* PROJECT_RECENT = "project/recent";

        // Lisp interpreter settings
        static constexpr const char* LISP_INTERPRETER_PATH = "lisp/interpreterPath";
        static constexpr const char* LISP_INTERPRETER_ARGS = "lisp/interpreterArgs";

        // Window settings
        static constexpr const char* WINDOW_GEOMETRY = "window/geometry";
        static constexpr const char* WINDOW_STATE = "window/state";

        // Syntax highlighting colors
        static constexpr const char* SYNTAX_KEYWORD = "syntax/keyword";
        static constexpr const char* SYNTAX_FUNCTION = "syntax/function";
        static constexpr const char* SYNTAX_COMMENT = "syntax/comment";
        static constexpr const char* SYNTAX_STRING = "syntax/string";
        static constexpr const char* SYNTAX_NUMBER = "syntax/number";
        static constexpr const char* SYNTAX_PARENTHESIS = "syntax/parenthesis";
        static constexpr const char* SYNTAX_QUOTE = "syntax/quote";
    };

    static Settings* instance();

    // Editor settings
    QFont editorFont() const;
    void setEditorFont(const QFont& font);

    int tabSize() const;
    void setTabSize(int size);

    bool showLineNumbers() const;
    void setShowLineNumbers(bool show);

    bool wordWrap() const;
    void setWordWrap(bool wrap);

    QStringList openedFiles() const;
    void setOpenedFiles(const QStringList& files);

    // Styles settings
    QString currentTheme() const;
    void setCurrentTheme(const QString& stylePath);

    // Project settings
    QString lastProjectPath() const;
    void setLastProjectPath(const QString& path);

    QStringList recentProjects() const;
    void addRecentProject(const QString& path);
    void clearRecentProjects();

    // Lisp interpreter settings
    QString lispInterpreterPath() const;
    void setLispInterpreterPath(const QString& path);

    QStringList lispInterpreterArgs() const;
    void setLispInterpreterArgs(const QStringList& args);

    // Window settings
    QByteArray mainWindowGeometry() const;
    void setMainWindowGeometry(const QByteArray& geometry);

    QByteArray mainWindowState() const;
    void setMainWindowState(const QByteArray& state);

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

    template<typename T>
    T value(const QString& key, const T& defaultValue) const;

    template<typename T>
    void setValue(const QString& key, const T& value);

signals:
    void editorFontChanged(const QFont& font);
    void themeChanged(const QString& theme);
    void settingsChanged();

private:
    explicit Settings(QObject* parent = nullptr);
    ~Settings();

    static Settings* m_instance;
    QSettings m_settings;

    // Default values
    QFont m_defaultFont;
};