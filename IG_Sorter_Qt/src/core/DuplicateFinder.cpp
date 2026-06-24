#include "core/DuplicateFinder.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QFile>
#include <QRegularExpression>
#include <QImage>
#include <QImageReader>
#include <algorithm>
#include <climits>
#include <cmath>
#include <QCryptographicHash>
#include "utils/LogManager.h"
#include <cstdint>
#include <QThread>
#include <QtConcurrent>
#include <QFuture>
#include <QElapsedTimer>
#include <QCoreApplication>
#include <bitset>
#include <atomic>
#include <QStandardPaths>
#include <QDateTime>
#include <QDataStream>
#include <QMutex>
#include <QMutexLocker>

struct CachedHash {
    qint64 size = 0;
    qint64 lastModified = 0;
    bool isImage = false;
    uint64_t dHash = 0;
    QByteArray exactHash;
    QSize dimensions;
    bool isColor = false;
};

inline QDataStream &operator<<(QDataStream &out, const CachedHash &c) {
    out << c.size << c.lastModified << c.isImage << (quint64)c.dHash << c.exactHash << c.dimensions << c.isColor;
    return out;
}

inline QDataStream &operator>>(QDataStream &in, CachedHash &c) {
    quint64 dHash;
    in >> c.size >> c.lastModified >> c.isImage >> dHash >> c.exactHash >> c.dimensions >> c.isColor;
    c.dHash = dHash;
    return in;
}

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

    QImageReader readerA(pathA);
    readerA.setScaledSize(QSize(thumbSize, thumbSize));
    QImage imgA = readerA.read();

    QImageReader readerB(pathB);
    readerB.setScaledSize(QSize(thumbSize, thumbSize));
    QImage imgB = readerB.read();

    if (imgA.isNull() || imgB.isNull()) {
        // If we can't load them as images (e.g. video files, or unsupported formats),
        // but they matched on size and name, treat them as duplicates.
        return 1.0;
    }

    // Convert to grayscale and resize to small thumbnail
    imgA = imgA.convertToFormat(QImage::Format_Grayscale8);
    imgB = imgB.convertToFormat(QImage::Format_Grayscale8);

    // Compare pixel-by-pixel using normalized average difference
    double totalDiff = 0.0;
    int pixelCount = thumbSize * thumbSize;
    for (int y = 0; y < thumbSize; ++y) {
        for (int x = 0; x < thumbSize; ++x) {
            int diff = std::abs(qGray(imgA.pixel(x, y)) - qGray(imgB.pixel(x, y)));
            totalDiff += diff;
        }
    }

    double avgDiff = totalDiff / pixelCount;
    return 1.0 - (avgDiff / 255.0);
}

