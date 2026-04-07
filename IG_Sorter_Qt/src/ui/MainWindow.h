#pragma once

#include <QMainWindow>
#include "core/Types.h"

class QStackedWidget;
class QStatusBar;
class MenuScreen;
class CleanupScreen;
class SortingScreen;
class ReportScreen;
class SettingsDialog;
class DatabaseManager;
class SorterEngine;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void showMenuScreen();
    void showCleanupScreen();
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

    QStackedWidget* m_stackedWidget;
    QStatusBar*     m_statusBar;
    DatabaseManager* m_db;
    SorterEngine*    m_engine;
    SettingsDialog*  m_settingsDialog;

    MenuScreen*     m_menuScreen;
    CleanupScreen*  m_cleanupScreen;
    SortingScreen*  m_sortingScreen;
    ReportScreen*   m_reportScreen;
};
