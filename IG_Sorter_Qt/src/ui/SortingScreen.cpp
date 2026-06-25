#include "ui/SortingScreen.h"

#include <algorithm>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QFile>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>
#include <utility>
#include <QtConcurrent>

#include "core/DatabaseManager.h"
#include "core/SorterEngine.h"
#include "ui/AddPersonDialog.h"
#include "ui/ImagePreviewGrid.h"
#include "ui/SortPanel.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"

namespace {
    struct DeleteResult {
        int deletedCount = 0;
        int failedCount = 0;
        QStringList failedPaths;
    };

    struct SortTaskResult {
        SortResult result;
        QStringList missingPaths;
    };
}

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
    LogManager::instance()->info(QString("SortingScreen::setGroups called with %1 groups").arg(groups.size()));
    m_groups = groups;
    m_currentGroup = 0;
    m_currentSubBatch = 0;
    m_filesSorted = 0;
    m_filesSkipped = 0;
    m_errors = 0;
    m_errorMessages.clear();
    m_newAccountsAdded.clear();
    m_filesByAccountType.clear();
    m_currentSourceType.clear();
    m_nameCounts.clear();

    if (!m_groups.isEmpty()) {
        while (m_currentGroup < m_groups.size() && m_groups[m_currentGroup].filePaths.isEmpty()) {
            LogManager::instance()->info(QString("SortingScreen::setGroups skipping empty group %1").arg(m_currentGroup));
            ++m_currentGroup;
        }

        if (m_currentGroup < m_groups.size()) {
            loadNextBatch();
            return;
        }
    }

    m_headerLabel->setText("No files found in source directory.");
    m_finishButton->setEnabled(true);
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
    LogManager::instance()->info(QString("SortingScreen::loadNextBatch start: currentGroup=%1, currentSubBatch=%2, groupCount=%3")
        .arg(m_currentGroup).arg(m_currentSubBatch).arg(m_groups.size()));

    if (m_currentGroup < 0) {
        LogManager::instance()->warning(QString("SortingScreen::loadNextBatch corrected negative currentGroup %1 to 0").arg(m_currentGroup));
        m_currentGroup = 0;
    }
    if (m_currentSubBatch < 0) {
        LogManager::instance()->warning(QString("SortingScreen::loadNextBatch corrected negative currentSubBatch %1 to 0").arg(m_currentSubBatch));
        m_currentSubBatch = 0;
    }

    const int batchSize = qMax(1, ConfigManager::instance()->batchSize());

    while (m_currentGroup < m_groups.size()) {
        const FileGroup& group = std::as_const(m_groups)[m_currentGroup];
        int groupSize = group.filePaths.size();
        LogManager::instance()->info(QString("SortingScreen::loadNextBatch evaluating group %1 with %2 file(s)")
            .arg(m_currentGroup).arg(groupSize));

        int startIdx = m_currentSubBatch * batchSize;
        if (startIdx >= groupSize) {
            LogManager::instance()->info(QString("SortingScreen::loadNextBatch group %1 exhausted, advancing to next group").arg(m_currentGroup));
            ++m_currentGroup;
            m_currentSubBatch = 0;
            continue;
        }

        int endIdx = qMin(startIdx + batchSize, groupSize);
        LogManager::instance()->info(QString("SortingScreen::loadNextBatch loading batch %1..%2 for group %3")
            .arg(startIdx).arg(endIdx - 1).arg(m_currentGroup));

        QStringList batchFiles = group.filePaths.mid(startIdx, endIdx - startIdx);

        m_previewGrid->setImages(batchFiles);
        m_sortPanel->setAccountInfo(group.accountHandle, group.irlName,
                                    group.isKnown, group.accountType);
        // Only clear name input for unknown accounts — known accounts should have name pre-filled
        if (!group.isKnown || group.irlName.isEmpty()) {
            m_sortPanel->clearNameInput();
        }

        updateHeader();
        return;
    }

    m_finishButton->setEnabled(true);
    m_headerLabel->setText("All batches complete!");
    m_previewGrid->clear();
    m_sortPanel->clearSelections();
    emit allBatchesDone();
}

