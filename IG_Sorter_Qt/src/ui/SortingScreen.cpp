#include "ui/SortingScreen.h"
#include "ui/ImagePreviewGrid.h"
#include "ui/SortPanel.h"
#include "ui/AddPersonDialog.h"
#include "core/DatabaseManager.h"
#include "core/SorterEngine.h"
#include "utils/ConfigManager.h"
#include <algorithm>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFile>

SortingScreen::SortingScreen(QWidget* parent)
    : QWidget(parent), m_currentGroup(0), m_currentSubBatch(0),
      m_db(nullptr), m_engine(nullptr),
      m_filesSorted(0), m_filesSkipped(0), m_errors(0)
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(10);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // Header
    m_headerLabel = new QLabel("Ready", this);
    m_headerLabel->setAlignment(Qt::AlignCenter);
    QFont headerFont = m_headerLabel->font();
    headerFont.setPointSize(16);
    headerFont.setBold(true);
    m_headerLabel->setFont(headerFont);
    m_mainLayout->addWidget(m_headerLabel);

    // Preview grid
    m_previewGrid = new ImagePreviewGrid(this);
    m_mainLayout->addWidget(m_previewGrid, 1);

    // Sort panel (output buttons + IRL name)
    m_sortPanel = new SortPanel(this);
    m_mainLayout->addWidget(m_sortPanel);

    // Bottom buttons
    m_buttonLayout = new QHBoxLayout();
    m_backButton = new QPushButton("Menu", this);
    m_finishButton = new QPushButton("Finish", this);
    m_finishButton->setEnabled(false);

    m_buttonLayout->addWidget(m_backButton);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_finishButton);

    m_mainLayout->addLayout(m_buttonLayout);

    // Connect signals
    connect(m_backButton, &QPushButton::clicked,
            this, &SortingScreen::backClicked);
    connect(m_finishButton, &QPushButton::clicked,
            this, &SortingScreen::finishClicked);
    connect(m_previewGrid, &ImagePreviewGrid::selectionChanged,
            m_sortPanel, &SortPanel::updateSelectedCount);
    connect(m_sortPanel, &SortPanel::sortToFolderClicked,
            this, &SortingScreen::handleSortToFolder);
    connect(m_sortPanel, &SortPanel::skipClicked,
            this, &SortingScreen::handleSkip);
    connect(m_sortPanel, &SortPanel::deleteSelectedClicked,
            this, &SortingScreen::handleDeleteSelected);
    connect(m_sortPanel, &SortPanel::addUnknownAccount,
            this, &SortingScreen::handleAddUnknownAccount);
    connect(m_sortPanel, &SortPanel::openInstagramClicked,
            this, &SortingScreen::handleOpenInstagram);
    connect(m_sortPanel, &SortPanel::curatorResolvedName,
            this, &SortingScreen::handleCuratorResolvedName);
}

void SortingScreen::setGroups(const QList<FileGroup>& groups) {
    m_groups = groups;
    m_currentGroup = 0;
    m_currentSubBatch = 0;
    m_filesSorted = 0;
    m_filesSkipped = 0;
    m_errors = 0;
    m_errorMessages.clear();
    m_newAccountsAdded.clear();
    m_filesByAccountType.clear();

    if (!groups.isEmpty()) {
        loadNextBatch();
    } else {
        m_headerLabel->setText("No files found in source directory.");
        m_finishButton->setEnabled(true);
    }
}

void SortingScreen::setOutputFolders(const QVector<OutputFolderConfig>& folders) {
    m_outputFolders = folders;
    m_sortPanel->setOutputFolders(folders);
}

void SortingScreen::setDatabaseManager(DatabaseManager* db) {
    m_db = db;
    m_sortPanel->setDatabaseManager(db);
}

void SortingScreen::setEngine(SorterEngine* engine) {
    m_engine = engine;
}

