#include "core/SorterEngine.h"
#include "core/DatabaseManager.h"
#include "utils/FileUtils.h"
#include "utils/LogManager.h"
#include <algorithm>
#include <QCryptographicHash>
#include <QFile>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFuture>
#include <QFutureSynchronizer>
#include <QMutex>
#include <QtConcurrent>

namespace {
    QByteArray getFileHash(const QString& filePath) {
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) return QByteArray();
        QCryptographicHash hash(QCryptographicHash::Sha256);
        if (hash.addData(&file)) {
            return hash.result();
        }
        return QByteArray();
    }

    using DirCache = QHash<QString, QVector<QPair<QString, qint64>>>;

    QString findExactDuplicate(const QString& sourcePath, const QString& irlName, const QStringList& outputDirs, DirCache& dirCache) {
        QFileInfo sourceInfo(sourcePath);
        qint64 sourceSize = sourceInfo.size();
        if (sourceSize <= 0) return QString();

        QString key = irlName.toLower().trimmed();
        if (!dirCache.contains(key)) {
            QVector<QPair<QString, qint64>> targetFiles;
            for (const QString& outDir : outputDirs) {
                QDir dir(outDir);
                QStringList filters;
                filters << QString("%1 *.*").arg(irlName);
                filters << QString("%1.*").arg(irlName);
                QFileInfoList infoList = dir.entryInfoList(filters, QDir::Files);
                for (const QFileInfo& info : infoList) {
                    targetFiles.append({info.absoluteFilePath(), info.size()});
                }
            }
            dirCache.insert(key, targetFiles);
        }

        QByteArray sourceHash; // Lazy evaluation
        const auto& targetFiles = dirCache[key];

        for (const auto& target : targetFiles) {
            if (target.second == sourceSize) {
                if (sourceHash.isEmpty()) {
                    sourceHash = getFileHash(sourcePath);
                    if (sourceHash.isEmpty()) return QString();
                }
                QByteArray targetHash = getFileHash(target.first);
                if (sourceHash == targetHash) {
                    return target.first;
                }
            }
        }
        return QString();
    }
}

SorterEngine::SorterEngine(DatabaseManager* db, QObject* parent)
    : QObject(parent), m_db(db), m_cacheDirModTime(0), m_cacheValid(false) {}

bool SorterEngine::initialize(const QString& sourceDir, const QStringList& outputDirs) {
    m_sourceDir = sourceDir;
    m_outputDirs = outputDirs;

    QDir sourceDirCheck(sourceDir);
    if (!sourceDirCheck.exists()) {
        return false;
    }

    for (const auto& dir : outputDirs) {
        QDir outputDir(dir);
        if (!outputDir.exists()) {
            if (!outputDir.mkpath(".")) {
                return false;
            }
        }
    }

    return true;
}

CleanupReport SorterEngine::runCleanup() {
    LogManager::instance()->info(QString("Starting cleanup on %1 directories (sequential)").arg(m_outputDirs.size()));

    CleanupReport combinedReport;

    // Run each directory sequentially to prevent concurrent logging crashes and race conditions
    for (const QString& dir : m_outputDirs) {
        LogManager::instance()->info(QString("Cleaning directory: %1").arg(dir));

        // Create DirectoryCleanup instance
        auto* cleanup = new DirectoryCleanup(m_db);

        // Connect progress signal — emit via queued invokeMethod to main thread
        QObject::connect(cleanup, &DirectoryCleanup::directoryProgress,
                         this, [this, dir](const QString& d, int current, int total) {
            Q_UNUSED(d);
            QMetaObject::invokeMethod(this, [this, dir, current, total]() {
                emit cleanupProgress(dir, current, total);
            }, Qt::QueuedConnection);
        }, Qt::DirectConnection);

        CleanupReport report = cleanup->run(dir);
        delete cleanup; 

        // Merge results
        combinedReport.totalDirectoriesScanned += report.totalDirectoriesScanned;
        combinedReport.totalFilesRenamed += report.totalFilesRenamed;
        combinedReport.unresolvedIssues.append(report.unresolvedIssues);
        LogManager::instance()->logDirectoryCleaned(dir, report.totalFilesRenamed);

        // Signal that this directory is done (queued to main thread)
        QMetaObject::invokeMethod(this, [this, dir, count = report.totalFilesRenamed]() {
            emit cleanupDirectoryDone(dir, count);
        }, Qt::QueuedConnection);
    }

    return combinedReport;
}

