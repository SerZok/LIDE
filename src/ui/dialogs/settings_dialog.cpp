#include "settings_dialog.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSizePolicy>
#include <QUrl>

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

    ui.parser_mode_comboBox->addItem(tr("Минимальный"), 0);
    ui.parser_mode_comboBox->addItem(tr("Простой"), 1);
    ui.parser_mode_comboBox->addItem(tr("Полный"), 2);

    ui.sbcl_file_format_comboBox->addItem("default", ":default");
    ui.sbcl_file_format_comboBox->addItem("utf-8", ":utf-8");
    ui.sbcl_file_format_comboBox->addItem("latin-1", ":latin-1");
    ui.ai_context_files_label->setText(tr("Файлы для RAG"));
    ui.ai_context_files_listWidget->setMinimumHeight(160);
    ui.ai_context_files_listWidget->setMinimumWidth(0);
    ui.ai_context_files_listWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui.ai_prompt_group->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui.gridLayout_ai->setRowStretch(0, 0);
    ui.gridLayout_ai->setRowStretch(1, 1);
    ui.ai_context_files_layout->setStretch(0, 1);
    ui.ai_context_files_layout->setStretch(1, 0);
    ui.ai_context_files_add_button->setFixedSize(28, 28);
    ui.ai_context_files_remove_button->setFixedSize(28, 28);

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
    connect(ui.ai_refresh_models_button, &QPushButton::clicked, this, &SettingsDialog::fetchAiModels);
    connect(ui.ai_embedding_refresh_models_button, &QPushButton::clicked,
            this, &SettingsDialog::fetchAiEmbeddingModels);
    connect(ui.ai_context_files_add_button, &QToolButton::clicked,
            this, &SettingsDialog::addContextFiles);
    connect(ui.ai_context_files_remove_button, &QToolButton::clicked,
            this, &SettingsDialog::removeContextFiles);
    connect(ui.ai_context_files_listWidget->selectionModel(),
            &QItemSelectionModel::selectionChanged, this,
            [this]() {
                ui.ai_context_files_remove_button->setEnabled(
                    !ui.ai_context_files_listWidget->selectedItems().isEmpty());
            });

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

    ui.auto_save_every_spinBox->setValue(m_settings->saveTime());
    ui.auto_save_exit_checkBox->setChecked(m_settings->saveExit());

    // ── Editor ──────────────────────────────────────
    ui.tabSize_spinBox->setValue(m_settings->tabSize());
    ui.showLineNum_checkBox->setChecked(m_settings->showLineNumbers());
    // auto-brackets (TODO)
    // auto-indent (TODO)

    // ── Console (REPL output limits) ─────────────────
    ui.timout_spinBox->setValue(m_settings->replOutputDelay());
    ui.chunkSize_spinBox->setValue(m_settings->replChunkSize());
    ui.maxLines_spinBox->setValue(m_settings->replMaxLines());

    // ── Project ──────────────────────────────────────
    ui.project_default_line->setText(m_settings->projectDefaultPath());
    ui.project_filter_files_line->setText(m_settings->projectExcludeFilters());

    // ── AI Assistant ──────────────────────────────────
    ui.ai_endpoint_lineEdit->setText(m_settings->aiEndpoint());
    ui.ai_model_comboBox->clear();
    ui.ai_model_comboBox->setCurrentText(m_settings->aiModel());
    ui.ai_api_key_lineEdit->setText(m_settings->aiApiKey());
    ui.ai_embedding_endpoint_lineEdit->setText(m_settings->aiEmbeddingEndpoint());
    ui.ai_embedding_model_comboBox->setCurrentText(m_settings->aiEmbeddingModel());
    ui.ai_context_files_listWidget->clear();
    for (const QString &path : m_settings->aiKnowledgeFiles())
    {
        QListWidgetItem *item = new QListWidgetItem(QFileInfo(path).fileName());
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        ui.ai_context_files_listWidget->addItem(item);
    }
    ui.ai_context_files_remove_button->setEnabled(false);
    ui.ai_system_prompt_textEdit->setPlainText(m_settings->aiSystemPrompt());

    // ── SBCL ─────────────────────────────────────────
    ui.sbcl_path_lineEdit->setText(m_settings->sbclPath());
    ui.sbcl_auto_restart_afterCrash_checkBox->setChecked(m_settings->replAutoRestart());

    int parserIdx = ui.parser_mode_comboBox->findData(static_cast<int>(m_settings->replParseOutputMode()));
    if (parserIdx >= 0)
        ui.parser_mode_comboBox->setCurrentIndex(parserIdx);

    // ── SBCL: Аргументы ─────────────────────────────────
    QStringList args = m_settings->sbclArgs();

    bool hasDebug = true;
    bool hasInform = true;
    QStringList custom;

    for (const QString& arg : args) {
        QString trimmed = arg.trimmed();  // убираем лишние пробелы

        if (trimmed == "--noinform") {
            hasInform = false;
        }
        else if (trimmed == "--disable-debugger") {
            hasDebug = false;
        }
        else if (trimmed.startsWith("--")) {
            custom << trimmed;
        }
    }

    ui.sbcl_debug_checkBox->setChecked(hasDebug);
    ui.sbcl_no_inform_checkBox->setChecked(hasInform);
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
    m_settings->setSaveTime(ui.auto_save_every_spinBox->value());
    m_settings->setSaveExit(ui.auto_save_exit_checkBox->isChecked());

    // ── Editor ──────────────────────────────────────
    m_settings->setTabSize(ui.tabSize_spinBox->value());
    m_settings->setShowLineNumbers(ui.showLineNum_checkBox->isChecked());

    // ── Console (REPL output limits) ─────────────────
    m_settings->setReplOutputDelay(ui.timout_spinBox->value());
    m_settings->setReplChunkSize(ui.chunkSize_spinBox->value());
    m_settings->setReplMaxLines(ui.maxLines_spinBox->value());

    // ── Project ──────────────────────────────────────
    m_settings->setProjectDefaultPath(ui.project_default_line->text());
    m_settings->setProjectExcludeFilters(ui.project_filter_files_line->text());

    // ── AI ──────────────────────────────────────
    m_settings->setAiApiKey(ui.ai_api_key_lineEdit->text());
    m_settings->setAiEndpoint(ui.ai_endpoint_lineEdit->text());
    m_settings->setAiModel(ui.ai_model_comboBox->currentText().trimmed());
    m_settings->setAiEmbeddingEndpoint(ui.ai_embedding_endpoint_lineEdit->text().trimmed());
    m_settings->setAiEmbeddingModel(ui.ai_embedding_model_comboBox->currentText().trimmed());
    QStringList knowledgeFiles;
    for (int i = 0; i < ui.ai_context_files_listWidget->count(); ++i)
        knowledgeFiles.append(
            ui.ai_context_files_listWidget->item(i)->data(Qt::UserRole).toString());
    m_settings->setAiKnowledgeFiles(knowledgeFiles);
    m_settings->setAiSystemPrompt(ui.ai_system_prompt_textEdit->toPlainText());

    // ── SBCL ─────────────────────────────────────────
    m_settings->setSbclPath(ui.sbcl_path_lineEdit->text());
    m_settings->setReplAutoRestart(ui.sbcl_auto_restart_afterCrash_checkBox->isChecked());
    m_settings->setReplOutputParseMode(static_cast<Settings::ParseOutputMode>(ui.parser_mode_comboBox->currentData().toInt()));

    // Аргументы из чекбоксов + кастомных
    QStringList args;
    if (!ui.sbcl_no_inform_checkBox->isChecked()) args << "--noinform";
    if (!ui.sbcl_debug_checkBox->isChecked()) args << "--disable-debugger";

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