void SortingScreen::loadNextBatch() {
    if (m_currentGroup >= m_groups.size()) {
        m_finishButton->setEnabled(true);
        m_headerLabel->setText("All batches complete!");
        m_previewGrid->clear();
        m_sortPanel->clearSelections();
        emit allBatchesDone();
        return;
    }

    const FileGroup& group = m_groups[m_currentGroup];
    int batchSize = ConfigManager::instance()->batchSize();

    // Detect source type change — reset leaderboard when switching sources
    QString newSource = group.accountHandle;
    bool showFavorites = (group.accountType == AccountType::IrlOnly) ||
                         (!group.isKnown && group.accountType == AccountType::Personal);
    if (showFavorites && newSource != m_currentSourceType) {
        m_currentSourceType = newSource;
        m_nameCounts.clear();
        m_sortPanel->setQuickFillNames(QStringList());
    } else if (!showFavorites) {
        // Known personal or curator — hide favorites entirely
        m_currentSourceType.clear();
        m_nameCounts.clear();
        m_sortPanel->setQuickFillNames(QStringList());
    }

    // Calculate sub-batch boundaries
    int startIdx = m_currentSubBatch * batchSize;
    int endIdx = qMin(startIdx + batchSize, group.filePaths.size());

    if (startIdx >= group.filePaths.size()) {
        m_currentGroup++;
        m_currentSubBatch = 0;
        loadNextBatch();
        return;
    }

    QStringList batchFiles;
    for (int i = startIdx; i < endIdx; ++i) {
        batchFiles.append(group.filePaths[i]);
    }

    m_previewGrid->setImages(batchFiles);
    m_sortPanel->setAccountInfo(group.accountHandle, group.irlName,
                                group.isKnown, group.accountType);

    updateHeader();
}

