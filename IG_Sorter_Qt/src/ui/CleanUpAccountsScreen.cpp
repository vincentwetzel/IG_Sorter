#include "ui/CleanUpAccountsScreen.h"
#include "core/DatabaseManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QListWidget>
#include <QListWidgetItem>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QMessageBox>
#include <QScrollArea>

// ─── AccountEditRow ─────────────────────────────────────────────────────────

AccountEditRow::AccountEditRow(int dbIndex, const QString& account, const QString& currentName,
                               DatabaseManager* db, QWidget* parent)
    : QWidget(parent), m_dbIndex(dbIndex), m_account(account), m_db(db),
      m_originalName(currentName)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(12);

    // Account handle label (fixed width)
    auto* accountLabel = new QLabel(account.isEmpty() ? "(no account)" : account, this);
    accountLabel->setMinimumWidth(200);
    accountLabel->setMaximumWidth(250);
    QFont accountFont = accountLabel->font();
    accountFont.setBold(true);
    accountLabel->setFont(accountFont);
    layout->addWidget(accountLabel);

    // Name input
    m_nameEdit = new QLineEdit(currentName, this);
    m_nameEdit->setPlaceholderText("Enter IRL name...");
    m_nameEdit->setMinimumWidth(200);
    layout->addWidget(m_nameEdit, 1);

    // Account type combo
    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem("Personal");
    m_typeCombo->addItem("Curator");
    m_typeCombo->addItem("IRL Only");

    // Set current type
    auto entry = m_db->getEntry(account);
    int typeIndex = 0;  // Personal
    switch (entry.type) {
    case AccountType::Curator:  typeIndex = 1; break;
    case AccountType::IrlOnly:  typeIndex = 2; break;
    default: break;
    }
    m_typeCombo->setCurrentIndex(typeIndex);
    m_originalTypeIndex = typeIndex;
    layout->addWidget(m_typeCombo);

    // Save button
    m_saveButton = new QPushButton("Save", this);
    m_saveButton->setFixedWidth(100);
    connect(m_saveButton, &QPushButton::clicked, this, [this]() {
        if (apply()) {
            emit saved();
        }
    });
    layout->addWidget(m_saveButton);
}

bool AccountEditRow::apply() {
    QString newName = m_nameEdit->text().trimmed();
    if (newName.isEmpty()) {
        QMessageBox::warning(this, "Empty Name",
            "Please enter a name for this account.");
        return false;
    }

    AccountType newType = AccountType::Personal;
    switch (m_typeCombo->currentIndex()) {
    case 1: newType = AccountType::Curator; break;
    case 2: newType = AccountType::IrlOnly; break;
    }

    bool ok = m_db->updateEntryByIndex(m_dbIndex, m_account, newName, newType);
    if (ok) {
        m_wasSaved = true;
        m_originalName = newName;
        m_originalTypeIndex = m_typeCombo->currentIndex();

        // Visual feedback — green button with checkmark
        m_saveButton->setText("✓ Saved");
        m_saveButton->setStyleSheet(
            "QPushButton { background-color: #4caf50; color: white; font-weight: bold; }");
        m_saveButton->setEnabled(false);
        m_nameEdit->setStyleSheet("QLineEdit { background-color: #e8f5e9; }");
    }
    return ok;
}

bool AccountEditRow::hasChanges() const {
    return m_nameEdit->text().trimmed() != m_originalName ||
           m_typeCombo->currentIndex() != m_originalTypeIndex;
}

// ─── CleanUpAccountsScreen ──────────────────────────────────────────────────

