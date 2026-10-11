#include "core/DirectoryCleanup.h"
#include "core/DatabaseManager.h"
#include "utils/LogManager.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QUuid>
#include <algorithm>

DirectoryCleanup::DirectoryCleanup(DatabaseManager* db, QObject* parent)
    : QObject(parent), m_db(db) {}

CleanupReport DirectoryCleanup::run(const QString& dirPath) {
    return cleanupDirectoryInstance(dirPath);
}

// Instance-based cleanup that emits progress signals
CleanupReport DirectoryCleanup::cleanupDirectoryInstance(const QString& dirPath) {
    CleanupReport report;
    report.totalDirectoriesScanned = 1;

    QDir dir(dirPath);
    if (!dir.exists()) {
        LogManager::instance()->debug(QString("Directory does not exist, skipping: %1").arg(dirPath));
        return report;
    }

    QStringList fileNames = dir.entryList(QDir::Files | QDir::NoDotAndDotDot,
                                          QDir::Name | QDir::IgnoreCase);
    fileNames.erase(std::remove_if(fileNames.begin(), fileNames.end(), [](const QString& fileName) {
        return fileName.compare("thumbs.db", Qt::CaseInsensitive) == 0 ||
               fileName.compare("desktop.ini", Qt::CaseInsensitive) == 0;
    }), fileNames.end());

    struct FileEntry {
        QString fullPath;
        QString personName;
        int number;
        QString extension;
    };

    QList<FileEntry> entries;
    QSet<QString> checkedNames;
    static const QRegularExpression nameRegex(R"((.+?)\s+(\d+))");

    const int totalFiles = fileNames.size();
    int fileIndex = 0;
    for (const QString& fileName : fileNames) {
        const QString file = dir.filePath(fileName);
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

            if (m_db) {
                const QString& personName = entry.personName;
                const QString normalizedName = personName.toCaseFolded();
                if (!checkedNames.contains(normalizedName)) {
                    checkedNames.insert(normalizedName);
                    const bool isKnown = m_db->hasIrlName(personName) ||
                                         m_db->hasAccount(personName);
                    if (!isKnown) {
                        report.unresolvedNames.append(personName);
                    }
                }
            }
        }

        fileIndex++;
        emit directoryProgress(dirPath, fileIndex, totalFiles);
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

        for (const auto& entry : personFiles) {
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

            for (const auto& [originalPath, tempPath] : tempRenames) {
                if (QFile::exists(tempPath) && !QFile::remove(tempPath)) {
                    continue;
                }
                if (QFile::rename(originalPath, tempPath)) {
                    LogManager::instance()->logFileRenamed(originalPath, tempPath);
                }
            }

            for (int i = 0; i < personFiles.size(); ++i) {
                QString finalName = QString("%1 %2.%3")
                                        .arg(personFiles[i].personName)
                                        .arg(expectedNum)
                                        .arg(personFiles[i].extension);
                QString finalPath = dirPath + "/" + finalName;
                const QString& tempPath = tempRenames[i].second;

                if (QFile::exists(finalPath) && !QFile::remove(finalPath)) {
                    continue;
                }
                if (QFile::rename(tempPath, finalPath)) {
                    report.totalFilesRenamed++;
                    LogManager::instance()->logFileRenamed(tempPath, finalPath);
                }

                expectedNum++;
            }
        }
    }

    emit directoryProgress(dirPath, totalFiles, totalFiles);
    return report;
}
