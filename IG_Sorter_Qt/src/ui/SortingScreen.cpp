#include "ui/SortingScreen.h"
#include "ui/ImagePreviewGrid.h"
#include "ui/SortPanel.h"
#include "core/DatabaseManager.h"
#include "core/SorterEngine.h"
#include "utils/ConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>

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
    m_backButton = new QPushButton("Back", this);
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

    // If unknown, require user to add to DB first
    if (!group.isKnown && irlName.isEmpty()) {
        QMessageBox::information(this, "Unknown Account",
            "Please add this account to the database before sorting.");
        return;
    }

    // For curator accounts, require the user to enter who is in the photos
    if (group.accountType == AccountType::Curator) {
        if (!m_sortPanel->isCuratorNameResolved()) {
            QMessageBox::information(this, "Missing Name",
                "Please enter who is in these photos before sorting.");
            return;
        }
        irlName = m_sortPanel->getCuratorResolvedName();
    }

    QString outputDir = m_outputFolders[folderIndex].path;

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
    }

    // Remove sorted files from grid
    m_previewGrid->removeSelected();

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

void SortingScreen::handleAddUnknownAccount(const QString& account,
                                             const QString& irlName,
                                             AccountType type) {
    if (!m_db) return;

    if (irlName.isEmpty()) {
        QMessageBox::warning(this, "Empty Name",
            "Please enter an IRL name for this account.");
        return;
    }

    bool added = m_db->addEntry(account, irlName, type);
    if (added) {
        m_db->save();
        m_newAccountsAdded.append(
            QString("%1 → %2 (%3)")
                .arg(account, irlName, DatabaseManager::accountTypeToString(type)));

        // Update current group
        if (m_currentGroup < m_groups.size()) {
            m_groups[m_currentGroup].irlName = irlName;
            m_groups[m_currentGroup].isKnown = true;
            m_groups[m_currentGroup].accountType = type;
        }

        // Refresh account display
        m_sortPanel->setAccountInfo(account, irlName, true, type);

        QMessageBox::information(this, "Account Added",
            QString("Added \"%1\" → \"%2\" (%3) to the database.")
                .arg(account, irlName, DatabaseManager::accountTypeToString(type)));
    } else {
        QMessageBox::warning(this, "Duplicate Account",
            QString("The account \"%1\" already exists in the database.").arg(account));
    }
}

void SortingScreen::handleOpenInstagram(const QString& account) {
    if (!account.isEmpty()) {
        QString url = "https://www.instagram.com/" + account;
        QDesktopServices::openUrl(QUrl(url));
    }
}

void SortingScreen::handleCuratorResolvedName(const QString& irlName) {
    // Curator accounts don't need DB entries — just resolve the name for this batch
    if (m_currentGroup < m_groups.size()) {
        m_groups[m_currentGroup].irlName = irlName;
        m_groups[m_currentGroup].isKnown = true;
    }
    m_sortPanel->setAccountInfo(m_groups[m_currentGroup].accountHandle,
                                irlName, true, m_groups[m_currentGroup].accountType);
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
