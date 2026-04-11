#include "ui/DuplicateFinderScreen.h"
#include "ui/ImagePreviewGrid.h"
#include "ui/ImageThumbnail.h"
#include "core/DuplicateFinder.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QMessageBox>
#include <QDateTime>
#include <QtConcurrent>
#include <QFutureWatcher>

// ─── DuplicateFinderScreen ──────────────────────────────────────────────────

DuplicateFinderScreen::DuplicateFinderScreen(QWidget* parent)
    : QWidget(parent), m_currentGroup(-1), m_totalDeleted(0), m_totalSkipped(0)
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(8);
    m_mainLayout->setContentsMargins(20, 20, 20, 20);

    // Header
    m_headerLabel = new QLabel("Find Duplicate Files", this);
    m_headerLabel->setAlignment(Qt::AlignCenter);
    QFont headerFont = m_headerLabel->font();
    headerFont.setPointSize(24);
    headerFont.setBold(true);
    m_headerLabel->setFont(headerFont);
    m_mainLayout->addWidget(m_headerLabel);

    // Progress bar (hidden until scanning)
    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_mainLayout->addWidget(m_progressBar);

    // Group summary
    m_groupSummaryLabel = new QLabel(this);
    m_groupSummaryLabel->setAlignment(Qt::AlignCenter);
    QFont summaryFont = m_groupSummaryLabel->font();
    summaryFont.setPointSize(14);
    summaryFont.setBold(true);
    m_groupSummaryLabel->setFont(summaryFont);
    m_groupSummaryLabel->setVisible(false);
    m_mainLayout->addWidget(m_groupSummaryLabel);

    // Image preview grid
    m_previewGrid = new ImagePreviewGrid(this);
    m_previewGrid->setVisible(false);
    m_mainLayout->addWidget(m_previewGrid, 1);

    // Keeping / Deleting labels — side by side
    auto* infoLayout = new QHBoxLayout();
    infoLayout->setSpacing(20);

    m_keepingLabel = new QLabel(this);
    m_keepingLabel->setAlignment(Qt::AlignCenter);
    m_keepingLabel->setWordWrap(true);
    QFont keepFont = m_keepingLabel->font();
    keepFont.setPointSize(13);
    keepFont.setBold(true);
    m_keepingLabel->setFont(keepFont);
    m_keepingLabel->setStyleSheet("color: #2e7d32;");
    m_keepingLabel->setVisible(false);
    infoLayout->addWidget(m_keepingLabel, 1);

    m_deletingLabel = new QLabel(this);
    m_deletingLabel->setAlignment(Qt::AlignCenter);
    m_deletingLabel->setWordWrap(true);
    QFont deleteFont = m_deletingLabel->font();
    deleteFont.setPointSize(13);
    deleteFont.setBold(true);
    m_deletingLabel->setFont(deleteFont);
    m_deletingLabel->setStyleSheet("color: #c62828;");
    m_deletingLabel->setVisible(false);
    infoLayout->addWidget(m_deletingLabel, 1);

    m_mainLayout->addLayout(infoLayout);

    // Status label
    m_statusLabel = new QLabel(
        "Click Scan to search output folders for duplicate files.", this);
    m_statusLabel->setAlignment(Qt::AlignCenter);
    m_statusLabel->setWordWrap(true);
    QFont statusFont = m_statusLabel->font();
    statusFont.setPointSize(12);
    m_statusLabel->setFont(statusFont);
    m_mainLayout->addWidget(m_statusLabel);

    // Scan button
    m_scanButton = new QPushButton("Scan for Duplicates", this);
    m_scanButton->setMinimumHeight(50);
    QFont scanFont = m_scanButton->font();
    scanFont.setPointSize(14);
    scanFont.setBold(true);
    m_scanButton->setFont(scanFont);
    m_scanButton->setObjectName("scanButton");
    m_mainLayout->addWidget(m_scanButton);

    // Bottom buttons
    m_buttonLayout = new QHBoxLayout();
    m_buttonLayout->setSpacing(12);

    m_backButton = new QPushButton("Back to Menu", this);
    m_backButton->setMinimumHeight(45);
    QFont btnFont = m_backButton->font();
    btnFont.setPointSize(13);
    m_backButton->setFont(btnFont);
    m_buttonLayout->addWidget(m_backButton);

    m_buttonLayout->addStretch();

    m_undoButton = new QPushButton("Undo", this);
    m_undoButton->setMinimumHeight(45);
    m_undoButton->setFont(btnFont);
    m_undoButton->setVisible(false);
    m_buttonLayout->addWidget(m_undoButton);

    m_skipButton = new QPushButton("Skip", this);
    m_skipButton->setMinimumHeight(45);
    m_skipButton->setFont(btnFont);
    m_skipButton->setVisible(false);
    m_buttonLayout->addWidget(m_skipButton);

    m_deleteNextButton = new QPushButton("Delete & Next →", this);
    m_deleteNextButton->setMinimumHeight(45);
    m_deleteNextButton->setFont(btnFont);
    m_deleteNextButton->setObjectName("deleteNextButton");
    m_deleteNextButton->setVisible(false);
    m_buttonLayout->addWidget(m_deleteNextButton);

    m_mainLayout->addLayout(m_buttonLayout);

    // Connect
    connect(m_scanButton, &QPushButton::clicked,
            this, &DuplicateFinderScreen::startScan);
    connect(m_backButton, &QPushButton::clicked,
            this, &DuplicateFinderScreen::handleBackToMenu);
    connect(m_skipButton, &QPushButton::clicked,
            this, &DuplicateFinderScreen::handleSkip);
    connect(m_deleteNextButton, &QPushButton::clicked,
            this, &DuplicateFinderScreen::handleDeleteAndNext);
    connect(m_undoButton, &QPushButton::clicked,
            this, &DuplicateFinderScreen::handleUndo);
    connect(m_previewGrid, &ImagePreviewGrid::selectionChanged,
            this, &DuplicateFinderScreen::handleSelectionChanged);
}

