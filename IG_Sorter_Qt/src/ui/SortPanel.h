#pragma once

#include <QWidget>
#include <QString>
#include <QVector>
#include <QList>
#include "core/DatabaseManager.h"

class QCompleter;
class QStringListModel;

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

    void setDatabaseManager(DatabaseManager* db);

    // Refresh the name completer from the database (call after DB changes)
    void refreshCompleter();

    // Explicitly clear the name input field (used when loading new batches)
    void clearNameInput();

    // Set the name input field to a specific value
    void setNameInput(const QString& name);

    // Check if curator name has been entered
    bool isCuratorNameResolved() const;
    QString getCuratorResolvedName() const;

    // Get the type combo index for unknown accounts
    int getUnknownAccountTypeIndex() const;

    // Set quick-fill name buttons for IrlOnly sources (top N names)
    void setQuickFillNames(const QStringList& names);

    // Update the select all button text based on current selection state
    void updateSelectAllButtonText(bool allSelected);

signals:
    void sortToFolderClicked(int folderIndex);
    void skipClicked();
    void deleteSelectedClicked();
    void selectAllClicked(bool selectAll);
    void addUnknownAccount(const QString& account, const QString& irlName, AccountType type);
    void openInstagramClicked(const QString& account);
    // For curator accounts — resolve who is in the photos (no DB change)
    void curatorResolvedName(const QString& irlName);
    // Favorite name button clicked
    void favoriteNameSelected(const QString& name);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuildFolderButtons();
    void rebuildFavoriteButtons();
    void trimOverflowFavoriteButtons();

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

    // Quick-fill name buttons for IrlOnly sources
    QWidget*       m_favoritesWidget;
    QHBoxLayout*   m_favoritesLayout;
    QList<QPushButton*> m_favoriteButtons;

    QVector<OutputFolderConfig> m_outputFolders;
    QList<QPushButton*>         m_folderButtons;
    QPushButton*                m_selectAllButton;
    QPushButton*                m_skipButton;
    QPushButton*                m_deleteButton;

    QString         m_accountHandle;
    QString         m_irlName;
    bool            m_isKnown;
    AccountType     m_accountType;
    DatabaseManager* m_db;
    QCompleter*      m_completer;
    QStringListModel* m_completerModel;
    // Full unfiltered list of display strings: "Sean Dawn (eurotunnell)"
    QStringList m_allCompleterEntries;
    // Parallel list of IRL names (what gets inserted on selection)
    QStringList m_allIrlNames;
    bool m_allSelected;  // updated externally via updateSelectAllButtonText
};
