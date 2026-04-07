#include "ui/SettingsDialog.h"
#include "utils/ConfigManager.h"
#include "utils/ThemeManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QPushButton>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QFileDialog>
#include <QLabel>

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Settings");
    setMinimumSize(600, 500);
    setupUI();
    loadSettings();
}

void SettingsDialog::setupUI() {
    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(15);

    // Source folder
    auto* sourceLayout = new QHBoxLayout();
    sourceLayout->addWidget(new QLabel("Source Folder:", this));
    m_sourceFolderEdit = new QLineEdit(this);
    m_sourceFolderEdit->setReadOnly(true);
    sourceLayout->addWidget(m_sourceFolderEdit);
    m_sourceBrowseButton = new QPushButton("Browse...", this);
    sourceLayout->addWidget(m_sourceBrowseButton);
    m_layout->addLayout(sourceLayout);

    connect(m_sourceBrowseButton, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Source Folder");
        if (!dir.isEmpty()) {
            m_sourceFolderEdit->setText(dir);
        }
    });

    // Output folders
    m_layout->addWidget(new QLabel("Output Folders:", this));
    m_outputFoldersTable = new QTableWidget(0, 2, this);
    m_outputFoldersTable->setHorizontalHeaderLabels({"Name", "Path"});
    m_outputFoldersTable->horizontalHeader()->setStretchLastSection(true);
    m_outputFoldersTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_layout->addWidget(m_outputFoldersTable);

    auto* folderButtonLayout = new QHBoxLayout();
    m_addFolderButton = new QPushButton("Add", this);
    m_removeFolderButton = new QPushButton("Remove", this);
    folderButtonLayout->addWidget(m_addFolderButton);
    folderButtonLayout->addWidget(m_removeFolderButton);
    folderButtonLayout->addStretch();
    m_layout->addLayout(folderButtonLayout);

    connect(m_addFolderButton, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "Select Output Folder");
        if (!dir.isEmpty()) {
            int row = m_outputFoldersTable->rowCount();
            m_outputFoldersTable->insertRow(row);

            QFileInfo fi(dir);
            m_outputFoldersTable->setItem(row, 0, new QTableWidgetItem(fi.fileName()));
            m_outputFoldersTable->setItem(row, 1, new QTableWidgetItem(dir));
        }
    });

    connect(m_removeFolderButton, &QPushButton::clicked, this, [this]() {
        int row = m_outputFoldersTable->currentRow();
        if (row >= 0) {
            m_outputFoldersTable->removeRow(row);
        }
    });

    // Database file
    auto* dbLayout = new QHBoxLayout();
    dbLayout->addWidget(new QLabel("Database File:", this));
    m_databaseFileEdit = new QLineEdit(this);
    m_databaseFileEdit->setReadOnly(true);
    dbLayout->addWidget(m_databaseFileEdit);
    m_databaseBrowseButton = new QPushButton("Browse...", this);
    dbLayout->addWidget(m_databaseBrowseButton);
    m_layout->addLayout(dbLayout);

    connect(m_databaseBrowseButton, &QPushButton::clicked, this, [this]() {
        QString file = QFileDialog::getOpenFileName(this, "Select Database File",
                                                     QString(), "JSON Files (*.json)");
        if (!file.isEmpty()) {
            m_databaseFileEdit->setText(file);
        }
    });

    // Batch size
    auto* batchLayout = new QHBoxLayout();
    batchLayout->addWidget(new QLabel("Batch Size:", this));
    m_batchSizeSpin = new QSpinBox(this);
    m_batchSizeSpin->setRange(1, 20);
    m_batchSizeSpin->setValue(5);
    batchLayout->addWidget(m_batchSizeSpin);
    batchLayout->addStretch();
    m_layout->addLayout(batchLayout);

    // Theme
    auto* themeLayout = new QHBoxLayout();
    themeLayout->addWidget(new QLabel("Theme:", this));
    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItem("Light");
    m_themeCombo->addItem("Dark");
    m_themeCombo->addItem("System");
    themeLayout->addWidget(m_themeCombo);
    themeLayout->addStretch();
    m_layout->addLayout(themeLayout);

    // Save/Cancel
    m_layout->addSpacing(20);
    auto* bottomLayout = new QHBoxLayout();
    bottomLayout->addStretch();
    m_saveButton = new QPushButton("Save", this);
    m_cancelButton = new QPushButton("Cancel", this);
    m_saveButton->setMinimumWidth(100);
    m_cancelButton->setMinimumWidth(100);
    bottomLayout->addWidget(m_saveButton);
    bottomLayout->addWidget(m_cancelButton);
    m_layout->addLayout(bottomLayout);

    connect(m_saveButton, &QPushButton::clicked, this, [this]() {
        saveSettings();
        accept();
    });
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void SettingsDialog::loadSettings() {
    auto* config = ConfigManager::instance();
    m_sourceFolderEdit->setText(config->sourceFolder());
    m_databaseFileEdit->setText(config->databaseFile());
    m_batchSizeSpin->setValue(config->batchSize());

    QString theme = config->theme();
    if (theme == "light") m_themeCombo->setCurrentIndex(0);
    else if (theme == "dark") m_themeCombo->setCurrentIndex(1);
    else m_themeCombo->setCurrentIndex(2);

    // Output folders
    m_outputFoldersTable->setRowCount(0);
    auto folders = config->outputFolders();
    for (const auto& folder : folders) {
        int row = m_outputFoldersTable->rowCount();
        m_outputFoldersTable->insertRow(row);
        m_outputFoldersTable->setItem(row, 0, new QTableWidgetItem(folder.name));
        m_outputFoldersTable->setItem(row, 1, new QTableWidgetItem(folder.path));
    }
}

void SettingsDialog::saveSettings() {
    auto* config = ConfigManager::instance();
    config->setSourceFolder(m_sourceFolderEdit->text());
    config->setDatabaseFile(m_databaseFileEdit->text());
    config->setBatchSize(m_batchSizeSpin->value());

    // Theme is managed exclusively by ThemeManager
    Theme theme;
    switch (m_themeCombo->currentIndex()) {
    case 0: theme = Theme::Light; break;
    case 1: theme = Theme::Dark; break;
    default: theme = Theme::System; break;
    }
    ThemeManager::instance()->setTheme(theme);

    // Output folders
    QVector<OutputFolderConfig> folders;
    for (int i = 0; i < m_outputFoldersTable->rowCount(); ++i) {
        OutputFolderConfig folder;
        if (m_outputFoldersTable->item(i, 0)) {
            folder.name = m_outputFoldersTable->item(i, 0)->text();
        }
        if (m_outputFoldersTable->item(i, 1)) {
            folder.path = m_outputFoldersTable->item(i, 1)->text();
        }
        if (!folder.name.isEmpty() && !folder.path.isEmpty()) {
            folders.append(folder);
        }
    }
    config->setOutputFolders(folders);
    config->save();
}