void DuplicateFinderScreen::setDirectories(const QStringList& dirs) {
    m_directories = dirs;
}

void DuplicateFinderScreen::startScan() {
    if (m_directories.isEmpty()) {
        return;
    }

    // Reset
    m_groups.clear();
    m_currentGroup = -1;
    m_totalDeleted = 0;
    m_totalSkipped = 0;
    m_deleteHistory.clear();

    // Show progress
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0);
    m_statusLabel->setText("Scanning directories for duplicate files...");
    m_scanButton->setEnabled(false);
    m_previewGrid->setVisible(false);
    m_groupSummaryLabel->setVisible(false);
    m_keepingLabel->setVisible(false);
    m_deletingLabel->setVisible(false);
    m_deleteNextButton->setVisible(false);
    m_skipButton->setVisible(false);
    m_undoButton->setVisible(false);

    // Async scan
    auto* watcher = new QFutureWatcher<DuplicateScanResult>(this);
    connect(watcher, &QFutureWatcher<DuplicateScanResult>::finished,
            this, [this, watcher]() {
                DuplicateScanResult result = watcher->result();
                watcher->deleteLater();
                m_groups = result.groups;
                scanFinishedSlot();
            });

    watcher->setFuture(QtConcurrent::run([this]() {
        DuplicateFinder finder;
        return finder.scan(m_directories);
    }));
}

