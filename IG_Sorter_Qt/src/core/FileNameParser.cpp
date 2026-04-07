#include "core/FileNameParser.h"
#include <QRegularExpression>
#include <QFileInfo>

ParsedResult FileNameParser::parse(const QString& filePath) {
    ParsedResult result;

    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();

    // Instaloader pattern: account_YYYY-MM-DD_HH-MM-SS_N.ext
    // e.g. "joeygore1_2026-03-13_15-30-00_1.jpg"
    QRegularExpression instaloaderRegex(
        R"((^.+?)(?=_\d{4}-\d{2}-\d{2})_(\d{4}-\d{2}-\d{2})_(\d{2}-\d{2}-\d{2})_(\d+))");

    QRegularExpressionMatch match = instaloaderRegex.match(fileName);
    if (match.hasMatch()) {
        result.accountHandle = match.captured(1);
        result.postDate = match.captured(2);
        result.postTime = match.captured(3);
        result.postTimestamp = result.postDate + "_" + result.postTime;
        result.sequenceNumber = match.captured(4).toInt();
        result.sourceType = "instaloader";
        result.matched = true;
    }

    return result;
}
