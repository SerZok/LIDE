#include "benchmark_dialog.h"

#include "app/settings.h"
#include "app/version.h"
#include "util/benchmark/sbcl_sandbox.h"

#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPalette>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTableWidget>
#include <QHeaderView>
#include <QThread>
#include <QTimer>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>
#include <QAbstractItemView>
#include <QTableWidgetItem>
#include <QMetaObject>
#include <QTextDocument>

#include <algorithm>

namespace
{
QString markdownBody(const QString &markdown)
{
    QTextDocument document;
    document.setMarkdown(markdown, QTextDocument::MarkdownNoHTML);
    const QString rendered = document.toHtml();
    const qsizetype bodyStart = rendered.indexOf(QStringLiteral("<body"));
    if (bodyStart < 0)
        return markdown.toHtmlEscaped();

    const qsizetype contentStart = rendered.indexOf(QLatin1Char('>'), bodyStart);
    const qsizetype bodyEnd = rendered.lastIndexOf(QStringLiteral("</body>"));
    if (contentStart < 0 || bodyEnd < contentStart)
        return markdown.toHtmlEscaped();

    return rendered.mid(contentStart + 1, bodyEnd - contentStart - 1);
}
}

BenchmarkDialog::BenchmarkDialog(QWidget *parent)
    : QDialog(parent),
      m_client(AiClient::instance()),
      m_repetitions(new QSpinBox(this)),
      m_progress(new QProgressBar(this)),
      m_status(new QLabel(this)),
      m_qualitySummary(new QLabel(tr("Оценок пока нет."), this)),
      m_results(new QTableWidget(this)),
      m_resultDetails(new QTextBrowser(this)),
      m_evaluation(new QComboBox(this)),
      m_evaluationNotes(new QPlainTextEdit(this)),
      m_runButton(new QPushButton(tr("Запустить тесты"), this)),
      m_cancelButton(new QPushButton(tr("Остановить"), this)),
      m_openReportButton(new QPushButton(tr("Открыть папку отчётов"), this)),
      m_saveEvaluationButton(new QPushButton(tr("Сохранить оценку"), this)),
      m_tasks(benchmarkTasks())
{
    setWindowTitle(tr("Исследовательский бенчмарк LIDE"));
    resize(1000, 720);
    setMinimumSize(800, 600);
    setModal(true);

    m_repetitions->setRange(1, 20);
    m_repetitions->setValue(5);
    m_repetitions->setToolTip(tr("Число повторов каждой задачи для оценки разброса"));

    m_progress->setRange(0, 1);
    m_progress->setValue(0);
    m_status->setWordWrap(true);
    m_status->setText(
        tr("%1 задач, %2 запросов с учётом повторов. Ответы и метрики сохраняются в JSON-отчёт.")
            .arg(m_tasks.size())
            .arg(m_tasks.size() * m_repetitions->value()));
    m_results->setColumnCount(9);
    m_results->setHorizontalHeaderLabels({
        tr("Задача"), tr("Повтор"), tr("Статус"), tr("Время, мс"),
        tr("Вход, токены"), tr("Ответ, токены"), tr("Скорость, ток/с"),
        tr("SBCL"), tr("Оценка")});
    m_results->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_results->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_results->horizontalHeader()->setMinimumSectionSize(78);
    m_results->verticalHeader()->setVisible(false);
    m_results->verticalHeader()->setDefaultSectionSize(30);
    m_results->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_results->setSelectionMode(QAbstractItemView::SingleSelection);
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->setAlternatingRowColors(true);
    m_results->setObjectName("benchmarkResults");
    m_resultDetails->setObjectName("benchmarkResponse");
    m_resultDetails->setOpenLinks(false);
    m_resultDetails->setOpenExternalLinks(false);
    m_resultDetails->setFrameShape(QFrame::NoFrame);
    m_resultDetails->setTextInteractionFlags(Qt::TextSelectableByMouse | Qt::LinksAccessibleByMouse);
    m_resultDetails->setPlaceholderText(tr("Выберите завершённый запрос, чтобы посмотреть ответ и критерии оценки."));
    m_evaluation->setObjectName("benchmarkEvaluation");
    m_evaluation->addItem(tr("Выберите оценку"), QString());
    m_evaluation->addItem(tr("Успешно"), QStringLiteral("successful"));
    m_evaluation->addItem(tr("Не успешно"), QStringLiteral("unsuccessful"));
    m_evaluation->setCurrentIndex(0);
    m_evaluationNotes->setMaximumHeight(70);
    m_evaluationNotes->setObjectName("benchmarkEvaluationNotes");
    m_evaluationNotes->setPlaceholderText(tr("Заметки к оценке (необязательно)"));
    m_qualitySummary->setObjectName("benchmarkQualitySummary");
    m_qualitySummary->setWordWrap(true);
    m_saveEvaluationButton->setObjectName("benchmarkSaveEvaluation");
    m_saveEvaluationButton->setEnabled(false);

    m_cancelButton->setEnabled(false);
    m_openReportButton->setEnabled(false);

    auto *repetitionLayout = new QHBoxLayout();
    repetitionLayout->addWidget(new QLabel(tr("Повторов на задачу:"), this));
    repetitionLayout->addWidget(m_repetitions);
    repetitionLayout->addStretch();
    repetitionLayout->addWidget(m_runButton);
    repetitionLayout->addWidget(m_cancelButton);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(
        tr("Фиксированный набор задач по Common Lisp. Используются текущие настройки модели; "
           "температура фиксирована на 0,2, максимум ответа — 512 токенов. Тестовые запросы, "
           "системная инструкция, ответы и метрики сохраняются в отчёте."),
        this));
    layout->addLayout(repetitionLayout);
    layout->addWidget(m_progress);
    layout->addWidget(m_status);
    auto *resultSplitter = new QSplitter(Qt::Vertical, this);
    resultSplitter->addWidget(m_results);
    auto *detailsPanel = new QWidget(resultSplitter);
    auto *detailsLayout = new QVBoxLayout(detailsPanel);
    detailsLayout->setContentsMargins(0, 0, 0, 0);
    detailsLayout->setSpacing(6);
    detailsLayout->addWidget(new QLabel(tr("Ответ и критерии оценки"), detailsPanel));
    detailsLayout->addWidget(m_resultDetails, 1);
    resultSplitter->addWidget(detailsPanel);
    resultSplitter->setStretchFactor(0, 3);
    resultSplitter->setStretchFactor(1, 2);
    layout->addWidget(resultSplitter, 1);
    layout->addWidget(m_qualitySummary);
    auto *evaluationLayout = new QHBoxLayout();
    evaluationLayout->addWidget(new QLabel(tr("Оценка:"), this));
    evaluationLayout->addWidget(m_evaluation);
    evaluationLayout->addWidget(m_evaluationNotes, 1);
    evaluationLayout->addWidget(m_saveEvaluationButton);
    layout->addLayout(evaluationLayout);
    layout->addWidget(m_openReportButton, 0, Qt::AlignRight);

    connect(m_runButton, &QPushButton::clicked, this, &BenchmarkDialog::startBenchmark);
    connect(m_cancelButton, &QPushButton::clicked, this, &BenchmarkDialog::cancelBenchmark);
    connect(m_openReportButton, &QPushButton::clicked, this, &BenchmarkDialog::openReportFolder);
    connect(m_saveEvaluationButton, &QPushButton::clicked,
            this, &BenchmarkDialog::saveEvaluation);
    connect(m_evaluation, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BenchmarkDialog::updateEvaluationControls);
    connect(m_results, &QTableWidget::currentCellChanged,
            this, &BenchmarkDialog::showSelectedResult);
    connect(m_repetitions, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int repetitions) {
                m_status->setText(
                    tr("%1 задач, %2 запросов с учётом повторов. Ответы и метрики сохраняются в JSON-отчёт.")
                        .arg(m_tasks.size())
                        .arg(m_tasks.size() * repetitions));
            });
    connect(m_client, &AiClient::benchmarkRequestFinished,
            this, &BenchmarkDialog::onRequestFinished);
    connect(m_client, &AiClient::benchmarkReportError, this, [this](const QString &error) {
        m_status->setText(error);
    });
}

