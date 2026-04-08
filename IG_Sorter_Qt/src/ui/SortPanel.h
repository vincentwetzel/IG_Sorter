#pragma once

#include <QWidget>
#include <QString>
#include <QVector>
#include <QList>
#include "core/DatabaseManager.h"

class QVBoxLayout;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QLineEdit;
class QComboBox;

struct OutputFolderConfig;

class SortPanel : public QWidget {
    Q_OBJECT
public:
    explicit SortPanel(QWidget* parent = nullptr);

    void setOutputFolders(const QVector<OutputFolderConfig>& folders);
    void setAccountInfo(const QString& accountHandle,
                        const QString& irlName,
                        bool isKnown,
                        AccountType type);
    void clearSelections();
    void updateSelectedCount(int count);

    // Check if curator name has been entered
    bool isCuratorNameResolved() const;
    QString getCuratorResolvedName() const;

signals:
    void sortToFolderClicked(int folderIndex);
    void skipClicked();
    void addUnknownAccount(const QString& account, const QString& irlName, AccountType type);
    void openInstagramClicked(const QString& account);
    // For curator accounts — resolve who is in the photos (no DB change)
    void curatorResolvedName(const QString& irlName);

private:
    void rebuildFolderButtons();

    QVBoxLayout*   m_mainLayout;
    QHBoxLayout*   m_folderButtonsLayout;
    QLabel*        m_irlNameLabel;
    QLabel*        m_selectedCountLabel;

    // Unknown account UI (shown/hidden as needed)
    QWidget*       m_unknownAccountWidget;
    QLineEdit*     m_unknownNameEdit;
    QComboBox*     m_unknownTypeCombo;
    QPushButton*   m_unknownAddButton;
    QPushButton*   m_openInstagramButton;
    QLabel*       m_unknownAccountLabel;

    QVector<OutputFolderConfig> m_outputFolders;
    QList<QPushButton*>         m_folderButtons;
    QPushButton*                m_skipButton;

    QString         m_accountHandle;
    QString         m_irlName;
    bool            m_isKnown;
    AccountType     m_accountType;
};