QList<FileGroup> SorterEngine::groupFiles() {
    // Check if we have a valid cache
    if (isCacheValid()) {
        LogManager::instance()->info("Using cached grouping results (skipping disk scan)");
        emit groupsReady(m_cachedGroups);
        return m_cachedGroups;
    }

    LogManager::instance()->info("Cache invalid or missing — performing full disk scan");

    LogManager::instance()->info(QString("Grouping files from: %1").arg(m_sourceDir));
    FileGrouper grouper(m_db);
    connect(&grouper, &FileGrouper::groupsReady, this, &SorterEngine::groupsReady,
            Qt::DirectConnection);
    QList<FileGroup> groups = grouper.groupStreaming(m_sourceDir);
    LogManager::instance()->info(QString("Found %1 groups").arg(groups.size()));

    // Exact duplicate checks run when a batch is sorted. Deferring hashing keeps slow checks from delaying the first preview and prevents the worker from changing files already visible in the sorting screen.

    // Cache the results
    m_cachedGroups = groups;
    updateCache(m_sourceDir);

    return groups;
}

SortResult SorterEngine::sortFiles(const QStringList& filePaths,
                                   const QString& accountHandle,
                                   const QString& irlName,
                                   AccountType accountType,
                                   const QString& outputDir,
                                   const QString& outputFolderName) {
    Q_UNUSED(accountHandle);
    Q_UNUSED(accountType);

    if (irlName.trimmed().isEmpty()) {
        SortResult errResult;
        errResult.errors = filePaths.size();
        errResult.errorMessages.append("Cannot sort files: IRL name is empty.");
        LogManager::instance()->error("Failed to sort files: IRL name is empty.");
        return errResult;
    }

    LogManager::instance()->info(
        QString("Sorting %1 files to %2 as '%3'")
            .arg(filePaths.size()).arg(outputDir).arg(irlName));

    // Shared state protected by mutex
    QMutex mutex;
    SortResult result;
    QList<SortOperation::FileMove> fileMoves;
    DirCache dirCache;

    // Run each file move in its own thread.
    // The name generation + file move is done under the mutex to avoid
    // two threads generating the same filename.
    QFutureSynchronizer<void> sync;
    for (const QString& filePath : filePaths) {
        sync.addFuture(QtConcurrent::run([&, filePath]() {
            // Name generation + file move must be atomic to avoid duplicate names and race conditions
            QMutexLocker locker(&mutex);

            QFileInfo sourceInfo(filePath);
            qint64 sourceSize = sourceInfo.size();

            // Check if an exact duplicate already exists in any of the output directories
            QString duplicatePath = findExactDuplicate(filePath, irlName, m_outputDirs, dirCache);
            if (!duplicatePath.isEmpty()) {
                LogManager::instance()->info(
                    QString("File %1 is an exact duplicate of already sorted file %2. Deleting source.")
                        .arg(sourceInfo.fileName(), duplicatePath));
                if (QFile::remove(filePath)) {
                    result.duplicatesRemoved++;
                } else {
                    result.errors++;
                    result.errorMessages.append(
                        QString("Failed to remove duplicate source file %1").arg(sourceInfo.fileName()));
                }
                return;
            }

            QString ext = sourceInfo.suffix();
            if (!ext.isEmpty()) {
                ext = "." + ext;
            }

            QString baseName = irlName;
            QString nextName = FileUtils::nextAvailableName(outputDir, baseName, ext);
            QString destFileName = nextName + ext;

            QString errorMsg;
            QString destPath = FileUtils::safeMove(filePath, outputDir, destFileName, &errorMsg);

            if (!destPath.isEmpty()) {
                result.filesSorted++;
                SortOperation::FileMove move;
                move.sourcePath = filePath;
                move.destPath = destPath;
                fileMoves.append(move);

                // Add the newly sorted file to dirCache so that subsequent duplicate checks
                // in the same batch can identify identical files.
                QString key = irlName.toLower().trimmed();
                if (dirCache.contains(key)) {
                    dirCache[key].append({destPath, sourceSize});
                }
            } else {
                result.errors++;
                result.errorMessages.append(
                    QString("Failed to move %1: %2").arg(sourceInfo.fileName(), errorMsg));
            }
        }));
    }

    sync.waitForFinished();

    // Build sort operation for undo
    SortOperation op;
    op.outputFolderName = outputFolderName;
    op.irlName = irlName;
    op.fileMoves = fileMoves;

    // Push to undo stack if any files were sorted
    if (op.fileMoves.size() > 0) {
        m_sortHistory.append(op);
        // Invalidate cache since source directory contents changed
        m_cacheValid = false;
    }

    return result;
}