void SortingScreen::handleSortToFolder(int folderIndex) {
    QStringList selected = m_previewGrid->selectedFilePaths();
    if (selected.isEmpty()) {
        QMessageBox::information(this, "No Selection",
            "Please select one or more images before sorting.");
        return;
    }

    if (folderIndex < 0 || folderIndex >= m_outputFolders.size()) {
        return;
    }

    const FileGroup& group = m_groups[m_currentGroup];
    QString irlName = group.irlName;

    // If unknown, check if user has entered a name in the text field
    if (!group.isKnown && irlName.isEmpty()) {
        QString enteredName = m_sortPanel->getCuratorResolvedName();
        if (enteredName.isEmpty()) {
            QMessageBox::information(this, "Unknown Account",
                "Please enter an IRL name for this unknown account before sorting.");
            return;
        }

        // User entered a name — resolve it automatically
        AccountType type = AccountType::Personal;
        switch (m_sortPanel->getUnknownAccountTypeIndex()) {
        case 1: type = AccountType::Curator; break;
        case 2: type = AccountType::IrlOnly; break;
        default: type = AccountType::Personal; break;
        }

        // Resolve through the add unknown account handler (prompts for IG account if new)
        handleAddUnknownAccount(group.accountHandle, enteredName, type);

        // Read the name from the text field (handleAddUnknownAccount doesn't modify the group)
        irlName = m_sortPanel->getCuratorResolvedName();
        if (irlName.isEmpty()) {
            return;  // User cancelled the Add dialog
        }
    }

    // For curator and IrlOnly (download source types) accounts, require the user
    // to enter who is in the photos
    if (group.accountType == AccountType::Curator ||
        group.accountType == AccountType::IrlOnly) {
        if (!m_sortPanel->isCuratorNameResolved()) {
            QMessageBox::information(this, "Missing Name",
                "Please enter who is in these photos before sorting.");
            return;
        }
        irlName = m_sortPanel->getCuratorResolvedName();
    }

    // For curator and IrlOnly accounts, check if this name exists in the database
    if (group.accountType == AccountType::Curator ||
        group.accountType == AccountType::IrlOnly) {
        // Check if this name exists in the database
        if (m_db && !m_db->hasIrlName(irlName)) {
            // Name not in DB — prompt user to add with optional account
            AddPersonDialog dialog(irlName, this);
            if (dialog.exec() == QDialog::Accepted) {
                QString confirmedName = dialog.irlName();
                if (confirmedName.isEmpty()) {
                    QMessageBox::warning(this, "Empty Name",
                        "A name is required to add this person.");
                    return;
                }
                QString account = dialog.accountHandle();

                // Add to database (personal type since this is the person's own identity)
                if (!account.isEmpty()) {
                    m_db->addEntry(account, confirmedName, AccountType::Personal);
                } else {
                    m_db->addEntry(QString(), confirmedName, AccountType::IrlOnly);
                }
                m_db->save();

                // Refresh completer so the new name appears next time
                m_sortPanel->refreshCompleter();

                irlName = confirmedName;
            } else {
                return;  // User cancelled
            }
        }
    }

    QString outputDir = m_outputFolders[folderIndex].path;

    // Release image handles from thumbnails before moving files
    // This frees any file handles held by Qt's image readers on Windows
    m_previewGrid->releaseImages(selected);

    if (m_engine) {
        SortResult result = m_engine->sortFiles(
            selected, group.accountHandle, irlName,
            group.accountType, outputDir);

        m_filesSorted += result.filesSorted;
        m_errors += result.errors;
        m_errorMessages.append(result.errorMessages);

        // Track by account type
        QString typeKey = DatabaseManager::accountTypeToString(group.accountType);
        m_filesByAccountType[typeKey] += result.filesSorted;

        // Only remove successfully sorted files from the grid
        // Build a set of successfully moved file paths
        int expectedSuccess = result.filesSorted;
        for (const QString& path : selected) {
            if (expectedSuccess <= 0) break;
            m_previewGrid->removePath(path);
            expectedSuccess--;
        }

        // For IrlOnly sources (Twitter, TikTok, etc.) and unknown personal accounts,
        // reset group state after sorting so the next sub-batch asks for a new name
        if (group.accountType == AccountType::IrlOnly) {
            // Track name usage for leaderboard
            m_nameCounts[irlName]++;

            m_groups[m_currentGroup].irlName.clear();
            m_sortPanel->setAccountInfo(group.accountHandle, QString(), true, group.accountType);
            updateFavoriteButtons();
        } else if (group.accountHandle == "Unknown") {
            // Unknown personal account resolved just for this sub-batch — reset for next
            m_nameCounts[irlName]++;

            m_groups[m_currentGroup].irlName.clear();
            m_groups[m_currentGroup].isKnown = false;
            m_groups[m_currentGroup].accountType = AccountType::Personal;
            m_sortPanel->setAccountInfo(group.accountHandle, QString(), false, AccountType::Personal);
            updateFavoriteButtons();
        }

        // Show error if any files failed to sort
        if (result.errors > 0) {
            QString msg;
            if (result.filesSorted > 0) {
                msg = QString("%1 file%2 sorted successfully.\n"
                              "%3 file%4 failed to sort:")
                          .arg(result.filesSorted)
                          .arg(result.filesSorted > 1 ? "s" : "")
                          .arg(result.errors)
                          .arg(result.errors > 1 ? "s" : "");
                // Append first few error messages
                int shown = 0;
                for (const QString& err : result.errorMessages) {
                    if (shown >= 3) {
                        msg += QString("\n... and %1 more.").arg(result.errorMessages.size() - shown);
                        break;
                    }
                    msg += "\n• " + err;
                    shown++;
                }
            } else {
                msg = QString("Failed to sort %1 file%2:").arg(result.errors)
                          .arg(result.errors > 1 ? "s" : "");
                int shown = 0;
                for (const QString& err : result.errorMessages) {
                    if (shown >= 5) {
                        msg += QString("\n... and %1 more.").arg(result.errorMessages.size() - shown);
                        break;
                    }
                    msg += "\n• " + err;
                    shown++;
                }
            }
            QMessageBox::warning(this, "Sort Error", msg);
        }
    }

    // If grid is empty, advance to next sub-batch
    if (!m_previewGrid->hasImages()) {
        m_currentSubBatch++;
        loadNextBatch();
    }
}

void SortingScreen::handleSkip() {
    // Skip means "don't sort these, move to next batch"
    int visibleCount = 0;
    if (m_currentGroup < m_groups.size()) {
        const FileGroup& group = m_groups[m_currentGroup];
        int batchSize = ConfigManager::instance()->batchSize();
        int startIdx = m_currentSubBatch * batchSize;
        int endIdx = qMin(startIdx + batchSize, group.filePaths.size());
        visibleCount = endIdx - startIdx;
    }
    m_filesSkipped += visibleCount;

    m_previewGrid->clear();
    m_currentSubBatch++;
    loadNextBatch();
}

