#include "core/DatabaseManager.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFileInfo>

DatabaseManager::DatabaseManager(const QString& dbPath, QObject* parent)
    : QObject(parent), m_dbPath(dbPath) {}

bool DatabaseManager::load() {
    QFileInfo fi(m_dbPath);
    if (!fi.exists()) {
        return false;
    }

    QFile file(m_dbPath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();

    if (!doc.isArray()) {
        return false;
    }

    m_entries.clear();
    m_accountIndex.clear();

    QJsonArray arr = doc.array();
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject obj = arr[i].toObject();

        PersonEntry entry;
        entry.irlName = obj.value("name").toString();

        if (obj.contains("account") && !obj.value("account").isNull()) {
            entry.account = obj.value("account").toString().toLower();
        }

        QString typeStr = obj.value("type").toString();
        entry.type = stringToAccountType(typeStr);

        if (!entry.account.isEmpty()) {
            m_accountIndex[entry.account] = m_entries.size();
        }

        m_entries.append(entry);
    }

    emit databaseChanged();
    return true;
}

bool DatabaseManager::save() {
    QJsonArray arr;

    for (const auto& entry : m_entries) {
        QJsonObject obj;
        obj["name"] = entry.irlName;

        if (entry.type == AccountType::IrlOnly) {
            obj["account"] = QJsonValue::Null;
        } else {
            obj["account"] = entry.account;
        }

        obj["type"] = accountTypeToString(entry.type);
        arr.append(obj);
    }

    QJsonDocument doc(arr);

    QFile file(m_dbPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    file.write(doc.toJson());
    file.close();
    return true;
}

bool DatabaseManager::hasAccount(const QString& handle) const {
    return m_accountIndex.contains(handle.toLower());
}

QString DatabaseManager::getIrlName(const QString& handle) const {
    QString key = handle.toLower();
    if (m_accountIndex.contains(key)) {
        return m_entries[m_accountIndex.value(key)].irlName;
    }
    return QString();
}

PersonEntry DatabaseManager::getEntry(const QString& handle) const {
    QString key = handle.toLower();
    if (m_accountIndex.contains(key)) {
        return m_entries[m_accountIndex.value(key)];
    }
    return PersonEntry();
}

QList<PersonEntry> DatabaseManager::allEntries() const {
    return m_entries;
}

bool DatabaseManager::addEntry(const QString& account, const QString& irlName, AccountType type) {
    // Don't add duplicate accounts
    if (!account.isEmpty() && hasAccount(account)) {
        return false;
    }

    PersonEntry entry;
    entry.account = account.isEmpty() ? QString() : account.toLower();
    entry.irlName = irlName;
    entry.type = type;

    if (!entry.account.isEmpty()) {
        m_accountIndex[entry.account] = m_entries.size();
    }

    m_entries.append(entry);
    emit databaseChanged();
    return true;
}

bool DatabaseManager::removeEntry(const QString& account) {
    QString key = account.toLower();
    if (!m_accountIndex.contains(key)) {
        return false;
    }

    int index = m_accountIndex.value(key);
    m_accountIndex.remove(key);
    m_entries.removeAt(index);

    // Rebuild index
    m_accountIndex.clear();
    for (int i = 0; i < m_entries.size(); ++i) {
        if (!m_entries[i].account.isEmpty()) {
            m_accountIndex[m_entries[i].account] = i;
        }
    }

    emit databaseChanged();
    return true;
}

bool DatabaseManager::updateEntry(const QString& oldAccount, const QString& newAccount,
                                  const QString& newName, AccountType newType) {
    QString key = oldAccount.toLower();
    if (!m_accountIndex.contains(key)) {
        return false;
    }

    int index = m_accountIndex.value(key);
    m_entries[index].account = newAccount.isEmpty() ? QString() : newAccount.toLower();
    m_entries[index].irlName = newName;
    m_entries[index].type = newType;

    // Rebuild index
    m_accountIndex.clear();
    for (int i = 0; i < m_entries.size(); ++i) {
        if (!m_entries[i].account.isEmpty()) {
            m_accountIndex[m_entries[i].account] = i;
        }
    }

    emit databaseChanged();
    return true;
}

bool DatabaseManager::hasIrlName(const QString& name) const {
    for (const auto& entry : m_entries) {
        if (entry.irlName.compare(name, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

QString DatabaseManager::accountTypeToString(AccountType type) {
    switch (type) {
    case AccountType::Personal: return "personal";
    case AccountType::Curator:  return "curator";
    case AccountType::IrlOnly:  return "irl_only";
    default: return "unknown";
    }
}

AccountType DatabaseManager::stringToAccountType(const QString& str) {
    if (str == "personal") return AccountType::Personal;
    if (str == "curator")  return AccountType::Curator;
    if (str == "irl_only") return AccountType::IrlOnly;
    return AccountType::Personal;  // default
}
