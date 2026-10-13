#include "core/FileGrouper.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <utility>

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include "core/DatabaseManager.h"
#include "core/ExtensionFixer.h"

// Time window in seconds for merging screenshot groups taken close together
static const int SCREENSHOT_MERGE_WINDOW_SEC = 15;
static const qint64 UNKNOWN_FILE_MERGE_WINDOW_MSEC = 5 * 1000;

static const QSet<QString>& supportedImageExtensions() {
    static const QSet<QString> extensions = {
        "jpg", "jpeg", "png", "gif", "bmp", "webp", "tiff", "tif", "svg", "ico"
    };
    return extensions;
}

// Parse "YYYY-MM-DD_HH-MM-SS" timestamp to QDateTime
static QDateTime parseTimestamp(const QString& ts) {
    if (ts.isEmpty()) {
        return QDateTime();
    }
    const static QRegularExpression re(R"((\d{4})-(\d{2})-(\d{2})_(\d{2})-(\d{2})-(\d{2}))");
    auto m = re.match(ts);
    if (!m.hasMatch()) {
        return QDateTime();
    }
    return QDateTime(
        QDate(m.capturedView(1).toInt(), m.capturedView(2).toInt(), m.capturedView(3).toInt()),
        QTime(m.capturedView(4).toInt(), m.capturedView(5).toInt(), m.capturedView(6).toInt())
    );
}

// Check if a source type is an Android screenshot variant
static bool isAndroidScreenshot(SourceType t) {
    return t == SourceType::AndroidScreenshot_Instagram ||
           t == SourceType::AndroidScreenshot_Snapchat ||
           t == SourceType::AndroidScreenshot_Chrome;
}

static void resolveGroupAccount(FileGroup& group, const ParsedResult& parsed,
                                DatabaseManager* db) {
    if (parsed.sourceEnum != SourceType::Instagram && parsed.sourceEnum != SourceType::Unknown) {
        group.isKnown = true;
        group.accountType = AccountType::IrlOnly;
        return;
    }
    if (!parsed.matched || !db) {
        return;
    }

    const PersonEntry entry = db->getEntry(group.accountHandle);
    if (entry.account.isEmpty()) {
        return;
    }

    // Curator and IrlOnly entries can intentionally have no name: their
    // batch-specific model name is entered in the UI, not stored on the account.
    if (!entry.irlName.isEmpty() || entry.type == AccountType::Curator ||
        entry.type == AccountType::IrlOnly) {
        group.isKnown = true;
        group.accountType = entry.type;
    }
    if (group.isKnown && entry.type != AccountType::Curator &&
        entry.type != AccountType::IrlOnly) {
        group.irlName = entry.irlName;
    }
}

static void mergePersonalPostsByAccount(QList<FileGroup>& groups) {
    std::sort(groups.begin(), groups.end(),
        [](const FileGroup& a, const FileGroup& b) {
            const int accountOrder = QString::compare(a.accountHandle, b.accountHandle, Qt::CaseInsensitive);
            if (accountOrder != 0) {
                return accountOrder < 0;
            }
            return a.postTimestamp < b.postTimestamp;
        });

    QList<FileGroup> mergedGroups;
    mergedGroups.reserve(groups.size());
    for (const FileGroup& group : std::as_const(groups)) {
        const bool canMerge = group.isKnown &&
                              group.accountType == AccountType::Personal &&
                              !group.accountHandle.isEmpty() &&
                              group.accountHandle != "Unknown" &&
                              group.accountHandle != "Screenshots";
        if (canMerge && !mergedGroups.isEmpty()) {
            FileGroup& previous = mergedGroups.last();
            if (previous.accountHandle.compare(group.accountHandle, Qt::CaseInsensitive) == 0 &&
                previous.isKnown &&
                previous.accountType == AccountType::Personal &&
                previous.accountHandle != "Unknown" &&
                previous.accountHandle != "Screenshots") {
                previous.filePaths.append(group.filePaths);
                continue;
            }
        }
        mergedGroups.append(group);
    }
    groups = std::move(mergedGroups);
}