void SortingScreen::handleDeleteSelected() {
    QStringList selected = m_previewGrid->selectedFilePaths();
    if (selected.isEmpty()) {
        QMessageBox::information(this, "No Selection",
            "Please select one or more images to delete.");
        return;
    }

    int count = selected.size();
    int ret = QMessageBox::question(this, "Delete Files",
        QString("Move %1 selected file%2 to the Recycle Bin?")
            .arg(count).arg(count > 1 ? "s" : ""),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (ret != QMessageBox::Yes) {
        return;
    }

    int deletedCount = 0;
    int failedCount = 0;
    for (const QString& path : selected) {
        QFile file(path);
        if (file.moveToTrash()) {
            deletedCount++;
        } else {
            failedCount++;
            m_errorMessages.append("Failed to delete: " + path);
            m_errors++;
        }
    }

    // Remove deleted thumbnails from grid
    m_previewGrid->removeSelected();

    // Report results
    QString msg;
    if (failedCount == 0) {
        msg = QString("%1 file%2 moved to Recycle Bin.")
                  .arg(deletedCount).arg(deletedCount > 1 ? "s" : "");
        QMessageBox::information(this, "Deleted", msg);
    } else {
        msg = QString("%1 file%2 moved to Recycle Bin.\n%3 failed.")
                  .arg(deletedCount).arg(deletedCount > 1 ? "s" : "").arg(failedCount);
        QMessageBox::warning(this, "Partial Delete", msg);
    }

    // If grid is empty, advance to next sub-batch
    if (!m_previewGrid->hasImages()) {
        m_currentSubBatch++;
        loadNextBatch();
    }
}

void SortingScreen::handleAddUnknownAccount(const QString& account,
                                             const QString& irlName,
                                             AccountType type) {
    if (!m_db) return;

    if (irlName.isEmpty()) {
        QMessageBox::warning(this, "Empty Name",
            "Please enter an IRL name for this account.");
        return;
    }

    // Check if this IRL name already exists in the database
    if (m_db->hasIrlName(irlName)) {
        // Name exists — update the group so sorting can proceed
        FileGroup& g = m_groups[m_currentGroup];
        g.irlName = irlName;
        g.isKnown = true;
        g.accountType = type;

        // Update display to show resolved name and hide input widget
        m_sortPanel->setAccountInfo(account, irlName, true, type);
    } else {
        // New name — prompt for Instagram account handle
        AddPersonDialog dialog(irlName, this);
        if (dialog.exec() == QDialog::Accepted) {
            QString confirmedName = dialog.irlName();
            if (confirmedName.isEmpty()) {
                QMessageBox::warning(this, "Empty Name",
                    "A name is required to add this person.");
                return;
            }
            QString accountHandle = dialog.accountHandle();

            // Add to database
            bool added;
            if (!accountHandle.isEmpty()) {
                added = m_db->addEntry(accountHandle, confirmedName, type);
            } else {
                added = m_db->addEntry(QString(), confirmedName, AccountType::IrlOnly);
            }

            if (added) {
                m_db->save();
                m_newAccountsAdded.append(
                    QString("%1 → %2 (%3)")
                        .arg(accountHandle.isEmpty() ? "(no account)" : accountHandle,
                             confirmedName, DatabaseManager::accountTypeToString(type)));

                // Refresh completer so the new name appears in the search
                m_sortPanel->refreshCompleter();

                // Update the group so sorting can proceed
                FileGroup& g = m_groups[m_currentGroup];
                g.irlName = confirmedName;
                g.isKnown = true;
                g.accountType = type;

                // Update display to show resolved name and hide input widget
                m_sortPanel->setAccountInfo(account, confirmedName, true, type);

                QMessageBox::information(this, "Account Added",
                    QString("Added \"%1\" → \"%2\" (%3) to the database.")
                        .arg(accountHandle.isEmpty() ? "(no account)" : accountHandle,
                             confirmedName, DatabaseManager::accountTypeToString(type)));
            } else {
                QMessageBox::warning(this, "Duplicate Account",
                    QString("The account \"%1\" already exists in the database.")
                        .arg(accountHandle));
                return;
            }
        } else {
            return;  // User cancelled
        }
    }
}

void SortingScreen::handleOpenInstagram(const QString& account) {
    if (!account.isEmpty()) {
        QString url = "https://www.instagram.com/" + account;
        QDesktopServices::openUrl(QUrl(url));
    }
}

void SortingScreen::handleCuratorResolvedName(const QString& irlName) {
    if (m_currentGroup >= m_groups.size()) return;

    FileGroup& group = m_groups[m_currentGroup];

    if (group.accountType == AccountType::IrlOnly) {
        // Download source type — add person to database
        // First check if the name already exists
        if (m_db && m_db->hasIrlName(irlName)) {
            // Already in DB — just use it
            group.irlName = irlName;
            group.isKnown = true;
            m_sortPanel->setAccountInfo(group.accountHandle, irlName, true, group.accountType);
        } else {
            // Not in DB — prompt user to add with optional IG account
            AddPersonDialog dialog(irlName, this);
            if (dialog.exec() == QDialog::Accepted) {
                QString confirmedName = dialog.irlName();
                if (confirmedName.isEmpty()) {
                    QMessageBox::warning(this, "Empty Name",
                        "A name is required to add this person.");
                    return;
                }
                QString account = dialog.accountHandle();

                if (m_db) {
                    if (!account.isEmpty()) {
                        m_db->addEntry(account, confirmedName, AccountType::Personal);
                    } else {
                        m_db->addEntry(QString(), confirmedName, AccountType::IrlOnly);
                    }
                    m_db->save();
                }

                // Refresh completer so the new name appears in the search
                m_sortPanel->refreshCompleter();

                group.irlName = confirmedName;
                group.isKnown = true;
                m_sortPanel->setAccountInfo(group.accountHandle, confirmedName, true, group.accountType);

                QString entryText = account.isEmpty()
                    ? QString("Added \"%1\" (no account) to the database.").arg(confirmedName)
                    : QString("Added \"%1\" → \"%2\" to the database.").arg(account, confirmedName);
                QMessageBox::information(this, "Person Added", entryText);
            } else {
                return;  // User cancelled
            }
        }
    } else {
        // Curator accounts don't need DB entries — just resolve the name for this batch
        group.irlName = irlName;
        group.isKnown = true;
        m_sortPanel->setAccountInfo(group.accountHandle, irlName, true, group.accountType);
    }
}

void SortingScreen::updateHeader() {
    if (m_currentGroup < m_groups.size()) {
        const FileGroup& group = m_groups[m_currentGroup];
        int batchSize = ConfigManager::instance()->batchSize();
        int totalInGroup = group.filePaths.size();
        int numSubBatches = (totalInGroup + batchSize - 1) / batchSize;
        int currentSubBatchDisplay = m_currentSubBatch + 1;

        QString accountDisplay = group.accountHandle;
        if (group.accountType == AccountType::Curator) {
            accountDisplay += " [C]";
        }

        m_headerLabel->setText(
            QString("Batch %1 of %2  •  %3  •  %4 files")
                .arg(currentSubBatchDisplay)
                .arg(numSubBatches)
                .arg(accountDisplay)
                .arg(totalInGroup));
    }
}

void SortingScreen::recordNameUsed(const QString& name) {
    if (name.isEmpty()) return;
    m_nameCounts[name]++;
}

void SortingScreen::updateFavoriteButtons() {
    // Build sorted list of all names by count
    struct NameCount {
        QString name;
        int count;
    };
    QList<NameCount> sorted;
    for (auto it = m_nameCounts.constBegin(); it != m_nameCounts.constEnd(); ++it) {
        sorted.append({it.key(), it.value()});
    }
    std::sort(sorted.begin(), sorted.end(), [](const NameCount& a, const NameCount& b) {
        return a.count > b.count;
    });

    QStringList topNames;
    for (const auto& nc : sorted) {
        topNames.append(nc.name);
    }

    m_sortPanel->setQuickFillNames(topNames);
}

QString SortingScreen::getCurrentSourceType() const {
    if (m_currentGroup < m_groups.size()) {
        const FileGroup& group = m_groups[m_currentGroup];
        return group.accountHandle;
    }
    return QString();
}
