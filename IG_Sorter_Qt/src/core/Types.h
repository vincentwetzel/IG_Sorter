#pragma once

#include <QString>
#include <QStringList>
#include <QMap>

struct SortReportData {
    int filesSorted = 0;
    int filesSkipped = 0;
    int errors = 0;
    QStringList errorMessages;
    QMap<QString, int> directoryFileCounts;  // dir name -> file count
    QList<QString> newAccountsAdded;         // "account → name (type)"
    QMap<QString, int> filesByAccountType;   // "Personal" / "Curator" -> count
};
