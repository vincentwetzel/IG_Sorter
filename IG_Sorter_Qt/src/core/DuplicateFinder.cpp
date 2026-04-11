#include "core/DuplicateFinder.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QRegularExpression>
#include <QImage>
#include <algorithm>
#include <climits>
#include <cmath>

DuplicateFinder::DuplicateFinder(QObject* parent)
    : QObject(parent)
{
}

QString DuplicateFinder::extractPersonName(const QString& fileName) {
    // Strip extension first
    QFileInfo fi(fileName);
    QString baseName = fi.completeBaseName();  // "John Smith 13" from "John Smith 13.jpg"

    // Remove trailing number(s): everything from the last space+number sequence
    static QRegularExpression trailingNum(R"(\s+\d+\s*$)");
    QString name = baseName;
    while (trailingNum.match(name).hasMatch()) {
        name = trailingNum.match(name).capturedStart(0) == -1
                   ? name
                   : name.left(trailingNum.match(name).capturedStart(0));
    }
    return name.trimmed().toLower();
}

double DuplicateFinder::visualSimilarity(const QString& pathA, const QString& pathB) {
    const int thumbSize = 16;

    QImage imgA(pathA);
    QImage imgB(pathB);

    if (imgA.isNull() || imgB.isNull()) return 0.0;

    // Convert to grayscale and resize to small thumbnail
    imgA = imgA.convertToFormat(QImage::Format_Grayscale8)
               .scaled(thumbSize, thumbSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    imgB = imgB.convertToFormat(QImage::Format_Grayscale8)
               .scaled(thumbSize, thumbSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    // Compare pixel-by-pixel using normalized average difference
    double totalDiff = 0.0;
    int pixelCount = thumbSize * thumbSize;
    for (int y = 0; y < thumbSize; ++y) {
        for (int x = 0; x < thumbSize; ++x) {
            int diff = std::abs((int)imgA.pixelIndex(x, y) - (int)imgB.pixelIndex(x, y));
            totalDiff += diff;
        }
    }

    double avgDiff = totalDiff / pixelCount;
    return 1.0 - (avgDiff / 255.0);
}

DuplicateScanResult DuplicateFinder::scan(const QStringList& directories) {
    DuplicateScanResult result;
    int totalFiles = 0;

    // Step 1: Group by file size AND extracted person name
    auto groups = groupFilesBySizeAndName(directories, totalFiles);

    // Step 2: Build DuplicateGroup objects with visual similarity check
    const double similarityThreshold = 0.85;  // 85% visual similarity required

    for (auto it = groups.begin(); it != groups.end(); ++it) {
        const QList<QString>& files = it.value();
        if (files.size() < 2) continue;

        // Build DuplicateFile list
        QList<DuplicateFile> validFiles;
        for (const QString& path : files) {
            QFileInfo fi(path);
            DuplicateFile df;
            df.filePath = path;
            df.fileName = fi.fileName();
            df.fileSizeBytes = fi.size();
            validFiles.append(df);
        }

        // For groups with more than 2 files, check all against the first
        if (validFiles.size() == 2) {
            double sim = visualSimilarity(validFiles[0].filePath, validFiles[1].filePath);
            if (sim < similarityThreshold) continue;
        } else {
            // Keep only files similar to the first one
            QList<DuplicateFile> similarFiles;
            similarFiles.append(validFiles[0]);
            for (int i = 1; i < validFiles.size(); ++i) {
                double sim = visualSimilarity(validFiles[0].filePath, validFiles[i].filePath);
                if (sim >= similarityThreshold) {
                    similarFiles.append(validFiles[i]);
                }
            }
            if (similarFiles.size() < 2) continue;
            validFiles = similarFiles;
        }

        DuplicateGroup group;
        group.files = validFiles;
        group.keptIndex = -1;
        result.groups.append(group);
    }

    // Calculate stats
    result.totalDuplicateFiles = 0;
    result.reclaimableSpace = 0;
    for (const auto& group : result.groups) {
        int dupCount = (group.keptIndex >= 0) ? group.files.size() - 1
                                               : group.files.size();
        result.totalDuplicateFiles += dupCount;
        if (dupCount > 0 && !group.files.isEmpty()) {
            result.reclaimableSpace += group.files[0].fileSizeBytes * dupCount;
        }
    }

    emit scanProgress(totalFiles, totalFiles);
    return result;
}

bool DuplicateFinder::deleteFile(const DuplicateGroup& group, int fileIndex) {
    if (fileIndex < 0 || fileIndex >= group.files.size()) return false;

    const auto& file = group.files[fileIndex];
    if (QFile::exists(file.filePath)) {
        return QFile::remove(file.filePath);
    }
    return false;
}

QHash<QString, QList<QString>> DuplicateFinder::groupFilesBySizeAndName(
    const QStringList& directories, int& totalFiles)
{
    QHash<QString, QList<QString>> sizeNameMap;
    totalFiles = 0;

    for (const QString& dirPath : directories) {
        QDir dir(dirPath);
        if (!dir.exists()) continue;

        QFileInfoList entries = dir.entryInfoList(
            QDir::Files | QDir::NoDotAndDotDot);

        for (const QFileInfo& entry : entries) {
            totalFiles++;
            QString personName = extractPersonName(entry.fileName());
            if (personName.isEmpty()) continue;

            // Composite key: "size|personName"
            QString key = QString("%1|%2").arg(entry.size()).arg(personName);
            sizeNameMap[key].append(entry.absoluteFilePath());
        }
    }

    return sizeNameMap;
}