void DuplicateFinderScreen::scanFinishedSlot() {
    m_progressBar->setVisible(false);
    m_progressBar->setRange(0, 100);
    m_scanButton->setEnabled(true);

    if (m_groups.isEmpty()) {
        m_statusLabel->setText("No duplicate files found. Your folders are clean! ✓");
        return;
    }

    // Calculate stats
    int totalFiles = 0;
    qint64 reclaimableBytes = 0;
    for (const auto& g : m_groups) {
        totalFiles += g.files.size();
        if (!g.files.isEmpty()) {
            reclaimableBytes += g.files[0].fileSizeBytes * (g.files.size() - 1);
        }
    }

    QString reclaimableStr;
    if (reclaimableBytes >= (qint64)1024 * 1024 * 1024) {
        reclaimableStr = QString("%1 GB").arg(reclaimableBytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
    } else if (reclaimableBytes >= 1024 * 1024) {
        reclaimableStr = QString("%1 MB").arg(reclaimableBytes / (1024.0 * 1024.0), 0, 'f', 1);
    } else {
        reclaimableStr = QString("%1 KB").arg(reclaimableBytes / 1024.0, 0, 'f', 1);
    }

    m_statusLabel->setText(
        QString("Found <b>%1</b> group(s) of duplicates — "
                "%2 files could be removed, freeing <b>%3</b>.")
            .arg(m_groups.size()).arg(totalFiles).arg(reclaimableStr));

    // Auto-load first group
    m_currentGroup = 0;
    loadGroup(0);
}

void DuplicateFinderScreen::loadGroup(int index) {
    if (index < 0 || index >= m_groups.size()) return;

    const DuplicateGroup& group = m_groups[index];

    // Load images
    QStringList filePaths;
    for (const auto& file : group.files) {
        filePaths.append(file.filePath);
    }
    m_previewGrid->setImages(filePaths);
    m_previewGrid->setVisible(true);

    // Pre-select the first image as the default one to keep
    m_previewGrid->selectSingle(0);

    // Group summary
    QString fileSizeStr;
    qint64 size = group.files[0].fileSizeBytes;
    if (size >= (qint64)1024 * 1024 * 1024) {
        fileSizeStr = QString("%1 GB").arg(size / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
    } else if (size >= 1024 * 1024) {
        fileSizeStr = QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
    } else if (size >= 1024) {
        fileSizeStr = QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
    } else {
        fileSizeStr = QString("%1 B").arg(size);
    }

    m_groupSummaryLabel->setText(
        QString("Group %1 of %2  •  %3 files  •  %4 each")
            .arg(index + 1).arg(m_groups.size()).arg(group.files.size()).arg(fileSizeStr));
    m_groupSummaryLabel->setVisible(true);

    updateInfoLabels();

    m_deleteNextButton->setVisible(true);
    m_skipButton->setVisible(true);
    updateUndoButton();
    m_headerLabel->setText("Find Duplicate Files");
}

void DuplicateFinderScreen::updateInfoLabels() {
    if (m_currentGroup < 0 || m_currentGroup >= m_groups.size()) return;

    const DuplicateGroup& group = m_groups[m_currentGroup];
    QStringList selected = m_previewGrid->selectedFilePaths();
    int keepCount = selected.size();
    int deleteCount = group.files.size() - keepCount;

    if (keepCount == 0) {
        m_keepingLabel->clear();
        m_deletingLabel->setText("Select image(s) to keep — the rest will be deleted.");
    } else if (deleteCount == 0) {
        m_keepingLabel->setText("✓ All selected — no duplicates to delete.");
        m_deletingLabel->clear();
    } else {
        QString firstName = QFileInfo(selected.first()).fileName();
        m_keepingLabel->setText(
            QString("✓ Keeping <b>%1</b><br><small>%2</small>")
                .arg(keepCount).arg(firstName));
        m_deletingLabel->setText(
            QString("✗ Deleting <b>%1</b> duplicate(s)")
                .arg(deleteCount));
    }

    m_keepingLabel->setVisible(true);
    m_deletingLabel->setVisible(true);
}

void DuplicateFinderScreen::updateUndoButton() {
    bool hasUndo = !m_deleteHistory.isEmpty();
    m_undoButton->setVisible(hasUndo);
    if (hasUndo) {
        const auto& last = m_deleteHistory.last();
        int count = last.filePaths.size();
        m_undoButton->setText(QString("Undo (%1)").arg(count));
    }
}

void DuplicateFinderScreen::advanceToNextGroup() {
    m_currentGroup++;
    if (m_currentGroup >= m_groups.size()) {
        // All done
        m_previewGrid->clear();
        m_previewGrid->setVisible(false);
        m_groupSummaryLabel->setVisible(false);
        m_keepingLabel->setVisible(false);
        m_deletingLabel->setVisible(false);
        m_deleteNextButton->setVisible(false);
        m_skipButton->setVisible(false);

        m_headerLabel->setText("All Duplicate Groups Processed!");
        m_statusLabel->setText(
            QString("Complete — deleted <b>%1</b> duplicate file(s), "
                    "skipped <b>%2</b> group(s).")
                .arg(m_totalDeleted).arg(m_totalSkipped));
        return;
    }

    loadGroup(m_currentGroup);
}

void DuplicateFinderScreen::handleDeleteAndNext() {
    if (m_currentGroup < 0 || m_currentGroup >= m_groups.size()) return;

    const DuplicateGroup& group = m_groups[m_currentGroup];
    QStringList keepPaths = m_previewGrid->selectedFilePaths();
    int keepCount = keepPaths.size();
    int deleteCount = group.files.size() - keepCount;

    if (keepCount == 0) {
        return;
    }

    if (deleteCount == 0) {
        // All selected — treat as skip
        m_totalSkipped++;
        advanceToNextGroup();
        return;
    }

    // Build list of files to delete
    QSet<QString> keepSet(keepPaths.begin(), keepPaths.end());
    QStringList toDelete;
    for (const auto& file : group.files) {
        if (!keepSet.contains(file.filePath)) {
            toDelete.append(file.filePath);
        }
    }

    // Create staging directory for undo
    QString stagingDir = QDir::temp().filePath("ig_sorter_undo_" + QString::number(QDateTime::currentMSecsSinceEpoch()));
    QDir().mkpath(stagingDir);

    DeleteRecord record;
    record.stagingDir = stagingDir;

    m_previewGrid->releaseImages(toDelete);

    // Copy to staging, then send originals to trash
    int deletedCount = 0;
    int failedCount = 0;
    for (const QString& path : toDelete) {
        QFileInfo fi(path);
        QString stagingPath = QDir(stagingDir).filePath(fi.fileName());

        // Copy to staging for potential undo
        if (QFile::copy(path, stagingPath)) {
            record.filePaths.append(path);
        }

        QFile file(path);
        if (file.moveToTrash()) {
            deletedCount++;
            qDebug("  [TRASH]  %s", qPrintable(path));
        } else {
            failedCount++;
            qDebug("  [FAIL]   %s", qPrintable(path));
        }
        m_previewGrid->removePath(path);
    }

    if (!record.filePaths.isEmpty()) {
        m_deleteHistory.append(record);
        updateUndoButton();
    }

    // Log kept files
    for (const QString& path : keepPaths) {
        qDebug("  [KEEP]   %s", qPrintable(path));
    }

    if (failedCount > 0) {
        qDebug("  %d file(s) sent to trash, %d failed", deletedCount, failedCount);
    } else {
        qDebug("  %d file(s) sent to trash", deletedCount);
    }

    m_totalDeleted += deletedCount;
    advanceToNextGroup();
}

void DuplicateFinderScreen::handleUndo() {
    if (m_deleteHistory.isEmpty()) return;

    DeleteRecord record = m_deleteHistory.takeLast();
    int restoredCount = 0;
    int failedCount = 0;

    for (int i = 0; i < record.filePaths.size(); ++i) {
        const QString& originalPath = record.filePaths[i];
        QString stagingPath = QDir(record.stagingDir).filePath(QFileInfo(originalPath).fileName());

        // Ensure parent directory exists
        QDir().mkpath(QFileInfo(originalPath).absolutePath());

        if (QFile::exists(originalPath)) {
            // File already exists at original path — skip
            qDebug("  [SKIP]   %s (already exists)", qPrintable(originalPath));
            failedCount++;
            continue;
        }

        if (QFile::copy(stagingPath, originalPath)) {
            restoredCount++;
            qDebug("  [RESTORED] %s", qPrintable(originalPath));
        } else {
            failedCount++;
            qDebug("  [UNDO FAIL] %s", qPrintable(originalPath));
        }
    }

    // Clean up staging directory
    QDir stagingDir(record.stagingDir);
    for (const QFileInfo& entry : stagingDir.entryInfoList(QDir::Files)) {
        QFile::remove(entry.absoluteFilePath());
    }
    stagingDir.rmdir(stagingDir.absolutePath());

    if (failedCount == 0) {
        QMessageBox::information(this, "Undo Successful",
            QString("Restored %1 file(s).").arg(restoredCount));
    } else {
        QMessageBox::warning(this, "Undo Partial",
            QString("Restored %1 file(s), %1 failed.").arg(restoredCount).arg(failedCount));
    }

    m_totalDeleted -= restoredCount;
    updateUndoButton();
}

void DuplicateFinderScreen::handleSkip() {
    m_totalSkipped++;
    advanceToNextGroup();
}

void DuplicateFinderScreen::handleSelectionChanged(int count) {
    Q_UNUSED(count);
    updateInfoLabels();
}

void DuplicateFinderScreen::handleBackToMenu() {
    emit menuClicked();
}
