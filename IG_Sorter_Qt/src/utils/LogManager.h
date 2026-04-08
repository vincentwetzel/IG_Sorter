#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QMutex>

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class LogManager : public QObject {
    Q_OBJECT
public:
    explicit LogManager(QObject* parent = nullptr);
    static LogManager* instance();

    // Start logging to a file in the given directory
    // Creates timestamped log files and rotates when size exceeds maxBytes
    void start(const QString& logDir, int maxBytes = 5 * 1024 * 1024);

    // Log a message at the given level
    void log(LogLevel level, const QString& message);
    void debug(const QString& message);
    void info(const QString& message);
    void warning(const QString& message);
    void error(const QString& message);

    // Convenience methods for common operations
    void logFileMoved(const QString& src, const QString& dest, qint64 sizeBytes);
    void logFileDeleted(const QString& path);
    void logFileRenamed(const QString& oldPath, const QString& newPath);
    void logFileSkipped(const QString& path, const QString& reason);
    void logError(const QString& path, const QString& error);
    void logDirectoryCleaned(const QString& dir, int filesRenamed);
    void logSortComplete(int sorted, int skipped, int errors);

    // Get the current log file path (useful for displaying to user)
    QString currentLogFile() const;

private:
    static LogManager* s_instance;

    void rotateIfNeeded();
    void rotateLogs();
    QString timestamp() const;
    QString levelString(LogLevel level) const;

    QString        m_logDir;
    QString        m_currentLogPath;
    QFile          m_logFile;
    QTextStream    m_logStream;
    QMutex         m_mutex;
    int            m_maxBytes;
};
