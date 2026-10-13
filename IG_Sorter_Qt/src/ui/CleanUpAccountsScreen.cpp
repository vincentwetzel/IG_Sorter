#include "ui/CleanUpAccountsScreen.h"

#include "core/DatabaseManager.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStyledItemDelegate>
#include <QTableView>
#include <QVBoxLayout>

namespace {
enum AccountColumn {
    AccountColumnIndex = 0,
    NameColumnIndex,
    TypeColumnIndex,
    AccountColumnCount
};

class AccountTypeDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    QWidget* createEditor(QWidget* parent, const QStyleOptionViewItem&,
                          const QModelIndex&) const override {
        auto* combo = new QComboBox(parent);
        combo->addItems({"Personal", "Curator", "IRL Only"});
        return combo;
    }

    void setEditorData(QWidget* editor, const QModelIndex& index) const override {
        auto* combo = qobject_cast<QComboBox*>(editor);
        if (combo) combo->setCurrentIndex(index.data(Qt::EditRole).toInt());
    }

    void setModelData(QWidget* editor, QAbstractItemModel* model,
                      const QModelIndex& index) const override {
        auto* combo = qobject_cast<QComboBox*>(editor);
        if (combo) model->setData(index, combo->currentIndex(), Qt::EditRole);
    }
};
}

AccountEntryModel::AccountEntryModel(DatabaseManager* db, QObject* parent)
    : QAbstractTableModel(parent), m_db(db) {
    reload();
}

int AccountEntryModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

int AccountEntryModel::columnCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : AccountColumnCount;
}

QVariant AccountEntryModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    const auto& entry = m_entries[index.row()];
    if (role == Qt::EditRole) {
        switch (index.column()) {
        case AccountColumnIndex: return entry.account;
        case NameColumnIndex: return entry.irlName;
        case TypeColumnIndex: return entry.type;
        default: return {};
        }
    }
    if (role != Qt::DisplayRole) return {};

    switch (index.column()) {
    case AccountColumnIndex: return entry.account.isEmpty() ? QString("(no account)") : entry.account;
    case NameColumnIndex: return entry.irlName;
    case TypeColumnIndex:
        switch (static_cast<AccountType>(entry.type)) {
        case AccountType::Personal: return QString("Personal");
        case AccountType::Curator: return QString("Curator");
        case AccountType::IrlOnly: return QString("IRL Only");
        }
        return {};
    default: return {};
    }
}

QVariant AccountEntryModel::headerData(int section, Qt::Orientation orientation, int role) const {
    if (orientation != Qt::Horizontal || role != Qt::DisplayRole) return {};
    switch (section) {
    case AccountColumnIndex: return QString("Account");
    case NameColumnIndex: return QString("IRL Name");
    case TypeColumnIndex: return QString("Type");
    default: return {};
    }
}

Qt::ItemFlags AccountEntryModel::flags(const QModelIndex& index) const {
    if (!index.isValid()) return Qt::NoItemFlags;
    return QAbstractTableModel::flags(index) | Qt::ItemIsEditable;
}

bool AccountEntryModel::setData(const QModelIndex& index, const QVariant& value, int role) {
    if (!index.isValid() || role != Qt::EditRole
        || index.row() < 0 || index.row() >= m_entries.size()) {
        return false;
    }

    auto& entry = m_entries[index.row()];
    switch (index.column()) {
    case AccountColumnIndex: {
        const QString account = value.toString().trimmed();
        if (entry.account == account) return true;
        entry.account = account;
        break;
    }
    case NameColumnIndex: {
        const QString name = value.toString().trimmed();
        if (entry.irlName == name) return true;
        entry.irlName = name;
        break;
    }
    case TypeColumnIndex: {
        const int type = value.toInt();
        if (type < 0 || type > 2) return false;
        if (entry.type == type) return true;
        entry.type = type;
        if (type == static_cast<int>(AccountType::IrlOnly)) {
            entry.account.clear();
        }
        break;
    }
    default:
        return false;
    }

    entry.modified = true;
    emit dataChanged(this->index(index.row(), 0),
                     this->index(index.row(), AccountColumnCount - 1),
                     {Qt::DisplayRole, Qt::EditRole});
    return true;
}

