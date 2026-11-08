#pragma once

#include "network/ai_client.h"
#include "util/benchmark/sbcl_sandbox.h"

#include <QDialog>
#include <QHash>
#include <QVector>

class QCloseEvent;
class QComboBox;
class QLabel;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTextBrowser;
class QThread;

class BenchmarkDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BenchmarkDialog(QWidget *parent = nullptr);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void startBenchmark();
    void runNextRequest();
    void onRequestFinished(const QString &taskId, int repetition,
                           const QJsonObject &result);
    void cancelBenchmark();
    void openReportFolder();
    void showSelectedResult(int row, int column);
    void saveEvaluation();
    void finishSbclValidation(int row, const QString &taskId, int repetition,
                              const QJsonObject &result);

private:
    struct BenchmarkTask
    {
        QString id;
        QString title;
        QString prompt;
        QString rubric;
        QString sbclFunction;
        QVector<SbclSandbox::TestCase> sbclTests;
    };

    static QVector<BenchmarkTask> benchmarkTasks();
    QJsonArray taskDefinitions() const;
    void finishBenchmark(bool canceled);
    void updateSummary();
    void updateQualitySummary();
    void updateEvaluationControls();
    void startSbclValidation(int row, const QString &taskId, int repetition,
                             const QString &response, const BenchmarkTask &task);
    QString resultKey(const QString &taskId, int repetition) const;

    AiClient *m_client;
    QSpinBox *m_repetitions;
    QProgressBar *m_progress;
    QLabel *m_status;
    QLabel *m_qualitySummary;
    QTableWidget *m_results;
    QTextBrowser *m_resultDetails;
    QComboBox *m_evaluation;
    QPlainTextEdit *m_evaluationNotes;
    QPushButton *m_runButton;
    QPushButton *m_cancelButton;
    QPushButton *m_openReportButton;
    QPushButton *m_saveEvaluationButton;
    QThread *m_validationThread = nullptr;
    QVector<BenchmarkTask> m_tasks;
    int m_nextRequest = 0;
    int m_totalRequests = 0;
    int m_succeededRequests = 0;
    int m_activeRepetition = 0;
    int m_selectedRow = -1;
    QString m_activeTaskId;
    QHash<QString, QJsonObject> m_requestResults;
    bool m_running = false;
    bool m_validationRunning = false;
    bool m_cancelRequested = false;
    bool m_closeWhenFinished = false;
};
