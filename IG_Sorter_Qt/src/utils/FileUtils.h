#pragma once

#include <QString>

class FileUtils {
public:
    // Safely move a file from sourcePath to destDir/destFileName.
    // Uses copy-verify-delete with temp files to prevent data loss.
    // Returns the final destination path on success, empty string on failure.
    static QString safeMove(const QString& sourcePath,
                            const QString& destDir,
                            const QString& destFileName,
                            QString* errorMsg = nullptr);

    // Get the next available sequential name for a file in a directory.
    // e.g. "Joey Gore 1", ".jpg" → "Joey Gore 5" if 1-4 exist
    static QString nextAvailableName(const QString& dir,
                                     const QString& baseName,
                                     const QString& ext);
};