static void mergeUnknownFilesBySaveTime(QList<FileGroup>& groups) {
    struct Candidate {
        FileGroup group;
        QString directory;
        QDateTime savedAt;
    };

    QList<FileGroup> result;
    QList<Candidate> candidates;
    for (const FileGroup& group : std::as_const(groups)) {
        if (group.accountHandle != "Unknown" || group.isKnown ||
            group.sourceType != SourceType::Unknown) {
            result.append(group);
            continue;
        }

        // Unknown files are initially separate. Split each path here so this
        // stays safe even if another grouping path supplied a multi-file group.
        for (const QString& path : group.filePaths) {
            const QFileInfo fileInfo(path);
            QDateTime savedAt = fileInfo.birthTime();
            if (!savedAt.isValid()) {
                savedAt = fileInfo.lastModified();
            }

            if (!savedAt.isValid()) {
                result.append(group);
                result.last().filePaths = QStringList{path};
                continue;
            }

            Candidate candidate;
            candidate.group = group;
            candidate.group.filePaths = QStringList{path};
            candidate.directory = QDir::cleanPath(fileInfo.absolutePath());
            candidate.savedAt = savedAt;
            candidates.append(candidate);
        }
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        const int directoryOrder = QString::compare(a.directory, b.directory, Qt::CaseInsensitive);
        if (directoryOrder != 0) {
            return directoryOrder < 0;
        }
        return a.savedAt < b.savedAt;
    });

    int index = 0;
    while (index < candidates.size()) {
        Candidate cluster = candidates[index++];
        const QDateTime clusterStart = cluster.savedAt;
        while (index < candidates.size() &&
               candidates[index].directory.compare(cluster.directory, Qt::CaseInsensitive) == 0 &&
               clusterStart.msecsTo(candidates[index].savedAt) <= UNKNOWN_FILE_MERGE_WINDOW_MSEC) {
            cluster.group.filePaths.append(candidates[index].group.filePaths);
            ++index;
        }
        result.append(cluster.group);
    }

    groups = std::move(result);
}

FileGrouper::FileGrouper(DatabaseManager* db, QObject* parent)
    : QObject(parent), m_db(db) {}

