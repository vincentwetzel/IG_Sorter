#include "core/SorterEngine.h"
#include "core/DatabaseManager.h"
#include "utils/FileUtils.h"
#include "utils/LogManager.h"
#include <QDir>
#include <QtConcurrent>
#include <QFutureSynchronizer>
#include <QMutex>

SorterEngine::SorterEngine(DatabaseManager* db, QObject* parent)
    : QObject(parent), m_db(db) {}

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
    LogManager::instance()->info(QString("Starting cleanup on %1 directories (parallel)").arg(m_outputDirs.size()));

    // Shared results protected by mutex
    QMutex resultsMutex;
    CleanupReport combinedReport;

    // Run each directory in its own thread
    QFutureSynchronizer<void> sync;
    for (const QString& dir : m_outputDirs) {
        sync.addFuture(QtConcurrent::run([this, dir, &combinedReport, &resultsMutex]() {
            LogManager::instance()->info(QString("Cleaning directory: %1").arg(dir));

            // Create DirectoryCleanup instance on this worker thread
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
            cleanup->deleteLater();

            // Merge results under mutex
            QMutexLocker locker(&resultsMutex);
            combinedReport.totalDirectoriesScanned += report.totalDirectoriesScanned;
            combinedReport.totalFilesRenamed += report.totalFilesRenamed;
            combinedReport.unresolvedIssues.append(report.unresolvedIssues);
            LogManager::instance()->logDirectoryCleaned(dir, report.totalFilesRenamed);

            // Signal that this directory is done (queued to main thread)
            QMetaObject::invokeMethod(this, [this, dir, count = report.totalFilesRenamed]() {
                emit cleanupDirectoryDone(dir, count);
            }, Qt::QueuedConnection);
        }));
    }

    sync.waitForFinished();
    return combinedReport;
}

QList<FileGroup> SorterEngine::groupFiles() {
    // Fix file extensions before grouping so all images load correctly
    ExtensionFixReport fixReport = ExtensionFixer::run(m_sourceDir);
    if (fixReport.filesRenamed > 0) {
        LogManager::instance()->info(
            QString("Fixed %1 file extensions in source directory").arg(fixReport.filesRenamed));
    }

    LogManager::instance()->info(QString("Grouping files from: %1").arg(m_sourceDir));
    FileGrouper grouper(m_db);
    QList<FileGroup> groups = grouper.group(m_sourceDir);
    LogManager::instance()->info(QString("Found %1 groups").arg(groups.size()));
    return groups;
}

SortResult SorterEngine::sortFiles(const QStringList& filePaths,
                                   const QString& accountHandle,
                                   const QString& irlName,
                                   AccountType accountType,
                                   const QString& outputDir) {
    Q_UNUSED(accountHandle);
    Q_UNUSED(accountType);

    SortResult result;

    LogManager::instance()->info(
        QString("Sorting %1 files to %2 as '%3'")
            .arg(filePaths.size()).arg(outputDir).arg(irlName));

    for (const auto& filePath : filePaths) {
        QFileInfo fi(filePath);
        QString ext = fi.suffix();
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
        } else {
            result.errors++;
            result.errorMessages.append(
                QString("Failed to move %1: %2").arg(fi.fileName(), errorMsg));
        }
    }

    return result;
}