void SortingScreen::handleSortToFolder(int folderIndex) {
    if (m_currentGroup >= m_groups.size()) {
        return;
    }

    QStringList selected = m_previewGrid->selectedFilePaths();
    if (selected.isEmpty()) {
        QMessageBox::information(this, "No Selection",
            "Please select one or more images before sorting.");
        return;
    }

    if (folderIndex < 0 || folderIndex >= m_outputFolders.size()) {
        return;
    }

    const FileGroup& group = std::as_const(m_groups)[m_currentGroup];
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

    QString outputDir = m_outputFolders[folderIndex].path;

    if (m_engine) {
        // Disable UI first to prevent double-clicks and re-entrancy during event pumping
        m_sortPanel->setEnabled(false);
        m_previewGrid->setEnabled(false);
        m_headerLabel->setText("Sorting file(s)... Please wait.");

        // Release image handles from thumbnails before moving files
        // This frees any file handles held by Qt's image readers on Windows
        m_previewGrid->releaseImages(selected);
        QCoreApplication::processEvents();

        // Capture necessary variables safely by value
        const QString accountHandle = group.accountHandle;
        const AccountType accountType = group.accountType;
        const QString folderName = m_outputFolders[folderIndex].name;
        const int capturedGroupIndex = m_currentGroup;
        auto* enginePtr = m_engine;

        auto* watcher = new QFutureWatcher<SortTaskResult>(this);
        connect(watcher, &QFutureWatcher<SortTaskResult>::finished, this, [=]() {
            SortTaskResult taskResult = watcher->result();
            SortResult result = taskResult.result;

            m_filesSorted += result.filesSorted;
            m_errors += result.errors;
            m_errorMessages.append(result.errorMessages);

            // Track by account type
            QString typeKey = DatabaseManager::accountTypeToString(accountType);
            m_filesByAccountType[typeKey] += result.filesSorted;

            // Optimize: Bulk remove thumbnails from the UI in a single layout pass if all succeeded.
            // This eliminates O(N^2) lag caused by individual removePath layout recalculations.
            if (result.filesSorted == selected.size()) {
                m_previewGrid->removeSelected();
            } else {
                m_previewGrid->setUpdatesEnabled(false);
                for (const QString& path : std::as_const(taskResult.missingPaths)) {
                    m_previewGrid->removePath(path);
                }
                m_previewGrid->setUpdatesEnabled(true);
            }

            // Save the resolved name to the current group so it persists for any remaining sub-batches
            if (accountType == AccountType::Curator ||
                accountType == AccountType::IrlOnly ||
                accountHandle == "Unknown") {

                m_nameCounts[irlName]++;
                if (capturedGroupIndex >= 0 && capturedGroupIndex < m_groups.size()) {
                    m_groups[capturedGroupIndex].irlName = irlName;
                    m_groups[capturedGroupIndex].isKnown = true;
                }

                // Update UI immediately in case the sub-batch isn't finished yet
                m_sortPanel->setAccountInfo(accountHandle, irlName, true, accountType);
                updateFavoriteButtons();
            }

            // Show error if any files failed to sort
            if (result.errors > 0) {
                QString msg;
                if (result.filesSorted > 0) {
                    msg = QString("%1 file%2 sorted successfully.\n%3 file%4 failed to sort:")
                              .arg(result.filesSorted).arg(result.filesSorted > 1 ? "s" : "")
                              .arg(result.errors).arg(result.errors > 1 ? "s" : "");
                    QStringList errList;
                    errList.reserve(4);
                    int shown = 0;
                    for (const QString& err : std::as_const(result.errorMessages)) {
                        if (shown >= 3) { errList.append(QString("... and %1 more.").arg(result.errorMessages.size() - shown)); break; }
                        errList.append("• " + err); shown++;
                    }
                    msg += "\n" + errList.join("\n");
                } else {
                    msg = QString("Failed to sort %1 file%2:").arg(result.errors).arg(result.errors > 1 ? "s" : "");
                    QStringList errList;
                    errList.reserve(6);
                    int shown = 0;
                    for (const QString& err : std::as_const(result.errorMessages)) {
                        if (shown >= 5) { errList.append(QString("... and %1 more.").arg(result.errorMessages.size() - shown)); break; }
                        errList.append("• " + err); shown++;
                    }
                    msg += "\n" + errList.join("\n");
                }
                QMessageBox::warning(this, "Sort Error", msg);
            }

            // Restore UI Interaction
            m_sortPanel->setEnabled(true);
            m_previewGrid->setEnabled(true);

            // If grid is empty, advance to next sub-batch
            if (!m_previewGrid->hasImages()) {
                m_currentSubBatch++;
                loadNextBatch();
            } else {
                updateHeader();
            }
            updateUndoButton();
            watcher->deleteLater();
        });

        QFuture<SortTaskResult> future = QtConcurrent::run([selected, accountHandle, irlName, accountType, outputDir, folderName, enginePtr]() {
            SortTaskResult res;
            res.result = enginePtr->sortFiles(selected, accountHandle, irlName, accountType, outputDir, folderName);
            
            if (res.result.filesSorted < selected.size()) {
                for (const QString& path : std::as_const(selected)) {
                    if (!QFile::exists(path)) res.missingPaths.append(path);
                }
            }
            return res;
        });
        watcher->setFuture(future);
    }
}

