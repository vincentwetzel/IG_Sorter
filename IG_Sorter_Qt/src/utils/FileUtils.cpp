#include "utils/FileUtils.h"
#include "utils/LogManager.h"
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QUuid>

QString FileUtils::safeMove(const QString& sourcePath,
                            const QString& destDir,
                            const QString& destFileName,
                            QString* errorMsg) {
    QDir destDirectory(destDir);
    if (!destDirectory.exists()) {
        if (errorMsg) *errorMsg = "Destination directory does not exist.";
        LogManager::instance()->logError(sourcePath, *errorMsg);
        return QString();
    }

    QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists()) {
        if (errorMsg) *errorMsg = "Source file does not exist.";
        LogManager::instance()->logError(sourcePath, *errorMsg);
        return QString();
    }

    QString finalPath = destDir + "/" + destFileName;

    // If destination already exists, find next available number
    if (QFile::exists(finalPath)) {
        if (errorMsg) *errorMsg = "Destination file already exists.";
        LogManager::instance()->logFileSkipped(sourcePath, *errorMsg);
        return QString();
    }

    // Phase 1: Copy to temp file in destination
    QString tempName = destFileName + ".part." + QUuid::createUuid().toString();
    QString tempPath = destDir + "/" + tempName;

    if (!QFile::copy(sourcePath, tempPath)) {
        if (errorMsg) *errorMsg = "Failed to copy file to destination.";
        LogManager::instance()->logError(sourcePath, *errorMsg);
        return QString();
    }

    // Verify copy
    QFileInfo tempInfo(tempPath);
    if (!tempInfo.exists() || tempInfo.size() != sourceInfo.size()) {
        QFile::remove(tempPath);
        if (errorMsg) *errorMsg = "Copy verification failed.";
        LogManager::instance()->logError(sourcePath, *errorMsg);
        return QString();
    }

    // Phase 2: Rename temp to final name
    if (!QFile::rename(tempPath, finalPath)) {
        QFile::remove(tempPath);
        if (errorMsg) *errorMsg = "Failed to finalize rename.";
        LogManager::instance()->logError(sourcePath, *errorMsg);
        return QString();
    }

    // Phase 3: Delete source only after everything succeeded
    if (!QFile::remove(sourcePath)) {
        if (errorMsg) *errorMsg = "Source file could not be removed after copy.";
        LogManager::instance()->logError(sourcePath, *errorMsg);
        return QString();
    }

    LogManager::instance()->logFileMoved(sourcePath, finalPath, tempInfo.size());
    return finalPath;
}

QString FileUtils::nextAvailableName(const QString& dir,
                                     const QString& baseName,
                                     const QString& ext) {
    Q_UNUSED(ext);
    QDir directory(dir);
    if (!directory.exists()) {
        return baseName + " 1";
    }

    QStringList existingFiles = directory.entryList(QDir::Files);
    QSet<QString> existingBasenames;

    for (const auto& file : existingFiles) {
        QFileInfo fi(file);
        existingBasenames.insert(fi.completeBaseName());
    }

    int num = 1;
    while (true) {
        QString candidate = baseName + " " + QString::number(num);
        if (!existingBasenames.contains(candidate)) {
            return candidate;
        }
        num++;
    }
}
