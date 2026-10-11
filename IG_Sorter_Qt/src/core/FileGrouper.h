#pragma once

#include <QString>
#include <QList>
#include <QFileInfo>
#include <QObject>
#include <QMetaType>
#include "core/FileNameParser.h"
#include "core/DatabaseManager.h"

struct FileGroup {
    QString     accountHandle;    // e.g. "joeygore1", "pnv.male_modelnetwork"
    QString     irlName;          // resolved from DB; empty if unknown
    QString     postTimestamp;    // e.g. "2026-03-13_15-30-00"
    QStringList filePaths;        // full paths
    bool        isKnown;          // true if account exists in DB
    AccountType accountType;      // Personal, Curator, or IrlOnly

    FileGroup() : isKnown(false), accountType(AccountType::Personal) {}
};

Q_DECLARE_METATYPE(QList<FileGroup>)

class FileGrouper : public QObject {
    Q_OBJECT
public:
    explicit FileGrouper(DatabaseManager* db, QObject* parent = nullptr);

    // Blocking call — run in a worker thread
    QList<FileGroup> group(const QString& sourceDir);

    // Emits groups after walking the source tree so known personal-account
    // posts can share batches while curator posts remain separate.
    QList<FileGroup> groupStreaming(const QString& sourceDir);

signals:
    void progressChanged(int current, int total);
    void groupsReady(const QList<FileGroup>& groups);

private:
    DatabaseManager* m_db;
};
