#pragma once

#include <QAbstractTableModel>
#include <QList>
#include <QSortFilterProxyModel>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableView;
class DatabaseManager;

class AccountEntryModel : public QAbstractTableModel {
    Q_OBJECT
public:
    explicit AccountEntryModel(DatabaseManager* db, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value,
                 int role = Qt::EditRole) override;

    void reload();
    void setDatabaseManager(DatabaseManager* db);
    bool saveChanges(QString* errorMessage = nullptr);
    int databaseIndex(int row) const;

private:
    struct EditableEntry {
        QString account;
        QString irlName;
        int type = 0;
        int dbIndex = -1;
        bool modified = false;
    };

    DatabaseManager* m_db;
    QList<EditableEntry> m_entries;
};

class AccountFilterProxyModel : public QSortFilterProxyModel {
public:
    explicit AccountFilterProxyModel(QObject* parent = nullptr);
    void setMissingNamesOnly(bool enabled);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    bool m_missingNamesOnly;
};

class CleanUpAccountsScreen : public QWidget {
    Q_OBJECT
public:
    explicit CleanUpAccountsScreen(QWidget* parent = nullptr);

    void setDatabaseManager(DatabaseManager* db);
    void refreshAccountsList();

signals:
    void menuClicked();

private slots:
    void handleSaveChanges();
    void handleRemoveSelected();
    void handleBackToMenu();

private:
    DatabaseManager*       m_db;
    AccountEntryModel*     m_model;
    AccountFilterProxyModel* m_filterModel;
    QLabel*                m_subtitleLabel;
    QLineEdit*             m_searchEdit;
    QCheckBox*             m_missingNamesOnly;
    QTableView*            m_accountsView;
    QPushButton*           m_saveButton;
    QPushButton*           m_removeButton;
    QPushButton*           m_backButton;
};
