#include "core/FileNameParser.h"
#include <QRegularExpression>
#include <QFileInfo>
#include <QDateTime>

// Extract timestamp from Android screenshot filename
// Pattern: Screenshot_YYYYMMDD_HHMMSS_App.ext
static bool parseAndroidScreenshot(const QString& fileName,
                                    ParsedResult& result,
                                    const QString& appLabel,
                                    SourceType sourceEnum,
                                    const QString& accountLabel) {
    QRegularExpression androidRegex(
        "^Screenshot_(\\d{4})(\\d{2})(\\d{2})_(\\d{2})(\\d{2})(\\d{2})_"
        + QRegularExpression::escape(appLabel) + "\\.");
    QRegularExpressionMatch androidMatch = androidRegex.match(fileName);
    if (androidMatch.hasMatch()) {
        result.accountHandle = accountLabel;
        result.postDate = QString("%1-%2-%3")
            .arg(androidMatch.captured(1))
            .arg(androidMatch.captured(2))
            .arg(androidMatch.captured(3));
        result.postTime = QString("%1-%2-%3")
            .arg(androidMatch.captured(4))
            .arg(androidMatch.captured(5))
            .arg(androidMatch.captured(6));
        result.postTimestamp = result.postDate + "_" + result.postTime;
        result.sequenceNumber = -1;
        result.sourceType = "android_screenshot_" + appLabel.toLower();
        result.sourceEnum = sourceEnum;
        result.matched = true;
        return true;
    }
    return false;
}

ParsedResult FileNameParser::parse(const QString& filePath) {
    ParsedResult result;

    QFileInfo fileInfo(filePath);
    QString fileName = fileInfo.fileName();

    // Instaloader pattern: account_YYYY-MM-DD_HH-MM-SS_N.ext OR account_YYYY-MM-DD_HH-MM-SS.ext
    // e.g. "joeygore1_2026-03-13_15-30-00_1.jpg" or "ben_2025-01-01_12-00-00.jpg"
    // e.g. "_luke.bowman__2023-07-20_18-45-50_1.jpg" (accounts with underscores)
    // Capture groups: (1)=account, (2)=date, (3)=time, (4)=sequence (may be empty)
    // Note: .* at end required because Qt's match() does full-string matching
    // Use greedy (.+) to capture account handles that may contain underscores
    QRegularExpression instaloaderRegex(
        "(.+)(?=_"
        "\\d{4}-\\d{2}-\\d{2})_"
        "(\\d{4}-\\d{2}-\\d{2})_"
        "(\\d{2}-\\d{2}-\\d{2})"
        "(?:_(\\d+))?"
        ".*");

    QRegularExpressionMatch match = instaloaderRegex.match(fileName);
    if (match.hasMatch()) {
        result.accountHandle = match.captured(1);
        result.postDate = match.captured(2);
        result.postTime = match.captured(3);
        result.postTimestamp = result.postDate + "_" + result.postTime;
        result.sequenceNumber = match.captured(4).toInt();
        result.sourceType = "instaloader";
        result.sourceEnum = SourceType::Instagram;
        result.matched = true;
    } else {
        // TikTok slideshow pattern: 32-char hex string.ext
        // e.g. "6bf415004e6f7b395e9b3b14963a6e51.webp"
        QRegularExpression tiktokRegex("^([0-9a-fA-F]{32})\\.");
        QRegularExpressionMatch tiktokMatch = tiktokRegex.match(fileName);
        if (tiktokMatch.hasMatch()) {
            result.accountHandle = "TikTok Slideshow";
            result.postTimestamp = "";
            result.sequenceNumber = -1;
            result.sourceType = "tiktok_slideshow";
            result.sourceEnum = SourceType::TikTokSlideshow;
            result.matched = true;
        } else {
            // Facebook download pattern: FB_IMG_<unix_timestamp>.ext
            // e.g. "FB_IMG_1752582112941.jpg"
            QRegularExpression fbRegex("^FB_IMG_(\\d{13})\\.");
            QRegularExpressionMatch fbMatch = fbRegex.match(fileName);
            if (fbMatch.hasMatch()) {
                result.accountHandle = "Facebook";
                // Convert millisecond Unix timestamp to readable date
                qint64 ms = fbMatch.captured(1).toLongLong();
                QDateTime dt = QDateTime::fromMSecsSinceEpoch(ms);
                result.postTimestamp = dt.toString("yyyy-MM-dd_hh-mm-ss");
                result.sequenceNumber = -1;
                result.sourceType = "facebook";
                result.sourceEnum = SourceType::Facebook;
                result.matched = true;
            } else {
                // Twitter/X download pattern: YYYYMMDD_HHMMSS.ext
                // e.g. "20250902_193054.jpg"
                QRegularExpression twitterRegex(
                    "^(\\d{4})(\\d{2})(\\d{2})_(\\d{2})(\\d{2})(\\d{2})\\.");
                QRegularExpressionMatch twitterMatch = twitterRegex.match(fileName);
                if (twitterMatch.hasMatch()) {
                    result.accountHandle = "Twitter";
                    result.postDate = QString("%1-%2-%3")
                        .arg(twitterMatch.captured(1))
                        .arg(twitterMatch.captured(2))
                        .arg(twitterMatch.captured(3));
                    result.postTime = QString("%1-%2-%3")
                        .arg(twitterMatch.captured(4))
                        .arg(twitterMatch.captured(5))
                        .arg(twitterMatch.captured(6));
                    result.postTimestamp = result.postDate + "_" + result.postTime;
                    result.sequenceNumber = -1;
                    result.sourceType = "twitter";
                    result.sourceEnum = SourceType::Twitter;
                    result.matched = true;
                }
            }
        }
    }

    // Android screenshot patterns: Screenshot_YYYYMMDD_HHMMSS_App.ext
    if (!result.matched) {
        if (parseAndroidScreenshot(fileName, result, "Instagram",
                                     SourceType::AndroidScreenshot_Instagram,
                                     "Instagram")) {
            return result;
        }
        if (parseAndroidScreenshot(fileName, result, "Snapchat",
                                     SourceType::AndroidScreenshot_Snapchat,
                                     "Snapchat")) {
            return result;
        }
        if (parseAndroidScreenshot(fileName, result, "Chrome",
                                     SourceType::AndroidScreenshot_Chrome,
                                     "Chrome")) {
            return result;
        }
    }

    return result;
}
