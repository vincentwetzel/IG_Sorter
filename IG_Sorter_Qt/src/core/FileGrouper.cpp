#include "core/FileGrouper.h"
#include "core/DatabaseManager.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

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

            if (parsed.matched && m_db && m_db->hasAccount(accountHandle)) {
                group.irlName = m_db->getIrlName(accountHandle);
                group.isKnown = true;
                group.accountType = m_db->getEntry(accountHandle).type;
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

    return groups;
}