QList<FileGroup> FileGrouper::group(const QString& sourceDir) {
    QList<FileGroup> groups;
    QHash<QString, int> groupIndex;  // key -> index in groups
    QVector<bool> groupHasScreenshot; // parallel vector to track screenshot state on the fly
    const QSet<QString>& imageExtensions = supportedImageExtensions();

    // Collect all valid image files in a single pass to avoid running QDirIterator twice
    QStringList validFiles;
    QDirIterator it(sourceDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString path = it.filePath();
        if (imageExtensions.contains(QFileInfo(path).suffix().toLower())) {
            validFiles.append(path);
        }
    }

    int totalFiles = validFiles.size();
    int currentFile = 0;

    for (const QString& filePath : validFiles) {
        currentFile++;

        ParsedResult parsed = FileNameParser::parse(filePath);

        QString key;
        QString accountHandle;
        QString postTimestamp;

        if (parsed.matched) {
            accountHandle = parsed.accountHandle;
            postTimestamp = parsed.postTimestamp;
            key = accountHandle + "|||" + postTimestamp;
        } else {
            // Unknown filenames have no reliable account or post identifier.
            // Keep each file separate rather than treating its folder as an
            // account, which can combine unrelated people into one batch.
            accountHandle = "Unknown";
            QFileInfo fi(filePath);
            postTimestamp = fi.dir().dirName();
            key = "Unknown|||" + QDir::cleanPath(fi.absoluteFilePath());
        }

        auto itGroup = groupIndex.find(key);
        if (itGroup == groupIndex.end()) {
            FileGroup group;
            group.accountHandle = accountHandle;
            group.postTimestamp = postTimestamp;
            group.sourceType = parsed.sourceEnum;

            resolveGroupAccount(group, parsed, m_db);

            itGroup = groupIndex.insert(key, groups.size());
            groups.append(group);
            groupHasScreenshot.append(false);
        }

        int groupIdx = itGroup.value();
        if (groupIdx >= 0 && groupIdx < groups.size()) {
            groups[groupIdx].filePaths.append(filePath);
            if (isAndroidScreenshot(parsed.sourceEnum)) {
                if (groupIdx < groupHasScreenshot.size()) {
                    groupHasScreenshot[groupIdx] = true;
                }
            }
        }

        emit progressChanged(currentFile, totalFiles);
    }

    // Merge screenshot groups taken within a close time window.
    // Screenshot files from different apps (Instagram, Snapchat, Chrome)
    // taken within ~2 minutes are likely from the same browsing session
    // and should be grouped together.
    {
        // Cache parsed timestamps for all groups to avoid redundant parsing
        QVector<QDateTime> cachedTimes(groups.size());
        for (int i = 0; i < groups.size(); ++i) {
            cachedTimes[i] = parseTimestamp(groups[i].postTimestamp);
        }

        // Collect indices of screenshot groups with valid timestamps
        QList<int> screenshotIndices;
        for (int i = 0; i < groups.size(); ++i) {
            if (!cachedTimes[i].isNull() && groupHasScreenshot[i]) {
                screenshotIndices.append(i);
            }
        }

        if (screenshotIndices.size() > 1) {
            // Sort by timestamp
            std::sort(screenshotIndices.begin(), screenshotIndices.end(),
                [&cachedTimes](int a, int b) {
                    return cachedTimes[a] < cachedTimes[b];
                });

            // Merge adjacent groups within the time window
            QSet<int> mergedOut;  // indices that have been absorbed
            for (int i = 0; i < screenshotIndices.size() - 1; ++i) {
                int idxA = screenshotIndices[i];
                if (mergedOut.contains(idxA)) {
                    continue;
                }

                QDateTime tsA = cachedTimes[idxA];
                for (int j = i + 1; j < screenshotIndices.size(); ++j) {
                    int idxB = screenshotIndices[j];
                    if (mergedOut.contains(idxB)) {
                        continue;
                    }

                    QDateTime tsB = cachedTimes[idxB];
                    qint64 diffSecs = tsA.secsTo(tsB);
                    diffSecs = std::abs(diffSecs);

                    if (diffSecs <= SCREENSHOT_MERGE_WINDOW_SEC) {
                        // Merge idxB into idxA
                        if (idxA >= 0 && idxA < groups.size() && idxB >= 0 && idxB < groups.size()) {
                            groups[idxA].filePaths.append(groups[idxB].filePaths);
                            groups[idxA].accountHandle = "Screenshots";
                            // Use the earliest timestamp
                            if (tsB < tsA) {
                                groups[idxA].postTimestamp = groups[idxB].postTimestamp;
                                cachedTimes[idxA] = tsB;
                                tsA = tsB;
                            }
                        }
                        mergedOut.insert(idxB);
                    } else {
                        // Beyond window — no point checking further (sorted by time)
                        break;
                    }
                }
            }

            // Remove absorbed groups (in reverse order to maintain indices)
            QList<int> toRemove = mergedOut.values();
            std::sort(toRemove.begin(), toRemove.end(), std::greater<int>());
            for (int idx : toRemove) {
                if (idx >= 0 && idx < groups.size()) {
                    groups.removeAt(idx);
                }
            }
        }
    }

    // Unknown names have no account key; use save-time proximity within each
    // directory as a conservative heuristic for files downloaded together.
    mergeUnknownFilesBySaveTime(groups);

    // Combine known personal-account posts so batches can span post boundaries.
    // Curator and IrlOnly posts stay separate because each may need its own name.
    mergePersonalPostsByAccount(groups);

    return groups;
}

