#include "ui/CleanupScreen.h"
#include "core/DatabaseManager.h"
#include "utils/LogManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QMessageBox>

// ─── ResolutionRow ───────────────────────────────────────────────
ResolutionRow::ResolutionRow(const QString& irlName, DatabaseManager* db,
                              QVBoxLayout* parentLayout, QWidget* parent)
    : QWidget(parent), m_irlName(irlName), m_db(db), m_parentLayout(parentLayout)
{
    auto* rowLayout = new QHBoxLayout(this);
    rowLayout->setContentsMargins(0, 4, 0, 4);
    rowLayout->setSpacing(8);

    rowLayout->addWidget(new QLabel(irlName + " →", this), 0);

    m_accountEdit = new QLineEdit(this);
    m_accountEdit->setPlaceholderText("Enter Instagram account or leave blank and click apply if the Instagram account is unknown...");
    m_accountEdit->setMinimumWidth(180);
    rowLayout->addWidget(m_accountEdit, 1);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem("Personal");
    m_typeCombo->addItem("Curator");
    m_typeCombo->addItem("IRL Only");
    rowLayout->addWidget(m_typeCombo);

    m_applyButton = new QPushButton("Apply", this);
    m_applyButton->setMinimumWidth(80);
    rowLayout->addWidget(m_applyButton);

    connect(m_applyButton, &QPushButton::clicked, this, [this]() { apply(); });
}

void ResolutionRow::apply() {
    if (!m_db) return;

    QString account = m_accountEdit->text().trimmed();
    AccountType type = AccountType::Personal;
    switch (m_typeCombo->currentIndex()) {
    case 1: type = AccountType::Curator; break;
    case 2: type = AccountType::IrlOnly; break;
    default: type = AccountType::Personal; break;
    }

    LogManager::instance()->info(
        QString("ResolutionRow::apply: irlName=\"%1\" account=\"%2\" type=%3")
            .arg(m_irlName, account, DatabaseManager::accountTypeToString(type)));

    // Check for duplicate account
    if (!account.isEmpty() && m_db->hasAccount(account)) {
        QString existingName = m_db->getIrlName(account);
        LogManager::instance()->warning(
            QString("Account \"%1\" already exists in DB, associated with \"%2\"")
                .arg(account, existingName));
        QMessageBox::warning(this, "Duplicate Account",
            QString("The account \"%1\" is already in the database, associated with \"%2\".\n\n"
                    "If \"%1\" actually belongs to \"%3\" instead, please edit the database "
                    "manually or in Settings.")
                .arg(account, existingName, m_irlName));
        return;
    }

    bool added = false;
    if (type != AccountType::IrlOnly && !account.isEmpty()) {
        added = m_db->addEntry(account, m_irlName, type);
    } else {
        added = m_db->addEntry(QString(), m_irlName, AccountType::IrlOnly);
    }

    LogManager::instance()->info(
        QString("addEntry returned %1. DB entries now: %2").arg(added).arg(m_db->allEntries().size()));

    if (added) {
        bool saved = m_db->save();
        LogManager::instance()->info(QString("DB save returned %1").arg(saved));
    } else {
        LogManager::instance()->warning("addEntry returned FALSE — entry not added");
        QMessageBox::warning(this, "Add Failed",
            QString("Could not add \"%1\" to the database.").arg(m_irlName));
        return;  // Don't remove the row if it failed
    }

    // Remove this row from parent layout and delete
    m_parentLayout->removeWidget(this);
    deleteLater();
    emit resolved();
}