QVector<BenchmarkDialog::BenchmarkTask> BenchmarkDialog::benchmarkTasks()
{
    return {
        {
            "explain-recursion",
            BenchmarkDialog::tr("Объяснение рекурсивной функции"),
            QStringLiteral(
                "Объясни, что вычисляет этот код на Common Lisp, как работает базовый случай "
                "и что вернётся для аргументов 0 и 5. Не переписывай функцию.\n\n"
                "(defun factorial (n)\n"
                "  (if (zerop n)\n"
                "      1\n"
                "      (* n (factorial (1- n)))))"),
            BenchmarkDialog::tr("Корректно определить факториал, объяснить рекурсивный шаг и базовый "
                        "случай; указать результаты для 0 и 5 без фактических ошибок."),
            {}, {}
        },
        {
            "implement-count",
            BenchmarkDialog::tr("Реализация функции и примеры"),
            QStringLiteral(
                "Напиши функцию Common Lisp COUNT-OCCURRENCES, которая принимает элемент и "
                "список и возвращает число вхождений элемента. Используй сравнение EQL. "
                "Приведи код функции в блоке ```lisp ... ``` и три проверочных вызова "
                "с ожидаемыми результатами."),
            BenchmarkDialog::tr("Функция должна работать для пустого списка, учитывать EQL, "
                        "возвращать целое число и давать согласованные примеры."),
            "COUNT-OCCURRENCES",
            {{"duplicates", "2 '(2 3 2)", "2"},
             {"empty-list", "'a nil", "0"},
             {"symbol-matches", "'a '(a b a)", "2"}}
        },
        {
            "debug-empty-list",
            BenchmarkDialog::tr("Поиск и исправление ошибки"),
            QStringLiteral(
                "Функция должна вернуть первый положительный элемент списка или NIL, если "
                "такого элемента нет. Найди проблему в коде и предложи исправленную функцию. "
                "Объясни поведение для пустого списка. Верни исправленную функцию "
                "в блоке ```lisp ... ```.\n\n"
                "(defun first-positive (numbers)\n"
                "  (if (> (first numbers) 0)\n"
                "      (first numbers)\n"
                "      (first-positive (rest numbers))))"),
            BenchmarkDialog::tr("Обнаружить отсутствие обработки пустого списка и бесконечную рекурсию "
                        "для списка без положительных значений; исправление должно завершаться "
                        "и возвращать NIL при отсутствии результата."),
            "FIRST-POSITIVE",
            {{"first-positive", "'(-1 0 4 9)", "4"},
             {"no-positive", "'(-2 0)", "cl:nil"},
             {"empty-list", "cl:nil", "cl:nil"}}
        },
        {
            "stable-unique",
            BenchmarkDialog::tr("Обработка списка с сохранением порядка"),
            QStringLiteral(
                "Реализуй функцию UNIQUE-PRESERVING-ORDER для Common Lisp: она удаляет "
                "повторные элементы списка, сохраняя первое вхождение и исходный порядок. "
                "Сравнивай элементы через EQUAL. Приведи проверки для пустого списка и "
                "списка (a b a c b). Помести реализацию в блок ```lisp ... ```."),
            BenchmarkDialog::tr("Результат для (a b a c b) должен быть (a b c), порядок первых "
                        "вхождений сохранён, пустой список обрабатывается."),
            "UNIQUE-PRESERVING-ORDER",
            {{"remove-duplicates", "'(a b a c b)", "'(a b c)"},
             {"empty-list", "cl:nil", "cl:nil"}}
        },
        {
            "reduce-empty",
            BenchmarkDialog::tr("Разбор поведения стандартной функции"),
            QStringLiteral(
                "Реализуй функцию SUM-LIST на Common Lisp, используя REDUCE с "
                ":INITIAL-VALUE 0. Объясни, почему начальное значение важно для пустого "
                "списка. Верни функцию в блоке ```lisp ... ```."),
            BenchmarkDialog::tr("Объяснить отсутствие начального значения при пустой последовательности "
                        "и предложить корректный вариант, возвращающий 0 для пустого списка."),
            "SUM-LIST",
            {{"sum", "'(1 2 3 4)", "10"},
             {"empty-list", "cl:nil", "0"}}
        }
    };
}

