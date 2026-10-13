#include "code_block_widget.h"

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

    QLabel* langLabel = new QLabel(language.isEmpty() ? "code" : language, this);
    langLabel->setObjectName("codeLangLabel");
    headerLayout->addWidget(langLabel);
    headerLayout->addStretch();

    QPushButton* copyBtn = new QPushButton("📋 Копировать", this);
    copyBtn->setObjectName("codeActionBtn");
    connect(copyBtn, &QPushButton::clicked, this, &CodeBlockWidget::onCopyClicked);
    headerLayout->addWidget(copyBtn);

    QPushButton* insertBtn = new QPushButton("➕ Вставить в редактор", this);
    insertBtn->setObjectName("codeActionBtn");
    connect(insertBtn, &QPushButton::clicked, this, &CodeBlockWidget::onInsertClicked);
    headerLayout->addWidget(insertBtn);

    mainLayout->addWidget(header);

    // --- Сам код ---
    m_codeEdit = new QPlainTextEdit(this);
    m_codeEdit->setObjectName("codeBlockEditor");
    m_codeEdit->setPlainText(m_code);
    m_codeEdit->setReadOnly(true);
    m_codeEdit->setWordWrapMode(QTextOption::NoWrap); // Код не переносим, нужен скролл

    mainLayout->addWidget(m_codeEdit);
}

void CodeBlockWidget::onCopyClicked() {
    QGuiApplication::clipboard()->setText(m_code);
    // Можно добавить всплывающее уведомление "Скопировано!"
}

void CodeBlockWidget::onInsertClicked() {
    emit insertToEditorRequested(m_code);
}