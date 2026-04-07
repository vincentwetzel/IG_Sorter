#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include "DatabaseManager.h"
#include "DirectoryCleanup.h"
#include "FileGrouper.h"

struct SortResult {
    int filesSorted = 0;
    int filesSkipped = 0;
    int errors = 0;
    QStringList errorMessages;
};

class SorterEngine : public QObject {
    Q_OBJECT
public:
    explicit SorterEngine(DatabaseManager* db, QObject* parent = nullptr);

    bool         initialize(const QString& sourceDir, const QStringList& outputDirs);
    CleanupReport runCleanup();
    QList<FileGroup> groupFiles();

    // Called by UI when user assigns a selection of files to an output dir
    SortResult sortFiles(const QStringList& filePaths,
                         const QString& accountHandle,
                         const QString& irlName,
                         AccountType accountType,
                         const QString& outputDir);

    const QString&    sourceDir() const { return m_sourceDir; }
    const QStringList& outputDirs() const { return m_outputDirs; }

signals:
    void cleanupProgress(const QString& dir, int current, int total);
    void groupProgress(int current, int total);

private:
    DatabaseManager* m_db;
    QString          m_sourceDir;
    QStringList      m_outputDirs;
};