CleanUpAccountsScreen::CleanUpAccountsScreen(QWidget* parent)
    : QWidget(parent), m_db(nullptr)
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(12);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // Title
    m_titleLabel = new QLabel("Clean Up Accounts", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    m_mainLayout->addWidget(m_titleLabel);

    // Subtitle
    m_subtitleLabel = new QLabel(
        "Accounts below don't have an IRL name. Enter names to clean them up.", this);
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    QFont subFont = m_subtitleLabel->font();
    subFont.setPointSize(12);
    m_subtitleLabel->setFont(subFont);
    m_mainLayout->addWidget(m_subtitleLabel);

    // Select All checkbox
    auto* selectAllLayout = new QHBoxLayout();
    m_selectAllCheckbox = new QCheckBox("Select All", this);
    connect(m_selectAllCheckbox, &QCheckBox::stateChanged,
            this, &CleanUpAccountsScreen::handleSelectAll);
    selectAllLayout->addWidget(m_selectAllCheckbox);
    selectAllLayout->addStretch();
    m_mainLayout->addLayout(selectAllLayout);

    // Scrollable accounts list
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setMinimumHeight(400);

    auto* listContainer = new QWidget();
    m_accountsList = new QListWidget(listContainer);
    m_accountsList->setSpacing(4);

    auto* listLayout = new QVBoxLayout(listContainer);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->addWidget(m_accountsList);
    scrollArea->setWidget(listContainer);

    m_mainLayout->addWidget(scrollArea, 1);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);

    m_backButton = new QPushButton("Back to Menu", this);
    m_backButton->setMinimumHeight(40);
    QFont btnFont = m_backButton->font();
    btnFont.setPointSize(12);
    m_backButton->setFont(btnFont);
    buttonLayout->addWidget(m_backButton);

    buttonLayout->addStretch();

    m_saveButton = new QPushButton("Save Selected", this);
    m_saveButton->setMinimumHeight(40);
    m_saveButton->setFont(btnFont);
    m_saveButton->setObjectName("saveButton");
    buttonLayout->addWidget(m_saveButton);

    m_mainLayout->addLayout(buttonLayout);

    // Connect buttons
    connect(m_saveButton, &QPushButton::clicked,
            this, &CleanUpAccountsScreen::handleSaveSelected);
    connect(m_backButton, &QPushButton::clicked,
            this, &CleanUpAccountsScreen::handleBackToMenu);
}

void CleanUpAccountsScreen::setDatabaseManager(DatabaseManager* db) {
    m_db = db;
}

void CleanUpAccountsScreen::refreshAccountsList() {
    m_accountsList->clear();
    buildAccountsList();
}

void CleanUpAccountsScreen::buildAccountsList() {
    if (!m_db) return;

    auto entries = m_db->allEntries();
    int emptyCount = 0;

    for (int i = 0; i < entries.size(); ++i) {
        const auto& entry = entries[i];
        // Only show entries without an IRL name
        if (entry.irlName.trimmed().isEmpty()) {
            auto* item = new QListWidgetItem(m_accountsList);
            auto* row = new AccountEditRow(i, entry.account, entry.irlName, m_db);
            item->setSizeHint(QSize(0, 50));
            m_accountsList->addItem(item);
            m_accountsList->setItemWidget(item, row);
            emptyCount++;
        }
    }

    if (emptyCount == 0) {
        auto* item = new QListWidgetItem("All accounts have names! ✓", m_accountsList);
        item->setTextAlignment(Qt::AlignCenter);
        QFont font = item->font();
        font.setPointSize(14);
        font.setBold(true);
        item->setFont(font);
        m_accountsList->addItem(item);
        m_selectAllCheckbox->setVisible(false);
        m_saveButton->setEnabled(false);
    } else {
        m_subtitleLabel->setText(
            QString("Found %1 account(s) without IRL names. Enter names below.")
                .arg(emptyCount));
        m_selectAllCheckbox->setVisible(true);
        m_saveButton->setEnabled(true);
    }
}

void CleanUpAccountsScreen::handleSelectAll(int state) {
    // Select all is just a visual hint — rows handle themselves
    Q_UNUSED(state);
}

void CleanUpAccountsScreen::handleSaveSelected() {
    int savedCount = 0;
    int failedCount = 0;

    for (int i = 0; i < m_accountsList->count(); ++i) {
        auto* item = m_accountsList->item(i);
        auto* row = qobject_cast<AccountEditRow*>(m_accountsList->itemWidget(item));
        if (row) {
            if (row->apply()) {
                savedCount++;
            } else {
                failedCount++;
            }
        }
    }

    bool ok = m_db->save();
    if (!ok) {
        QMessageBox::critical(this, "Save Failed",
            "Failed to save the database file. Check the log for details.");
        return;
    }

    if (failedCount > 0) {
        QMessageBox::warning(this, "Partial Save",
            QString("Saved %1 account(s), but %2 failed (empty names).")
                .arg(savedCount).arg(failedCount));
    } else {
        QMessageBox::information(this, "Accounts Saved",
            QString("✓ Successfully saved %1 account(s) to the database.").arg(savedCount));
    }
    refreshAccountsList();
}

void CleanUpAccountsScreen::handleBackToMenu() {
    emit menuClicked();
}
