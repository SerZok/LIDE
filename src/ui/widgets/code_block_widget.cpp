#include "code_block_widget.h"
#include <QFontMetrics>
#include <QSizePolicy>

CodeBlockWidget::CodeBlockWidget(const QString &language, const QString &code, QWidget *parent)
    : QWidget(parent), m_code(code)
{

    setObjectName("codeBlockWidget");
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // --- Шапка блока кода ---
    QWidget *header = new QWidget(this);
    header->setObjectName("codeBlockHeader");
    QHBoxLayout *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 2, 8, 2);

    QLabel *langLabel = new QLabel(language.isEmpty() ? "text" : language.toUpper(), this);
    langLabel->setObjectName("codeLangLabel");
    headerLayout->addWidget(langLabel);
    headerLayout->addStretch();

    QPushButton *copyBtn = new QPushButton("📋 Копировать", this);
    copyBtn->setObjectName("codeActionBtn");
    copyBtn->setCursor(Qt::PointingHandCursor);
    connect(copyBtn, &QPushButton::clicked, this, &CodeBlockWidget::onCopyClicked);
    headerLayout->addWidget(copyBtn);

    mainLayout->addWidget(header);

    // --- Сам код ---
    m_codeEdit = new QTextEdit(this);
    m_codeEdit->setObjectName("codeBlockEditor");
    m_codeEdit->setReadOnly(true);
    m_codeEdit->setFrameShape(QFrame::NoFrame);
    m_codeEdit->setWordWrapMode(QTextOption::NoWrap);
    m_codeEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_codeEdit->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_codeEdit->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_codeEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_codeEdit->setMinimumSize(0, 0);
    m_codeEdit->setPlainText(m_code);
    mainLayout->addWidget(m_codeEdit);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMinimumWidth(0);
    updateCodeHeight();
}

void CodeBlockWidget::updateCodeHeight()
{
    QFontMetrics fm(m_codeEdit->font());
    const int lineCount = qMax(1, m_codeEdit->document()->lineCount());
    const int verticalPadding = 2 * fm.height();
    const int frame = 2 * m_codeEdit->frameWidth();
    m_codeEdit->setFixedHeight(lineCount * fm.lineSpacing() + verticalPadding + frame);
}

void CodeBlockWidget::onCopyClicked()
{
    QGuiApplication::clipboard()->setText(m_code);

    QPushButton *btn = qobject_cast<QPushButton *>(sender());
    if (btn)
    {
        QString originalText = btn->text();
        btn->setText("✓ Скопировано!");
        btn->setEnabled(false);

        QTimer::singleShot(1500, this, [btn, originalText]()
                           {
            btn->setText(originalText);
            btn->setEnabled(true); });
    }
}
