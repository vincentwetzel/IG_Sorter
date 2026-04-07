#pragma once

#include <QWidget>
#include <QList>
#include <QStringList>
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
class DatabaseManager;

struct OutputFolderConfig;

class SortingScreen : public QWidget {
    Q_OBJECT
public:
    explicit SortingScreen(QWidget* parent = nullptr);

    void setGroups(const QList<FileGroup>& groups);
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
    void handleAddUnknownAccount(const QString& account, const QString& irlName,
                                 AccountType type);
    void handleOpenInstagram(const QString& account);
    void handleCuratorResolvedName(const QString& irlName);

private:
    void updateHeader();

    QVBoxLayout*      m_mainLayout;
    QLabel*           m_headerLabel;
    ImagePreviewGrid* m_previewGrid;
    SortPanel*        m_sortPanel;
    QHBoxLayout*      m_buttonLayout;
    QPushButton*      m_backButton;
    QPushButton*      m_finishButton;

    QList<FileGroup>  m_groups;
    int               m_currentGroup;
    int               m_currentSubBatch;

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
};