// ─── CleanupScreen ───────────────────────────────────────────────
CleanupScreen::CleanupScreen(QWidget* parent)
    : QWidget(parent), m_pendingDb(nullptr)
{
    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(20);
    m_layout->setContentsMargins(40, 40, 40, 40);

    m_titleLabel = new QLabel("Directory Cleanup", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(20);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_layout->addWidget(m_titleLabel);

    m_statusLabel = new QLabel("", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    m_layout->addWidget(m_statusLabel);

    m_layout->addSpacing(10);

    // Resolution area for unknown names
    m_resolutionWidget = new QWidget(this);
    m_resolutionWidget->hide();
    m_resolutionLayout = new QVBoxLayout(m_resolutionWidget);
    m_resolutionLayout->setSpacing(8);

    m_layout->addWidget(m_resolutionWidget);

    // Spacer
    m_layout->addStretch();

    // Bottom buttons (Menu left, Continue center)
    m_buttonLayout = new QHBoxLayout();
    m_menuButton = new QPushButton("Menu", this);
    m_menuButton->setMinimumHeight(45);
    QFont menuBtnFont = m_menuButton->font();
    menuBtnFont.setPointSize(12);
    m_menuButton->setFont(menuBtnFont);
    m_buttonLayout->addWidget(m_menuButton);

    m_continueButton = new QPushButton("Continue to Sorting", this);
    m_continueButton->setMinimumSize(250, 50);
    m_continueButton->setEnabled(false);
    m_continueButton->setObjectName("continueButton");
    QFont btnFont = m_continueButton->font();
    btnFont.setPointSize(14);
    m_continueButton->setFont(btnFont);
    m_buttonLayout->addWidget(m_continueButton);

    m_layout->addLayout(m_buttonLayout);

    connect(m_menuButton, &QPushButton::clicked,
            this, &CleanupScreen::menuClicked);
    connect(m_continueButton, &QPushButton::clicked,
            this, &CleanupScreen::continueClicked);
}

void CleanupScreen::setDirectories(const QStringList& dirs) {
    // Remove existing progress bars, labels, and path labels
    for (auto* bar : m_progressBars) {
        delete bar;
    }
    for (auto* label : m_progressLabels) {
        delete label;
    }
    for (auto* label : m_pathLabels) {
        delete label;
    }
    m_progressBars.clear();
    m_progressLabels.clear();
    m_pathLabels.clear();

    for (const auto& dir : dirs) {
        auto* pathLabel = new QLabel(dir, this);
        auto* statusLabel = new QLabel("0 / 0 files scanned", this);
        auto* bar = new QProgressBar(this);
        bar->setRange(0, 100);
        bar->setValue(0);
        m_layout->insertWidget(m_layout->count() - 2, pathLabel);
        m_layout->insertWidget(m_layout->count() - 2, statusLabel);
        m_layout->insertWidget(m_layout->count() - 2, bar);
        m_pathLabels[dir] = pathLabel;
        m_progressLabels[dir] = statusLabel;
        m_progressBars[dir] = bar;
    }

    m_continueButton->setEnabled(false);
    m_resolutionWidget->hide();
    m_statusLabel->clear();
}

void CleanupScreen::setStatusText(const QString& text) {
    m_statusLabel->setText(text);
}

void CleanupScreen::updateDirectoryProgress(const QString& dir, int current, int total) {
    if (m_progressBars.contains(dir)) {
        int percent = total > 0 ? (current * 100) / total : 100;
        m_progressBars[dir]->setValue(percent);
        m_progressLabels[dir]->setText(QString("%1 / %2 files scanned").arg(current).arg(total));

        // Turn green at 100%
        if (percent >= 100) {
            m_progressBars[dir]->setStyleSheet(
                "QProgressBar::chunk { background-color: #2ecc71; }"
            );
        } else {
            m_progressBars[dir]->setStyleSheet("");
        }
    }
}

void CleanupScreen::setDirectoryRenamed(const QString& dir, int filesRenamed) {
    if (m_progressLabels.contains(dir)) {
        QString existing = m_progressLabels[dir]->text();
        if (filesRenamed > 0) {
            m_progressLabels[dir]->setText(
                QString("%1 — %2 file(s) renumbered").arg(existing).arg(filesRenamed));
        } else {
            m_progressLabels[dir]->setText(
                QString("%1 — all files in order").arg(existing));
        }
    }
}

void CleanupScreen::showUnresolvedIssues(const QStringList& unknownNames, DatabaseManager* db) {
    m_pendingDb = db;
    m_pendingUnknownNames = unknownNames;

    // Clear everything from the resolution layout — title labels from previous calls
    // and any remaining unresolved rows
    QLayoutItem* child;
    while ((child = m_resolutionLayout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            delete child->widget();
        }
        delete child;
    }

    // Clear row tracking
    m_resolutionRows.clear();

    // Re-add title
    auto* resolutionTitle = new QLabel("Unknown names found — please provide Instagram accounts:",
                                       m_resolutionWidget);
    resolutionTitle->setWordWrap(true);
    QFont resFont = resolutionTitle->font();
    resFont.setPointSize(12);
    resFont.setBold(true);
    resolutionTitle->setFont(resFont);
    m_resolutionLayout->addWidget(resolutionTitle);

    // Add a resolution row for each unknown name
    for (const auto& name : unknownNames) {
        auto* row = new ResolutionRow(name, db, m_resolutionLayout, m_resolutionWidget);
        connect(row, &ResolutionRow::resolved, this, [this, row]() {
            m_resolutionRows.removeOne(row);
            if (m_resolutionRows.isEmpty()) {
                // All resolved — enable continue
                m_resolutionWidget->hide();
                m_statusLabel->setText("All unknown names resolved. You may continue.");
                m_continueButton->setEnabled(true);
            }
        });
        m_resolutionRows.append(row);
        m_resolutionLayout->addWidget(row);
    }

    m_resolutionWidget->show();

    m_statusLabel->setText(
        QString("%1 unknown name(s) found in output directories. "
                "Provide Instagram accounts and click Apply for each.")
            .arg(unknownNames.size()));
    m_continueButton->setEnabled(false);
}

void CleanupScreen::enableContinue(bool enabled) {
    m_continueButton->setEnabled(enabled);
    if (enabled) {
        m_resolutionWidget->hide();
    }
}
