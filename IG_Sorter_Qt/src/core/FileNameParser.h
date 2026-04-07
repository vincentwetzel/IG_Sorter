#pragma once

#include <QString>

struct ParsedResult {
    QString accountHandle;    // e.g. "joeygore1"
    QString postDate;         // e.g. "2026-03-13"
    QString postTime;         // e.g. "15-30-00"
    QString postTimestamp;    // Combined: "2026-03-13_15-30-00"
    int     sequenceNumber;   // e.g. 14
    QString sourceType;       // e.g. "instaloader"
    bool    matched;          // true if any regex matched

    ParsedResult() : sequenceNumber(-1), sourceType("unknown"), matched(false) {}
};

class FileNameParser {
public:
    static ParsedResult parse(const QString& filePath);
};