void SortingScreen::handleSkip() {
    // Skip means "don't sort these, move to next batch"
    int visibleCount = 0;
    if (m_currentGroup >= 0 && m_currentGroup < m_groups.size()) {
        const FileGroup& group = std::as_const(m_groups)[m_currentGroup];
        int batchSize = ConfigManager::instance()->batchSize();
        if (batchSize <= 0) batchSize = 5;
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

void SortingScreen::handleSelectionChanged(int /*count*/) {
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

    m_sortPanel->setEnabled(false);
    m_previewGrid->setEnabled(false);
    m_headerLabel->setText("Moving file(s) to Recycle Bin... Please wait.");

    auto* watcher = new QFutureWatcher<DeleteResult>(this);
    connect(watcher, &QFutureWatcher<DeleteResult>::finished, this, [=]() {
        DeleteResult result = watcher->result();

        // Remove deleted thumbnails from grid
        m_previewGrid->removeSelected();

        // Report errors if any
        if (result.failedCount > 0) {
            for (const QString& path : result.failedPaths) {
                m_errorMessages.append("Failed to delete: " + path);
            }
            m_errors += result.failedCount;
            QString msg = QString("%1 file%2 moved to Recycle Bin.\n%3 failed.")
                      .arg(result.deletedCount).arg(result.deletedCount > 1 ? "s" : "").arg(result.failedCount);
            QMessageBox::warning(this, "Partial Delete", msg);
        }

        m_sortPanel->setEnabled(true);
        m_previewGrid->setEnabled(true);

        // If grid is empty, advance to next sub-batch
        if (!m_previewGrid->hasImages()) {
            m_currentSubBatch++;
            loadNextBatch();
        } else {
            updateHeader();
        }
        watcher->deleteLater();
    });

    QFuture<DeleteResult> future = QtConcurrent::run([selected]() {
        DeleteResult res;
        res.failedPaths.reserve(selected.size());
        for (const QString& path : std::as_const(selected)) {
            QFile file(path);
            if (file.moveToTrash()) {
                res.deletedCount++;
            } else {
                res.failedCount++;
                res.failedPaths.append(path);
            }
        }
        return res;
    });
    watcher->setFuture(future);
}

void SortingScreen::handleUndo() {
    if (!m_engine || !m_engine->canUndo()) return;

    m_sortPanel->setEnabled(false);
    m_previewGrid->setEnabled(false);
    m_headerLabel->setText("Undoing last sort... Please wait.");
    auto* enginePtr = m_engine;

    auto* watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcher<QStringList>::finished, this, [=]() {
        QStringList restoredPaths = watcher->result();
        
        if (!restoredPaths.isEmpty()) {
            m_previewGrid->clear();
            loadNextBatch();
            updateUndoButton();

            int count = restoredPaths.size();
            QString msg = QString("Restored %1 file%2 to the source directory.\nYou can now re-sort these files.").arg(count).arg(count > 1 ? "s" : "");
            QMessageBox::information(this, "Undo Successful", msg);
        } else {
            QMessageBox::warning(this, "Undo Failed", "Failed to restore files. They may have been deleted or moved elsewhere.");
        }

        m_sortPanel->setEnabled(true);
        m_previewGrid->setEnabled(true);
        if (m_previewGrid->hasImages()) { updateHeader(); }
        watcher->deleteLater();
    });

    QFuture<QStringList> future = QtConcurrent::run([enginePtr]() { return enginePtr->undoLastSort(); });
    watcher->setFuture(future);
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

        // For Curator and IrlOnly, do NOT overwrite irlName with the photographer's name.
        // Instead, preserve whatever model name was already typed in the text field.
        if (type != AccountType::Curator && type != AccountType::IrlOnly) {
            g.irlName = confirmedName;
        } else {
            g.irlName = m_sortPanel->getCuratorResolvedName();
        }

        // Update UI — for Curator/IrlOnly, pass the preserved model name so text field isn't cleared
        QString displayName = (type == AccountType::Curator || type == AccountType::IrlOnly)
                                  ? g.irlName
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
                        if (dialogType == AccountType::Curator || dialogType == AccountType::IrlOnly) {
                            g.irlName = m_sortPanel->getCuratorResolvedName();
                        } else {
                            g.irlName = QString();
                        }
                        g.isKnown = true;
                        g.accountType = dialogType;
                        m_sortPanel->setAccountInfo(account, g.irlName, true, dialogType);
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

            // For Curator accounts, do NOT overwrite irlName with the photographer's name.
            // Instead, preserve whatever model name was already typed in the text field.
            if (dialogType != AccountType::Curator && dialogType != AccountType::IrlOnly) {
                g.irlName = confirmedName;
            } else {
                g.irlName = m_sortPanel->getCuratorResolvedName();
            }

            // Update UI — for Curator and IrlOnly, pass the preserved model name so text field isn't cleared
            QString displayName = (dialogType == AccountType::Curator || dialogType == AccountType::IrlOnly)
                                      ? g.irlName
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
        QDesktopServices::openUrl(QUrl(QStringLiteral("https://www.instagram.com/") + account));
    }
}

