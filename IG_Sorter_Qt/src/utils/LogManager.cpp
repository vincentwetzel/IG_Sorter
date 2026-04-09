#include "utils/LogManager.h"
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QtDebug>

LogManager* LogManager::s_instance = nullptr;

LogManager::LogManager(QObject* parent)
    : QObject(parent), m_maxFiles(5) {}

LogManager* LogManager::instance() {
    if (!s_instance) {
        s_instance = new LogManager();
    }
    return s_instance;
}

void LogManager::start(const QString& logDir, int maxFiles) {
    QMutexLocker locker(&m_mutex);

    m_logDir = logDir;
    m_maxFiles = maxFiles;

    QDir dir(m_logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Clean up old log files if we exceed the limit
    cleanupOldLogs();

    // Create a new timestamped log file for this launch
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    m_currentLogPath = dir.filePath(QString("ig_sorter_%1.log").arg(timestamp));

    if (m_logFile.isOpen()) {
        m_logFile.close();
    }

    m_logFile.setFileName(m_currentLogPath);
    m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);

    info(QString("Logging started — %1 (max %2 files)")
             .arg(m_currentLogPath).arg(m_maxFiles));
}

void LogManager::log(LogLevel level, const QString& message) {
    QMutexLocker locker(&m_mutex);

    QString line = QString("[%1] [%2] %3")
                       .arg(timestamp(), levelString(level), message);

    // Also output to console
    switch (level) {
    case LogLevel::Debug:   qDebug().noquote() << line; break;
    case LogLevel::Info:    qInfo().noquote() << line; break;
    case LogLevel::Warning: qWarning().noquote() << line; break;
    case LogLevel::Error:   qCritical().noquote() << line; break;
    }

    // Write to file
    if (m_logFile.isOpen()) {
        m_logFile.write(line.toUtf8() + "\n");
        m_logFile.flush();
    }

    rotateIfNeeded();
}

void LogManager::debug(const QString& message)   { log(LogLevel::Debug, message); }
void LogManager::info(const QString& message)    { log(LogLevel::Info, message); }
void LogManager::warning(const QString& message) { log(LogLevel::Warning, message); }
void LogManager::error(const QString& message)   { log(LogLevel::Error, message); }

void LogManager::logFileMoved(const QString& src, const QString& dest, qint64 sizeBytes) {
    info(QString("MOVED: \"%1\" -> \"%2\" (%3 bytes)")
             .arg(src, dest).arg(sizeBytes));
}

void LogManager::logFileDeleted(const QString& path) {
    info(QString("DELETED: \"%1\"").arg(path));
}

void LogManager::logFileRenamed(const QString& oldPath, const QString& newPath) {
    info(QString("RENAMED: \"%1\" -> \"%2\"").arg(oldPath, newPath));
}

void LogManager::logFileSkipped(const QString& path, const QString& reason) {
    info(QString("SKIPPED: \"%1\" - %2").arg(path, reason));
}

void LogManager::logError(const QString& path, const QString& errorMsg) {
    error(QString("ERROR: \"%1\" - %2").arg(path, errorMsg));
}

void LogManager::logDirectoryCleaned(const QString& dir, int filesRenamed) {
    info(QString("CLEANUP: \"%1\" - %2 files renumbered").arg(dir).arg(filesRenamed));
}

void LogManager::logSortComplete(int sorted, int skipped, int errors) {
    info(QString("SORT COMPLETE - sorted: %1, skipped: %2, errors: %3")
             .arg(sorted).arg(skipped).arg(errors));
}

QString LogManager::currentLogFile() const {
    return m_currentLogPath;
}

void LogManager::rotateIfNeeded() {
    // No size-based rotation — each launch gets its own file
    Q_UNUSED(m_logFile)
}

void LogManager::cleanupOldLogs() {
    QDir dir(m_logDir);

    // Find all timestamped log files, sorted by name (oldest first)
    QStringList logFiles = dir.entryList(QStringList() << "ig_sorter_*.log", QDir::Files, QDir::Name);

    if (logFiles.size() >= m_maxFiles) {
        int filesToRemove = logFiles.size() - m_maxFiles + 1;
        for (int i = 0; i < filesToRemove; ++i) {
            QFile::remove(dir.filePath(logFiles.at(i)));
        }
    }
}

QString LogManager::timestamp() const {
    return QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
}

QString LogManager::levelString(LogLevel level) const {
    switch (level) {
    case LogLevel::Debug:   return "DEBUG";
    case LogLevel::Info:    return "INFO";
    case LogLevel::Warning: return "WARN";
    case LogLevel::Error:   return "ERROR";
    }
    return "UNKNOWN";
}