void SettingsDialog::fetchAiModels()
{
    fetchModels(false);
}

void SettingsDialog::fetchAiEmbeddingModels()
{
    fetchModels(true);
}

void SettingsDialog::fetchModels(bool embedding)
{
    QLineEdit *endpointField = embedding ? ui.ai_embedding_endpoint_lineEdit
                                         : ui.ai_endpoint_lineEdit;
    QComboBox *modelCombo = embedding ? ui.ai_embedding_model_comboBox
                                      : ui.ai_model_comboBox;
    QPushButton *refreshButton = embedding ? ui.ai_embedding_refresh_models_button
                                           : ui.ai_refresh_models_button;
    QUrl endpoint(endpointField->text().trimmed());
    if (!endpoint.isValid() || (endpoint.scheme() != "http" && endpoint.scheme() != "https") || endpoint.host().isEmpty())
    {
        QMessageBox::warning(this, tr("Некорректный адрес"), tr("Укажите корректный URL HTTP-эндпоинта AI."));
        return;
    }

    QString path = endpoint.path();
    while (path.endsWith('/') && !path.isEmpty())
        path.chop(1);

    if (path.endsWith("/api/chat") || path.endsWith("/api/generate") ||
        path.endsWith("/api/embed"))
    {
        path = path.left(path.lastIndexOf("/api/")) + "/api/tags";
    }
    else if (path.endsWith("/v1/chat/completions") || path.endsWith("/v1/embeddings"))
    {
        path = path.left(path.lastIndexOf("/v1/")) + "/v1/models";
    }
    else if (!path.endsWith("/api/tags") && !path.endsWith("/v1/models"))
    {
        if (path.isEmpty())
            path = "/api/tags";
        else
        {
            const QString hint = embedding
                                     ? tr("Для Ollama укажите URL вида http://localhost:11434/api/embed, "
                                          "для OpenAI-совместимого API — URL вида .../v1/embeddings.")
                                     : tr("Для Ollama укажите URL вида http://localhost:11434/api/chat, "
                                          "для OpenAI-совместимого API — URL вида .../v1/chat/completions.");
            QMessageBox::warning(this, tr("Неизвестный эндпоинт"), hint);
            return;
        }
    }

    endpoint.setPath(path);
    endpoint.setQuery(QString());
    endpoint.setFragment(QString());

    QNetworkRequest request(endpoint);
    request.setTransferTimeout(10000);
    const QString apiKey = ui.ai_api_key_lineEdit->text().trimmed();
    if (!apiKey.isEmpty())
        request.setRawHeader("Authorization", ("Bearer " + apiKey).toUtf8());

    refreshButton->setEnabled(false);
    refreshButton->setText(tr("Загрузка..."));

    auto *networkManager = new QNetworkAccessManager(this);
    QNetworkReply *reply = networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, networkManager, reply, embedding]() {
        const QByteArray response = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString errorString = reply->errorString();
        reply->deleteLater();
        networkManager->deleteLater();

        QPushButton *refreshButton = embedding ? ui.ai_embedding_refresh_models_button
                                               : ui.ai_refresh_models_button;
        QComboBox *modelCombo = embedding ? ui.ai_embedding_model_comboBox
                                          : ui.ai_model_comboBox;
        refreshButton->setEnabled(true);
        refreshButton->setText(tr("Обновить"));

        if (networkError != QNetworkReply::NoError)
        {
            QMessageBox::warning(this, tr("Не удалось получить модели"),
                                 tr("Проверьте, что сервер запущен и доступен.\n%1").arg(errorString));
            return;
        }

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(response, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
        {
            QMessageBox::warning(this, tr("Ошибка ответа сервера"),
                                 tr("Сервер вернул некорректный список моделей."));
            return;
        }

        const QJsonObject root = document.object();
        QJsonArray models = root.value("models").toArray();
        if (models.isEmpty())
            models = root.value("data").toArray();

        QStringList modelNames;
        for (const QJsonValue &value : models)
        {
            if (!value.isObject())
                continue;

            const QJsonObject model = value.toObject();
            QString name = model.value("name").toString();
            if (name.isEmpty())
                name = model.value("id").toString();
            if (!name.isEmpty())
                modelNames.append(name);
        }

        if (modelNames.isEmpty())
        {
            QMessageBox::information(this, tr("Модели не найдены"),
                                     tr("Сервер не вернул модели. В Ollama сначала загрузите модель командой ollama pull."));
            return;
        }

        const QString selectedModel = modelCombo->currentText();
        modelCombo->clear();
        modelCombo->addItems(modelNames);
        modelCombo->setCurrentText(
            modelNames.contains(selectedModel) ? selectedModel : modelNames.first());
    });
}

