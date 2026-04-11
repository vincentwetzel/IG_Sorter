#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QHash>

// Represents a single duplicate file entry
struct DuplicateFile {
    QString filePath;
    QString fileName;
    qint64  fileSizeBytes;
};

// A group of files that are likely duplicates of each other
struct DuplicateGroup {
    QList<DuplicateFile> files;
    int keptIndex = -1;  // -1 = none kept yet, index into files list, -2 = keep all
};

// Result of a duplicate scan
struct DuplicateScanResult {
    QList<DuplicateGroup> groups;
    int totalDuplicateFiles;   // total files that are duplicates (excluding kept ones)
    qint64 reclaimableSpace;   // bytes that could be freed by removing duplicates
};

class DuplicateFinder : public QObject {
    Q_OBJECT
public:
    explicit DuplicateFinder(QObject* parent = nullptr);

    // Scan directories and find duplicate groups
    DuplicateScanResult scan(const QStringList& directories);

    // Delete a file from the filesystem and update the group
    static bool deleteFile(const DuplicateGroup& group, int fileIndex);

signals:
    void scanProgress(int filesScanned, int totalFiles);

private:
    // Extract the person name from a filename (e.g. "Michael Doherty 2.jpg" → "michael doherty")
    static QString extractPersonName(const QString& fileName);

    // Check if two images are visually similar using thumbnail comparison
    // Returns similarity score 0.0-1.0
    static double visualSimilarity(const QString& pathA, const QString& pathB);

    // Group files by byte size, then by extracted person name
    // Returns map of "size|personName" → file paths
    QHash<QString, QList<QString>> groupFilesBySizeAndName(
        const QStringList& directories, int& totalFiles);
};