QStringList SorterEngine::undoLastSort() {
    if (m_sortHistory.isEmpty()) return {};

    SortOperation op = m_sortHistory.takeLast();
    int successCount = 0;
    QStringList restoredPaths;  // track restored file paths for reporting

    // Move files back in reverse order using their ORIGINAL filenames
    for (int i = op.fileMoves.size() - 1; i >= 0; --i) {
        const auto& move = op.fileMoves[i];
        QFileInfo destFi(move.destPath);
        QFileInfo origFi(move.sourcePath);
        QString destDir = origFi.absolutePath();
        QString originalFileName = origFi.fileName();  // Use the ORIGINAL filename

        QString errorMsg;
        QString restoredPath = FileUtils::safeMove(move.destPath, destDir, originalFileName, &errorMsg);
        if (!restoredPath.isEmpty()) {
            successCount++;
            restoredPaths.append(restoredPath);
        } else {
            LogManager::instance()->warning(
                QString("Undo failed for %1: %2").arg(destFi.fileName(), errorMsg));
        }
    }

    LogManager::instance()->info(
        QString("Undo: restored %1/%2 files").arg(successCount).arg(op.fileMoves.size()));

    return restoredPaths;
}

void SorterEngine::invalidateCache() {
    m_cacheValid = false;
}

void SorterEngine::updateCacheForAccount(const QString& accountHandle) {
    if (!m_db || accountHandle.isEmpty() || !m_cacheValid) return;

    // Look up the account in the database
    if (!m_db->hasAccount(accountHandle)) return;

    QString irlName = m_db->getIrlName(accountHandle);
    AccountType type = m_db->getEntry(accountHandle).type;

    // Update all matching cached groups
    for (auto& group : m_cachedGroups) {
        if (group.accountHandle == accountHandle) {
            group.isKnown = true;
            group.accountType = type;
            // For Curator and IrlOnly, do NOT set irlName — the account is the photographer,
            // but the name field is for the model (changes per batch)
            if (type != AccountType::Curator && type != AccountType::IrlOnly) {
                group.irlName = irlName;
            }
        }
    }
}

bool SorterEngine::isCacheValid() const {
    if (!m_cacheValid) return false;
    if (m_cacheSourceDir != m_sourceDir) return false;

    // Check if source directory still exists
    QFileInfo dirInfo(m_sourceDir);
    if (!dirInfo.exists() || !dirInfo.isDir()) return false;

    // Check if directory modification time has changed
    // This is a fast check that doesn't require scanning files
    qint64 currentModTime = dirInfo.lastModified().toMSecsSinceEpoch();
    if (currentModTime != m_cacheDirModTime) return false;

    return true;
}

void SorterEngine::updateCache(const QString& sourceDir) {
    m_cacheSourceDir = sourceDir;

    QFileInfo dirInfo(sourceDir);
    if (dirInfo.exists()) {
        m_cacheDirModTime = dirInfo.lastModified().toMSecsSinceEpoch();
    }

    m_cacheValid = true;
}
