#pragma once

#include <QDialog>
#include <QVector>

class QVBoxLayout;
class QTableWidget;
class QPushButton;
class QLineEdit;
class QSpinBox;
class QComboBox;

struct OutputFolderConfig;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    void loadSettings();

private:
    void setupUI();
    void saveSettings();

    QVBoxLayout*   m_layout;
    QLineEdit*     m_sourceFolderEdit;
    QPushButton*   m_sourceBrowseButton;
    QTableWidget*  m_outputFoldersTable;
    QPushButton*   m_addFolderButton;
    QPushButton*   m_removeFolderButton;
    QLineEdit*     m_databaseFileEdit;
    QPushButton*   m_databaseBrowseButton;
    QSpinBox*      m_batchSizeSpin;
    QComboBox*     m_themeCombo;
    QPushButton*   m_saveButton;
    QPushButton*   m_cancelButton;
};
