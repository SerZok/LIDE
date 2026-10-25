#include "code_block_widget.h"
#include <QFontMetrics>
#include <QResizeEvent>

CodeBlockWidget::CodeBlockWidget(const QString& language, const QString& code, QWidget* parent)
    : QWidget(parent), m_code(code) {

    setObjectName("codeBlockWidget");
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // --- Шапка блока кода ---
    QWidget* header = new QWidget(this);
    header->setObjectName("codeBlockHeader");
    QHBoxLayout* headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 4, 8, 4);

    QLabel* langLabel = new QLabel(language.isEmpty() ? "text" : language.toUpper(), this);
    langLabel->setObjectName("codeLangLabel");
    headerLayout->addWidget(langLabel);
    headerLayout->addStretch();

    QPushButton* copyBtn = new QPushButton("📋 Копировать", this);
    copyBtn->setObjectName("codeActionBtn");
    copyBtn->setCursor(Qt::PointingHandCursor);
    connect(copyBtn, &QPushButton::clicked, this, &CodeBlockWidget::onCopyClicked);
    headerLayout->addWidget(copyBtn);

    QPushButton* insertBtn = new QPushButton("➕ В редактор", this);
    insertBtn->setObjectName("codeActionBtn");
    insertBtn->setCursor(Qt::PointingHandCursor);
    connect(insertBtn, &QPushButton::clicked, this, &CodeBlockWidget::onInsertClicked);
    headerLayout->addWidget(insertBtn);

    mainLayout->addWidget(header);

    // --- Сам код ---
    m_codeEdit = new QTextEdit(this);
    m_codeEdit->setObjectName("codeBlockEditor");
    m_codeEdit->setReadOnly(true);
    m_codeEdit->setFrameShape(QFrame::NoFrame);
    m_codeEdit->setWordWrapMode(QTextOption::NoWrap);
    m_codeEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_codeEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_codeEdit->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_codeEdit->setPlainText(m_code);
    mainLayout->addWidget(m_codeEdit);

    updateCodeHeight();
}

void CodeBlockWidget::updateCodeHeight() {
    QFontMetrics fm(m_codeEdit->font());
    int lineHeight = fm.lineSpacing();

    int lineCount = m_codeEdit->document()->lineCount();
    int padding = 20;
    int neededHeight = (lineCount * lineHeight) + padding;
    int minHeight = (4 * lineHeight) + padding;
    int maxHeight = (20 * lineHeight) + padding;
    int finalHeight = qBound(minHeight, neededHeight, maxHeight);

    if (m_codeEdit->height() != finalHeight) {
        m_codeEdit->setFixedHeight(finalHeight);
    }
}

void CodeBlockWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    updateCodeHeight();
}

void CodeBlockWidget::onCopyClicked() {
    QGuiApplication::clipboard()->setText(m_code);

    QPushButton* btn = qobject_cast<QPushButton*>(sender());
    if (btn) {
        QString originalText = btn->text();
        btn->setText("✓ Скопировано!");
        btn->setEnabled(false);

        QTimer::singleShot(1500, this, [btn, originalText]() {
            btn->setText(originalText);
            btn->setEnabled(true);
            });
    }
}

void CodeBlockWidget::onInsertClicked() {
    emit insertToEditorRequested(m_code);
}