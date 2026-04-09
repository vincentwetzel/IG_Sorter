#include "core/ExtensionFixer.h"
#include "utils/LogManager.h"
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

QString ExtensionFixer::detectFormat(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return QString();
    }

    QByteArray header = file.read(32);
    file.close();

    if (header.size() < 8) {
        return QString();
    }

    // PNG: 89 50 4E 47 0D 0A 1A 0A
    if (header[0] == '\x89' && header[1] == 'P' &&
        header[2] == 'N' && header[3] == 'G' &&
        header[4] == '\x0D' && header[5] == '\x0A' &&
        header[6] == '\x1A' && header[7] == '\x0A') {
        return "png";
    }

    // JPEG: FF D8 FF
    if (header[0] == '\xFF' && header[1] == '\xD8' && header[2] == '\xFF') {
        return "jpg";
    }

    // WebP: RIFF....WEBP
    if (header.size() >= 12 &&
        header[0] == 'R' && header[1] == 'I' &&
        header[2] == 'F' && header[3] == 'F' &&
        header[8] == 'W' && header[9] == 'E' &&
        header[10] == 'B' && header[11] == 'P') {
        return "webp";
    }

    // GIF: GIF87a or GIF89a
    if (header[0] == 'G' && header[1] == 'I' && header[2] == 'F' &&
        header[3] == '8' && (header[4] == '7' || header[4] == '9') &&
        header[5] == 'a') {
        return "gif";
    }

    // BMP: BM
    if (header[0] == 'B' && header[1] == 'M') {
        return "bmp";
    }

    // TIFF: II* or MM*
    if ((header[0] == 'I' && header[1] == 'I' && header[2] == '\x2A' && header[3] == '\x00') ||
        (header[0] == 'M' && header[1] == 'M' && header[2] == '\x00' && header[3] == '\x2A')) {
        return "tiff";
    }

    return QString();
}

bool ExtensionFixer::needsExtensionFix(const QString& filePath, const QString& detectedFormat) {
    if (detectedFormat.isEmpty()) {
        return false;
    }

    QFileInfo fi(filePath);
    QString currentExt = fi.suffix().toLower();

    // Check if current extension matches detected format
    if (currentExt == detectedFormat) {
        return false;
    }

    // Special case: jpeg files often have .jpg extension
    if (detectedFormat == "jpg" && (currentExt == "jpeg" || currentExt == "jpg")) {
        return false;
    }

    // Special case: tiff files often have .tif extension
    if (detectedFormat == "tiff" && currentExt == "tif") {
        return false;
    }

    return true;
}

bool ExtensionFixer::fixFileExtension(const QString& filePath, const QString& correctFormat,
                                        ExtensionFixReport& report) {
    QFileInfo fi(filePath);
    QString baseName = fi.completeBaseName();
    QString newPath = fi.absolutePath() + "/" + baseName + "." + correctFormat;

    // Check if target file already exists
    if (QFile::exists(newPath)) {
        report.errors.append(
            QString("Cannot rename %1 → %2: target already exists").arg(fi.fileName(), 
                QFileInfo(newPath).fileName()));
        return false;
    }

    if (!QFile::rename(filePath, newPath)) {
        report.errors.append(
            QString("Failed to rename %1 → %2").arg(fi.fileName(), 
                QFileInfo(newPath).fileName()));
        return false;
    }

    report.filesRenamed++;
    report.renamedFiles.append(
        QString("%1 → %2").arg(fi.fileName(), QFileInfo(newPath).fileName()));

    LogManager::instance()->info(
        QString("Fixed extension: %1 → %2 (detected: %3)")
            .arg(fi.fileName(), QFileInfo(newPath).fileName(), correctFormat));

    return true;
}

ExtensionFixReport ExtensionFixer::run(const QString& sourceDir) {
    ExtensionFixReport report;

    QDir dir(sourceDir);
    if (!dir.exists()) {
        LogManager::instance()->warning(
            QString("ExtensionFixer: source directory does not exist: %1").arg(sourceDir));
        return report;
    }

    // Supported image extensions to check
    static const QSet<QString> imageExtensions = {
        "jpg", "jpeg", "png", "gif", "bmp", "webp", "tiff", "tif"
    };

    // Scan all files
    QDirIterator it(sourceDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString filePath = it.next();
        QFileInfo fi(filePath);
        QString ext = fi.suffix().toLower();

        if (!imageExtensions.contains(ext)) {
            continue;  // Skip non-image files
        }

        report.totalFilesScanned++;

        QString detectedFormat = detectFormat(filePath);
        if (detectedFormat.isEmpty()) {
            continue;  // Unknown format, skip
        }

        if (needsExtensionFix(filePath, detectedFormat)) {
            fixFileExtension(filePath, detectedFormat, report);
        }
    }

    if (report.filesRenamed > 0) {
        LogManager::instance()->info(
            QString("ExtensionFixer: fixed %1/%2 files").arg(report.filesRenamed).arg(report.totalFilesScanned));
    } else {
        LogManager::instance()->info(
            QString("ExtensionFixer: all %1 files have correct extensions").arg(report.totalFilesScanned));
    }

    return report;
}
