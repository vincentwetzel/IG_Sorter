#pragma once

#include <QString>
class ExtensionFixer {
public:
    // Used by the streaming grouper to fix each file as it is discovered.
    static QString detectFormat(const QString& filePath);
    static bool    needsExtensionFix(const QString& filePath, const QString& detectedFormat);
    static bool    fixFileExtension(const QString& filePath, const QString& correctFormat);
};