QJsonArray BenchmarkDialog::taskDefinitions() const
{
    QJsonArray tasks;
    for (const BenchmarkTask &task : m_tasks)
    {
        QJsonArray tests;
        for (const SbclSandbox::TestCase &test : task.sbclTests)
        {
            tests.append(QJsonObject{
                {"name", test.name},
                {"arguments", test.arguments},
                {"expected", test.expected}});
        }
        tasks.append(QJsonObject{
            {"id", task.id},
            {"title", task.title},
            {"prompt", task.prompt},
            {"evaluationRubric", task.rubric},
            {"sbclFunction", task.sbclFunction},
            {"sbclTests", tests}});
    }
    return tasks;
}

void BenchmarkDialog::startBenchmark()
{
    if (Settings::instance()->aiModel().trimmed().isEmpty())
    {
        QMessageBox::warning(this, tr("Модель не выбрана"),
                             tr("Сначала выберите модель в настройках AI."));
        return;
    }
    const QUrl endpoint(Settings::instance()->aiEndpoint());
    if (!endpoint.isValid() ||
        (endpoint.scheme() != "http" && endpoint.scheme() != "https") ||
        endpoint.host().isEmpty())
    {
        QMessageBox::warning(this, tr("Некорректный адрес"),
                             tr("Проверьте адрес AI-сервера в настройках."));
        return;
    }

    m_totalRequests = m_tasks.size() * m_repetitions->value();
    if (!m_client->beginBenchmarkRun("lide-common-lisp-v2-sbcl", m_repetitions->value(),
                                     taskDefinitions()))
    {
        QMessageBox::warning(
            this, tr("Не удалось начать бенчмарк"),
            tr("Уже выполняется запрос или другой бенчмарк. Дождитесь его завершения и повторите."));
        return;
    }

    m_nextRequest = 0;
    m_succeededRequests = 0;
    m_cancelRequested = false;
    m_running = true;
    m_repetitions->setEnabled(false);
    m_runButton->setEnabled(false);
    m_cancelButton->setEnabled(true);
    m_openReportButton->setEnabled(false);
    m_saveEvaluationButton->setEnabled(false);
    m_results->setRowCount(0);
    m_requestResults.clear();
    m_selectedRow = -1;
    m_resultDetails->clear();
    m_evaluation->setCurrentIndex(0);
    for (int repetition = 1; repetition <= m_repetitions->value(); ++repetition)
    {
        for (const BenchmarkTask &task : m_tasks)
        {
            const int row = m_results->rowCount();
            m_results->insertRow(row);
            auto *taskItem = new QTableWidgetItem(task.title);
            taskItem->setData(Qt::UserRole, task.id);
            m_results->setItem(row, 0, taskItem);
            m_results->setItem(row, 1, new QTableWidgetItem(QString::number(repetition)));
            m_results->setItem(row, 2, new QTableWidgetItem(tr("Ожидание")));
            m_results->setItem(row, 3, new QTableWidgetItem("—"));
            m_results->setItem(row, 4, new QTableWidgetItem("—"));
            m_results->setItem(row, 5, new QTableWidgetItem("—"));
            m_results->setItem(row, 6, new QTableWidgetItem("—"));
            m_results->setItem(row, 7, new QTableWidgetItem("—"));
            m_results->setItem(row, 8, new QTableWidgetItem("—"));
        }
    }
    m_progress->setRange(0, m_totalRequests);
    m_progress->setValue(0);
    updateQualitySummary();
    m_status->setText(tr("Модель: %1").arg(Settings::instance()->aiModel()));
    QTimer::singleShot(0, this, &BenchmarkDialog::runNextRequest);
}

