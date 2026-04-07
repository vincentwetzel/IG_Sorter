#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class DatabaseManager;

struct CleanupIssue {
    QString     directory;
    QString     personName;
    QStringList filePaths;
};

struct CleanupReport {
    int totalDirectoriesScanned = 0;
    int totalFilesRenamed = 0;
    QList<CleanupIssue> unresolvedIssues;
};

class DirectoryCleanup : public QObject {
    Q_OBJECT
public:
    explicit DirectoryCleanup(DatabaseManager* db, QObject* parent = nullptr);

    // Run cleanup on a single directory (called by QtConcurrent)
    static CleanupReport runSingle(const QString& dirPath, DatabaseManager* db);

    // Instance-based cleanup (uses stored db pointer, emits signals)
    CleanupReport run(const QString& dirPath);

signals:
    void directoryProgress(const QString& dirPath, int current, int total);
    void fileRenamed(const QString& oldPath, const QString& newPath);

private:
    CleanupReport cleanupDirectoryInstance(const QString& dirPath);

    // Static version used by runSingle — no signals
    static CleanupReport cleanupDirectory(const QString& dirPath, DatabaseManager* db);

    DatabaseManager* m_db;
};