void SortingScreen::handleCuratorResolvedName(const QString& irlNameParam) {
    if (m_currentGroup < 0 || m_currentGroup >= m_groups.size()) return;

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

        // Persist model name to group so it is remembered for current and subsequent sub-batches
        m_groups[m_currentGroup].irlName = irlName;
        m_sortPanel->setAccountInfo(group.accountHandle, irlName, true, group.accountType);
    }
}

void SortingScreen::updateHeader() {
    if (m_currentGroup < 0 || m_currentGroup >= m_groups.size()) return;

    int batchSize = ConfigManager::instance()->batchSize();
    if (batchSize <= 0) batchSize = 5;

    // Calculate global sub-batch index and total across ALL groups
    int globalBatchIndex = 0;
    int totalSubBatches = 0;
    int totalRemaining = 0;

    int i = 0;
    for (const auto& group : std::as_const(m_groups)) {
        const int filesInGroup = group.filePaths.size();
        const int numSubBatches = (filesInGroup + batchSize - 1) / batchSize;
        totalSubBatches += numSubBatches;

        if (i < m_currentGroup) {
            globalBatchIndex += numSubBatches;
        } else if (i == m_currentGroup) {
            globalBatchIndex += m_currentSubBatch;
            totalRemaining += qMax(0, filesInGroup - (m_currentSubBatch * batchSize));
        } else {
            totalRemaining += filesInGroup;
        }
        ++i;
    }

    globalBatchIndex += 1;  // 1-based display

    // Calculate total files (sorted + remaining) for progress display
    int totalFiles = m_filesSorted + totalRemaining;

    QLocale locale;
    m_headerLabel->setText(
        QString("Batch %1 of %2  •  %3 / %4 sorted")
            .arg(locale.toString(globalBatchIndex))
            .arg(locale.toString(totalSubBatches))
            .arg(locale.toString(m_filesSorted))
            .arg(locale.toString(totalFiles)));
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
    sorted.reserve(m_nameCounts.size());
    for (const auto& [name, count] : std::as_const(m_nameCounts).asKeyValueRange()) {
        sorted.append({name, count});
    }

    constexpr int maxFavorites = 15;
    const int numToSort = qMin(static_cast<int>(sorted.size()), maxFavorites);
    std::partial_sort(sorted.begin(), sorted.begin() + numToSort, sorted.end(),
                      [](const NameCount& a, const NameCount& b) {
                          return a.count > b.count;
                      });

    QStringList topNames;
    topNames.reserve(numToSort);
    for (int i = 0; i < numToSort; ++i) {
        topNames.append(std::move(sorted[i].name));
    }

    m_sortPanel->setQuickFillNames(topNames);
}

QString SortingScreen::getCurrentSourceType() const {
    if (m_currentGroup >= 0 && m_currentGroup < m_groups.size()) {
        const FileGroup& group = std::as_const(m_groups)[m_currentGroup];
        return group.accountHandle;
    }
    return QString();
}

void SortingScreen::updateGroupsForNewAccount(const QString& accountHandle) {
    if (!m_db || accountHandle.isEmpty()) return;

    // Look up the account in the database
    if (!m_db->hasAccount(accountHandle)) return;

    PersonEntry entry = m_db->getEntry(accountHandle.toLower());
    QString irlName = entry.irlName;
    AccountType type = entry.type;

    // Update all matching groups
    bool currentGroupUpdated = false;
    int index = 0;
    for (auto& group : m_groups) {
        if (group.accountHandle == accountHandle) {
            group.isKnown = true;
            group.accountType = type;

            // For Curator and IrlOnly, preserve the existing model name for the active group.
            if (type != AccountType::Curator && type != AccountType::IrlOnly) {
                group.irlName = irlName;
            } else if (index == m_currentGroup) {
                group.irlName = m_sortPanel->getCuratorResolvedName();
            }

            if (index == m_currentGroup) {
                currentGroupUpdated = true;
            }
        }
        ++index;
    }

    // Update the current group's UI if it was affected
    if (currentGroupUpdated && m_currentGroup < m_groups.size()) {
        // For Curator and IrlOnly, pass current model name so text field isn't cleared
        QString displayName = (type == AccountType::Curator || type == AccountType::IrlOnly)
                                  ? m_groups[m_currentGroup].irlName
                                  : irlName;
        m_sortPanel->setAccountInfo(accountHandle, displayName, true, type);
    }

    // Also update the SorterEngine cache
    if (m_engine) {
        m_engine->updateCacheForAccount(accountHandle);
    }
}