QList<FileGroup> FileGrouper::groupStreaming(const QString& sourceDir) {
    const QSet<QString>& imageExtensions = supportedImageExtensions();
    constexpr int filesPerChunk = 20;

    QList<FileGroup> allGroups;
    QStringList pendingFiles;
    int scannedFiles = 0;

    auto flushChunk = [this, &allGroups, &pendingFiles, &scannedFiles]() {
        if (pendingFiles.isEmpty()) {
            return;
        }

        QList<FileGroup> chunkGroups;
        QHash<QString, int> groupIndex;
        for (const QString& filePath : std::as_const(pendingFiles)) {
            ParsedResult parsed = FileNameParser::parse(filePath);
            QString accountHandle;
            QString postTimestamp;
            QString key;
            if (parsed.matched) {
                accountHandle = parsed.accountHandle;
                postTimestamp = parsed.postTimestamp;
                key = accountHandle + "|||" + postTimestamp;
            } else {
                accountHandle = "Unknown";
                const QFileInfo fileInfo(filePath);
                postTimestamp = fileInfo.dir().dirName();
                key = "Unknown|||" + QDir::cleanPath(fileInfo.absoluteFilePath());
            }

            auto it = groupIndex.find(key);
            if (it == groupIndex.end()) {
                FileGroup group;
                group.accountHandle = accountHandle;
                group.postTimestamp = postTimestamp;
                group.sourceType = parsed.sourceEnum;
                resolveGroupAccount(group, parsed, m_db);
                it = groupIndex.insert(key, chunkGroups.size());
                chunkGroups.append(group);
            }
            chunkGroups[it.value()].filePaths.append(filePath);
        }

        std::sort(chunkGroups.begin(), chunkGroups.end(), [](const FileGroup& a, const FileGroup& b) {
            if (a.accountHandle != b.accountHandle) {
                return a.accountHandle < b.accountHandle;
            }
            return a.postTimestamp < b.postTimestamp;
        });
        allGroups.append(chunkGroups);
        scannedFiles += pendingFiles.size();
        emit progressChanged(scannedFiles, 0);
        pendingFiles.clear();
    };

    std::function<void(const QString&)> scanDirectory;
    scanDirectory = [&scanDirectory, &pendingFiles, &flushChunk, &imageExtensions](
                        const QString& directoryPath) {
        const QDir directory(directoryPath);
        const QFileInfoList files = directory.entryInfoList(QDir::Files, QDir::Name);
        const QFileInfoList subdirectories = directory.entryInfoList(
            QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

        // Snapshot each directory before processing its files so extension
        // fixes cannot make the iterator miss entries it has not visited yet.
        for (const QFileInfo& fileInfo : files) {
            QString path = fileInfo.absoluteFilePath();
            const QString ext = fileInfo.suffix().toLower();
            if (!imageExtensions.contains(ext)) {
                continue;
            }

            const QString detectedFormat = ExtensionFixer::detectFormat(path);
            if (ExtensionFixer::needsExtensionFix(path, detectedFormat)) {
                if (ExtensionFixer::fixFileExtension(path, detectedFormat)) {
                    path = fileInfo.absolutePath() + "/" + fileInfo.completeBaseName() + "." + detectedFormat;
                }
            }
            pendingFiles.append(path);
            if (pendingFiles.size() >= filesPerChunk) {
                flushChunk();
            }
        }

        // Flush this directory's files before descending into nested folders.
        flushChunk();

        for (const QFileInfo& subdirectory : subdirectories) {
            if (!subdirectory.isSymLink()) {
                scanDirectory(subdirectory.absoluteFilePath());
            }
        }
    };
    scanDirectory(sourceDir);
    flushChunk();

    // A later chunk may contain more posts from an account already seen, so
    // wait until scanning is complete before emitting consolidated groups.
    mergeUnknownFilesBySaveTime(allGroups);
    mergePersonalPostsByAccount(allGroups);
    if (!allGroups.isEmpty()) {
        emit groupsReady(allGroups);
    }
    return allGroups;
}
