#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

class MenuScreen : public QWidget {
    Q_OBJECT
public:
    explicit MenuScreen(QWidget* parent = nullptr);

    void refreshConfigStatus();

signals:
    void startSortingClicked();
    void settingsClicked();
    void cleanUpAccountsClicked();

private:
    QLabel*     m_titleLabel;
    QLabel*     m_subtitleLabel;
    QLabel*     m_configStatusLabel;
    QPushButton* m_startButton;
    QPushButton* m_cleanUpAccountsButton;
    QPushButton* m_settingsButton;

private slots:
    void openSourceFolder();
};
