#include "ui/SortingScreen.h"

#include <algorithm>
#include <QDesktopServices>
#include <QFile>
#include <QFutureWatcher>
#include <QHBoxLayout>
#include <QMutex>
#include <QMutexLocker>
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

    QMutex s_sortMutex;
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
    m_groupingComplete = true;
    m_waitingForGroups = false;
    m_currentGroup = 0;
    m_currentSubBatch = 0;
    m_filesSorted = 0;
    m_filesSkipped = 0;
    m_errors = 0;
    m_errorMessages.clear();
    m_newAccountsAdded.clear();
    m_filesByAccountType.clear();
    m_nameCounts.clear();
    m_nameLastUsed.clear();
    m_nameUseSequence = 0;

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

void SortingScreen::beginStreamingGroups() {
    m_groups.clear();
    m_currentGroup = 0;
    m_currentSubBatch = 0;
    m_groupingComplete = false;
    m_waitingForGroups = true;
    m_filesSorted = 0;
    m_filesSkipped = 0;
    m_errors = 0;
    m_errorMessages.clear();
    m_newAccountsAdded.clear();
    m_filesByAccountType.clear();
    m_nameCounts.clear();
    m_nameLastUsed.clear();
    m_nameUseSequence = 0;
    m_previewGrid->clear();
    m_previewGrid->setEnabled(false);
    m_sortPanel->setEnabled(false);
    m_headerLabel->setText("Finding the first files...");
    m_finishButton->setEnabled(false);
}

void SortingScreen::appendGroups(const QList<FileGroup>& groups) {
    if (groups.isEmpty()) {
        return;
    }
    m_groups.append(groups);
    if (m_waitingForGroups) {
        m_waitingForGroups = false;
        m_sortPanel->setEnabled(true);
        m_previewGrid->setEnabled(true);
        loadNextBatch();
    } else {
        updateHeader();
    }
}

