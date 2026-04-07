#include "ui/MainWindow.h"
#include "ui/MenuScreen.h"
#include "ui/CleanupScreen.h"
#include "ui/SortingScreen.h"
#include "ui/ReportScreen.h"
#include "ui/SettingsDialog.h"
#include "core/SorterEngine.h"
#include "core/DatabaseManager.h"
#include "core/DirectoryCleanup.h"
#include "core/FileGrouper.h"
#include "utils/ConfigManager.h"
#include "utils/ThemeManager.h"
#include "utils/FileUtils.h"
#include <QStackedWidget>
#include <QStatusBar>
#include <QApplication>
#include <QFileInfo>
#include <QDir>
#include <QFile>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QMessageBox>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("IG Sorter");
    resize(1200, 800);

    // Initialize subsystems
    ConfigManager::instance();
    ThemeManager::instance();

    // Database
    QString dbPath = ConfigManager::instance()->databaseFile();
    m_db = new DatabaseManager(dbPath, this);
    // Attempt to load at startup; warn only if file exists but fails to load
    if (!dbPath.isEmpty() && !m_db->load()) {
        // Silently skip — user may not have configured a database yet
    }

    // Engine
    m_engine = new SorterEngine(m_db, this);

    // Status bar
    m_statusBar = statusBar();
    m_statusBar->showMessage("Ready");

    // Settings dialog (modal, not in stack)
    m_settingsDialog = new SettingsDialog(this);
    m_stackedWidget = new QStackedWidget(this);
    setCentralWidget(m_stackedWidget);

    // Create screens
    m_menuScreen = new MenuScreen(this);
    m_cleanupScreen = new CleanupScreen(this);
    m_sortingScreen = new SortingScreen(this);
    m_reportScreen = new ReportScreen(this);

    m_stackedWidget->addWidget(m_menuScreen);        // index 0
    m_stackedWidget->addWidget(m_cleanupScreen);     // index 1
    m_stackedWidget->addWidget(m_sortingScreen);     // index 2
    m_stackedWidget->addWidget(m_reportScreen);      // index 3

    // === Connect screen navigation signals ===

    // MenuScreen -> Settings or Start
    connect(m_menuScreen, &MenuScreen::settingsClicked,
            this, &MainWindow::showSettings);
    connect(m_menuScreen, &MenuScreen::startSortingClicked,
            this, &MainWindow::startSortingPipeline);

    // CleanupScreen -> SortingScreen
    connect(m_cleanupScreen, &CleanupScreen::continueClicked,
            this, &MainWindow::showSortingScreen);

    // SortingScreen -> ReportScreen when done
    connect(m_sortingScreen, &SortingScreen::allBatchesDone,
            this, [this]() {
                // Build report and show it
                SortReportData report;
                report.filesSorted = m_sortingScreen->totalFilesSorted();
                report.filesSkipped = m_sortingScreen->totalFilesSkipped();
                report.errors = m_sortingScreen->totalErrors();
                report.errorMessages = m_sortingScreen->errorMessages();

                // Count files in output dirs
                for (const auto& folder : ConfigManager::instance()->outputFolders()) {
                    QDir dir(folder.path);
                    if (dir.exists()) {
                        int fileCount = dir.entryList(QDir::Files | QDir::NoDotAndDotDot).size();
                        report.directoryFileCounts[folder.name] = fileCount;
                    }
                }

                report.newAccountsAdded = m_sortingScreen->newAccountsAdded();
                report.filesByAccountType = m_sortingScreen->filesByAccountType();

                m_reportScreen->setReport(report);
                showReportScreen();
            });

    connect(m_sortingScreen, &SortingScreen::backClicked,
            this, &MainWindow::showMenuScreen);
    connect(m_sortingScreen, &SortingScreen::finishClicked,
            this, [this]() {
                // Build report early if user clicks Finish before all batches done
                SortReportData report;
                report.filesSorted = m_sortingScreen->totalFilesSorted();
                report.filesSkipped = m_sortingScreen->totalFilesSkipped();
                report.errors = m_sortingScreen->totalErrors();
                report.errorMessages = m_sortingScreen->errorMessages();

                for (const auto& folder : ConfigManager::instance()->outputFolders()) {
                    QDir dir(folder.path);
                    if (dir.exists()) {
                        int fileCount = dir.entryList(QDir::Files | QDir::NoDotAndDotDot).size();
                        report.directoryFileCounts[folder.name] = fileCount;
                    }
                }
                report.newAccountsAdded = m_sortingScreen->newAccountsAdded();
                report.filesByAccountType = m_sortingScreen->filesByAccountType();

                m_reportScreen->setReport(report);
                showReportScreen();
            });

    // ReportScreen -> MenuScreen or Settings
    connect(m_reportScreen, &ReportScreen::doneClicked,
            this, &MainWindow::showMenuScreen);
    connect(m_reportScreen, &ReportScreen::settingsClicked,
            this, &MainWindow::showSettings);

    // Apply theme
    ThemeManager::instance()->applyTheme();

    // Show menu screen
    showMenuScreen();
}

