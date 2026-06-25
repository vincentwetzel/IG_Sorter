#include "core/FileGrouper.h"

#include <algorithm>
#include <cmath>

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include "core/DatabaseManager.h"

// Time window in seconds for merging screenshot groups taken close together
static const int SCREENSHOT_MERGE_WINDOW_SEC = 15;

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

FileGrouper::FileGrouper(DatabaseManager* db, QObject* parent)
    : QObject(parent), m_db(db) {}

QList<FileGroup> FileGrouper::group(const QString& sourceDir) {
    QList<FileGroup> groups;
    QHash<QString, int> groupIndex;  // key -> index in groups
    QVector<bool> groupHasScreenshot; // parallel vector to track screenshot state on the fly

    // Image file extensions to process (without dot, as QFileInfo::suffix() returns extension without dot)
    static const QSet<QString> imageExtensions = {
        "jpg", "jpeg", "png", "gif", "bmp", "webp", "tiff", "tif", "svg", "ico"
    };

    // Collect all valid image files in a single pass to avoid running QDirIterator twice
    QStringList validFiles;
    QDirIterator it(sourceDir, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QString path = it.filePath();
        const int lastDot = path.lastIndexOf('.');
        if (lastDot != -1) {
            if (imageExtensions.contains(path.mid(lastDot + 1).toLower())) {
                validFiles.append(path);
            }
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
            // Unknown file type — group by parent directory
            accountHandle = "Unknown";
            QFileInfo fi(filePath);
            postTimestamp = fi.dir().dirName();
            key = "Unknown|||" + postTimestamp;
        }

        auto itGroup = groupIndex.find(key);
        if (itGroup == groupIndex.end()) {
            FileGroup group;
            group.accountHandle = accountHandle;
            group.postTimestamp = postTimestamp;

            // Known source types (non-Instagram, e.g. TikTok Slideshow) don't need DB lookup
            if (parsed.sourceEnum != SourceType::Instagram && parsed.sourceEnum != SourceType::Unknown) {
                group.isKnown = true;
                group.accountType = AccountType::IrlOnly;
            } else if (parsed.matched && m_db && m_db->hasAccount(accountHandle)) {
                PersonEntry entry = m_db->getEntry(accountHandle.toLower());
                if (!entry.irlName.isEmpty()) {
                    AccountType acctType = entry.type;
                    group.isKnown = true;
                    group.accountType = acctType;
                    // For Curator and IrlOnly, do NOT set irlName — the account is the photographer,
                    // but the name field is for the model (changes per batch)
                    if (acctType != AccountType::Curator && acctType != AccountType::IrlOnly) {
                        group.irlName = entry.irlName;
                    }
                } else {
                    group.isKnown = false;
                    group.accountType = AccountType::Personal;
                }
            } else {
                group.isKnown = false;
                group.accountType = AccountType::Personal;
            }

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

    // Sort groups by account handle, then by post timestamp.
    // This ensures all posts for the same account are consecutive.
    std::sort(groups.begin(), groups.end(),
        [](const FileGroup& a, const FileGroup& b) {
            if (a.accountHandle != b.accountHandle) {
                return a.accountHandle < b.accountHandle;
            }
            return a.postTimestamp < b.postTimestamp;
        });

    return groups;
}
