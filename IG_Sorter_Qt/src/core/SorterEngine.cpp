#include "core/SorterEngine.h"
#include "core/DatabaseManager.h"
#include "utils/FileUtils.h"
#include <QtConcurrent>
#include <QFutureSynchronizer>
#include <QDir>

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
    CleanupReport combinedReport;

    QFutureSynchronizer<CleanupReport> sync;
    // Map each dir to its index so we can correlate results
    QVector<QString> dirList = m_outputDirs;

    for (int i = 0; i < dirList.size(); ++i) {
        QString dir = dirList[i];
        // Create a DirectoryCleanup instance on the heap so signals work
        auto* cleanup = new DirectoryCleanup(m_db);
        // Capture via queued connection for thread safety
        QObject::connect(cleanup, &DirectoryCleanup::directoryProgress,
                         this, [this, dir](const QString& d, int current, int total) {
            Q_UNUSED(d);
            emit cleanupProgress(dir, current, total);
        }, Qt::QueuedConnection);

        auto future = QtConcurrent::run([cleanup, dir]() {
            CleanupReport result = cleanup->run(dir);
            cleanup->deleteLater();
            return result;
        });
        sync.addFuture(future);
    }

    sync.waitForFinished();

    for (const auto& future : sync.futures()) {
        CleanupReport report = future.result();
        combinedReport.totalDirectoriesScanned += report.totalDirectoriesScanned;
        combinedReport.totalFilesRenamed += report.totalFilesRenamed;
        combinedReport.unresolvedIssues.append(report.unresolvedIssues);
    }

    return combinedReport;
}

QList<FileGroup> SorterEngine::groupFiles() {
    FileGrouper grouper(m_db);
    return grouper.group(m_sourceDir);
}

SortResult SorterEngine::sortFiles(const QStringList& filePaths,
                                   const QString& accountHandle,
                                   const QString& irlName,
                                   AccountType accountType,
                                   const QString& outputDir) {
    Q_UNUSED(accountHandle);
    Q_UNUSED(accountType);

    SortResult result;

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