void BenchmarkDialog::runNextRequest()
{
    if (!m_running || m_validationRunning)
        return;

    if (m_cancelRequested || m_nextRequest >= m_totalRequests)
    {
        finishBenchmark(m_cancelRequested);
        return;
    }

    const int taskIndex = m_nextRequest % m_tasks.size();
    m_activeRepetition = m_nextRequest / m_tasks.size() + 1;
    const BenchmarkTask &task = m_tasks.at(taskIndex);
    m_activeTaskId = task.id;
    m_status->setText(tr("Задача %1 из %2: %3, повтор %4 из %5")
                          .arg(taskIndex + 1)
                          .arg(m_tasks.size())
                          .arg(task.title)
                          .arg(m_activeRepetition)
                          .arg(m_repetitions->value()));
    m_client->sendBenchmarkRequest(task.id, task.prompt, m_activeRepetition);
}

void BenchmarkDialog::onRequestFinished(const QString &taskId, int repetition,
                                        const QJsonObject &result)
{
    if (!m_running || taskId != m_activeTaskId || repetition != m_activeRepetition)
        return;

    const auto task = std::find_if(m_tasks.cbegin(), m_tasks.cend(),
                                   [&taskId](const BenchmarkTask &candidate) {
                                       return candidate.id == taskId;
                                   });
    if (task == m_tasks.cend())
        return;

    const bool success = result.value("success").toBool();
    if (success)
        ++m_succeededRequests;
    else
        m_status->setText(tr("Ошибка запроса %1, повтор %2: %3")
                              .arg(taskId)
                              .arg(repetition)
                              .arg(result.value("error").toString()));

    const int row = (repetition - 1) * m_tasks.size() + (m_nextRequest % m_tasks.size());
    m_requestResults.insert(resultKey(taskId, repetition), result);
    m_results->item(row, 2)->setText(success ? tr("Готово") : tr("Ошибка"));
    m_results->item(row, 3)->setText(
        result.contains("elapsedMs") ? QString::number(result.value("elapsedMs").toInteger()) : "—");
    m_results->item(row, 4)->setText(
        result.contains("promptTokens") ? QString::number(result.value("promptTokens").toInteger()) : "—");
    m_results->item(row, 5)->setText(
        result.contains("completionTokens") ? QString::number(result.value("completionTokens").toInteger()) : "—");
    const double speed = result.value("generationTokensPerSecond").toDouble(
        result.value("endToEndTokensPerSecond").toDouble(-1.0));
    m_results->item(row, 6)->setText(speed >= 0.0 ? QString::number(speed, 'f', 1) : "—");
    m_results->item(row, 7)->setText(
        task->sbclTests.isEmpty() ? "—" : (success ? tr("Ожидание") : tr("Не запускалось")));
    m_results->item(row, 8)->setText("—");
    updateSummary();
    m_results->selectRow(row);
    showSelectedResult(row, 0);

    ++m_nextRequest;
    m_progress->setValue(m_nextRequest);
    if (success && !task->sbclTests.isEmpty())
    {
        startSbclValidation(row, taskId, repetition,
                            result.value("response").toString(), *task);
        return;
    }
    QTimer::singleShot(0, this, &BenchmarkDialog::runNextRequest);
}

