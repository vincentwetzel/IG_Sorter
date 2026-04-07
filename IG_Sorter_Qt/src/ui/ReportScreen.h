#pragma once

#include <QWidget>
#include <QString>
#include "core/Types.h"

class QVBoxLayout;
class QLabel;
class QPushButton;
class QTextEdit;

class ReportScreen : public QWidget {
    Q_OBJECT
public:
    explicit ReportScreen(QWidget* parent = nullptr);

    void setReport(const SortReportData& report);

signals:
    void doneClicked();
    void settingsClicked();

private:
    QVBoxLayout* m_layout;
    QLabel*      m_titleLabel;
    QTextEdit*   m_reportText;
    QPushButton* m_doneButton;
    QPushButton* m_settingsButton;
};
