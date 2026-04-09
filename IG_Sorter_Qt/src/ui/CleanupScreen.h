#pragma once

#include <QWidget>
#include <QMap>
#include <QStringList>

class QVBoxLayout;
class QHBoxLayout;
class QLabel;
class QProgressBar;
class QPushButton;
class QLineEdit;
class QComboBox;
class DatabaseManager;

// Single row for resolving one unknown name
class ResolutionRow : public QWidget {
    Q_OBJECT
public:
    ResolutionRow(const QString& irlName, DatabaseManager* db,
                  QVBoxLayout* parentLayout, QWidget* parent = nullptr);
    void apply();
    QString irlName() const { return m_irlName; }

signals:
    void resolved();

private:
    QString         m_irlName;
    DatabaseManager* m_db;
    QVBoxLayout*    m_parentLayout;
    QLineEdit*      m_accountEdit;
    QComboBox*      m_typeCombo;
    QPushButton*    m_applyButton;
};

class CleanupScreen : public QWidget {
    Q_OBJECT
public:
    explicit CleanupScreen(QWidget* parent = nullptr);

    void setDirectories(const QStringList& dirs);
    void setStatusText(const QString& text);
    void updateDirectoryProgress(const QString& dir, int current, int total);
    void setDirectoryRenamed(const QString& dir, int filesRenamed);
    void showUnresolvedIssues(const QStringList& unknownNames, DatabaseManager* db);
    void enableContinue(bool enabled);

signals:
    void continueClicked();
    void menuClicked();

private:
    QVBoxLayout*         m_layout;
    QLabel*              m_titleLabel;
    QLabel*              m_statusLabel;
    QMap<QString, QLabel*> m_pathLabels;
    QMap<QString, QLabel*> m_progressLabels;
    QMap<QString, QProgressBar*> m_progressBars;

    // Unknown names resolution area
    QWidget*             m_resolutionWidget;
    QVBoxLayout*         m_resolutionLayout;
    QList<ResolutionRow*> m_resolutionRows;
    DatabaseManager*     m_pendingDb;
    QStringList          m_pendingUnknownNames;

    QHBoxLayout*         m_buttonLayout;
    QPushButton*         m_menuButton;
    QPushButton*         m_continueButton;
};