void BenchmarkDialog::startSbclValidation(int row, const QString &taskId, int repetition,
                                           const QString &response, const BenchmarkTask &task)
{
    m_validationRunning = true;
    m_status->setText(tr("Запуск автоматических тестов в изолированном SBCL..."));
    const QString sbclExecutable = Settings::instance()->sbclPath();
    const QString functionName = task.sbclFunction;
    const QVector<SbclSandbox::TestCase> tests = task.sbclTests;
    QPointer<BenchmarkDialog> dialog(this);
    m_validationThread = QThread::create(
        [dialog, sbclExecutable, response, functionName, tests, row, taskId, repetition]() {
            const QJsonObject validation =
                SbclSandbox::run(sbclExecutable, response, functionName, tests);
            if (!dialog)
                return;
            QMetaObject::invokeMethod(
                dialog,
                [dialog, row, taskId, repetition, validation]() {
                    if (dialog)
                        dialog->finishSbclValidation(row, taskId, repetition, validation);
                },
                Qt::QueuedConnection);
        });
    connect(m_validationThread, &QThread::finished,
            m_validationThread, &QObject::deleteLater);
    m_validationThread->start();
}

void BenchmarkDialog::finishSbclValidation(int row, const QString &taskId, int repetition,
                                           const QJsonObject &result)
{
    m_validationThread = nullptr;
    m_validationRunning = false;
    QJsonObject requestResult = m_requestResults.value(resultKey(taskId, repetition));
    requestResult.insert("sbclTest", result);
    m_requestResults.insert(resultKey(taskId, repetition), requestResult);

    const QString status = result.value("status").toString();
    QString displayStatus = status;
    if (status == "passed")
        displayStatus = tr("Пройдено %1/%2").arg(result.value("passedTests").toInt())
                            .arg(result.value("totalTests").toInt());
    else if (status == "failed")
        displayStatus = tr("Не пройдено %1/%2").arg(result.value("passedTests").toInt())
                            .arg(result.value("totalTests").toInt());
    else if (status == "timeout")
        displayStatus = tr("Тайм-аут");
    else if (status == "no-code")
        displayStatus = tr("Код не найден");
    else if (status == "sandbox-error")
        displayStatus = tr("Ошибка песочницы");
    else if (status == "unavailable")
        displayStatus = tr("SBCL недоступен");

    m_results->item(row, 7)->setText(displayStatus);
    if (m_selectedRow == row)
        showSelectedResult(row, 0);
    if (!m_client->setBenchmarkExecutionResult(taskId, repetition, result))
        QMessageBox::warning(this, tr("Не удалось сохранить результат SBCL"),
                             tr("Результат автоматической проверки не записан в JSON-отчёт."));

    if (m_cancelRequested)
        finishBenchmark(true);
    else
        QTimer::singleShot(0, this, &BenchmarkDialog::runNextRequest);
}

