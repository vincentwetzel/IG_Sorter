#pragma once

#include <QImage>
#include <QString>

// Decode a WebP image using Windows Imaging Component (WIC).
// Returns a null QImage if decoding fails or WIC doesn't support WebP.
QImage decodeWebpViaWic(const QString& filePath);
