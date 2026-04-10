#pragma once

#include <QMainWindow>
#include "core/Types.h"

class QStackedWidget;
class QStatusBar;
template <typename T> class QFutureWatcher;
class MenuScreen;
class CleanupScreen;
class CleanUpAccountsScreen;
class SortingScreen;
class ReportScreen;
class SettingsDialog;
class DatabaseManager;
class SorterEngine;
struct FileGroup;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void showMenuScreen();
    void showCleanupScreen();
    void showCleanUpAccountsScreen();
    void showSortingScreen();
    void showReportScreen();
    void showSettings();

    DatabaseManager* database() const { return m_db; }
    SorterEngine* engine() const { return m_engine; }

signals:
    void cleanupFinished();
    void sortingComplete(const SortReportData& report);

private:
    void startSortingPipeline();
    void cancelGrouping();

    QStackedWidget* m_stackedWidget;
    QStatusBar*     m_statusBar;
    DatabaseManager* m_db;
    SorterEngine*    m_engine;
    SettingsDialog*  m_settingsDialog;

    MenuScreen*     m_menuScreen;
    CleanupScreen*  m_cleanupScreen;
    CleanUpAccountsScreen* m_cleanUpAccountsScreen;
    SortingScreen*  m_sortingScreen;
    ReportScreen*   m_reportScreen;

    QFutureWatcher<QList<FileGroup>>* m_groupWatcher;  // null when no grouping in progress
};
