#include "core/DirectoryCleanup.h"
#include "core/DatabaseManager.h"
#include "utils/LogManager.h"
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QRegularExpression>
#include <QUuid>
#include <algorithm>

DirectoryCleanup::DirectoryCleanup(DatabaseManager* db, QObject* parent)
    : QObject(parent), m_db(db) {}

CleanupReport DirectoryCleanup::run(const QString& dirPath) {
    return cleanupDirectoryInstance(dirPath);
}

CleanupReport DirectoryCleanup::runSingle(const QString& dirPath, DatabaseManager* db) {
    return cleanupDirectory(dirPath, db);
}

// Instance-based cleanup that emits progress signals
CleanupReport DirectoryCleanup::cleanupDirectoryInstance(const QString& dirPath) {
    CleanupReport report;
    report.totalDirectoriesScanned = 1;

    QDir dir(dirPath);
    if (!dir.exists()) {
        return report;
    }

    QStringList files;
    QDirIterator it(dirPath, QDir::Files, QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        QString file = it.next();
        QFileInfo fi(file);
        QString lowerName = fi.fileName().toLower();
        if (lowerName == "thumbs.db" || lowerName == "desktop.ini") {
            continue;
        }
        files.append(file);
    }

    std::sort(files.begin(), files.end(), [](const QString& a, const QString& b) {
        QFileInfo fa(a), fb(b);
        return fa.fileName().compare(fb.fileName(), Qt::CaseInsensitive) < 0;
    });

    struct FileEntry {
        QString fullPath;
        QString personName;
        int number;
        QString extension;
    };

    QList<FileEntry> entries;
    QRegularExpression nameRegex(R"((.+?)\s+(\d+))");

    int fileIndex = 0;
    for (const auto& file : files) {
        QFileInfo fi(file);
        QString baseName = fi.completeBaseName();

        QRegularExpressionMatch match = nameRegex.match(baseName);
        if (match.hasMatch()) {
            FileEntry entry;
            entry.fullPath = file;
            entry.personName = match.captured(1).trimmed();
            entry.number = match.captured(2).toInt();
            entry.extension = fi.suffix();
            entries.append(entry);

            if (m_db && !m_db->hasIrlName(entry.personName)) {
                if (!m_db->hasAccount(entry.personName)) {
                    CleanupIssue issue;
                    issue.directory = dirPath;
                    issue.personName = entry.personName;
                    issue.filePaths.append(file);
                    report.unresolvedIssues.append(issue);
                }
            }
        }

        fileIndex++;
        emit directoryProgress(dirPath, fileIndex, files.size());
    }

    // Group by person name
    QHash<QString, QList<FileEntry>> byPerson;
    for (const auto& entry : entries) {
        byPerson[entry.personName].append(entry);
    }

    // Fix numbering per person
    for (auto personIt = byPerson.begin(); personIt != byPerson.end(); ++personIt) {
        QList<FileEntry>& personFiles = personIt.value();
        std::sort(personFiles.begin(), personFiles.end(),
                  [](const FileEntry& a, const FileEntry& b) { return a.number < b.number; });

        int expectedNum = 1;
        bool needsRenaming = false;

        for (auto& entry : personFiles) {
            if (entry.number != expectedNum) {
                needsRenaming = true;
                break;
            }
            expectedNum++;
        }

        if (needsRenaming) {
            expectedNum = 1;

            QList<QPair<QString, QString>> tempRenames;
            for (const auto& entry : personFiles) {
                QString tempName = QString("%1.__TMP_%2.%3")
                                       .arg(entry.personName, QUuid::createUuid().toString(), entry.extension);
                QString tempPath = dirPath + "/" + tempName;
                tempRenames.append({entry.fullPath, tempPath});
            }

            for (const auto& rename : tempRenames) {
                if (QFile::exists(rename.second) && !QFile::remove(rename.second)) {
                    continue;
                }
                if (QFile::rename(rename.first, rename.second)) {
                    LogManager::instance()->logFileRenamed(rename.first, rename.second);
                }
            }

            for (int i = 0; i < personFiles.size(); ++i) {
                QString finalName = QString("%1 %2.%3")
                                        .arg(personFiles[i].personName)
                                        .arg(expectedNum)
                                        .arg(personFiles[i].extension);
                QString finalPath = dirPath + "/" + finalName;

                if (QFile::exists(finalPath) && !QFile::remove(finalPath)) {
                    continue;
                }
                if (QFile::rename(tempRenames[i].second, finalPath)) {
                    report.totalFilesRenamed++;
                    LogManager::instance()->logFileRenamed(tempRenames[i].second, finalPath);
                    emit fileRenamed(tempRenames[i].first, finalPath);
                }

                expectedNum++;
            }
        }
    }

    emit directoryProgress(dirPath, files.size(), files.size());
    return report;
}

