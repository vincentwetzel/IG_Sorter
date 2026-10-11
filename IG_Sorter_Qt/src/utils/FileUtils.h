#pragma once

#include <QString>
#include <QSet>

class FileUtils {
public:
    // Safely move a file from sourcePath to destDir/destFileName.
    // Uses a native move where available and a verified temporary copy otherwise.
    // Returns the final destination path on success, empty string on failure.
    static QString safeMove(const QString& sourcePath,
                            const QString& destDir,
                            const QString& destFileName,
                            QString* errorMsg = nullptr);

    // Collect occupied file basenames once when sorting a batch.
    static QSet<QString> existingBaseNames(const QString& dir);

    // Get the next available sequential basename, e.g. "Joey Gore 5".
    static QString nextAvailableName(const QSet<QString>& existingBaseNames,
                                     const QString& baseName);
};