void SortingScreen::finishStreamingGroups() {
    m_groupingComplete = true;
    if (m_groups.isEmpty()) {
        m_waitingForGroups = false;
        m_sortPanel->setEnabled(false);
        m_previewGrid->setEnabled(false);
        m_headerLabel->setText("No files found in source directory.");
        m_finishButton->setEnabled(true);
        return;
    }
    if (m_waitingForGroups || m_currentGroup >= m_groups.size()) {
        m_waitingForGroups = false;
        m_sortPanel->setEnabled(true);
        m_previewGrid->setEnabled(true);
        loadNextBatch();
    } else {
        updateHeader();
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

        // Groups are built once at the start of sorting, so paths for files
        // that have already been moved remain in the group. Undo can return
        // the cursor to one of those batches; only display files that are
        // currently back in the source directory to avoid empty preview tiles.
        QStringList batchFiles;
        const QStringList candidateFiles = group.filePaths.mid(startIdx, endIdx - startIdx);
        batchFiles.reserve(candidateFiles.size());
        for (const QString& filePath : candidateFiles) {
            if (QFile::exists(filePath)) {
                batchFiles.append(filePath);
            }
        }

        if (batchFiles.isEmpty()) {
            ++m_currentSubBatch;
            continue;
        }

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

    if (!m_groupingComplete) {
        m_waitingForGroups = true;
        m_sortPanel->setEnabled(false);
        m_previewGrid->setEnabled(false);
        m_finishButton->setEnabled(false);
        m_headerLabel->setText("All found files sorted. Looking for more...");
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
    // The name field can be edited even when the account already has a
    // database name. Prefer that current input for this sort; fall back to
    // the group's resolved name when the field is blank.
    const QString enteredName = m_sortPanel->getCuratorResolvedName().trimmed();
    QString irlName = enteredName.isEmpty() ? group.irlName : enteredName;

    // If the group has no associated name or name is only whitespace, resolve it
    if (irlName.trimmed().isEmpty()) {
        if (enteredName.isEmpty()) {
            QMessageBox::information(this, "Missing Name",
                "Please enter an IRL name before sorting.");
            return;
        }

        if (!group.isKnown) {
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
            irlName = m_sortPanel->getCuratorResolvedName().trimmed();
            if (irlName.isEmpty()) {
                return;  // User cancelled the Add dialog
            }
        } else {
            // A Personal Instagram account can be nameless in the DB, so link
            // this entered name to its real handle. For Curator and IRL Only
            // sources, the entered name is the model in this batch, not the
            // source account's identity; never persist it as a name-only row.
            irlName = enteredName;
            if (m_db && group.accountType == AccountType::Personal
                && !group.accountHandle.isEmpty()) {
                m_db->addEntry(group.accountHandle, irlName, group.accountType);
                m_db->save();
                updateGroupsForNewAccount(group.accountHandle);
                m_sortPanel->refreshCompleter();
            }
        }
    }

    // For curator and IrlOnly (download source types) accounts, require the user
    // to enter who is in the photos
    if (group.accountType == AccountType::Curator ||
        group.accountType == AccountType::IrlOnly) {
        QString enteredName = m_sortPanel->getCuratorResolvedName().trimmed();
        if (enteredName.isEmpty() || !m_sortPanel->isCuratorNameResolved()) {
            QMessageBox::information(this, "Missing Name",
                "Please enter who is in these photos before sorting.");
            return;
        }
        irlName = enteredName;
    }

    // For IrlOnly accounts: only check if the ACCOUNT needs linking to DB.
    // The name entered is the MODEL's name — don't do DB lookup on it.
    // For Curator accounts: check if model name exists and prompt to link if needed.
    if (group.accountType == AccountType::IrlOnly) {
        // Only an Instagram download's handle is a real account. Android
        // screenshots use labels such as "Instagram" for the source app; that
        // label must never trigger an Add Person prompt.
        bool accountNeedsLinking = group.sourceType == SourceType::Instagram &&
                                   !group.accountHandle.isEmpty() && m_db &&
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

    // Typing a name is not enough to establish who the photos belong to.
    // Require a database identity before any files are moved: a real Instagram
    // account can identify a Personal batch, while Curator/IRL Only batches
    // need the person in the photos to be present by name in the database.
    const bool hasDatabaseAccount = m_db &&
        group.sourceType == SourceType::Instagram &&
        !group.accountHandle.trimmed().isEmpty() &&
        m_db->hasAccount(group.accountHandle);
    const bool hasDatabaseName = m_db && m_db->hasIrlName(irlName);
    const bool hasDatabaseIdentity = group.accountType == AccountType::Personal
        ? (hasDatabaseAccount || hasDatabaseName)
        : hasDatabaseName;
    if (!hasDatabaseIdentity) {
        QMessageBox::information(this, "Person Not In Database",
            "Add this account or an IRL name only entry to the database before sorting.");
        return;
    }

    QString outputDir = m_outputFolders[folderIndex].path;

    if (m_engine) {
        // Do not allow Undo to inspect the history while this sort task is
        // mutating it on the worker thread.
        ++m_pendingSortTasks;
        m_undoButton->setEnabled(false);

        // Show the selected name immediately instead of waiting for the
        // background file operations to finish. If any files fail to sort,
        // the completion handler corrects this provisional photo count.
        const bool hasQuickFillName = !irlName.trimmed().isEmpty();
        const quint64 previousNameUse = hasQuickFillName
            ? m_nameLastUsed.value(irlName, 0) : 0;
        quint64 provisionalNameUse = 0;
        if (hasQuickFillName) {
            m_nameCounts[irlName] += selected.size();
            provisionalNameUse = ++m_nameUseSequence;
            m_nameLastUsed[irlName] = provisionalNameUse;
            updateFavoriteButtons();
        }

        // Release image handles from thumbnails before moving files
        // This frees any file handles held by Qt's image readers on Windows
        m_previewGrid->releaseImages(selected);

        // Optimistically update the UI immediately for instant responsiveness
        m_previewGrid->removeSelected();
        m_filesSorted += selected.size();

        // Capture necessary variables safely by value
        const QString accountHandle = group.accountHandle;
        const AccountType accountType = group.accountType;
        const QString folderName = m_outputFolders[folderIndex].name;
        const int capturedGroupIndex = m_currentGroup;
        auto* enginePtr = m_engine;

        // Advance to next batch immediately if this one is exhausted
        if (!m_previewGrid->hasImages()) {
            m_currentSubBatch++;
            loadNextBatch();
        } else {
            updateHeader();
        }

        auto* watcher = new QFutureWatcher<SortTaskResult>(this);
        connect(watcher, &QFutureWatcher<SortTaskResult>::finished, this,
                [this, selected, accountHandle, accountType, capturedGroupIndex, irlName,
                 previousNameUse, provisionalNameUse, watcher]() {
            m_pendingSortTasks = qMax(0, m_pendingSortTasks - 1);
            SortTaskResult taskResult = watcher->result();
            SortResult result = taskResult.result;

            // Adjust the optimistic photo count for files that did not sort.
            int expectedSorted = selected.size();
            int actualSorted = result.filesSorted + result.duplicatesRemoved;
            if (actualSorted < expectedSorted) {
                m_filesSorted -= (expectedSorted - actualSorted);
                updateHeader();
            }

            if (actualSorted < expectedSorted && !irlName.trimmed().isEmpty()) {
                auto countIt = m_nameCounts.find(irlName);
                if (countIt != m_nameCounts.end()) {
                    countIt.value() -= expectedSorted - actualSorted;
                    if (countIt.value() <= 0) {
                        m_nameCounts.erase(countIt);
                        m_nameLastUsed.remove(irlName);
                    }
                }
            }

            m_errors += result.errors;
            m_errorMessages.append(result.errorMessages);

            // Restore the previous tie-break position if the whole sort failed.
            if (actualSorted == 0 && !irlName.trimmed().isEmpty()) {
                if (m_nameLastUsed.value(irlName, 0) == provisionalNameUse) {
                    if (m_nameCounts.contains(irlName)) {
                        m_nameLastUsed[irlName] = previousNameUse;
                    } else {
                        m_nameLastUsed.remove(irlName);
                    }
                }
                updateFavoriteButtons();
            } else if (actualSorted < expectedSorted) {
                updateFavoriteButtons();
            }

            // Track by account type
            QString typeKey = DatabaseManager::accountTypeToString(accountType);
            m_filesByAccountType[typeKey] += result.filesSorted;

            // Save the resolved name to the current group so it persists for any remaining sub-batches
            if (accountType == AccountType::Curator ||
                accountType == AccountType::IrlOnly ||
                accountHandle == "Unknown") {

                if (capturedGroupIndex >= 0 && capturedGroupIndex < m_groups.size()) {
                    m_groups[capturedGroupIndex].irlName = irlName;
                    m_groups[capturedGroupIndex].isKnown = true;
                }

                // Update UI only if we are still on the same group
                if (m_currentGroup == capturedGroupIndex) {
                    m_sortPanel->setAccountInfo(accountHandle, irlName, true, accountType);
                }
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

            updateUndoButton();
            watcher->deleteLater();
        });

        QFuture<SortTaskResult> future = QtConcurrent::run([selected, accountHandle, irlName, accountType, outputDir, folderName, enginePtr]() {
            QMutexLocker locker(&s_sortMutex);
            SortTaskResult res;
            res.result = enginePtr->sortFiles(selected, irlName, outputDir, folderName);

            if (res.result.filesSorted + res.result.duplicatesRemoved < selected.size()) {
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
    connect(watcher, &QFutureWatcher<DeleteResult>::finished, this, [this, watcher]() {
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
            if (QFile::moveToTrash(path)) {
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
    m_undoButton->setEnabled(false);
    m_headerLabel->setText("Undoing last sort... Please wait.");
    auto* enginePtr = m_engine;

    auto* watcher = new QFutureWatcher<QStringList>(this);
    connect(watcher, &QFutureWatcher<QStringList>::finished, this, [this, watcher]() {
        QStringList restoredPaths = watcher->result();
        
        if (!restoredPaths.isEmpty()) {
            m_filesSorted = qMax(0, m_filesSorted - restoredPaths.size());

            // Put the UI back on the batch containing the files that were
            // restored. The current cursor may already have advanced since
            // the sort completed, so blindly loading the next batch can hide
            // the files Undo just returned to the source directory.
            const int batchSize = qMax(1, ConfigManager::instance()->batchSize());
            bool foundRestoredBatch = false;
            for (const QString& restoredPath : std::as_const(restoredPaths)) {
                for (int groupIndex = 0; groupIndex < m_groups.size(); ++groupIndex) {
                    const int fileIndex = m_groups[groupIndex].filePaths.indexOf(restoredPath);
                    if (fileIndex >= 0) {
                        m_currentGroup = groupIndex;
                        m_currentSubBatch = fileIndex / batchSize;
                        foundRestoredBatch = true;
                        break;
                    }
                }
                if (foundRestoredBatch) break;
            }

            if (foundRestoredBatch) {
                loadNextBatch();
            } else {
                m_previewGrid->clear();
                loadNextBatch();
            }
            updateUndoButton();

            int count = restoredPaths.size();
            QString msg = QString("Restored %1 file%2 to the source directory.\nYou can now re-sort these files.").arg(count).arg(count > 1 ? "s" : "");
            QMessageBox::information(this, "Undo Successful", msg);
        } else {
            QMessageBox::warning(this, "Undo Failed", "Failed to restore files. They may have been deleted or moved elsewhere.");
        }

        m_sortPanel->setEnabled(true);
        m_previewGrid->setEnabled(true);
        updateUndoButton();
        if (m_previewGrid->hasImages()) { updateHeader(); }
        watcher->deleteLater();
    });

    QFuture<QStringList> future = QtConcurrent::run([enginePtr]() {
        QMutexLocker locker(&s_sortMutex);
        return enginePtr->undoLastSort();
    });
    watcher->setFuture(future);
}

void SortingScreen::updateUndoButton() {
    m_undoButton->setEnabled(m_pendingSortTasks == 0 && m_engine && m_engine->canUndo());
}

void SortingScreen::handleAddUnknownAccount(const QString& account,
                                             const QString& irlName,
                                             AccountType type) {
    if (!m_db) return;

    if (irlName.trimmed().isEmpty()) {
        QMessageBox::information(this, "Empty Name",
            "Please enter an IRL name in the text entry field before adding a person.");
        return;
    }

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
                        if (dialogType == AccountType::IrlOnly) {
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

            // A newly added Curator account is the source of these photos, so its
            // database name must not become the person name for this batch.
            // IrlOnly sources can still use the typed name for this batch.
            if (dialogType == AccountType::Curator) {
                g.irlName = QString();
            } else if (dialogType == AccountType::IrlOnly) {
                g.irlName = m_sortPanel->getCuratorResolvedName();
            } else {
                g.irlName = confirmedName;
            }

            // setAccountInfo clears the field when a Curator has no batch name.
            QString displayName = dialogType == AccountType::Curator
                                      ? QString()
                                      : dialogType == AccountType::IrlOnly
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

void SortingScreen::handleCuratorResolvedName(const QString& irlNameParam,
                                               AccountType accountType) {
    if (m_currentGroup < 0 || m_currentGroup >= m_groups.size()) return;

    FileGroup& group = m_groups[m_currentGroup];
    QString irlName = irlNameParam.trimmed();

    if (irlName.isEmpty()) {
        QMessageBox::information(this, "Empty Name",
            "Please enter who is in these photos in the text entry field.");
        return;
    }

    if (group.accountType == AccountType::IrlOnly) {
        if (!m_db) {
            QMessageBox::warning(this, "Database Unavailable",
                "Load an account database before adding a person.");
            return;
        }

        // IrlOnly: the name entered is the MODEL, not the account owner.
        // Only link a real Instagram download handle. Screenshot source labels
        // (for example "Instagram") are not user accounts.
        bool accountNeedsLinking = group.sourceType == SourceType::Instagram &&
                                   !group.accountHandle.isEmpty() && m_db &&
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

        // The batch subject must have a database identity of its own. Add a
        // name-only row when the user confirms a new subject with the Enter
        // button; typing into the field alone never creates a database entry.
        if (!m_db->hasIrlName(irlName)) {
            if (!m_db->addEntry(QString(), irlName, AccountType::IrlOnly)) {
                QMessageBox::warning(this, "Add Failed",
                    QString("Could not add \"%1\" to the database.").arg(irlName));
                return;
            }
            if (!m_db->save()) {
                QMessageBox::warning(this, "Save Failed",
                    "Could not save the new IRL name to the database.");
                return;
            }
            m_sortPanel->refreshCompleter();
            m_newAccountsAdded.append(QString("(no account) → %1 (irl_only)").arg(irlName));
        }

        // For IrlOnly, the name is per-batch (model name). Don't persist it to group.
        // Just update the UI to reflect the name for this batch.
        m_sortPanel->setAccountInfo(group.accountHandle, irlName, true,
                                    group.accountType);
    } else {
        if (!m_db) {
            QMessageBox::warning(this, "Database Unavailable",
                "Load an account database before adding a person.");
            return;
        }

        // The source account is the curator; the entered name is the model.
        const bool nameExists = m_db->hasIrlName(irlName);
        const bool accountNeedsLinking = !group.accountHandle.isEmpty() &&
                                         !m_db->hasAccount(group.accountHandle);
        if (accountNeedsLinking) {
            const int answer = QMessageBox::question(
                this, "Add Curator Account",
                QString("Add the source account \"%1\" as a Curator (photographer)?")
                    .arg(group.accountHandle),
                QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
            if (answer != QMessageBox::Yes) {
                return;
            }
            if (!m_db->addEntry(group.accountHandle, QString(), AccountType::Curator)) {
                QMessageBox::warning(this, "Duplicate Account",
                    QString("The source account \"%1\" is already in the database.")
                        .arg(group.accountHandle));
                return;
            }
            m_db->save();
            updateGroupsForNewAccount(group.accountHandle);
            m_sortPanel->refreshCompleter();
        }

        if (nameExists) {
            QMessageBox::information(this, "Person Found",
                QString("\"%1\" is already in the database.").arg(irlName));
        } else {
            // Do not prefill the curator's handle as if it belonged to the model.
            AddPersonDialog dialog(irlName, QString(), accountType, this);
            if (dialog.exec() != QDialog::Accepted) {
                return;
            }

            const QString confirmedName = dialog.irlName();
            const QString account = dialog.accountHandle();
            const AccountType dialogType = dialog.accountType();
            if (confirmedName.isEmpty()) {
                QMessageBox::warning(this, "Empty Name",
                    "Please enter a name for this person.");
                return;
            }
            if (!account.isEmpty() && m_db->hasAccount(account)) {
                QMessageBox::warning(this, "Duplicate Account",
                    QString("The account \"%1\" is already in the database.").arg(account));
                return;
            }

            if (m_db->hasIrlName(confirmedName) && account.isEmpty()) {
                QMessageBox::information(this, "Person Found",
                    QString("\"%1\" is already in the database.").arg(confirmedName));
            } else {
                if (!m_db->addEntry(account, confirmedName, dialogType)) {
                    QMessageBox::warning(this, "Add Failed",
                        QString("Could not add \"%1\" to the database.")
                            .arg(confirmedName));
                    return;
                }
                m_db->save();
                if (!account.isEmpty()) {
                    updateGroupsForNewAccount(account);
                }
                m_sortPanel->refreshCompleter();
                m_newAccountsAdded.append(
                    QString("%1 → %2 (%3)")
                        .arg(account.isEmpty() ? "(no account)" : account,
                             confirmedName, DatabaseManager::accountTypeToString(dialogType)));
            }
            irlName = confirmedName;
        }

        // Remember the model for this group without changing its curator source type.
        m_groups[m_currentGroup].irlName = irlName;
        m_sortPanel->setAccountInfo(group.accountHandle, irlName, true,
                                    group.accountType);
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

void SortingScreen::updateFavoriteButtons() {
    // Build sorted list of all names by count
    struct NameCount {
        QString name;
        int count;
        quint64 lastUsed;
    };
    QList<NameCount> sorted;
    sorted.reserve(m_nameCounts.size());
    for (const auto& [name, count] : std::as_const(m_nameCounts).asKeyValueRange()) {
        sorted.append({name, count, m_nameLastUsed.value(name, 0)});
    }

    std::sort(sorted.begin(), sorted.end(),
                      [](const NameCount& a, const NameCount& b) {
                          if (a.count != b.count) {
                              return a.count > b.count;
                          }
                          return a.lastUsed < b.lastUsed;
                      });

    QStringList topNames;
    topNames.reserve(sorted.size());
    for (int i = 0; i < sorted.size(); ++i) {
        topNames.append(std::move(sorted[i].name));
    }

    m_sortPanel->setQuickFillNames(topNames);
}

void SortingScreen::updateGroupsForNewAccount(const QString& accountHandle) {
    if (!m_db || accountHandle.isEmpty()) return;

    const PersonEntry entry = m_db->getEntry(accountHandle);
    if (entry.account.isEmpty()) return;
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
