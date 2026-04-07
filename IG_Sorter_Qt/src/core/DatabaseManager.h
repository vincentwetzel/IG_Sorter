#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QHash>

enum class AccountType {
    Personal,   // Account belongs to the named person
    Curator,    // Account posts content featuring the named person
    IrlOnly     // No account — name-only entry for validation
};

struct PersonEntry {
    QString     account;    // empty QString when null (irl_only entries)
    QString     irlName;
    AccountType type;

    PersonEntry() : type(AccountType::IrlOnly) {}
    PersonEntry(const QString& a, const QString& n, AccountType t)
        : account(a), irlName(n), type(t) {}
};

class DatabaseManager : public QObject {
    Q_OBJECT
public:
    explicit DatabaseManager(const QString& dbPath, QObject* parent = nullptr);

    bool        load();
    bool        save();
    bool        hasAccount(const QString& handle) const;
    QString     getIrlName(const QString& handle) const;
    PersonEntry getEntry(const QString& handle) const;
    QList<PersonEntry> allEntries() const;
    bool        addEntry(const QString& account, const QString& irlName, AccountType type);
    bool        removeEntry(const QString& account);
    bool        updateEntry(const QString& oldAccount, const QString& newAccount,
                            const QString& newName, AccountType newType);
    bool        hasIrlName(const QString& name) const;

    static QString accountTypeToString(AccountType type);
    static AccountType stringToAccountType(const QString& str);

signals:
    void databaseChanged();

private:
    QString              m_dbPath;
    QList<PersonEntry>   m_entries;
    QHash<QString, int>  m_accountIndex;  // lowercase account -> index in m_entries
};
