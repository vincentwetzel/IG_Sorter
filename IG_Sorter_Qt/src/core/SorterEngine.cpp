#include "core/SorterEngine.h"
#include "core/DatabaseManager.h"
#include "utils/FileUtils.h"
#include "utils/LogManager.h"
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

    LogManager::instance()->info(QString("Starting cleanup on %1 directories").arg(m_outputDirs.size()));

    for (int i = 0; i < m_outputDirs.size(); ++i) {
        const QString& dir = m_outputDirs[i];

        LogManager::instance()->info(QString("Cleaning directory: %1").arg(dir));

        // Create a DirectoryCleanup instance and run it synchronously
        // (runCleanup itself runs in a background thread via QFutureWatcher)
        auto* cleanup = new DirectoryCleanup(m_db);

        // Connect progress signal before running
        QObject::connect(cleanup, &DirectoryCleanup::directoryProgress,
                         this, [this, dir](const QString& d, int current, int total) {
            Q_UNUSED(d);
            emit cleanupProgress(dir, current, total);
        }, Qt::DirectConnection);

        CleanupReport report = cleanup->run(dir);
        cleanup->deleteLater();

        combinedReport.totalDirectoriesScanned += report.totalDirectoriesScanned;
        combinedReport.totalFilesRenamed += report.totalFilesRenamed;
        combinedReport.unresolvedIssues.append(report.unresolvedIssues);
        LogManager::instance()->logDirectoryCleaned(dir, report.totalFilesRenamed);

        // Signal that this directory is done
        emit cleanupDirectoryDone(dir, report.totalFilesRenamed);
    }

    return combinedReport;
}

QList<FileGroup> SorterEngine::groupFiles() {
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
