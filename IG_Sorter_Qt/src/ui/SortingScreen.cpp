#include "ui/SortingScreen.h"
#include "ui/ImagePreviewGrid.h"
#include "ui/SortPanel.h"
#include "ui/AddPersonDialog.h"
#include "core/DatabaseManager.h"
#include "core/SorterEngine.h"
#include "utils/ConfigManager.h"
#include <QLocale>
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
    m_undoButton = new QPushButton("Undo", this);
    m_undoButton->setEnabled(false);
    m_finishButton = new QPushButton("Finish", this);
    m_finishButton->setEnabled(false);

    m_buttonLayout->addWidget(m_backButton);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_undoButton);
    m_buttonLayout->addStretch();
    m_buttonLayout->addWidget(m_finishButton);

    m_mainLayout->addLayout(m_buttonLayout);

    // Connect signals
    connect(m_backButton, &QPushButton::clicked,
            this, &SortingScreen::backClicked);
    connect(m_undoButton, &QPushButton::clicked,
            this, &SortingScreen::handleUndo);
    connect(m_finishButton, &QPushButton::clicked,
            this, &SortingScreen::finishClicked);
    connect(m_previewGrid, &ImagePreviewGrid::selectionChanged,
            m_sortPanel, &SortPanel::updateSelectedCount);
    connect(m_previewGrid, &ImagePreviewGrid::selectionChanged,
            this, &SortingScreen::handleSelectionChanged);
    connect(m_previewGrid, &ImagePreviewGrid::deleteKeyPressed,
            this, &SortingScreen::handleDeleteSelected);
    connect(m_sortPanel, &SortPanel::sortToFolderClicked,
            this, &SortingScreen::handleSortToFolder);
    connect(m_sortPanel, &SortPanel::skipClicked,
            this, &SortingScreen::handleSkip);
    connect(m_sortPanel, &SortPanel::deleteSelectedClicked,
            this, &SortingScreen::handleDeleteSelected);
    connect(m_sortPanel, &SortPanel::selectAllClicked,
            this, &SortingScreen::handleSelectAll);
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
    // Only clear name input for unknown accounts — known accounts should have name pre-filled
    if (!group.isKnown || group.irlName.isEmpty()) {
        m_sortPanel->clearNameInput();
    }

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

    // For IrlOnly accounts: only check if the ACCOUNT needs linking to DB.
    // The name entered is the MODEL's name — don't do DB lookup on it.
    // For Curator accounts: check if model name exists and prompt to link if needed.
    if (group.accountType == AccountType::IrlOnly) {
        // IrlOnly: just check if the account handle needs to be added to DB
        bool accountNeedsLinking = !group.accountHandle.isEmpty() && m_db &&
                                   !m_db->hasAccount(group.accountHandle);
        if (accountNeedsLinking) {
            // Show dialog to add the photographer account (empty name, just account)
            AddPersonDialog dialog(QString(), group.accountHandle, AccountType::IrlOnly, this);
            if (dialog.exec() == QDialog::Accepted) {
                QString account = dialog.accountHandle();
                if (!account.isEmpty() && !m_db->hasAccount(account)) {
                    m_db->addEntry(account, QString(), AccountType::IrlOnly);
                    m_db->save();
                    updateGroupsForNewAccount(account);
                    m_sortPanel->refreshCompleter();
                }
            } else {
                return;  // User cancelled
            }
        }
        // Proceed with sorting using the model name from text field — don't update group state
    } else if (group.accountType == AccountType::Curator) {
        // Curator accounts: the text field contains the MODEL's name,
        // not the curator's. The curator account (source of photos) is separate.
        bool nameExists = m_db && m_db->hasIrlName(irlName);
        bool accountNeedsLinking = !group.accountHandle.isEmpty() && m_db &&
                                   !m_db->hasAccount(group.accountHandle);

        if (nameExists && !accountNeedsLinking) {
            // Model exists and account is already linked — proceed directly
            // Don't persist model name to group — each batch can have a different model
        } else if (nameExists && accountNeedsLinking) {
            // Model exists but curator account doesn't — show a clear confirmation
            // to add the SOURCE ACCOUNT (photographer/curator), not the model
            int ret = QMessageBox::question(this, "Add Curator Account",
                QString("The model \"%1\" is in the database.\n\n"
                        "Add the source account \"%2\" as a Curator (photographer)?")
                    .arg(irlName, group.accountHandle),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

            if (ret == QMessageBox::Yes) {
                m_db->addEntry(group.accountHandle, QString(), AccountType::Curator);
                m_db->save();
                updateGroupsForNewAccount(group.accountHandle);
                m_sortPanel->refreshCompleter();
            } else {
                return;  // User cancelled — don't sort
            }
        } else {
            // Neither model nor account are ready — check specific cases
            if (nameExists && accountNeedsLinking) {
                // Model exists but curator account doesn't — simple confirmation
                int ret = QMessageBox::question(this, "Add Curator Account",
                    QString("The model \"%1\" is in the database.\n\n"
                            "Add the source account \"%2\" as a Curator (photographer)?")
                        .arg(irlName, group.accountHandle),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

                if (ret == QMessageBox::No) {
                    return;  // User cancelled — don't sort
                }
                // User said Yes — add the curator account
                m_db->addEntry(group.accountHandle, QString(), AccountType::Curator);
                m_db->save();
                updateGroupsForNewAccount(group.accountHandle);
                m_sortPanel->refreshCompleter();
            } else {
                // Model doesn't exist — show full Add Person dialog
                AddPersonDialog dialog(irlName, group.accountHandle,
                                       AccountType::Personal, this);
                if (dialog.exec() == QDialog::Accepted) {
                    QString confirmedName = dialog.irlName();
                    QString account = dialog.accountHandle();
                    AccountType dialogType = dialog.accountType();

                    if (confirmedName.isEmpty() && account.isEmpty()) {
                        QMessageBox::warning(this, "Empty Name",
                            "Please enter a name or an account handle.");
                        return;
                    }

                    // Case 1: Account given but no name — just add the account
                    if (confirmedName.isEmpty() && !account.isEmpty()) {
                        if (!m_db->hasAccount(account)) {
                            m_db->addEntry(account, QString(), AccountType::Personal);
                            m_db->save();
                            updateGroupsForNewAccount(account);
                            m_sortPanel->refreshCompleter();
                            QMessageBox::information(this, "Account Added",
                                QString("Added \"%1\" (no name) to the database.")
                                    .arg(account));
                        } else {
                            QMessageBox::information(this, "Account Found",
                                QString("\"%1\" is already in the database.").arg(account));
                        }
                        return;
                    }

                    // Case 2: Name given (account may or may not be given)
                    if (m_db->hasIrlName(confirmedName)) {
                        // Name exists — check if this account is new
                        if (!account.isEmpty() && !m_db->hasAccount(account)) {
                            m_db->addEntry(account, confirmedName, dialogType);
                            m_db->save();
                            updateGroupsForNewAccount(account);
                            m_sortPanel->refreshCompleter();
                        }
                        // Name exists — use it regardless of whether account was added
                    } else {
                        // Brand new person
                        if (!account.isEmpty()) {
                            m_db->addEntry(account, confirmedName, dialogType);
                        } else {
                            m_db->addEntry(QString(), confirmedName, dialogType);
                        }
                        m_db->save();
                        if (!account.isEmpty()) {
                            updateGroupsForNewAccount(account);
                        }
                        m_sortPanel->refreshCompleter();
                    }

                    // Update group state so next batch pre-fills the name
                    // For Curator accounts, don't persist model name — each batch has a different model
                    FileGroup& g = m_groups[m_currentGroup];
                    if (group.accountType != AccountType::Curator) {
                        g.irlName = confirmedName;
                    }
                    g.isKnown = true;
                    irlName = confirmedName;
                    // For Curator, don't pre-fill the name field — keep it clear for next model
                    if (group.accountType != AccountType::Curator) {
                        m_sortPanel->setAccountInfo(group.accountHandle, confirmedName, true,
                                                    group.accountType);
                    }
                } else {
                    return;  // User cancelled — don't sort
                }
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
            group.accountType, outputDir,
            m_outputFolders[folderIndex].name);

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
            m_sortPanel->clearNameInput();
            updateFavoriteButtons();
        } else if (group.accountHandle == "Unknown") {
            // Unknown personal account resolved just for this sub-batch — reset for next
            m_nameCounts[irlName]++;

            m_groups[m_currentGroup].irlName.clear();
            m_groups[m_currentGroup].isKnown = false;
            m_groups[m_currentGroup].accountType = AccountType::Personal;
            m_sortPanel->setAccountInfo(group.accountHandle, QString(), false, AccountType::Personal);
            m_sortPanel->clearNameInput();
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
    } else {
        // Grid still has images — update header to reflect sorted count
        updateHeader();
    }

    // Update undo button state
    updateUndoButton();
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

void SortingScreen::handleSelectAll(bool selectAll) {
    if (selectAll) {
        m_previewGrid->selectAll();
    } else {
        m_previewGrid->deselectAll();
    }
}

void SortingScreen::handleSelectionChanged(int count) {
    Q_UNUSED(count);
    // Update the select all button text based on current selection
    bool allSelected = m_previewGrid->allSelected();
    m_sortPanel->updateSelectAllButtonText(allSelected);
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

void SortingScreen::handleUndo() {
    if (!m_engine || !m_engine->canUndo()) return;

    QStringList restoredPaths = m_engine->undoLastSort();
    if (!restoredPaths.isEmpty()) {
        // Clear the grid and reload current batch — restored files will reappear
        m_previewGrid->clear();
        loadNextBatch();
        updateUndoButton();

        int count = restoredPaths.size();
        QString msg = QString("Restored %1 file%2 to the source directory.\n"
                              "You can now re-sort these files.")
                          .arg(count)
                          .arg(count > 1 ? "s" : "");
        QMessageBox::information(this, "Undo Successful", msg);
    } else {
        QMessageBox::warning(this, "Undo Failed",
            "Failed to restore files. They may have been deleted or moved elsewhere.");
    }
}

void SortingScreen::updateUndoButton() {
    m_undoButton->setEnabled(m_engine && m_engine->canUndo());
}

void SortingScreen::handleAddUnknownAccount(const QString& account,
                                             const QString& irlName,
                                             AccountType type) {
    if (!m_db) return;

    // Check if we can proceed without showing the dialog
    bool nameExists = m_db->hasIrlName(irlName);
    bool accountNeedsLinking = !account.isEmpty() && !m_db->hasAccount(account);

    if (nameExists && !accountNeedsLinking) {
        // Name exists and account is already linked (or no account to link) — proceed directly
        QString confirmedName = irlName;

        // Update the group so sorting can proceed
        FileGroup& g = m_groups[m_currentGroup];
        g.isKnown = true;
        g.accountType = type;

        // For Curator and IrlOnly, do NOT set irlName — the account is the photographer,
        // but the name field is for the MODEL in the photos (changes per batch)
        if (type != AccountType::Curator && type != AccountType::IrlOnly) {
            g.irlName = confirmedName;
        }

        // Update UI — for Curator/IrlOnly, pass empty name so text field stays clear
        QString displayName = (type == AccountType::Curator || type == AccountType::IrlOnly)
                                  ? QString()
                                  : confirmedName;
        m_sortPanel->setAccountInfo(account, displayName, true, type);
    } else {
        // Show the dialog so the user can link this account to the person
        // (even if the IRL name already exists — people can have multiple accounts)
        AddPersonDialog dialog(irlName, account, type, this);
        if (dialog.exec() == QDialog::Accepted) {
            QString confirmedName = dialog.irlName();
            QString accountHandle = dialog.accountHandle();
            AccountType dialogType = dialog.accountType();

            if (confirmedName.isEmpty() && accountHandle.isEmpty()) {
                QMessageBox::warning(this, "Empty Name",
                    "Please enter a name or an account handle.");
                return;
            }

            // Case 1: Account given but no name — just add the account with empty name
            if (confirmedName.isEmpty() && !accountHandle.isEmpty()) {
                if (!m_db->hasAccount(accountHandle)) {
                    bool added = m_db->addEntry(accountHandle, QString(), dialogType);
                    if (added) {
                        m_db->save();
                        updateGroupsForNewAccount(accountHandle);
                        m_sortPanel->refreshCompleter();
                        QMessageBox::information(this, "Account Added",
                            QString("Added \"%1\" (no name) to the database.")
                                .arg(accountHandle));

                        // Update group so sorting can proceed
                        FileGroup& g = m_groups[m_currentGroup];
                        g.irlName = QString();
                        g.isKnown = true;
                        g.accountType = dialogType;
                        m_sortPanel->setAccountInfo(account, QString(), true, dialogType);
                    } else {
                        QMessageBox::warning(this, "Duplicate Account",
                            QString("The account \"%1\" already exists in the database.")
                                .arg(accountHandle));
                        return;
                    }
                } else {
                    QMessageBox::information(this, "Account Found",
                        QString("\"%1\" is already in the database.").arg(accountHandle));
                }
                return;
            }

            // Case 2: Name given (account may or may not be given)
            if (m_db->hasIrlName(confirmedName)) {
                // Name already exists — check if this account is new
                if (!accountHandle.isEmpty() && !m_db->hasAccount(accountHandle)) {
                    // Add the new account to the existing person
                    bool added = m_db->addEntry(accountHandle, confirmedName, dialogType);
                    if (added) {
                        m_db->save();
                        updateGroupsForNewAccount(accountHandle);
                        m_newAccountsAdded.append(
                            QString("%1 → %2 (%3)")
                                .arg(accountHandle, confirmedName,
                                     DatabaseManager::accountTypeToString(dialogType)));
                        QMessageBox::information(this, "Account Added",
                            QString("Linked \"%1\" to \"%2\" in the database.")
                                .arg(accountHandle, confirmedName));
                    } else {
                        QMessageBox::warning(this, "Duplicate Account",
                            QString("The account \"%1\" already exists in the database.")
                                .arg(accountHandle));
                        return;
                    }
                } else if (accountHandle.isEmpty()) {
                    QMessageBox::information(this, "Name Found",
                        QString("\"%1\" is already in the database. Sorting under that name.")
                            .arg(confirmedName));
                }
            } else {
                // Brand new person — add to database
                bool added;
                if (!accountHandle.isEmpty()) {
                    added = m_db->addEntry(accountHandle, confirmedName, dialogType);
                } else {
                    added = m_db->addEntry(QString(), confirmedName, dialogType);
                }

                if (added) {
                    m_db->save();
                    if (!accountHandle.isEmpty()) {
                        updateGroupsForNewAccount(accountHandle);
                    }
                    m_newAccountsAdded.append(
                        QString("%1 → %2 (%3)")
                            .arg(accountHandle.isEmpty() ? "(no account)" : accountHandle,
                                 confirmedName, DatabaseManager::accountTypeToString(dialogType)));
                    QMessageBox::information(this, "Account Added",
                        QString("Added \"%1\" → \"%2\" (%3) to the database.")
                            .arg(accountHandle.isEmpty() ? "(no account)" : accountHandle,
                                 confirmedName, DatabaseManager::accountTypeToString(dialogType)));
                } else {
                    QMessageBox::warning(this, "Duplicate Account",
                        QString("The account \"%1\" already exists in the database.")
                            .arg(accountHandle));
                    return;
                }
            }

            // Always reached when dialog accepted (name exists or brand new)
            // Refresh completer so the new name appears in the search
            m_sortPanel->refreshCompleter();

            // Update the group so sorting can proceed
            FileGroup& g = m_groups[m_currentGroup];
            g.isKnown = true;
            // Use dialogType (what user selected in dialog), not the original type
            g.accountType = dialogType;

            // For Curator accounts, do NOT set irlName — the account is the photographer,
            // but the name field is for the MODEL in the photos (changes per batch).
            // For IrlOnly, same logic — name field is for the model.
            if (dialogType != AccountType::Curator && dialogType != AccountType::IrlOnly) {
                g.irlName = confirmedName;
            }

            // Update UI — for Curator and IrlOnly, pass empty name so text field is
            // cleared for model input (the confirmed name was the photographer, not the model)
            QString displayName = (dialogType == AccountType::Curator || dialogType == AccountType::IrlOnly)
                                      ? QString()
                                      : confirmedName;
            m_sortPanel->setAccountInfo(account, displayName, true, dialogType);
        } else {
            // User cancelled — clear the text field so handleSortToFolder
            // knows to abort sorting
            m_sortPanel->clearNameInput();
            return;
        }
    }
}

void SortingScreen::handleOpenInstagram(const QString& account) {
    if (!account.isEmpty()) {
        QString url = "https://www.instagram.com/" + account;
        QDesktopServices::openUrl(QUrl(url));
    }
}

void SortingScreen::handleCuratorResolvedName(const QString& irlNameParam) {
    if (m_currentGroup >= m_groups.size()) return;

    FileGroup& group = m_groups[m_currentGroup];
    QString irlName = irlNameParam;

    if (group.accountType == AccountType::IrlOnly) {
        // IrlOnly: the name entered is the MODEL, not the account owner.
        // Don't do DB lookup on the model name — just check if the account
        // handle (photographer) needs to be added to DB.
        bool accountNeedsLinking = !group.accountHandle.isEmpty() && m_db &&
                                   !m_db->hasAccount(group.accountHandle);
        if (accountNeedsLinking) {
            // Show dialog to add the photographer account
            AddPersonDialog dialog(QString(), group.accountHandle, AccountType::IrlOnly, this);
            if (dialog.exec() == QDialog::Accepted) {
                QString account = dialog.accountHandle();
                if (!account.isEmpty() && !m_db->hasAccount(account)) {
                    m_db->addEntry(account, QString(), AccountType::IrlOnly);
                    m_db->save();
                    updateGroupsForNewAccount(account);
                    m_sortPanel->refreshCompleter();
                }
            } else {
                return;  // User cancelled
            }
        }
        // For IrlOnly, the name is per-batch (model name). Don't persist it to group.
        // Just update the UI to reflect the name for this batch.
        m_sortPanel->setAccountInfo(group.accountHandle, irlName, true,
                                    group.accountType);
    } else {
        // Curator accounts: the name entered is the MODEL, not the curator.
        // Check if model exists and add if needed.
        bool nameExists = m_db && m_db->hasIrlName(irlName);
        bool accountNeedsLinking = !group.accountHandle.isEmpty() && m_db &&
                                   !m_db->hasAccount(group.accountHandle);

        if (!nameExists || accountNeedsLinking) {
            // Model doesn't exist or account needs linking
            if (nameExists && accountNeedsLinking) {
                // Model exists but curator account doesn't — show clear confirmation
                int ret = QMessageBox::question(this, "Add Curator Account",
                    QString("The model \"%1\" is in the database.\n\n"
                            "Add the source account \"%2\" as a Curator (photographer)?")
                        .arg(irlName, group.accountHandle),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

                if (ret == QMessageBox::No) {
                    return;  // User cancelled — don't sort
                }
                // User said Yes — add the curator account
                m_db->addEntry(group.accountHandle, QString(), AccountType::Curator);
                m_db->save();
                updateGroupsForNewAccount(group.accountHandle);
                m_sortPanel->refreshCompleter();
            } else {
                // Model doesn't exist — show full Add Person dialog
                AddPersonDialog dialog(irlName, group.accountHandle, AccountType::Personal, this);
                if (dialog.exec() == QDialog::Accepted) {
                    QString confirmedName = dialog.irlName();
                    QString account = dialog.accountHandle();
                    AccountType dialogType = dialog.accountType();

                    if (!confirmedName.isEmpty()) {
                        if (m_db->hasIrlName(confirmedName)) {
                            // Model exists — just link account if needed
                            if (!account.isEmpty() && !m_db->hasAccount(account)) {
                                m_db->addEntry(account, confirmedName, dialogType);
                                m_db->save();
                                updateGroupsForNewAccount(account);
                                m_sortPanel->refreshCompleter();
                            }
                        } else {
                            // New model — add to DB
                            if (!account.isEmpty()) {
                                m_db->addEntry(account, confirmedName, dialogType);
                            } else {
                                m_db->addEntry(QString(), confirmedName, dialogType);
                            }
                            m_db->save();
                            if (!account.isEmpty()) {
                                updateGroupsForNewAccount(account);
                            }
                            m_sortPanel->refreshCompleter();
                        }
                        irlName = confirmedName;
                    }
                } else {
                    return;  // User cancelled — don't sort
                }
            }
        }

        // Don't persist model name to group — each batch can have a different model
        // Clear the name input for next batch
        m_sortPanel->clearNameInput();
    }
}

void SortingScreen::updateHeader() {
    if (m_currentGroup >= m_groups.size()) return;

    int batchSize = ConfigManager::instance()->batchSize();

    // Calculate global sub-batch index and total across ALL groups
    int globalBatchIndex = 0;
    int totalSubBatches = 0;

    for (int i = 0; i < m_groups.size(); ++i) {
        int filesInGroup = m_groups[i].filePaths.size();
        int numSubBatches = (filesInGroup + batchSize - 1) / batchSize;
        totalSubBatches += numSubBatches;

        if (i < m_currentGroup) {
            globalBatchIndex += numSubBatches;
        } else if (i == m_currentGroup) {
            globalBatchIndex += m_currentSubBatch;
        }
    }

    globalBatchIndex += 1;  // 1-based display

    const FileGroup& group = m_groups[m_currentGroup];
    int filesRemainingInCurrentBatch = group.filePaths.size() - (m_currentSubBatch * batchSize);

    // Count total remaining files (current group + all subsequent groups)
    int totalRemaining = 0;
    for (int i = m_currentGroup + 1; i < m_groups.size(); ++i) {
        totalRemaining += m_groups[i].filePaths.size();
    }
    totalRemaining += qMax(0, filesRemainingInCurrentBatch);

    // Calculate total files (sorted + remaining) for progress display
    int totalFiles = m_filesSorted + totalRemaining;

    m_headerLabel->setText(
        QString("Batch %1 of %2  •  %3 / %4 sorted")
            .arg(QLocale().toString(globalBatchIndex))
            .arg(QLocale().toString(totalSubBatches))
            .arg(QLocale().toString(m_filesSorted))
            .arg(QLocale().toString(totalFiles)));
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

void SortingScreen::updateGroupsForNewAccount(const QString& accountHandle) {
    if (!m_db || accountHandle.isEmpty()) return;

    // Look up the account in the database
    if (!m_db->hasAccount(accountHandle)) return;

    QString irlName = m_db->getIrlName(accountHandle);
    AccountType type = m_db->getEntry(accountHandle).type;

    // Update all matching groups
    bool currentGroupUpdated = false;
    for (int i = 0; i < m_groups.size(); ++i) {
        if (m_groups[i].accountHandle == accountHandle) {
            m_groups[i].isKnown = true;
            m_groups[i].accountType = type;

            // For Curator and IrlOnly, do NOT set irlName — the account is the photographer,
            // but the name field is for the model (changes per batch)
            if (type != AccountType::Curator && type != AccountType::IrlOnly) {
                m_groups[i].irlName = irlName;
            }

            if (i == m_currentGroup) {
                currentGroupUpdated = true;
            }
        }
    }

    // Update the current group's UI if it was affected
    if (currentGroupUpdated) {
        // For Curator and IrlOnly, pass empty name so text field is cleared for model input
        QString displayName = (type == AccountType::Curator || type == AccountType::IrlOnly)
                                  ? QString()
                                  : irlName;
        m_sortPanel->setAccountInfo(accountHandle, displayName, true, type);
    }

    // Also update the SorterEngine cache
    if (m_engine) {
        m_engine->updateCacheForAccount(accountHandle);
    }
}
