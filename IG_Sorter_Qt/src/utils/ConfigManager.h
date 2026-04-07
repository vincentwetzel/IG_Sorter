#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

struct OutputFolderConfig {
    QString name;   // Button label
    QString path;   // Full directory path
};

class ConfigManager : public QObject {
    Q_OBJECT
public:
    explicit ConfigManager(QObject* parent = nullptr);
    static ConfigManager* instance();

    // Source folder
    QString     sourceFolder() const;
    void        setSourceFolder(const QString& path);

    // Output folders
    QVector<OutputFolderConfig> outputFolders() const;
    void        setOutputFolders(const QVector<OutputFolderConfig>& folders);
    void        addOutputFolder(const QString& name, const QString& path);
    void        removeOutputFolder(int index);

    // Database file
    QString     databaseFile() const;
    void        setDatabaseFile(const QString& path);

    // Batch size
    int         batchSize() const;
    void        setBatchSize(int size);

    // Theme
    QString     theme() const;  // "light", "dark", "system"
    void        setTheme(const QString& theme);

    void        save();

private:
    static ConfigManager* s_instance;
};
