#pragma once

#include <QWidget>
#include <QList>
#include <QMap>
#include <QStringList>
#include <QVector>
#include "core/FileGrouper.h"
#include "core/Types.h"

class QVBoxLayout;
class QHBoxLayout;
class QLabel;
class ImagePreviewGrid;
class SortPanel;
class QPushButton;
class DatabaseManager;
class SorterEngine;

struct OutputFolderConfig;

class SortingScreen : public QWidget {
    Q_OBJECT
public:
    explicit SortingScreen(QWidget* parent = nullptr);

    void setGroups(const QList<FileGroup>& groups);
    void beginStreamingGroups();
    void appendGroups(const QList<FileGroup>& groups);
    void finishStreamingGroups();
    int availableGroupCount() const { return m_groups.size(); }
    void loadNextBatch();
    void setOutputFolders(const QVector<OutputFolderConfig>& folders);
    void setDatabaseManager(DatabaseManager* db);
    void setEngine(SorterEngine* engine);

    // Statistics
    int totalFilesSorted() const { return m_filesSorted; }
    int totalFilesSkipped() const { return m_filesSkipped; }
    int totalErrors() const { return m_errors; }
    QStringList errorMessages() const { return m_errorMessages; }
    QList<QString> newAccountsAdded() const { return m_newAccountsAdded; }
    QMap<QString, int> filesByAccountType() const { return m_filesByAccountType; }

signals:
    void batchSorted(const QStringList& filePaths, const QString& outputDir);
    void allBatchesDone();
    void backClicked();
    void finishClicked();

private slots:
    void handleSortToFolder(int folderIndex);
    void handleSkip();
    void handleDeleteSelected();
    void handleSelectAll(bool selectAll);
    void handleSelectionChanged(int count);
    void handleUndo();
    void handleAddUnknownAccount(const QString& account, const QString& irlName,
                                 AccountType type);
    void handleOpenInstagram(const QString& account);
    void handleCuratorResolvedName(const QString& irlName, AccountType accountType);

private:
    void updateHeader();
    void updateFavoriteButtons();
    void updateUndoButton();
    // Update all groups matching a newly-added account
    void updateGroupsForNewAccount(const QString& accountHandle);

    QVBoxLayout*      m_mainLayout;
    QLabel*           m_headerLabel;
    ImagePreviewGrid* m_previewGrid;
    SortPanel*        m_sortPanel;
    QHBoxLayout*      m_buttonLayout;
    QPushButton*      m_backButton;
    QPushButton*      m_undoButton;
    QPushButton*      m_finishButton;

    QList<FileGroup>  m_groups;
    int               m_currentGroup;
    int               m_currentSubBatch;
    int               m_pendingSortTasks = 0;
    bool              m_groupingComplete = true;
    bool              m_waitingForGroups = false;

    QVector<OutputFolderConfig> m_outputFolders;
    DatabaseManager*   m_db;
    SorterEngine*      m_engine;

    // Statistics
    int m_filesSorted;
    int m_filesSkipped;
    int m_errors;
    QStringList m_errorMessages;
    QList<QString> m_newAccountsAdded;
    QMap<QString, int> m_filesByAccountType;  // "Personal"/"Curator" -> count

    // Quick-fill name tracking (leaderboard for IrlOnly sources)
    QMap<QString, int> m_nameCounts;   // IRL name → sorted photo count
    QMap<QString, quint64> m_nameLastUsed; // Most recently used names sort last when counts tie
    quint64 m_nameUseSequence = 0;
};