// Static version — no signals emitted
CleanupReport DirectoryCleanup::cleanupDirectory(const QString& dirPath, DatabaseManager* db) {
    CleanupReport report;
    report.totalDirectoriesScanned = 1;

    QDir dir(dirPath);
    if (!dir.exists()) {
        return report;
    }

    // Get all files, skip system files
    QStringList files;
    QDirIterator it(dirPath, QDir::Files, QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        QString file = it.next();
        QFileInfo fi(file);
        QString lowerName = fi.fileName().toLower();
        if (lowerName == "thumbs.db" || lowerName == "desktop.ini") {
            continue;
        }
        files.append(file);
    }

    // Natural sort
    std::sort(files.begin(), files.end(), [](const QString& a, const QString& b) {
        QFileInfo fa(a), fb(b);
        return fa.fileName().compare(fb.fileName(), Qt::CaseInsensitive) < 0;
    });

    // Parse names and group by person
    struct FileEntry {
        QString fullPath;
        QString personName;
        int number;
        QString extension;
    };

    QList<FileEntry> entries;
    QRegularExpression nameRegex(R"((.+?)\s+(\d+))");

    for (const auto& file : files) {
        QFileInfo fi(file);
        QString baseName = fi.completeBaseName();

        QRegularExpressionMatch match = nameRegex.match(baseName);
        if (match.hasMatch()) {
            FileEntry entry;
            entry.fullPath = file;
            entry.personName = match.captured(1).trimmed();
            entry.number = match.captured(2).toInt();
            entry.extension = fi.suffix();
            entries.append(entry);

            // Validate person exists in database
            if (db && !db->hasIrlName(entry.personName)) {
                // Check if it's an account handle
                if (!db->hasAccount(entry.personName)) {
                    CleanupIssue issue;
                    issue.directory = dirPath;
                    issue.personName = entry.personName;
                    issue.filePaths.append(file);
                    report.unresolvedIssues.append(issue);
                }
            }
        }
    }

    // Group by person name
    QHash<QString, QList<FileEntry>> byPerson;
    for (const auto& entry : entries) {
        byPerson[entry.personName].append(entry);
    }

    // Fix numbering per person
    for (auto personIt = byPerson.begin(); personIt != byPerson.end(); ++personIt) {
        QList<FileEntry>& personFiles = personIt.value();
        // Sort by current number
        std::sort(personFiles.begin(), personFiles.end(),
                  [](const FileEntry& a, const FileEntry& b) { return a.number < b.number; });

        int expectedNum = 1;
        bool needsRenaming = false;

        for (auto& entry : personFiles) {
            if (entry.number != expectedNum) {
                needsRenaming = true;
                break;
            }
            expectedNum++;
        }

        if (needsRenaming) {
            expectedNum = 1;

            // Phase 1: Rename to temp names
            QList<QPair<QString, QString>> tempRenames;  // old -> temp
            for (const auto& entry : personFiles) {
                QString tempName = QString("%1.__TMP_%2.%3")
                                       .arg(entry.personName, QUuid::createUuid().toString(), entry.extension);
                QString tempPath = dirPath + "/" + tempName;
                tempRenames.append({entry.fullPath, tempPath});
            }

            for (const auto& rename : tempRenames) {
                if (QFile::exists(rename.second) && !QFile::remove(rename.second)) {
                    // Could not remove stale temp file — skip this rename
                    continue;
                }
                if (QFile::rename(rename.first, rename.second)) {
                    LogManager::instance()->logFileRenamed(rename.first, rename.second);
                }
            }

            // Phase 2: Rename to final sequential names
            for (int i = 0; i < personFiles.size(); ++i) {
                QString finalName = QString("%1 %2.%3")
                                        .arg(personFiles[i].personName)
                                        .arg(expectedNum)
                                        .arg(personFiles[i].extension);
                QString finalPath = dirPath + "/" + finalName;

                if (QFile::exists(finalPath) && !QFile::remove(finalPath)) {
                    continue;
                }
                if (QFile::rename(tempRenames[i].second, finalPath)) {
                    report.totalFilesRenamed++;
                    LogManager::instance()->logFileRenamed(tempRenames[i].second, finalPath);
                }

                expectedNum++;
            }
        }
    }

    return report;
}
