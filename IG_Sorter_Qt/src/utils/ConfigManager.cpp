#include "utils/ConfigManager.h"
#include <QSettings>
#include <QStandardPaths>

ConfigManager* ConfigManager::s_instance = nullptr;

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent) {}

ConfigManager* ConfigManager::instance() {
    if (!s_instance) {
        s_instance = new ConfigManager();
    }
    return s_instance;
}

QString ConfigManager::sourceFolder() const {
    QSettings settings;
    return settings.value("SourceFolder", "").toString();
}

void ConfigManager::setSourceFolder(const QString& path) {
    QSettings settings;
    settings.setValue("SourceFolder", path);
    settings.sync();
}

QVector<OutputFolderConfig> ConfigManager::outputFolders() const {
    QSettings settings;
    settings.sync();
    int size = settings.beginReadArray("OutputFolders");
    QVector<OutputFolderConfig> folders;
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        OutputFolderConfig folder;
        folder.name = settings.value("Name", QString("Folder %1").arg(i + 1)).toString();
        folder.path = settings.value("Path", "").toString();
        folders.append(folder);
    }
    settings.endArray();
    return folders;
}

void ConfigManager::setOutputFolders(const QVector<OutputFolderConfig>& folders) {
    QSettings settings;
    settings.beginWriteArray("OutputFolders");
    for (int i = 0; i < folders.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("Name", folders[i].name);
        settings.setValue("Path", folders[i].path);
    }
    settings.endArray();
    settings.sync();
}

void ConfigManager::addOutputFolder(const QString& name, const QString& path) {
    QVector<OutputFolderConfig> folders = outputFolders();
    OutputFolderConfig folder;
    folder.name = name;
    folder.path = path;
    folders.append(folder);
    setOutputFolders(folders);
}

void ConfigManager::removeOutputFolder(int index) {
    QVector<OutputFolderConfig> folders = outputFolders();
    if (index >= 0 && index < folders.size()) {
        folders.removeAt(index);
        setOutputFolders(folders);
    }
}

QString ConfigManager::databaseFile() const {
    QSettings settings;
    QString path = settings.value("DatabaseFile", "private-data/ig_people.json").toString();
    return path;
}

void ConfigManager::setDatabaseFile(const QString& path) {
    QSettings settings;
    settings.setValue("DatabaseFile", path);
    settings.sync();
}

int ConfigManager::batchSize() const {
    QSettings settings;
    settings.sync();
    return settings.value("BatchSize", 5).toInt();
}

void ConfigManager::setBatchSize(int size) {
    QSettings settings;
    settings.setValue("BatchSize", size);
    settings.sync();
}

QString ConfigManager::theme() const {
    QSettings settings;
    settings.sync();
    return settings.value("Theme", "system").toString();
}

void ConfigManager::setTheme(const QString& theme) {
    QSettings settings;
    settings.setValue("Theme", theme);
    settings.sync();
}

void ConfigManager::save() {
    QSettings settings;
    settings.sync();
    if (settings.status() != QSettings::NoError) {
        qWarning("ConfigManager::save() - QSettings error: %d", settings.status());
    }
}
