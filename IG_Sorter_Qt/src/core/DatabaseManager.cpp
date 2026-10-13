#include "core/DatabaseManager.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

#include "utils/LogManager.h"

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

    QJsonArray arr = doc.array();

    m_entries.clear();
    m_accountIndex.clear();
    m_entries.reserve(arr.size());
    m_accountIndex.reserve(arr.size());

    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject obj = arr[i].toObject();

        PersonEntry entry;
        entry.irlName = normalizeIrlName(obj.value(QStringLiteral("name")).toString());

        if (obj.contains(QStringLiteral("account")) && !obj.value(QStringLiteral("account")).isNull()) {
            entry.account = obj.value(QStringLiteral("account")).toString().toLower();
        }

        QString typeStr = obj.value(QStringLiteral("type")).toString();
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
        obj[QStringLiteral("name")] = normalizeIrlName(entry.irlName);

        if (entry.type == AccountType::IrlOnly) {
            obj[QStringLiteral("account")] = QJsonValue::Null;
        } else {
            obj[QStringLiteral("account")] = entry.account;
        }

        obj[QStringLiteral("type")] = accountTypeToString(entry.type);
        arr.append(obj);
    }

    QJsonDocument doc(arr);

    // Resolve to absolute path
    QString absPath = QFileInfo(m_dbPath).absoluteFilePath();
    LogManager::instance()->info(
        QString("Saving DB to %1 (%2 entries)").arg(absPath).arg(arr.size()));

    QSaveFile file(m_dbPath);
    if (!file.open(QIODevice::WriteOnly)) {
        LogManager::instance()->error(
            QString("Failed to open DB for writing: %1").arg(m_dbPath));
        return false;
    }

    const QByteArray json = doc.toJson();
    if (file.write(json) != json.size()) {
        LogManager::instance()->error(
            QString("Failed to write complete DB file: %1").arg(m_dbPath));
        file.cancelWriting();
        return false;
    }
    if (!file.commit()) {
        LogManager::instance()->error(
            QString("Failed to replace DB file: %1").arg(m_dbPath));
        return false;
    }
    LogManager::instance()->info(QString("DB saved successfully."));
    return true;
}

bool DatabaseManager::hasAccount(const QString& handle) const {
    return m_accountIndex.contains(handle.toLower());
}

PersonEntry DatabaseManager::getEntry(const QString& handle) const {
    const QString key = handle.toLower();
    auto it = m_accountIndex.find(key);
    if (it != m_accountIndex.end()) {
        int idx = it.value();
        if (idx >= 0 && idx < m_entries.size()) {
            return m_entries[idx];
        }
        LogManager::instance()->error(QString("DB Index out of bounds: %1 for handle %2").arg(idx).arg(key));
    }
    return PersonEntry();
}

const QList<PersonEntry>& DatabaseManager::allEntries() const {
    return m_entries;
}

bool DatabaseManager::addEntry(const QString& account, const QString& irlName, AccountType type) {
    const QString lowerAccount = account.toLower();
    // Don't add duplicate accounts
    if (!lowerAccount.isEmpty() && m_accountIndex.contains(lowerAccount)) {
        return false;
    }

    // A name-only row is an alias for a person, not a separate account. Avoid
    // recreating one when an account-backed entry already has this name.
    if (lowerAccount.isEmpty() && !normalizeIrlName(irlName).isEmpty()
        && hasIrlName(irlName)) {
        return false;
    }

    PersonEntry entry;
    entry.account = lowerAccount.isEmpty() ? QString() : lowerAccount;
    entry.irlName = normalizeIrlName(irlName);
    entry.type = type;

    if (!entry.account.isEmpty()) {
        m_accountIndex[entry.account] = m_entries.size();
    }

    m_entries.append(entry);
    emit databaseChanged();
    return true;
}

bool DatabaseManager::removeEntry(const QString& account) {
    const QString key = account.toLower();
    auto it = m_accountIndex.find(key);
    if (it == m_accountIndex.end()) {
        return false;
    }

    return removeEntryByIndex(it.value());
}

bool DatabaseManager::removeEntryByIndex(int index) {
    if (index < 0 || index >= m_entries.size()) {
        return false;
    }

    const QString account = m_entries[index].account;
    if (!account.isEmpty()) {
        m_accountIndex.remove(account);
    }
    m_entries.removeAt(index);

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
    const QString key = oldAccount.toLower();
    auto it = m_accountIndex.find(key);
    if (it == m_accountIndex.end()) {
        LogManager::instance()->warning(
            QString("updateEntry: account '%1' not found in index").arg(oldAccount));
        return false;
    }

    int index = it.value();
    return updateEntryByIndex(index, newAccount, newName, newType);
}

bool DatabaseManager::updateEntryByIndex(int index, const QString& newAccount,
                                         const QString& newName, AccountType newType) {
    if (index < 0 || index >= m_entries.size()) {
        LogManager::instance()->error(
            QString("updateEntryByIndex: index %1 out of range").arg(index));
        return false;
    }

    QString oldAccount = m_entries[index].account;
    QString newAccountLower = newAccount.isEmpty() ? QString() : newAccount.toLower();
    const auto duplicate = m_accountIndex.constFind(newAccountLower);
    if (!newAccountLower.isEmpty() && duplicate != m_accountIndex.cend()
        && duplicate.value() != index) {
        return false;
    }

    m_entries[index].account = newAccountLower;
    m_entries[index].irlName = normalizeIrlName(newName);
    m_entries[index].type = newType;

    LogManager::instance()->info(
        QString("Updated entry: '%1' → '%2' (%3)")
            .arg(oldAccount.isEmpty() ? "(no account)" : oldAccount,
                 newName,
                 accountTypeToString(newType)));

    // Update index incrementally instead of rebuilding
    if (oldAccount != newAccountLower) {
        if (!oldAccount.isEmpty()) {
            m_accountIndex.remove(oldAccount);
        }
        if (!newAccountLower.isEmpty()) {
            m_accountIndex[newAccountLower] = index;
        }
    }

    emit databaseChanged();
    return true;
}

bool DatabaseManager::hasIrlName(const QString& name) const {
    const QString normalizedName = normalizeIrlName(name);
    for (const auto& entry : m_entries) {
        if (normalizeIrlName(entry.irlName).compare(normalizedName, Qt::CaseInsensitive) == 0) {
            return true;
        }
    }
    return false;
}

QString DatabaseManager::normalizeIrlName(const QString& name) {
    return name.normalized(QString::NormalizationForm_KC).trimmed();
}

QString DatabaseManager::accountTypeToString(AccountType type) {
    switch (type) {
    case AccountType::Personal:
        return QStringLiteral("personal");
    case AccountType::Curator:
        return QStringLiteral("curator");
    case AccountType::IrlOnly:
        return QStringLiteral("irl_only");
    default:
        return QStringLiteral("unknown");
    }
}

AccountType DatabaseManager::stringToAccountType(const QString& str) {
    if (str == QStringLiteral("personal")) {
        return AccountType::Personal;
    }
    if (str == QStringLiteral("curator")) {
        return AccountType::Curator;
    }
    if (str == QStringLiteral("irl_only")) {
        return AccountType::IrlOnly;
    }
    return AccountType::Personal;  // default
}
