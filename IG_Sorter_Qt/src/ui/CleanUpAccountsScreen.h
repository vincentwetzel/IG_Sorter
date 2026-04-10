#pragma once

#include <QWidget>
#include <QList>

class QVBoxLayout;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QListWidget;
class QListWidgetItem;
class QLineEdit;
class QComboBox;
class QCheckBox;
class DatabaseManager;

// Single row for editing one account entry
class AccountEditRow : public QWidget {
    Q_OBJECT
public:
    explicit AccountEditRow(int dbIndex, const QString& account, const QString& currentName,
                            DatabaseManager* db, QWidget* parent = nullptr);
    bool apply();
    QString account() const { return m_account; }
    bool hasChanges() const;
    bool wasSaved() const { return m_wasSaved; }

signals:
    void saved();

private:
    int              m_dbIndex;
    QString          m_account;
    DatabaseManager* m_db;
    QLineEdit*       m_nameEdit;
    QComboBox*       m_typeCombo;
    QPushButton*     m_saveButton;
    QString          m_originalName;
    int              m_originalTypeIndex;
    bool             m_wasSaved = false;
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
    void handleSelectAll(int state);
    void handleSaveSelected();
    void handleBackToMenu();

private:
    void buildAccountsList();

    DatabaseManager*   m_db;

    QVBoxLayout*       m_mainLayout;
    QLabel*            m_titleLabel;
    QLabel*            m_subtitleLabel;
    QCheckBox*         m_selectAllCheckbox;
    QListWidget*       m_accountsList;
    QPushButton*       m_saveButton;
    QPushButton*       m_backButton;
};