void BenchmarkDialog::cancelBenchmark()
{
    if (!m_running)
        return;
    m_cancelRequested = true;
    m_cancelButton->setEnabled(false);
    m_status->setText(m_validationRunning
                          ? tr("Остановлю бенчмарк после завершения проверки SBCL...")
                          : tr("Остановлю бенчмарк после текущего запроса..."));
}

void BenchmarkDialog::finishBenchmark(bool canceled)
{
    m_client->endBenchmarkRun(canceled);
    m_running = false;
    m_repetitions->setEnabled(true);
    m_runButton->setEnabled(true);
    m_cancelButton->setEnabled(false);
    m_openReportButton->setEnabled(!m_client->benchmarkReportPath().isEmpty());
    m_status->setText(
        tr("%1. Успешно: %2 из %3. Среднее время: %4 мс.\nОтчёт: %5")
            .arg(canceled ? tr("Бенчмарк остановлен") : tr("Бенчмарк завершён"))
            .arg(m_succeededRequests)
            .arg(m_nextRequest)
            .arg(m_requestResults.isEmpty()
                     ? QStringLiteral("—")
                     : QString::number([this]() {
                           qint64 total = 0;
                           int count = 0;
                           for (const QJsonObject &result : m_requestResults)
                           {
                               if (result.contains("elapsedMs"))
                               {
                                   total += result.value("elapsedMs").toInteger();
                                   ++count;
                               }
                           }
                           return count ? total / count : 0;
                       }()))
            .arg(QDir::toNativeSeparators(m_client->benchmarkReportPath())));
    updateSummary();
    if (m_results->rowCount() > 0)
        m_results->resizeColumnsToContents();
    if (m_selectedRow >= 0)
        showSelectedResult(m_selectedRow, 0);
    if (m_closeWhenFinished)
        QTimer::singleShot(0, this, &QDialog::close);
}

