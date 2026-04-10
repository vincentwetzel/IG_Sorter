#include "core/FileGrouper.h"
#include "core/DatabaseManager.h"
#include <algorithm>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QDateTime>

// Time window in seconds for merging screenshot groups taken close together
static const int SCREENSHOT_MERGE_WINDOW_SEC = 15;

// Parse "YYYY-MM-DD_HH-MM-SS" timestamp to QDateTime
static QDateTime parseTimestamp(const QString& ts) {
    if (ts.isEmpty()) return QDateTime();
    QRegularExpression re(R"((\d{4})-(\d{2})-(\d{2})_(\d{2})-(\d{2})-(\d{2}))");
    auto m = re.match(ts);
    if (!m.hasMatch()) return QDateTime();
    return QDateTime(
        QDate(m.captured(1).toInt(), m.captured(2).toInt(), m.captured(3).toInt()),
        QTime(m.captured(4).toInt(), m.captured(5).toInt(), m.captured(6).toInt())
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

    // Image file extensions to process (without dot, as QFileInfo::suffix() returns extension without dot)
    static const QSet<QString> imageExtensions = {
        "jpg", "jpeg", "png", "gif", "bmp", "webp", "tiff", "tif", "svg", "ico"
    };

    // Count total files first (filtered by image types)
    int totalFiles = 0;
    QDirIterator countIt(sourceDir, QDir::Files, QDirIterator::Subdirectories);
    while (countIt.hasNext()) {
        countIt.next();
        QFileInfo fi(countIt.filePath());
        if (imageExtensions.contains(fi.suffix().toLower())) {
            totalFiles++;
        }
    }

    int currentFile = 0;
    QDirIterator it(sourceDir, QDir::Files, QDirIterator::Subdirectories);

    while (it.hasNext()) {
        QString filePath = it.next();
        QFileInfo fi(filePath);

        // Skip non-image files
        if (!imageExtensions.contains(fi.suffix().toLower())) {
            continue;
        }

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
            postTimestamp = fi.dir().dirName();
            key = "Unknown|||" + postTimestamp;
        }

        if (!groupIndex.contains(key)) {
            FileGroup group;
            group.accountHandle = accountHandle;
            group.postTimestamp = postTimestamp;

            // Known source types (non-Instagram, e.g. TikTok Slideshow) don't need DB lookup
            if (parsed.sourceEnum != SourceType::Instagram && parsed.sourceEnum != SourceType::Unknown) {
                group.isKnown = true;
                group.accountType = AccountType::IrlOnly;
            } else if (parsed.matched && m_db && m_db->hasAccount(accountHandle)) {
                AccountType acctType = m_db->getEntry(accountHandle).type;
                group.isKnown = true;
                group.accountType = acctType;
                // For Curator and IrlOnly, do NOT set irlName — the account is the photographer,
                // but the name field is for the model (changes per batch)
                if (acctType != AccountType::Curator && acctType != AccountType::IrlOnly) {
                    group.irlName = m_db->getIrlName(accountHandle);
                }
            } else {
                group.isKnown = false;
                group.accountType = AccountType::Personal;
            }

            groupIndex[key] = groups.size();
            groups.append(group);
        }

        groups[groupIndex[key]].filePaths.append(filePath);

        emit progressChanged(currentFile, totalFiles);
    }

    // Merge screenshot groups taken within a close time window.
    // Screenshot files from different apps (Instagram, Snapchat, Chrome)
    // taken within ~2 minutes are likely from the same browsing session
    // and should be grouped together.
    {
        // Collect indices of screenshot groups with valid timestamps
        QList<int> screenshotIndices;
        for (int i = 0; i < groups.size(); ++i) {
            if (!groups[i].postTimestamp.isEmpty() &&
                !parseTimestamp(groups[i].postTimestamp).isNull()) {
                // Check if this group came from any screenshot source type
                bool hasScreenshotSource = false;
                for (const auto& path : groups[i].filePaths) {
                    ParsedResult p = FileNameParser::parse(path);
                    if (isAndroidScreenshot(p.sourceEnum)) {
                        hasScreenshotSource = true;
                        break;
                    }
                }
                if (hasScreenshotSource) {
                    screenshotIndices.append(i);
                }
            }
        }

        if (screenshotIndices.size() > 1) {
            // Sort by timestamp
            std::sort(screenshotIndices.begin(), screenshotIndices.end(),
                [&groups](int a, int b) {
                    return parseTimestamp(groups[a].postTimestamp) <
                           parseTimestamp(groups[b].postTimestamp);
                });

            // Merge adjacent groups within the time window
            QSet<int> mergedOut;  // indices that have been absorbed
            for (int i = 0; i < screenshotIndices.size() - 1; ++i) {
                int idxA = screenshotIndices[i];
                if (mergedOut.contains(idxA)) continue;

                QDateTime tsA = parseTimestamp(groups[idxA].postTimestamp);
                for (int j = i + 1; j < screenshotIndices.size(); ++j) {
                    int idxB = screenshotIndices[j];
                    if (mergedOut.contains(idxB)) continue;

                    QDateTime tsB = parseTimestamp(groups[idxB].postTimestamp);
                    qint64 diffSecs = tsA.secsTo(tsB);
                    if (diffSecs < 0) diffSecs = -diffSecs;

                    if (diffSecs <= SCREENSHOT_MERGE_WINDOW_SEC) {
                        // Merge idxB into idxA
                        groups[idxA].filePaths.append(groups[idxB].filePaths);
                        groups[idxA].accountHandle = "Screenshots";
                        // Use the earliest timestamp
                        if (tsB < tsA) {
                            groups[idxA].postTimestamp = groups[idxB].postTimestamp;
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
                groups.removeAt(idx);
            }
        }
    }

    // Sort groups by account handle, then by post timestamp.
    // This ensures all posts for the same account are consecutive.
    std::sort(groups.begin(), groups.end(),
        [](const FileGroup& a, const FileGroup& b) {
            if (a.accountHandle != b.accountHandle)
                return a.accountHandle < b.accountHandle;
            return a.postTimestamp < b.postTimestamp;
        });

    return groups;
}