DuplicateScanResult DuplicateFinder::scan(const QStringList& directories) {
    LogManager::instance()->info(QString("DuplicateFinder: Starting scan of directories: %1").arg(directories.join(", ")));
    DuplicateScanResult result;

    emit scanProgress(0, 0); // Initialize UI state

    // Load hash cache upfront so we can do fast cache matching during the directory traversal
    QHash<QString, CachedHash> cache;
    QString cacheDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/IG_Sorter";
    QDir().mkpath(cacheDir);
    QString cachePath = cacheDir + "/hash_cache_v5.bin";
    QFile cacheFile(cachePath);
    if (cacheFile.open(QIODevice::ReadOnly)) {
        QDataStream in(&cacheFile);
        in.setVersion(QDataStream::Qt_6_0);
        in >> cache;
        cacheFile.close();
        LogManager::instance()->info(QString("DuplicateFinder: Loaded %1 entries from hash cache.").arg(cache.size()));
    }

    QMutex cacheMutex;
    auto saveCache = [&]() {
        if (cacheFile.open(QIODevice::WriteOnly)) {
            QDataStream out(&cacheFile);
            out.setVersion(QDataStream::Qt_6_0);
            out << cache;
            cacheFile.close();
            LogManager::instance()->info(QString("DuplicateFinder: Saved %1 entries to hash cache.").arg(cache.size()));
        }
    };

    struct FileHashData {
        DuplicateFile df;
        bool isImage = false;
        uint64_t pHash = 0;
        QByteArray exactHash;
        bool matched = false;
        int index = 0;
        bool cached = false;
        QImage lowResImg;
        QString parsedPersonName;
    };

    QVector<FileHashData> fileData;
    int fileCount = 0;

    for (const QString& dirPath : directories) {
        LogManager::instance()->info(QString("DuplicateFinder: Scanning directory: %1").arg(dirPath));
        QDirIterator it(dirPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            if (QThread::currentThread()->isInterruptionRequested()) return result;

            it.next();
            QFileInfo entry = it.fileInfo();
            if (entry.size() <= 0) continue; // Skip empty/corrupted files
            
            fileCount++;
            if (fileCount <= 20) {
                LogManager::instance()->info(QString("  -> Found file [%1]: %2 (%3 bytes)").arg(fileCount).arg(entry.fileName()).arg(entry.size()));
            } else if (fileCount == 21) {
                LogManager::instance()->info("  -> [Logging remaining file discoveries paused to prevent log spam]");
            }
            
            if (fileCount == 1 || fileCount % 100 == 0) {
                emit scanProgress(fileCount, 0); // 0 indicates indeterminate total
            }

            FileHashData fd;
            fd.df.filePath = entry.absoluteFilePath();
            fd.df.fileName = entry.fileName();
            fd.df.fileSizeBytes = entry.size();
            fd.index = fileData.size();

            // Query modification time and size from the already populated QFileInfo entry.
            // This is virtually free as QDirIterator already loaded it.
            qint64 size = entry.size();
            qint64 modTime = entry.lastModified().toMSecsSinceEpoch();

            auto cacheIt = cache.find(fd.df.filePath);
            if (cacheIt != cache.end() && cacheIt->size == size && cacheIt->lastModified == modTime) {
                fd.isImage = cacheIt->isImage;
                fd.pHash = cacheIt->dHash;
                fd.exactHash = cacheIt->exactHash;
                fd.df.dimensions = cacheIt->dimensions;
                fd.df.isColor = cacheIt->isColor;
                fd.cached = true;
            }

            // Pre-parse the person name if the file is already sorted
            static QRegularExpression sortedPattern(R"(^(.+?)\s+(\d+)$)");
            auto nameMatch = sortedPattern.match(entry.completeBaseName());
            if (nameMatch.hasMatch()) {
                fd.parsedPersonName = nameMatch.captured(1).trimmed().toLower();
            }

            fileData.append(fd);
        }
    }

    int totalFiles = fileData.size();
    LogManager::instance()->info(QString("DuplicateFinder: Step 1 complete. Found %1 files.").arg(totalFiles));

    if (totalFiles < 2) {
        emit scanProgress(totalFiles, totalFiles);
        return result;
    }

    int progressTotal = totalFiles * 2;
    emit scanProgress(0, progressTotal); // Force UI out of indeterminate state

    LogManager::instance()->info("DuplicateFinder: Step 2 - Generating perceptual hashes...");

    std::atomic<int> hashedCount{0};
    QThread* scanThread = QThread::currentThread();

    auto hashFunc = [&](FileHashData& data) {
        if (scanThread->isInterruptionRequested()) return;

        if (data.cached) {
            hashedCount++;
            return;
        }

        QByteArray format = QImageReader::imageFormat(data.df.filePath);
        if (!format.isEmpty()) {
            QImageReader reader(data.df.filePath);
            data.df.dimensions = reader.size();
            reader.setScaledSize(QSize(256, 256));
            QImage img = reader.read();
            
            if (!img.isNull()) {
                // Extract and cache 16x16 grayscale thumbnail for super-fast visual comparison in Step 3
                QImage thumb = img.scaled(16, 16, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                data.lowResImg = thumb.convertToFormat(QImage::Format_Grayscale8);

                img = img.scaled(9, 8, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
                
                // Check if the image has color before grayscaling
                QImage imgColor = img.convertToFormat(QImage::Format_RGB32);
                long colorDiff = 0;
                for (int y = 0; y < 8; ++y) {
                    const QRgb* line = reinterpret_cast<const QRgb*>(imgColor.constScanLine(y));
                    for (int x = 0; x < 9; ++x) {
                        QRgb pixel = line[x];
                        int r = qRed(pixel);
                        int g = qGreen(pixel);
                        int b = qBlue(pixel);
                        colorDiff += (std::max(r, std::max(g, b)) - std::min(r, std::min(g, b)));
                    }
                }
                // Average difference > 8 per pixel allows for compression artifacts and warm/sepia tones on B&W images
                data.df.isColor = (colorDiff / 72) > 8;

                img = img.convertToFormat(QImage::Format_Grayscale8);
                
                int sum = 0;
                for (int y = 0; y < 8; ++y) {
                    const uchar* line = img.constScanLine(y);
                    for (int x = 0; x < 9; ++x) {
                        sum += line[x];
                    }
                }
                int avg = sum / 72;
                int totalVariance = 0;
                for (int y = 0; y < 8; ++y) {
                    const uchar* line = img.constScanLine(y);
                    for (int x = 0; x < 9; ++x) {
                        totalVariance += std::abs(line[x] - avg);
                    }
                }
                int avgVariance = totalVariance / 72;

                if (avgVariance < 16) {
                    data.isImage = false; // Fallback to exact byte match
                } else {
                    data.isImage = true;
                    uint64_t hash = 0;
                    int bit = 0;
                    for (int y = 0; y < 8; ++y) {
                        const uchar* line = img.constScanLine(y);
                        for (int x = 0; x < 8; ++x) {
                            if (line[x] < line[x + 1]) {
                                hash |= (1ULL << bit);
                            }
                            bit++;
                        }
                    }
                    data.pHash = hash;
                    if (data.index <= 20) {
                        LogManager::instance()->info(QString("  -> Hash for '%1' | dHash: %2").arg(data.df.fileName, QString::number(hash, 16).rightJustified(16, '0')));
                    }
                }
            } else {
                LogManager::instance()->info(QString("  -> Failed to decode image: %1 (Format: %2)").arg(data.df.fileName, QString(format)));
            }
        } else {
            LogManager::instance()->info(QString("  -> Not a recognized image format: %1").arg(data.df.fileName));
        }

        QFileInfo fi(data.df.filePath);
        CachedHash ch;
        ch.size = data.df.fileSizeBytes;
        ch.lastModified = fi.lastModified().toMSecsSinceEpoch();
        ch.isImage = data.isImage;
        ch.dHash = data.pHash;
        ch.exactHash = data.exactHash;
        ch.dimensions = data.df.dimensions;
        ch.isColor = data.df.isColor;
        
        QMutexLocker locker(&cacheMutex);
        cache.insert(data.df.filePath, ch);

        hashedCount++;
    };

    QFuture<void> future = QtConcurrent::map(fileData, hashFunc);

    while (!future.isFinished()) {
        if (scanThread->isInterruptionRequested()) {
            future.cancel();
            break;
        }
        emit scanProgress(hashedCount.load(), progressTotal);
        if (QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread()) {
            QCoreApplication::processEvents();
        }
        QThread::msleep(50); // Prevent flooding the UI thread with thousands of progress signals per second
    }
    future.waitForFinished();
    
    if (scanThread->isInterruptionRequested()) {
        saveCache();
        return result;
    }

    emit scanProgress(totalFiles, progressTotal); // Ensure progress reaches exactly 50%

    LogManager::instance()->info("DuplicateFinder: Step 3 - Comparing hashes...");

    auto getLowResImg = [](FileHashData& fd) -> QImage {
        if (!fd.lowResImg.isNull()) {
            return fd.lowResImg;
        }
        QImageReader reader(fd.df.filePath);
        reader.setScaledSize(QSize(16, 16));
        QImage img = reader.read();
        if (!img.isNull()) {
            fd.lowResImg = img.convertToFormat(QImage::Format_Grayscale8);
        }
        return fd.lowResImg;
    };

    auto visualSimilarityFast = [](const QImage& imgA, const QImage& imgB) -> double {
        if (imgA.isNull() || imgB.isNull()) {
            return 1.0;
        }
        const int thumbSize = 16;
        double totalDiff = 0.0;
        int pixelCount = thumbSize * thumbSize;
        for (int y = 0; y < thumbSize; ++y) {
            const uchar* lineA = imgA.constScanLine(y);
            const uchar* lineB = imgB.constScanLine(y);
            for (int x = 0; x < thumbSize; ++x) {
                totalDiff += std::abs(lineA[x] - lineB[x]);
            }
        }
        double avgDiff = totalDiff / pixelCount;
        return 1.0 - (avgDiff / 255.0);
    };

    QElapsedTimer progressTimer;
    progressTimer.start();

    for (int i = 0; i < totalFiles; ++i) {
        if (QThread::currentThread()->isInterruptionRequested()) {
            saveCache();
            return result;
        }

        if (i % 10 == 0 && progressTimer.elapsed() >= 100) {
            emit scanProgress(totalFiles + i, progressTotal);
            if (QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread()) {
                QCoreApplication::processEvents();
            }
            progressTimer.restart();
        }

        if (fileData[i].matched) continue;

        QList<DuplicateFile> cluster;
        cluster.append(fileData[i].df);
        fileData[i].matched = true;

        for (int j = i + 1; j < totalFiles; ++j) {
            if (QThread::currentThread()->isInterruptionRequested()) {
                saveCache();
                return result;
            }

            if (j % 100 == 0 && progressTimer.elapsed() >= 100) {
                emit scanProgress(totalFiles + i, progressTotal);
                if (QCoreApplication::instance() && QThread::currentThread() == QCoreApplication::instance()->thread()) {
                    QCoreApplication::processEvents();
                }
                progressTimer.restart();
            }

            if (fileData[j].matched) continue;

            // 1. Fast Name Discard for sorted files (different people cannot be duplicates)
            if (!fileData[i].parsedPersonName.isEmpty() && !fileData[j].parsedPersonName.isEmpty()) {
                if (fileData[i].parsedPersonName != fileData[j].parsedPersonName) {
                    continue;
                }
            }

            bool isDupe = false;
            if (fileData[i].isImage && fileData[j].isImage) {
                uint64_t v = fileData[i].pHash ^ fileData[j].pHash;
                int dist = std::bitset<64>(v).count();
                if (dist <= 4) { // <= 4 bits difference means ~93%+ visually similar (drastically reduces false positives)
                    // 2. Fast in-memory pixel-level visual similarity check
                    QImage imgA = getLowResImg(fileData[i]);
                    QImage imgB = getLowResImg(fileData[j]);
                    double similarity = visualSimilarityFast(imgA, imgB);
                    if (similarity >= 0.85) {
                        isDupe = true;
                        LogManager::instance()->info(QString("  *** MATCH FOUND *** -> %1 == %2 (Distance: %3, Sim: %4)").arg(fileData[i].df.fileName, fileData[j].df.fileName).arg(dist).arg(similarity));
                    }
                }
            } else if (!fileData[i].isImage && !fileData[j].isImage) {
                if (fileData[i].df.fileSizeBytes == fileData[j].df.fileSizeBytes) {
                    if (fileData[i].exactHash.isEmpty()) {
                        QFile f(fileData[i].df.filePath);
                        if (f.open(QIODevice::ReadOnly)) {
                            QCryptographicHash h(QCryptographicHash::Sha256);
                            h.addData(&f);
                            fileData[i].exactHash = h.result();
                            cache[fileData[i].df.filePath].exactHash = fileData[i].exactHash;
                        }
                    }
                    if (fileData[j].exactHash.isEmpty()) {
                        QFile f(fileData[j].df.filePath);
                        if (f.open(QIODevice::ReadOnly)) {
                            QCryptographicHash h(QCryptographicHash::Sha256);
                            h.addData(&f);
                            fileData[j].exactHash = h.result();
                            cache[fileData[j].df.filePath].exactHash = fileData[j].exactHash;
                        }
                    }
                    if (!fileData[i].exactHash.isEmpty() && fileData[i].exactHash == fileData[j].exactHash) {
                        isDupe = true;
                        LogManager::instance()->info(QString("  *** EXACT BYTE MATCH FOUND *** -> %1 == %2").arg(fileData[i].df.fileName, fileData[j].df.fileName));
                    }
                }
            }

            if (isDupe) {
                cluster.append(fileData[j].df);
                fileData[j].matched = true;
            }
        }

        if (cluster.size() >= 2) {
            DuplicateGroup group;
            std::sort(cluster.begin(), cluster.end(), [](const DuplicateFile& a, const DuplicateFile& b) {
                // Primary sort: Color images prioritized over Black & White images
                if (a.isColor != b.isColor) {
                    return a.isColor; // true comes before false
                }

                // Secondary sort: Resolution (total pixels) descending
                long pixelsA = a.dimensions.isValid() ? (long)a.dimensions.width() * a.dimensions.height() : 0;
                long pixelsB = b.dimensions.isValid() ? (long)b.dimensions.width() * b.dimensions.height() : 0;
                if (pixelsA != pixelsB) {
                    return pixelsA > pixelsB;
                }
                
                // Tertiary sort: File size descending (larger file might mean less compression)
                if (a.fileSizeBytes != b.fileSizeBytes) {
                    return a.fileSizeBytes > b.fileSizeBytes;
                }

                // Quaternary sort: File name ascending
                return a.fileName < b.fileName;
            });
            group.files = cluster;
            group.keptIndex = -1;
            result.groups.append(group);
            emit groupFound(group);
        }
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

    saveCache();
    emit scanProgress(progressTotal, progressTotal);
    LogManager::instance()->info(QString("DuplicateFinder: Scan complete. Found %1 duplicate groups.").arg(result.groups.size()));
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

    emit scanProgress(0, 0); // Initialize UI state

    for (const QString& dirPath : directories) {
        qDebug() << "DuplicateFinder: Scanning directory:" << dirPath;
        QDirIterator it(dirPath, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            QFileInfo entry = it.fileInfo();
            totalFiles++;

            if (totalFiles == 1 || totalFiles % 25 == 0) {
                emit scanProgress(totalFiles, 0); // 0 indicates indeterminate total
                qDebug() << "DuplicateFinder: Found" << totalFiles << "files so far...";
            }

            if (entry.size() <= 0) continue; // Skip empty/corrupted files
            // Group purely by file size (to the byte) to guarantee we catch duplicate copies
            // even if they have different names or are sorted under different people.
            QString key = QString::number(entry.size());
            sizeNameMap[key].append(entry.absoluteFilePath());
        }
    }

    return sizeNameMap;
}
