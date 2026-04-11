#pragma once

#include <QWidget>
#include <QList>
#include <QString>
#include "core/DuplicateFinder.h"

class QVBoxLayout;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QProgressBar;
class ImagePreviewGrid;

class DuplicateFinderScreen : public QWidget {
    Q_OBJECT
public:
    explicit DuplicateFinderScreen(QWidget* parent = nullptr);

    void setDirectories(const QStringList& dirs);

signals:
    void menuClicked();

private slots:
    void startScan();
    void scanFinishedSlot();
    void handleDeleteAndNext();
    void handleSkip();
    void handleUndo();
    void handleSelectionChanged(int count);
    void handleBackToMenu();

private:
    struct DeleteRecord {
        QStringList filePaths;      // original paths that were deleted
        QString stagingDir;          // where copies were stashed
    };

    void loadGroup(int index);
    void advanceToNextGroup();
    void updateInfoLabels();
    void updateUndoButton();

    QStringList          m_directories;
    QVBoxLayout*         m_mainLayout;
    QLabel*              m_headerLabel;
    QProgressBar*        m_progressBar;
    QLabel*              m_groupSummaryLabel;
    ImagePreviewGrid*    m_previewGrid;
    QLabel*              m_keepingLabel;
    QLabel*              m_deletingLabel;
    QLabel*              m_statusLabel;
    QPushButton*         m_scanButton;
    QHBoxLayout*         m_buttonLayout;
    QPushButton*         m_backButton;
    QPushButton*         m_skipButton;
    QPushButton*         m_deleteNextButton;
    QPushButton*         m_undoButton;

    QList<DuplicateGroup> m_groups;
    int                  m_currentGroup;
    int                  m_totalDeleted;
    int                  m_totalSkipped;
    QList<DeleteRecord>  m_deleteHistory;  // stack of delete operations
};
