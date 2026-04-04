#include "settings_dialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>

SettingsDialog::SettingsDialog(QWidget* parent)
	: QDialog(parent),
	m_settings(Settings::instance())
{
	ui.setupUi(this);

	setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    // ComboBox'ы
    QStringList availableLocales = { "ru_RU", "en_US" };
    for (const QString& locale : availableLocales) {
        QLocale loc(locale);
        QString name = loc.name();
        ui.lang_comboBox->addItem(name, locale);
    }

    ui.theme_comboBox->addItem(tr("Светлая"), "light");
    ui.theme_comboBox->addItem(tr("Тёмная"), "dark");

    ui.parser_mode_comboBox->addItem(tr("Без парсинга"), 0);
    ui.parser_mode_comboBox->addItem(tr("Простой (ключевые слова)"), 1);
    ui.parser_mode_comboBox->addItem(tr("Полный (с выражениями)"), 2);

    ui.sbcl_file_format_comboBox->addItem("default", ":default");
    ui.sbcl_file_format_comboBox->addItem("utf-8", ":utf-8");
    ui.sbcl_file_format_comboBox->addItem("latin-1", ":latin-1");

    // Кнопки
    connect(ui.buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveFromUi();
        accept();
        });
    connect(ui.buttonBox, &QDialogButtonBox::rejected, this, &SettingsDialog::reject);

    connect(ui.reset_settings_button, &QPushButton::pressed, this, [this]() {
        if (QMessageBox::question(this, tr("Сброс"),
            tr("Сбросить настройки к значениям по умолчанию?"),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
            m_settings->resetToDefaults();

            loadToUi();
            updateSbclArgs();
            updateSbclLoad();
        }
        });

    // Browse кнопки
    connect(ui.project_default_browse_button, &QPushButton::clicked, this, &SettingsDialog::onBrowseProjectPath);
    connect(ui.sbcl_path_pushButton, &QPushButton::clicked, this, &SettingsDialog::onBrowseSBCLPath);

    // Live preview для load-команды
    connect(ui.sbcl_file_verbose_checkBox, &QCheckBox::toggled, this, &SettingsDialog::updateSbclLoad);
    connect(ui.sbcl_file_print_result_checkBox, &QCheckBox::toggled, this, &SettingsDialog::updateSbclLoad);
    connect(ui.sbcl_file_show_error_checkBox, &QCheckBox::toggled, this, &SettingsDialog::updateSbclLoad);
    connect(ui.sbcl_file_format_comboBox, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SettingsDialog::updateSbclLoad);

    // Live preview для аргументов SBCL
    connect(ui.sbcl_debug_checkBox, &QCheckBox::toggled, this, &SettingsDialog::updateSbclArgs);
    connect(ui.sbcl_no_inform_checkBox, &QCheckBox::toggled, this, &SettingsDialog::updateSbclArgs);
    connect(ui.sbcl_custom_args_textLine, &QLineEdit::textChanged, this, &SettingsDialog::updateSbclArgs);

    // Загрузка текущих значений
    loadToUi();
    updateSbclLoad();
    updateSbclArgs();
}

SettingsDialog::~SettingsDialog() = default;

void SettingsDialog::changeEvent(QEvent* event) {
    if (event->type() == QEvent::LanguageChange) {
        ui.retranslateUi(this);
    }
    QDialog::changeEvent(event);
}

void SettingsDialog::loadToUi()
{
    m_loading = true;

    // ── General ─────────────────────────────────────
    int langIdx = ui.lang_comboBox->findData(m_settings->currentLang());
    if (langIdx >= 0)
        ui.lang_comboBox->setCurrentIndex(langIdx);

    int themeIdx = ui.theme_comboBox->findData(m_settings->currentTheme());
    if (themeIdx >= 0)
        ui.theme_comboBox->setCurrentIndex(themeIdx);

    // ── Editor ──────────────────────────────────────
    ui.font_comboBox->setCurrentFont(m_settings->editorFont());
    ui.tabSize_spinBox->setValue(m_settings->tabSize());
    ui.showLineNum_checkBox->setChecked(m_settings->showLineNumbers());
    // ui.checkBox_2 = auto-brackets (TODO)
    // ui.checkBox_3 = auto-indent (TODO)

    // ── Console (REPL output limits) ─────────────────
    ui.timout_spinBox->setValue(m_settings->replOutputDelay());
    ui.chunkSize_spinBox->setValue(m_settings->replChunkSize());
    ui.maxLines_spinBox->setValue(m_settings->replMaxLines());

    // ── Project ──────────────────────────────────────
    ui.project_default_line->setText(m_settings->projectDefaultPath());
    ui.project_filter_files_line->setText(m_settings->projectExcludeFilters());

    // ── SBCL ─────────────────────────────────────────
    ui.sbcl_path_lineEdit->setText(m_settings->sbclPath());
    ui.sbcl_auto_restart_afterCrash_checkBox->setChecked(m_settings->replAutoRestart());

    int parserIdx = ui.parser_mode_comboBox->findData(m_settings->replParseMode());
    if (parserIdx >= 0)
        ui.parser_mode_comboBox->setCurrentIndex(parserIdx);

    // ── SBCL: Аргументы ─────────────────────────────────
    QStringList args = m_settings->sbclArgs();

    bool hasDebug = false;
    bool hasNoInform = false;
    QStringList custom;

    for (const QString& arg : args) {
        QString trimmed = arg.trimmed();  // убираем лишние пробелы

        if (trimmed == "--debug") {
            hasDebug = true;
        }
        else if (trimmed == "--noinform") {
            hasNoInform = true;
        }
        else if (trimmed != "--disable-debugger") {  // этот флаг игнорируем в UI
            custom << trimmed;
        }
    }

    ui.sbcl_debug_checkBox->setChecked(hasDebug);
    ui.sbcl_no_inform_checkBox->setChecked(hasNoInform);
    ui.sbcl_custom_args_textLine->setText(custom.join(" "));

    // Параметры загрузки файлов (SBCL LOAD)
    ui.sbcl_file_verbose_checkBox->setChecked(m_settings->sbclLoadVerbose());
    ui.sbcl_file_print_result_checkBox->setChecked(m_settings->sbclLoadPrint());

    // Инвертированная логика: чекбокс "Показывать ошибку" → храним "игнорировать"
    ui.sbcl_file_show_error_checkBox->setChecked(!m_settings->sbclLoadIfNotExists());

    int efIdx = ui.sbcl_file_format_comboBox->findData(m_settings->sbclLoadExternalFormat());
    if (efIdx >= 0)
        ui.sbcl_file_format_comboBox->setCurrentIndex(efIdx);

    m_loading = false;

    updateSbclLoad();
    updateSbclArgs();
}

