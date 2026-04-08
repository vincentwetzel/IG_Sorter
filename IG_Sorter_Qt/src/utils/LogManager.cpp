#include "utils/LogManager.h"
#include <QDir>
#include <QFileInfo>
#include <QDateTime>
#include <QtDebug>

LogManager* LogManager::s_instance = nullptr;

LogManager::LogManager(QObject* parent)
    : QObject(parent), m_maxBytes(5 * 1024 * 1024) {}

LogManager* LogManager::instance() {
    if (!s_instance) {
        s_instance = new LogManager();
    }
    return s_instance;
}

void LogManager::start(const QString& logDir, int maxBytes) {
    QMutexLocker locker(&m_mutex);

    m_logDir = logDir;
    m_maxBytes = maxBytes;

    QDir dir(m_logDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Create timestamped log file
    QString fileName = QString("ig_sorter_%1.log")
                           .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"));
    m_currentLogPath = dir.filePath(fileName);

    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
    m_logFile.setFileName(m_currentLogPath);
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        // Write directly via QFile, no QTextStream member
    }

    info(QString("Logging started — %1").arg(m_currentLogPath));
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

    // Check if we need to rotate
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
    if (!m_logFile.isOpen()) return;

    QFileInfo fi(m_currentLogPath);
    if (fi.exists() && fi.size() >= m_maxBytes) {
        rotateLogs();
    }
}

void LogManager::rotateLogs() {
    // Close current file
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }

    QDir dir(m_logDir);
    QStringList logFiles = dir.entryList(QStringList() << "ig_sorter_*.log",
                                         QDir::Files, QDir::Name);

    // Delete oldest log if more than 5 exist
    while (logFiles.size() >= 5) {
        QString oldest = dir.filePath(logFiles.first());
        QFile::remove(oldest);
        logFiles.removeFirst();
    }

    // Create new log file
    QString fileName = QString("ig_sorter_%1.log")
                           .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss"));
    m_currentLogPath = dir.filePath(fileName);
    m_logFile.setFileName(m_currentLogPath);
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        // File is ready for writing
    }

    info(QString("Log rotated - new file: %1").arg(m_currentLogPath));
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
