#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include "DatabaseManager.h"
#include "DirectoryCleanup.h"
#include "FileGrouper.h"
#include "ExtensionFixer.h"

struct SortResult {
    int filesSorted = 0;
    int filesSkipped = 0;
    int errors = 0;
    QStringList errorMessages;
};

// Records where sorted files were moved from/to for undo
struct SortOperation {
    struct FileMove {
        QString sourcePath;    // original location
        QString destPath;      // where it was moved to
    };
    QString outputFolderName;  // which output folder
    QString irlName;           // who it was sorted as
    QList<FileMove> fileMoves;
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
                         const QString& outputDir,
                         const QString& outputFolderName);

    // Undo the last sort operation — moves files back to their original locations
    // Returns list of restored file paths that can be reloaded into the UI
    QStringList undoLastSort();
    bool canUndo() const { return !m_sortHistory.isEmpty(); }

    // Invalidate the grouping cache (call when database changes)
    void invalidateCache();

    // Update cached groups for a specific account (call after adding account to DB)
    void updateCacheForAccount(const QString& accountHandle);

    const QString&    sourceDir() const { return m_sourceDir; }
    const QStringList& outputDirs() const { return m_outputDirs; }

signals:
    void cleanupProgress(const QString& dir, int current, int total);
    void cleanupDirectoryDone(const QString& dir, int filesRenamed);
    void groupProgress(int current, int total);

private:
    DatabaseManager* m_db;
    QString          m_sourceDir;
    QStringList      m_outputDirs;
    QList<SortOperation> m_sortHistory;  // stack of sort operations for undo

    // Caching for grouping results to avoid redundant disk scans
    QList<FileGroup> m_cachedGroups;
    QString          m_cacheSourceDir;
    qint64           m_cacheDirModTime;   // directory modification timestamp
    bool             m_cacheValid;        // true if cache is valid

    // Check if cache is valid for the current source directory
    bool isCacheValid() const;
    // Update cache metadata after successful grouping
    void updateCache(const QString& sourceDir);
};
