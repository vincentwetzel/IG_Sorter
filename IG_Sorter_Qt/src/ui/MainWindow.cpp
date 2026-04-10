#include "ui/MainWindow.h"
#include "ui/MenuScreen.h"
#include "ui/CleanupScreen.h"
#include "ui/CleanUpAccountsScreen.h"
#include "ui/SortingScreen.h"
#include "ui/ReportScreen.h"
#include "ui/SettingsDialog.h"
#include "core/SorterEngine.h"
#include "core/DatabaseManager.h"
#include "core/DirectoryCleanup.h"
#include "core/FileGrouper.h"
#include "utils/ConfigManager.h"
#include "utils/ThemeManager.h"
#include "utils/LogManager.h"
#include "utils/FileUtils.h"
#include <QStackedWidget>
#include <QStatusBar>
#include <QApplication>
#include <QCoreApplication>
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
    // Resolve relative paths relative to the application directory.
    // If not found, also check the parent directory (covers dev builds where
    // the binary lives in build/ but data is in the repo root).
    QFileInfo dbInfo(dbPath);
    if (!dbInfo.isAbsolute()) {
        QString appDir = QCoreApplication::applicationDirPath();
        QString candidate = QDir(appDir).filePath(dbPath);
        if (!QFile::exists(candidate)) {
            // Try one level up (repo root during development)
            candidate = QDir(appDir).filePath("../" + dbPath);
            if (!QFile::exists(candidate)) {
                // Last resort: fall back to submodule default
                candidate = QDir(appDir).filePath("../private-data/ig_people.json");
                if (QFile::exists(candidate)) {
                    LogManager::instance()->warning(
                        QString("Configured DB not found, using default: %1").arg(candidate));
                    dbPath = candidate;
                }
            } else {
                dbPath = QFileInfo(candidate).absoluteFilePath();
            }
        } else {
            dbPath = QFileInfo(candidate).absoluteFilePath();
        }
    }

    LogManager::instance()->info(QString("Database path: %1 (exists: %2)").arg(dbPath).arg(QFile::exists(dbPath)));

    m_db = new DatabaseManager(dbPath, this);
    // Attempt to load at startup; warn only if file exists but fails to load
    if (!dbPath.isEmpty()) {
        bool loaded = m_db->load();
        LogManager::instance()->info(QString("Database loaded: %1, entries: %2").arg(loaded).arg(m_db->allEntries().size()));
        if (!loaded && QFile::exists(dbPath)) {
            QMessageBox::warning(this, "Database Load Error",
                QString("Failed to load database from:\n%1").arg(dbPath));
        }
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
    m_cleanUpAccountsScreen = new CleanUpAccountsScreen(this);
    m_sortingScreen = new SortingScreen(this);
    m_reportScreen = new ReportScreen(this);

    m_stackedWidget->addWidget(m_menuScreen);              // index 0
    m_stackedWidget->addWidget(m_cleanupScreen);           // index 1
    m_stackedWidget->addWidget(m_cleanUpAccountsScreen);   // index 4
    m_stackedWidget->addWidget(m_sortingScreen);           // index 2
    m_stackedWidget->addWidget(m_reportScreen);            // index 3

    m_groupWatcher = nullptr;  // no grouping in progress at startup

    // === Connect screen navigation signals ===

    // MenuScreen -> Settings or Start
    connect(m_menuScreen, &MenuScreen::settingsClicked,
            this, &MainWindow::showSettings);
    connect(m_menuScreen, &MenuScreen::startSortingClicked,
            this, &MainWindow::startSortingPipeline);

    // CleanupScreen -> SortingScreen
    connect(m_cleanupScreen, &CleanupScreen::continueClicked,
            this, &MainWindow::showSortingScreen);
    connect(m_cleanupScreen, &CleanupScreen::menuClicked,
            this, &MainWindow::showMenuScreen);

    // CleanUpAccountsScreen
    m_cleanUpAccountsScreen->setDatabaseManager(m_db);
    connect(m_cleanUpAccountsScreen, &CleanUpAccountsScreen::menuClicked,
            this, &MainWindow::showMenuScreen);

    // MenuScreen -> CleanUpAccountsScreen
    connect(m_menuScreen, &MenuScreen::cleanUpAccountsClicked,
            this, &MainWindow::showCleanUpAccountsScreen);

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

MainWindow::~MainWindow() {
    cancelGrouping();
}

void MainWindow::showMenuScreen() {
    cancelGrouping();
    m_menuScreen->refreshConfigStatus();
    m_stackedWidget->setCurrentWidget(m_menuScreen);
    m_statusBar->showMessage("Ready");
}

void MainWindow::showCleanupScreen() {
    m_stackedWidget->setCurrentWidget(m_cleanupScreen);
}

void MainWindow::showCleanUpAccountsScreen() {
    m_cleanUpAccountsScreen->refreshAccountsList();
    m_stackedWidget->setCurrentWidget(m_cleanUpAccountsScreen);
}

void MainWindow::cancelGrouping() {
    if (m_groupWatcher) {
        m_groupWatcher->cancel();
        m_groupWatcher->deleteLater();
        m_groupWatcher = nullptr;
        m_statusBar->showMessage("Grouping cancelled.");
    }
}

void MainWindow::showSortingScreen() {
    // Cancel any previous grouping that might still be running
    cancelGrouping();

    // Run file grouping asynchronously to avoid blocking the UI
    m_statusBar->showMessage("Scanning and grouping files...");
    m_cleanupScreen->setDirectories(QStringList());  // clear old progress bars
    m_cleanupScreen->setStatusText("Grouping files from source directory...");
    m_cleanupScreen->enableContinue(false);
    showCleanupScreen();  // keep showing cleanup screen while grouping

    m_groupWatcher = new QFutureWatcher<QList<FileGroup>>();

    connect(m_groupWatcher, &QFutureWatcher<QList<FileGroup>>::finished,
            this, [this]() {
                if (!m_groupWatcher) return;  // was cancelled

                QList<FileGroup> groups = m_groupWatcher->result();
                m_groupWatcher->deleteLater();
                m_groupWatcher = nullptr;

                // Load output folders into the sort panel
                m_sortingScreen->setOutputFolders(ConfigManager::instance()->outputFolders());
                m_sortingScreen->setGroups(groups);
                m_sortingScreen->setDatabaseManager(m_db);
                m_sortingScreen->setEngine(m_engine);

                m_stackedWidget->setCurrentWidget(m_sortingScreen);
                m_statusBar->showMessage(QString("Sorting: %1 groups found").arg(groups.size()));
            });

    m_groupWatcher->setFuture(QtConcurrent::run([this]() {
        return m_engine->groupFiles();
    }));
}

void MainWindow::showReportScreen() {
    m_stackedWidget->setCurrentWidget(m_reportScreen);
    m_statusBar->showMessage("Sorting complete.");
}

void MainWindow::showSettings() {
    m_settingsDialog->loadSettings();
    m_settingsDialog->exec();
    // Refresh menu screen in case settings changed
    if (m_stackedWidget->currentWidget() == m_menuScreen) {
        m_menuScreen->refreshConfigStatus();
    }
}

void MainWindow::startSortingPipeline() {
    // Cancel any previous grouping in progress
    cancelGrouping();

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

    // Reload database if path changed — resolve relative path the same way
    QString dbPath = ConfigManager::instance()->databaseFile();
    QFileInfo dbReloadInfo(dbPath);
    if (!dbReloadInfo.isAbsolute()) {
        QString appDir = QCoreApplication::applicationDirPath();
        QString candidate = QDir(appDir).filePath(dbPath);
        if (!QFile::exists(candidate)) {
            candidate = QDir(appDir).filePath("../" + dbPath);
            if (!QFile::exists(candidate)) {
                candidate = QDir(appDir).filePath("../private-data/ig_people.json");
                if (QFile::exists(candidate)) {
                    dbPath = candidate;
                }
            }
        }
        dbPath = QFileInfo(candidate).absoluteFilePath();
    }
    int entriesBefore = m_db->allEntries().size();
    if (!dbPath.isEmpty() && QFile::exists(dbPath)) {
        bool reloaded = m_db->load();
        LogManager::instance()->info(
            QString("DB reload: %1, entries before=%2 after=%3")
                .arg(reloaded).arg(entriesBefore).arg(m_db->allEntries().size()));
    }

    // Validate database is loaded before proceeding
    if (m_db->allEntries().isEmpty()) {
        int ret = QMessageBox::warning(this, "No Database Loaded",
            "No accounts database is loaded. The app cannot identify any names.\n\n"
            "Would you like to open Settings to configure the database path?",
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Yes);

        if (ret == QMessageBox::Yes) {
            showSettings();
        }
        return;
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
    connect(m_engine, &SorterEngine::cleanupDirectoryDone,
            m_cleanupScreen, &CleanupScreen::setDirectoryRenamed);

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
