#include "ui/MainWindow.h"
#include "utils/ConfigManager.h"
#include "utils/LogManager.h"
#include <QApplication>
#include <QStyleHints>
#include <QStandardPaths>
#include <QFont>
#include <QSettings>
#include <QStyleFactory>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Use Fusion style for full stylesheet support (Windows style overrides QSS)
    app.setStyle(QStyleFactory::create("Fusion"));

    // Use INI format for cross-platform config storage
    QSettings::setDefaultFormat(QSettings::IniFormat);

    // Set application info for QSettings
    app.setOrganizationName("IG_Sorter");
    app.setApplicationName("IG Sorter");
    app.setApplicationVersion("0.1.0");

    // Tell Qt where to find image format plugins (for dev builds)
    app.addLibraryPath("C:/Qt/6.10.2/msvc2022_64/plugins");

    // Set default font
    QFont defaultFont("Segoe UI", 11);
    app.setFont(defaultFont);

    // Initialize log manager — creates timestamped log file per launch, keeps max 5 files
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    LogManager::instance()->start(logDir);

    // Create and show main window
    MainWindow window;
    window.show();

    return app.exec();
}