void SettingsDialog::saveFromUi()
{
    if (m_loading) return;

    // ── General ─────────────────────────────────────
    m_settings->setCurrentLang(ui.lang_comboBox->currentData().toString());
    m_settings->setCurrentTheme(ui.theme_comboBox->currentData().toString());

    // ── Editor ──────────────────────────────────────
    m_settings->setEditorFont(ui.font_comboBox->currentFont());
    m_settings->setTabSize(ui.tabSize_spinBox->value());
    m_settings->setShowLineNumbers(ui.showLineNum_checkBox->isChecked());

    // ── Console (REPL output limits) ─────────────────
    m_settings->setReplOutputDelay(ui.timout_spinBox->value());
    m_settings->setReplChunkSize(ui.chunkSize_spinBox->value());
    m_settings->setReplMaxLines(ui.maxLines_spinBox->value());

    // ── Project ──────────────────────────────────────
    m_settings->setProjectDefaultPath(ui.project_default_line->text());
    m_settings->setProjectExcludeFilters(ui.project_filter_files_line->text());

    // ── SBCL ─────────────────────────────────────────
    m_settings->setSbclPath(ui.sbcl_path_lineEdit->text());
    m_settings->setReplAutoRestart(ui.sbcl_auto_restart_afterCrash_checkBox->isChecked());
    m_settings->setReplParseMode(ui.parser_mode_comboBox->currentData().toInt());

    // Формируем аргументы из чекбоксов + кастомных
    QStringList args;
    if (ui.sbcl_debug_checkBox->isChecked()) args << "--debug";
    if (ui.sbcl_no_inform_checkBox->isChecked()) args << "--noinform";

    QString custom = ui.sbcl_custom_args_textLine->text().trimmed();
    if (!custom.isEmpty()) {
        args << custom.split(' ', Qt::SkipEmptyParts);
    }
    m_settings->setSbclArgs(args);

    // ── SBCL LOAD settings ───────────────────────────
    m_settings->setSbclLoadVerbose(ui.sbcl_file_verbose_checkBox->isChecked());
    m_settings->setSbclLoadPrint(ui.sbcl_file_print_result_checkBox->isChecked());
    m_settings->setSbclLoadIfNotExists(!ui.sbcl_file_show_error_checkBox->isChecked());
    m_settings->setSbclLoadExternalFormat(ui.sbcl_file_format_comboBox->currentData().toString());

}

void SettingsDialog::updateSbclLoad()
{
    if (m_loading) return;

    // Пример пути для превью
    const QString demoPath = "file.lisp";

    QStringList keywords;
    if (ui.sbcl_file_verbose_checkBox->isChecked())
        keywords << ":verbose t";
    if (ui.sbcl_file_print_result_checkBox->isChecked())
        keywords << ":print t";

    // :if-does-not-exist
    if (!ui.sbcl_file_show_error_checkBox->isChecked())
        keywords << ":if-does-not-exist nil";

    // :external-format
    QString ef = ui.sbcl_file_format_comboBox->currentData().toString();
    if (ef != ":default")
        keywords << QString(":external-format %1").arg(ef);

    QString argsStr = keywords.isEmpty() ? "" : " " + keywords.join(" ");
    QString preview = QString("(load \"%1\"%2)").arg(demoPath, argsStr);

    ui.sbcl_file_resultTextLine->setText(preview);
}

void SettingsDialog::updateSbclArgs()
{
    if (m_loading) return;

    QStringList args;
    if (ui.sbcl_debug_checkBox->isChecked())
        args << "--debug";
    if (ui.sbcl_no_inform_checkBox->isChecked())
        args << "--noinform";

    QString custom = ui.sbcl_custom_args_textLine->text().trimmed();
    if (!custom.isEmpty()) {
        args << custom.split(' ', Qt::SkipEmptyParts);
    }

    // Формируем строку команды
    QString preview = "sbcl";
    for (const QString& arg : args) {
        preview += " " + arg;
    }

    ui.sbcl_args_resultText->setText(preview);
}

void SettingsDialog::onBrowseProjectPath()
{
    QString path = QFileDialog::getExistingDirectory(
        this,
        tr("Выберите папку проектов"),
        ui.project_default_line->text(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    if (!path.isEmpty()) {
        ui.project_default_line->setText(path);
    }
}

void SettingsDialog::onBrowseSBCLPath()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("Выберите исполняемый файл SBCL"),
        ui.sbcl_path_lineEdit->text(),
        tr("Executables (*)")
    );
    if (!path.isEmpty()) {
        ui.sbcl_path_lineEdit->setText(path);
        // Опционально: обновить превью аргументов, если путь влияет на доступные флаги
        updateSbclArgs();
    }
}