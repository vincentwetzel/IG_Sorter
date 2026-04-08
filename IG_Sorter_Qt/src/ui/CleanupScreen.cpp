#include "ui/CleanupScreen.h"
#include "core/DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QGroupBox>

CleanupScreen::CleanupScreen(QWidget* parent)
    : QWidget(parent), m_pendingDb(nullptr)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(20);
    m_layout->setContentsMargins(40, 40, 40, 40);

    m_titleLabel = new QLabel("Cleaning Up Directories", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_layout->addWidget(m_titleLabel);

    m_statusLabel = new QLabel("Checking file numbering in output directories...", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_layout->addWidget(m_statusLabel);

    m_layout->addSpacing(10);

    // Resolution area for unknown names
    m_resolutionWidget = new QWidget(this);
    m_resolutionWidget->hide();
    m_resolutionLayout = new QVBoxLayout(m_resolutionWidget);
    m_resolutionLayout->setSpacing(10);

    auto* resolutionTitle = new QLabel("Unknown names found — please assign IRL names:", this);
    resolutionTitle->setWordWrap(true);
    QFont resFont = resolutionTitle->font();
    resFont.setPointSize(12);
    resFont.setBold(true);
    resolutionTitle->setFont(resFont);
    m_resolutionLayout->addWidget(resolutionTitle);

    m_applyResolutionsButton = new QPushButton("Apply Resolutions", this);
    m_applyResolutionsButton->setMinimumHeight(40);
    m_resolutionLayout->addWidget(m_applyResolutionsButton);

    connect(m_applyResolutionsButton, &QPushButton::clicked, this, [this]() {
        if (!m_pendingDb) return;

        for (auto it = m_resolutionFields.begin(); it != m_resolutionFields.end(); ++it) {
            QString unknownName = it.key();
            QLineEdit* nameEdit = it.value().first;
            QComboBox* typeCombo = it.value().second;

            QString irlName = nameEdit->text().trimmed();
            if (irlName.isEmpty()) continue;

            AccountType type = AccountType::IrlOnly;
            switch (typeCombo->currentIndex()) {
            case 0: type = AccountType::Personal; break;
            case 1: type = AccountType::Curator; break;
            case 2: type = AccountType::IrlOnly; break;
            }

            m_pendingDb->addEntry(QString(), irlName, type);
        }

        m_pendingDb->save();

        // Hide resolution area and enable continue
        m_resolutionWidget->hide();
        m_statusLabel->setText("All unknown names resolved. You may continue.");
        m_continueButton->setEnabled(true);
    });

    m_layout->addWidget(m_resolutionWidget);

    // Spacer
    m_layout->addStretch();

    m_continueButton = new QPushButton("Continue to Sorting", this);
    m_continueButton->setMinimumSize(250, 50);
    m_continueButton->setEnabled(false);
    m_continueButton->setObjectName("continueButton");
    QFont btnFont = m_continueButton->font();
    btnFont.setPointSize(14);
    m_continueButton->setFont(btnFont);
    m_layout->addWidget(m_continueButton, 0, Qt::AlignCenter);

    connect(m_continueButton, &QPushButton::clicked,
            this, &CleanupScreen::continueClicked);
}

void CleanupScreen::setDirectories(const QStringList& dirs) {
    // Remove existing progress bars and labels
    for (auto* bar : m_progressBars) {
        delete bar;
    }
    for (auto* label : m_progressLabels) {
        delete label;
    }
    m_progressBars.clear();
    m_progressLabels.clear();
    m_progressTotals.clear();

    // Clear resolution fields
    for (auto* label : m_resolutionWidget->findChildren<QLabel*>()) {
        if (label != m_resolutionWidget->findChild<QLabel*>(QString(), Qt::FindDirectChildrenOnly)) {
            // Only remove labels added dynamically
        }
    }

    for (const auto& dir : dirs) {
        auto* pathLabel = new QLabel(dir, this);
        auto* statusLabel = new QLabel("0 / 0 files scanned", this);
        auto* bar = new QProgressBar(this);
        bar->setRange(0, 100);  // Determinate mode (0-100%)
        bar->setValue(0);
        m_layout->insertWidget(m_layout->count() - 2, pathLabel);
        m_layout->insertWidget(m_layout->count() - 2, statusLabel);
        m_layout->insertWidget(m_layout->count() - 2, bar);
        m_progressBars[dir] = bar;
        m_progressLabels[dir] = statusLabel;
        m_progressTotals[dir] = 0;  // Will be set on first progress update
    }

    m_continueButton->setEnabled(false);
    m_resolutionWidget->hide();
    m_statusLabel->setText("Running cleanup...");
}

void CleanupScreen::updateDirectoryProgress(const QString& dir, int current, int total) {
    if (m_progressBars.contains(dir)) {
        int percent = total > 0 ? (current * 100) / total : 100;
        m_progressBars[dir]->setValue(percent);
        m_progressLabels[dir]->setText(QString("%1 / %2 files scanned").arg(current).arg(total));
    }
}

void CleanupScreen::showUnresolvedIssues(const QStringList& unknownNames, DatabaseManager* db) {
    m_pendingDb = db;
    m_pendingUnknownNames = unknownNames;

    // Clear previous resolution fields
    QLayoutItem* child;
    while ((child = m_resolutionLayout->takeAt(0)) != nullptr) {
        delete child->widget();
        delete child;
    }

    // Re-add title
    auto* resolutionTitle = new QLabel("Unknown names found — please assign IRL names:",
                                       m_resolutionWidget);
    resolutionTitle->setWordWrap(true);
    QFont resFont = resolutionTitle->font();
    resFont.setPointSize(12);
    resFont.setBold(true);
    resolutionTitle->setFont(resFont);
    m_resolutionLayout->addWidget(resolutionTitle);

    // Add fields for each unknown name
    m_resolutionFields.clear();
    for (const auto& name : unknownNames) {
        auto* rowLayout = new QHBoxLayout();
        rowLayout->addWidget(new QLabel(name + " →", m_resolutionWidget));

        auto* nameEdit = new QLineEdit(m_resolutionWidget);
        nameEdit->setPlaceholderText("Enter IRL name...");
        rowLayout->addWidget(nameEdit);

        auto* typeCombo = new QComboBox(m_resolutionWidget);
        typeCombo->addItem("Personal");
        typeCombo->addItem("Curator");
        typeCombo->addItem("IRL Only");
        rowLayout->addWidget(typeCombo);

        m_resolutionFields[name] = qMakePair(nameEdit, typeCombo);
        m_resolutionLayout->addLayout(rowLayout);
    }

    m_resolutionLayout->addWidget(m_applyResolutionsButton);
    m_resolutionWidget->show();

    m_statusLabel->setText(
        QString("%1 unknown name(s) found in output directories. "
                "Please provide IRL names below to continue.")
            .arg(unknownNames.size()));
    m_continueButton->setEnabled(false);
}

void CleanupScreen::enableContinue(bool enabled) {
    m_continueButton->setEnabled(enabled);
    if (enabled) {
        m_resolutionWidget->hide();
    }
}
