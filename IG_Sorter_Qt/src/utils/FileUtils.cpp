#include "utils/FileUtils.h"
#include "utils/LogManager.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

QString FileUtils::safeMove(const QString& sourcePath,
                            const QString& destDir,
                            const QString& destFileName,
                            QString* errorMsg) {
    QDir destDirectory(destDir);
    if (!destDirectory.exists()) {
        const QString message = "Destination directory does not exist.";
        if (errorMsg) *errorMsg = message;
        LogManager::instance()->logError(sourcePath, message);
        return QString();
    }

    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists()) {
        const QString message = "Source file does not exist.";
        if (errorMsg) *errorMsg = message;
        LogManager::instance()->logError(sourcePath, message);
        return QString();
    }

    QString finalPath = destDir + "/" + destFileName;

    // Avoid replacing a file already at the target path.
    if (QFile::exists(finalPath)) {
        const QString message = "Destination file already exists.";
        if (errorMsg) *errorMsg = message;
        LogManager::instance()->logFileSkipped(sourcePath, message);
        return QString();
    }

#ifdef Q_OS_WIN
    // Use native MoveFileExW with COPY_ALLOWED — handles cross-volume moves
    // atomically: copies to dest then deletes source in one OS-level operation
    // This avoids the copy-then-delete issue where Qt image handles block deletion
    BOOL moved = MoveFileExW(
        reinterpret_cast<const wchar_t*>(sourcePath.utf16()),
        reinterpret_cast<const wchar_t*>(finalPath.utf16()),
        MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING
    );

    if (moved) {
        QFileInfo destInfo(finalPath);
        LogManager::instance()->logFileMoved(sourcePath, finalPath, destInfo.size());
        return finalPath;
    }

    DWORD err = GetLastError();
    const QString message = QString("MoveFileEx failed (error %1).").arg(err);
    if (errorMsg) {
        *errorMsg = message;
    }
    LogManager::instance()->logError(sourcePath, message);
    return QString();

#else
    // Fallback for non-Windows: copy + delete
    QString tempName = destFileName + ".part." + QCoreApplication::applicationName();
    QString tempPath = destDir + "/" + tempName;

    if (!QFile::copy(sourcePath, tempPath)) {
        const QString message = "Failed to copy file to destination.";
        if (errorMsg) *errorMsg = message;
        LogManager::instance()->logError(sourcePath, message);
        return QString();
    }

    QFileInfo tempInfo(tempPath);
    if (!tempInfo.exists() || tempInfo.size() != sourceInfo.size()) {
        QFile::remove(tempPath);
        const QString message = "Copy verification failed.";
        if (errorMsg) *errorMsg = message;
        LogManager::instance()->logError(sourcePath, message);
        return QString();
    }

    if (!QFile::rename(tempPath, finalPath)) {
        QFile::remove(tempPath);
        const QString message = "Failed to finalize rename.";
        if (errorMsg) *errorMsg = message;
        LogManager::instance()->logError(sourcePath, message);
        return QString();
    }

    if (!QFile::remove(sourcePath)) {
        const QString message = "Source file could not be removed after copy.";
        if (errorMsg) *errorMsg = message;
        LogManager::instance()->logError(sourcePath, message);
        return QString();
    }

    LogManager::instance()->logFileMoved(sourcePath, finalPath, tempInfo.size());
    return finalPath;
#endif
}

QSet<QString> FileUtils::existingBaseNames(const QString& dir) {
    QDir directory(dir);
    QSet<QString> existingBasenames;
    if (directory.exists()) {
        const QStringList existingFiles = directory.entryList(QDir::Files);
        existingBasenames.reserve(existingFiles.size());
        for (const QString& file : existingFiles) {
            existingBasenames.insert(QFileInfo(file).completeBaseName());
        }
    }
    return existingBasenames;
}

QString FileUtils::nextAvailableName(const QSet<QString>& existingBaseNames,
                                     const QString& baseName) {
    int num = 1;
    while (true) {
        QString candidate = baseName + " " + QString::number(num);
        if (!existingBaseNames.contains(candidate)) {
            return candidate;
        }
        num++;
    }
}
