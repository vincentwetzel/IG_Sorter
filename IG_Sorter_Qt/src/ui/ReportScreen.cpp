#include "ui/ReportScreen.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>

ReportScreen::ReportScreen(QWidget* parent)
    : QWidget(parent)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(20);
    m_layout->setContentsMargins(40, 40, 40, 40);

    m_titleLabel = new QLabel("Sorting Complete", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_layout->addWidget(m_titleLabel);

    m_reportText = new QTextEdit(this);
    m_reportText->setReadOnly(true);
    QFont monoFont("Consolas", 11);
    monoFont.setStyleHint(QFont::Monospace);
    m_reportText->setFont(monoFont);
    m_layout->addWidget(m_reportText, 1);

    // Bottom buttons
    auto* buttonLayout = new QHBoxLayout();

    m_doneButton = new QPushButton("Done", this);
    m_doneButton->setMinimumSize(200, 50);
    QFont btnFont = m_doneButton->font();
    btnFont.setPointSize(14);
    m_doneButton->setFont(btnFont);
    buttonLayout->addWidget(m_doneButton);

    buttonLayout->addStretch();

    m_settingsButton = new QPushButton("Settings", this);
    m_settingsButton->setMinimumSize(200, 50);
    m_settingsButton->setFont(btnFont);
    buttonLayout->addWidget(m_settingsButton);

    m_layout->addLayout(buttonLayout);

    connect(m_doneButton, &QPushButton::clicked,
            this, &ReportScreen::doneClicked);
    connect(m_settingsButton, &QPushButton::clicked,
            this, &ReportScreen::settingsClicked);
}

void ReportScreen::setReport(const SortReportData& report) {
    QString text;
    text += "═══════════════════════════════════════\n";
    text += "          SORTING COMPLETE\n";
    text += "═══════════════════════════════════════\n\n";

    text += QString("Files sorted:        %1\n").arg(report.filesSorted);
    text += QString("Files skipped:       %1\n").arg(report.filesSkipped);
    text += QString("Errors:              %1\n\n").arg(report.errors);

    if (!report.errorMessages.isEmpty()) {
        text += "── Errors ──\n";
        for (const auto& err : report.errorMessages) {
            text += "  • " + err + "\n";
        }
        text += "\n";
    }

    if (!report.directoryFileCounts.isEmpty()) {
        text += "── Directory Summary ──\n";
        for (auto it = report.directoryFileCounts.begin();
             it != report.directoryFileCounts.end(); ++it) {
            text += QString("%1:    %2 files\n").arg(it.key(), -10).arg(it.value());
        }
        text += "\n";
    }

    if (!report.newAccountsAdded.isEmpty()) {
        text += "── New Accounts Added ──\n";
        for (const auto& account : report.newAccountsAdded) {
            text += "• " + account + "\n";
        }
        text += "\n";
    }

    if (!report.filesByAccountType.isEmpty()) {
        text += "── Accounts by Type ──\n";
        for (auto it = report.filesByAccountType.begin();
             it != report.filesByAccountType.end(); ++it) {
            text += QString("%1:   %2 files\n").arg(it.key(), -12).arg(it.value());
        }
        text += "\n";
    }

    text += "═══════════════════════════════════════\n";

    m_reportText->setPlainText(text);
}