MainWindow::~MainWindow() = default;

void MainWindow::showMenuScreen() {
    m_menuScreen->refreshConfigStatus();
    m_stackedWidget->setCurrentWidget(m_menuScreen);
    m_statusBar->showMessage("Ready");
}

void MainWindow::showCleanupScreen() {
    m_stackedWidget->setCurrentWidget(m_cleanupScreen);
}

void MainWindow::showSortingScreen() {
    // Group files and set on sorting screen
    QList<FileGroup> groups = m_engine->groupFiles();

    // Load output folders into the sort panel
    m_sortingScreen->setOutputFolders(ConfigManager::instance()->outputFolders());
    m_sortingScreen->setGroups(groups);
    m_sortingScreen->setDatabaseManager(m_db);
    m_sortingScreen->setEngine(m_engine);

    m_stackedWidget->setCurrentWidget(m_sortingScreen);
    m_statusBar->showMessage(QString("Sorting: %1 groups found").arg(groups.size()));
}

void MainWindow::showReportScreen() {
    m_stackedWidget->setCurrentWidget(m_reportScreen);
    m_statusBar->showMessage("Sorting complete.");
}

void MainWindow::showSettings() {
    m_settingsDialog->loadSettings();
    m_settingsDialog->exec();
}

void MainWindow::startSortingPipeline() {
    // Validate config
    QString sourceDir = ConfigManager::instance()->sourceFolder();
    if (sourceDir.isEmpty() || !QDir(sourceDir).exists()) {
        QMessageBox::warning(this, "Configuration Error",
            "No source folder configured. Please open Settings and set the source folder.");
        return;
    }

    auto outputFolders = ConfigManager::instance()->outputFolders();
    if (outputFolders.isEmpty()) {
        QMessageBox::warning(this, "Configuration Error",
            "No output folders configured. Please open Settings and add at least one output folder.");
        return;
    }

    QStringList outputDirPaths;
    for (const auto& folder : outputFolders) {
        outputDirPaths.append(folder.path);
    }

    // Initialize engine
    if (!m_engine->initialize(sourceDir, outputDirPaths)) {
        QMessageBox::critical(this, "Initialization Error",
            "Failed to initialize the sorting engine. Check your configuration.");
        return;
    }

    // Reload database if path changed
    QString dbPath = ConfigManager::instance()->databaseFile();
    if (!dbPath.isEmpty() && QFile::exists(dbPath)) {
        m_db->load();
    }

    // Show cleanup screen
    showCleanupScreen();
    m_cleanupScreen->setDirectories(outputDirPaths);
    m_statusBar->showMessage("Running directory cleanup...");

    // Connect engine cleanup progress to UI
    connect(m_engine, &SorterEngine::cleanupProgress,
            m_cleanupScreen, [this](const QString& dir, int current, int total) {
        m_cleanupScreen->updateDirectoryProgress(dir, current, total);
    });

    // Run cleanup via the engine (async, with progress signals)
    QFutureWatcher<CleanupReport>* cleanupWatcher =
        new QFutureWatcher<CleanupReport>(this);

    connect(cleanupWatcher, &QFutureWatcher<CleanupReport>::finished,
            this, [this, cleanupWatcher]() {
                CleanupReport report = cleanupWatcher->result();

                QStringList unknownNames;
                for (const auto& issue : report.unresolvedIssues) {
                    if (!unknownNames.contains(issue.personName)) {
                        unknownNames.append(issue.personName);
                    }
                }

                if (!unknownNames.isEmpty()) {
                    m_cleanupScreen->showUnresolvedIssues(unknownNames, m_db);
                    m_statusBar->showMessage(
                        QString("Cleanup complete — %1 unknown name(s) to resolve.").arg(unknownNames.size()));
                } else {
                    m_statusBar->showMessage(
                        QString("Cleanup complete. %1 files renumbered across %2 directories.")
                            .arg(report.totalFilesRenamed)
                            .arg(report.totalDirectoriesScanned));
                    m_cleanupScreen->enableContinue(true);
                }

                cleanupWatcher->deleteLater();
            });

    cleanupWatcher->setFuture(QtConcurrent::run([this]() {
        return m_engine->runCleanup();
    }));
}