void SettingsDialog::addContextFiles()
{
    const QStringList selectedFiles = QFileDialog::getOpenFileNames(
        this, tr("Добавить файлы контекста"), QString(),
        tr("Документы (*.pdf *.txt *.md *.rst *.html *.htm *.xml *.csv *.tsv *.json *.jsonl *.yaml *.yml *.toml *.ini *.cfg *.conf *.properties *.log *.sql *.lisp *.lsp *.cl *.h *.hh *.hpp *.c *.cc *.cpp *.cxx *.py *.pyi *.js *.jsx *.ts *.tsx *.java *.kt *.go *.rs *.cs *.php *.rb *.swift *.css *.scss *.sh *.bash *.ps1 *.bat *.cmd);;Все файлы (*)"));
    for (const QString &selectedPath : selectedFiles)
    {
        const QString path = QFileInfo(selectedPath).absoluteFilePath();
        bool alreadyAdded = false;
        for (int i = 0; i < ui.ai_context_files_listWidget->count(); ++i)
        {
            if (ui.ai_context_files_listWidget->item(i)->data(Qt::UserRole).toString() == path)
            {
                alreadyAdded = true;
                break;
            }
        }
        if (alreadyAdded)
            continue;

        QListWidgetItem *item = new QListWidgetItem(QFileInfo(path).fileName());
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        ui.ai_context_files_listWidget->addItem(item);
    }
}

void SettingsDialog::removeContextFiles()
{
    const QList<QListWidgetItem *> selectedItems =
        ui.ai_context_files_listWidget->selectedItems();
    for (QListWidgetItem *item : selectedItems)
        delete item;
    ui.ai_context_files_remove_button->setEnabled(false);
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
    //--noinform - должен быть первым
    if (!ui.sbcl_no_inform_checkBox->isChecked())
        args << "--noinform";
    if (!ui.sbcl_debug_checkBox->isChecked())
        args << "--disable-debugger";

    QString custom = ui.sbcl_custom_args_textLine->text().trimmed();
    if (!custom.isEmpty()) {
        args << custom.split(' ', Qt::SkipEmptyParts);
    }

    // Формируем строку команды1
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
