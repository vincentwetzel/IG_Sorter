#pragma once

#include <QWidget>
#include <QMap>
#include <QStringList>

class QVBoxLayout;
class QLabel;
class QProgressBar;
class QPushButton;
class QLineEdit;
class QComboBox;
class DatabaseManager;

class CleanupScreen : public QWidget {
    Q_OBJECT
public:
    explicit CleanupScreen(QWidget* parent = nullptr);

    void setDirectories(const QStringList& dirs);
    void updateDirectoryProgress(const QString& dir, int current, int total);
    void showUnresolvedIssues(const QStringList& unknownNames, DatabaseManager* db);
    void enableContinue(bool enabled);

signals:
    void continueClicked();

private:
    QVBoxLayout*         m_layout;
    QLabel*              m_titleLabel;
    QLabel*              m_statusLabel;
    QMap<QString, QProgressBar*> m_progressBars;

    // Unknown names resolution area
    QWidget*             m_resolutionWidget;
    QVBoxLayout*         m_resolutionLayout;
    QMap<QString, QPair<QLineEdit*, QComboBox*>> m_resolutionFields;
    QPushButton*         m_applyResolutionsButton;
    DatabaseManager*     m_pendingDb;
    QStringList          m_pendingUnknownNames;

    QPushButton*         m_continueButton;
};
