#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class DatabaseManager;

struct CleanupReport {
    int totalDirectoriesScanned = 0;
    int totalFilesRenamed = 0;
    QStringList unresolvedNames;
};

class DirectoryCleanup : public QObject {
    Q_OBJECT
public:
    explicit DirectoryCleanup(DatabaseManager* db, QObject* parent = nullptr);

    // Instance-based cleanup (uses stored db pointer, emits signals)
    CleanupReport run(const QString& dirPath);

signals:
    void directoryProgress(const QString& dirPath, int current, int total);

private:
    CleanupReport cleanupDirectoryInstance(const QString& dirPath);

    DatabaseManager* m_db;
};
