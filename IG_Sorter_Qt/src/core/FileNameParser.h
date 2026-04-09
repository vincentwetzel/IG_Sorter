#pragma once

#include <QString>
#include "core/DatabaseManager.h"

struct ParsedResult {
    QString accountHandle;    // e.g. "joeygore1"
    QString postDate;         // e.g. "2026-03-13"
    QString postTime;         // e.g. "15-30-00"
    QString postTimestamp;    // Combined: "2026-03-13_15-30-00"
    int     sequenceNumber;   // e.g. 14
    QString sourceType;       // e.g. "instaloader", "tiktok_slideshow"
    SourceType sourceEnum;    // Typed source enum
    bool    matched;          // true if any regex matched

    ParsedResult() : sequenceNumber(-1), sourceType("unknown"), sourceEnum(SourceType::Unknown), matched(false) {}
};

class FileNameParser {
public:
    static ParsedResult parse(const QString& filePath);
};