void AccountEntryModel::reload() {
    beginResetModel();
    m_entries.clear();
    if (m_db) {
        const auto& dbEntries = m_db->allEntries();
        m_entries.reserve(dbEntries.size());
        for (int i = 0; i < dbEntries.size(); ++i) {
            const auto& source = dbEntries[i];
            EditableEntry entry;
            entry.account = source.account;
            entry.irlName = source.irlName;
            entry.type = static_cast<int>(source.type);
            entry.dbIndex = i;
            m_entries.append(entry);
        }
    }
    endResetModel();
}

void AccountEntryModel::setDatabaseManager(DatabaseManager* db) {
    m_db = db;
    reload();
}

bool AccountEntryModel::saveChanges(QString* errorMessage) {
    if (!m_db) {
        if (errorMessage) *errorMessage = "The account database is unavailable.";
        return false;
    }

    bool changed = false;
    for (int i = 0; i < m_entries.size(); ++i) {
        const auto& entry = m_entries[i];
        if (!entry.modified) continue;

        AccountType type = static_cast<AccountType>(entry.type);
        const QString account = type == AccountType::IrlOnly ? QString() : entry.account;
        if (!m_db->updateEntryByIndex(entry.dbIndex, account, entry.irlName, type)) {
            if (errorMessage) {
                *errorMessage = QString("Could not save row %1. The handle may already belong to another entry.")
                    .arg(i + 1);
            }
            return false;
        }
        changed = true;
    }

    if (changed && !m_db->save()) {
        if (errorMessage) *errorMessage = "The account database could not be written. Check the log for details.";
        return false;
    }

    reload();
    return true;
}

int AccountEntryModel::databaseIndex(int row) const {
    if (row < 0 || row >= m_entries.size()) return -1;
    return m_entries[row].dbIndex;
}

AccountFilterProxyModel::AccountFilterProxyModel(QObject* parent)
    : QSortFilterProxyModel(parent), m_missingNamesOnly(false) {
    setFilterKeyColumn(-1);
    setFilterCaseSensitivity(Qt::CaseInsensitive);
}

void AccountFilterProxyModel::setMissingNamesOnly(bool enabled) {
    if (m_missingNamesOnly == enabled) return;
    beginFilterChange();
    m_missingNamesOnly = enabled;
    endFilterChange(Direction::Rows);
}

bool AccountFilterProxyModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const {
    if (m_missingNamesOnly) {
        const QModelIndex nameIndex = sourceModel()->index(sourceRow, NameColumnIndex, sourceParent);
        if (!nameIndex.data(Qt::DisplayRole).toString().trimmed().isEmpty()) return false;
    }
    return QSortFilterProxyModel::filterAcceptsRow(sourceRow, sourceParent);
}