void BenchmarkDialog::showSelectedResult(int row, int)
{
    m_selectedRow = row;
    if (row < 0 || !m_results->item(row, 0))
    {
        m_resultDetails->clear();
        m_saveEvaluationButton->setEnabled(false);
        return;
    }

    const QString taskId = m_results->item(row, 0)->data(Qt::UserRole).toString();
    const int repetition = m_results->item(row, 1)->text().toInt();
    const auto task = std::find_if(m_tasks.cbegin(), m_tasks.cend(),
                                   [&taskId](const BenchmarkTask &candidate) {
                                       return candidate.id == taskId;
                                   });
    if (task == m_tasks.cend())
        return;

    const QJsonObject result = m_requestResults.value(resultKey(taskId, repetition));
    const QString response = result.isEmpty()
                                 ? tr("Запрос ещё не завершён.")
                                 : result.value("response").toString(
                                       result.value("error").toString(tr("Ответ отсутствует.")));
    const QJsonObject sbclTest = result.value("sbclTest").toObject();
    const QColor textColor = m_resultDetails->palette().color(QPalette::Text);
    const QColor baseColor = m_resultDetails->palette().color(QPalette::Base);
    const QColor accentColor = m_resultDetails->palette().color(QPalette::Highlight);
    QString html = QStringLiteral(
        "<html><head><style>"
        "body { color: %1; background: %2; font-family: 'Segoe UI', sans-serif; "
        "font-size: 10pt; line-height: 1.4; }"
        "h2 { margin: 0 0 12px; font-size: 14pt; }"
        "h3 { color: %3; margin: 14px 0 6px; font-size: 10pt; }"
        "pre { white-space: pre-wrap; overflow-wrap: anywhere; margin: 0; padding: 10px; "
        "background: %4; border-left: 3px solid %3; font-family: Consolas, monospace; }"
        ".response p { margin: 0 0 8px; }"
        ".response pre { margin: 8px 0; border-radius: 4px; }"
        ".response code { font-family: Consolas, monospace; }"
        ".response a { color: %3; }"
        "</style></head><body>")
                            .arg(textColor.name(QColor::HexRgb),
                                 baseColor.name(QColor::HexRgb),
                                 accentColor.name(QColor::HexRgb),
                                 baseColor.darker(108).name(QColor::HexRgb));
    html += QStringLiteral("<h2>%1 — %2</h2>")
                .arg(task->title.toHtmlEscaped(),
                     tr("Повтор %1").arg(repetition).toHtmlEscaped());
    const auto appendSection = [&html](const QString &title, const QString &content) {
        html += QStringLiteral("<h3>%1</h3><pre>%2</pre>")
                    .arg(title.toHtmlEscaped(), content.toHtmlEscaped());
    };
    const auto appendMarkdownSection = [&html](const QString &title, const QString &content) {
        html += QStringLiteral("<h3>%1</h3><div class=\"response\">%2</div>")
                    .arg(title.toHtmlEscaped(), markdownBody(content));
    };
    appendSection(tr("Запрос"), task->prompt);
    appendSection(tr("Критерии оценки"), task->rubric);
    appendMarkdownSection(tr("Ответ модели"), response);
    if (!sbclTest.isEmpty())
    {
        const QString output = sbclTest.value("output").toString().isEmpty()
                                   ? sbclTest.value("message").toString(
                                         sbclTest.value("status").toString())
                                   : sbclTest.value("output").toString();
        appendSection(tr("Проверка SBCL"), output);
    }
    html += QStringLiteral("</body></html>");
    m_resultDetails->setHtml(html);

    const QTableWidgetItem *evaluationItem = m_results->item(row, 8);
    const QString savedEvaluation = evaluationItem->data(Qt::UserRole).toString();
    m_evaluation->setCurrentIndex(qMax(0, m_evaluation->findData(savedEvaluation)));
    m_evaluationNotes->setPlainText(evaluationItem->data(Qt::UserRole + 1).toString());
    updateEvaluationControls();
}

