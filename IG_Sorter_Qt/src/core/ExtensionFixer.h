#pragma once

#include <QString>
#include <QStringList>

struct ExtensionFixReport {
    int totalFilesScanned = 0;
    int filesRenamed = 0;
    int filesSkipped = 0;
    QStringList renamedFiles;   // "oldName.ext → newName.ext"
    QStringList errors;
};

class ExtensionFixer {
public:
    // Scan a directory and fix files with wrong extensions
    static ExtensionFixReport run(const QString& sourceDir);

private:
    static QString detectFormat(const QString& filePath);
    static bool    needsExtensionFix(const QString& filePath, const QString& detectedFormat);
    static bool    fixFileExtension(const QString& filePath, const QString& correctFormat,
                                     ExtensionFixReport& report);
};