CleanUpAccountsScreen::CleanUpAccountsScreen(QWidget* parent)
    : QWidget(parent), m_db(nullptr), m_model(nullptr), m_filterModel(nullptr),
      m_subtitleLabel(nullptr), m_searchEdit(nullptr), m_missingNamesOnly(nullptr),
      m_accountsView(nullptr), m_saveButton(nullptr), m_removeButton(nullptr),
      m_backButton(nullptr) {
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    auto* titleLabel = new QLabel("Clean Up Accounts", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(24);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    m_subtitleLabel = new QLabel("Search, edit, or remove entries in your account database.", this);
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(m_subtitleLabel);

    auto* filterLayout = new QHBoxLayout();
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("Search handles or names...");
    filterLayout->addWidget(m_searchEdit, 1);
    m_missingNamesOnly = new QCheckBox("Missing names only", this);
    filterLayout->addWidget(m_missingNamesOnly);
    mainLayout->addLayout(filterLayout);

    m_accountsView = new QTableView(this);
    m_accountsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_accountsView->setSelectionMode(QAbstractItemView::SingleSelection);
    m_accountsView->setEditTriggers(QAbstractItemView::DoubleClicked
        | QAbstractItemView::EditKeyPressed);
    m_accountsView->setAlternatingRowColors(true);
    m_accountsView->verticalHeader()->hide();
    m_accountsView->horizontalHeader()->setStretchLastSection(true);
    m_accountsView->setItemDelegateForColumn(TypeColumnIndex, new AccountTypeDelegate(m_accountsView));
    mainLayout->addWidget(m_accountsView, 1);

    auto* buttonLayout = new QHBoxLayout();
    m_backButton = new QPushButton("Back to Menu", this);
    m_backButton->setMinimumHeight(40);
    buttonLayout->addWidget(m_backButton);
    buttonLayout->addStretch();
    m_removeButton = new QPushButton("Remove Selected", this);
    m_removeButton->setMinimumHeight(40);
    buttonLayout->addWidget(m_removeButton);
    m_saveButton = new QPushButton("Save Changes", this);
    m_saveButton->setMinimumHeight(40);
    buttonLayout->addWidget(m_saveButton);
    mainLayout->addLayout(buttonLayout);

    m_model = new AccountEntryModel(m_db, this);
    m_filterModel = new AccountFilterProxyModel(this);
    m_filterModel->setSourceModel(m_model);
    m_accountsView->setModel(m_filterModel);
    m_accountsView->setColumnWidth(AccountColumnIndex, 260);
    m_accountsView->setColumnWidth(NameColumnIndex, 260);
    m_accountsView->setColumnWidth(TypeColumnIndex, 130);

    connect(m_searchEdit, &QLineEdit::textChanged, m_filterModel,
            &QSortFilterProxyModel::setFilterFixedString);
    connect(m_missingNamesOnly, &QCheckBox::toggled, m_filterModel,
            &AccountFilterProxyModel::setMissingNamesOnly);
    connect(m_saveButton, &QPushButton::clicked,
            this, &CleanUpAccountsScreen::handleSaveChanges);
    connect(m_removeButton, &QPushButton::clicked,
            this, &CleanUpAccountsScreen::handleRemoveSelected);
    connect(m_backButton, &QPushButton::clicked,
            this, &CleanUpAccountsScreen::handleBackToMenu);
    connect(m_accountsView->selectionModel(), &QItemSelectionModel::selectionChanged,
            this, [this]() {
                m_removeButton->setEnabled(m_accountsView->selectionModel()->hasSelection());
            });
    m_removeButton->setEnabled(false);
}

void CleanUpAccountsScreen::setDatabaseManager(DatabaseManager* db) {
    m_db = db;
    m_model->setDatabaseManager(db);
}

void CleanUpAccountsScreen::refreshAccountsList() {
    m_model->reload();
}

void CleanUpAccountsScreen::handleSaveChanges() {
    QString errorMessage;
    if (!m_model->saveChanges(&errorMessage)) {
        QMessageBox::warning(this, "Changes Not Saved", errorMessage);
        return;
    }
    QMessageBox::information(this, "Accounts Saved", "Account changes have been saved.");
}

void CleanUpAccountsScreen::handleRemoveSelected() {
    const QModelIndex proxyIndex = m_accountsView->currentIndex();
    if (!proxyIndex.isValid()) return;

    const QModelIndex sourceIndex = m_filterModel->mapToSource(proxyIndex);
    const int dbIndex = m_model->databaseIndex(sourceIndex.row());
    if (dbIndex < 0) return;

    const auto answer = QMessageBox::question(this, "Remove Account Entry",
        "Remove this entry from the account database? This does not delete any files.",
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes) return;

    if (!m_db->removeEntryByIndex(dbIndex)) {
        QMessageBox::critical(this, "Remove Failed", "The entry is no longer available.");
        return;
    }

    m_model->reload();
    if (!m_db->save()) {
        QMessageBox::critical(this, "Save Failed",
            "The entry was removed in this session, but the account database could not be written. Check the log for details.");
        return;
    }
}

void CleanUpAccountsScreen::handleBackToMenu() {
    emit menuClicked();
}