void BenchmarkDialog::saveEvaluation()
{
    if (m_selectedRow < 0 || m_evaluation->currentData().toString().isEmpty())
        return;
    const QString taskId = m_results->item(m_selectedRow, 0)->data(Qt::UserRole).toString();
    const int repetition = m_results->item(m_selectedRow, 1)->text().toInt();
    const bool successful = m_evaluation->currentData().toString() == "successful";
    if (!m_client->setBenchmarkEvaluation(
            taskId, repetition, successful, m_evaluationNotes->toPlainText()))
    {
        QMessageBox::warning(this, tr("Не удалось сохранить оценку"),
                             tr("Не удалось записать оценку в JSON-отчёт."));
        return;
    }

    QTableWidgetItem *scoreItem = m_results->item(m_selectedRow, 8);
    scoreItem->setText(m_evaluation->currentText());
    scoreItem->setData(Qt::UserRole, m_evaluation->currentData().toString());
    scoreItem->setData(Qt::UserRole + 1, m_evaluationNotes->toPlainText());
    updateQualitySummary();
}

void BenchmarkDialog::updateSummary()
{
    if (m_running)
    {
        m_status->setText(tr("Прогресс: %1 из %2 запросов. Успешных: %3.")
                              .arg(m_nextRequest)
                              .arg(m_totalRequests)
                              .arg(m_succeededRequests));
    }
}

void BenchmarkDialog::updateQualitySummary()
{
    int evaluationCount = 0;
    int successfulCount = 0;
    int unsuccessfulCount = 0;
    for (int row = 0; row < m_results->rowCount(); ++row)
    {
        const QString evaluation = m_results->item(row, 8)->data(Qt::UserRole).toString();
        if (evaluation == "successful" || evaluation == "unsuccessful")
        {
            ++evaluationCount;
            if (evaluation == "successful")
                ++successfulCount;
            else
                ++unsuccessfulCount;
        }
    }

    if (evaluationCount == 0)
    {
        m_qualitySummary->setText(tr("Оценок пока нет."));
        return;
    }

    m_qualitySummary->setText(
        tr("Оценено ответов: %1. Успешно: %2, не успешно: %3 (%4%).")
            .arg(evaluationCount)
            .arg(successfulCount)
            .arg(unsuccessfulCount)
            .arg(QString::number(successfulCount * 100.0 / evaluationCount, 'f', 1)));
}

void BenchmarkDialog::updateEvaluationControls()
{
    const bool validSelection = m_selectedRow >= 0 &&
        m_results->item(m_selectedRow, 0) && m_results->item(m_selectedRow, 1);
    const bool hasResult = validSelection &&
        m_requestResults.value(resultKey(
            m_results->item(m_selectedRow, 0)->data(Qt::UserRole).toString(),
            m_results->item(m_selectedRow, 1)->text().toInt())).value("success").toBool();
    m_saveEvaluationButton->setEnabled(
        !m_running && hasResult && !m_evaluation->currentData().toString().isEmpty());
}

QString BenchmarkDialog::resultKey(const QString &taskId, int repetition) const
{
    return taskId + QLatin1Char('#') + QString::number(repetition);
}

void BenchmarkDialog::openReportFolder()
{
    const QString reportPath = m_client->benchmarkReportPath();
    if (reportPath.isEmpty())
        return;
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(reportPath).absolutePath())))
        QMessageBox::warning(this, tr("Не удалось открыть папку"),
                             tr("Не удалось открыть папку с отчётом бенчмарка."));
}

void BenchmarkDialog::closeEvent(QCloseEvent *event)
{
    if (m_running)
    {
        m_closeWhenFinished = true;
        cancelBenchmark();
        event->ignore();
        return;
    }
    QDialog::closeEvent(event);
}
