#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <QSize>

// Represents a single duplicate file entry
struct DuplicateFile {
    QString filePath;
    QString fileName;
    qint64  fileSizeBytes;
    QSize   dimensions;
    bool    isColor = false;
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

signals:
    void scanProgress(int filesScanned, int totalFiles);
    void groupFound(const DuplicateGroup& group);

};
