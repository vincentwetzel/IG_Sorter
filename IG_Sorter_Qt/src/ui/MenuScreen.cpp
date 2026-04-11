#include "ui/MenuScreen.h"
#include "utils/ConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileInfo>
#include <QDesktopServices>
#include <QUrl>

MenuScreen::MenuScreen(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(30, 30, 30, 30);

    // Small spacer at top
    mainLayout->addSpacing(20);

    // Title
    m_titleLabel = new QLabel("IG Sorter", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setObjectName("menuTitle");
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(36);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    mainLayout->addWidget(m_titleLabel);

    // Subtitle
    m_subtitleLabel = new QLabel("Organize your image collection", this);
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    m_subtitleLabel->setObjectName("menuSubtitle");
    QFont subFont = m_subtitleLabel->font();
    subFont.setPointSize(16);
    m_subtitleLabel->setFont(subFont);
    mainLayout->addWidget(m_subtitleLabel);

    // Config status
    m_configStatusLabel = new QLabel(this);
    m_configStatusLabel->setAlignment(Qt::AlignCenter);
    m_configStatusLabel->setTextFormat(Qt::RichText);
    m_configStatusLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    QFont configFont = m_configStatusLabel->font();
    configFont.setPointSize(12);
    m_configStatusLabel->setFont(configFont);
    mainLayout->addWidget(m_configStatusLabel);

    // Initial refresh
    refreshConfigStatus();

    // Spacer before buttons
    mainLayout->addSpacing(30);

    // Buttons — centered vertical layout
    auto* buttonLayout = new QVBoxLayout();
    buttonLayout->setSpacing(16);
    buttonLayout->setAlignment(Qt::AlignCenter);

    m_startButton = new QPushButton("Start Sorting", this);
    m_startButton->setMinimumSize(320, 70);
    m_startButton->setMaximumSize(400, 80);
    QFont btnFont = m_startButton->font();
    btnFont.setPointSize(18);
    btnFont.setBold(true);
    m_startButton->setFont(btnFont);
    m_startButton->setObjectName("startButton");
    buttonLayout->addWidget(m_startButton);

    m_cleanUpAccountsButton = new QPushButton("Clean Up Accounts", this);
    m_cleanUpAccountsButton->setMinimumSize(320, 70);
    m_cleanUpAccountsButton->setMaximumSize(400, 80);
    m_cleanUpAccountsButton->setFont(btnFont);
    m_cleanUpAccountsButton->setObjectName("cleanUpAccountsButton");
    buttonLayout->addWidget(m_cleanUpAccountsButton);

    m_findDuplicatesButton = new QPushButton("Find Duplicates", this);
    m_findDuplicatesButton->setMinimumSize(320, 70);
    m_findDuplicatesButton->setMaximumSize(400, 80);
    m_findDuplicatesButton->setFont(btnFont);
    m_findDuplicatesButton->setObjectName("findDuplicatesButton");
    buttonLayout->addWidget(m_findDuplicatesButton);

    m_settingsButton = new QPushButton("Settings", this);
    m_settingsButton->setMinimumSize(320, 70);
    m_settingsButton->setMaximumSize(400, 80);
    m_settingsButton->setFont(btnFont);
    m_settingsButton->setObjectName("settingsButton");
    buttonLayout->addWidget(m_settingsButton);

    mainLayout->addLayout(buttonLayout);

    // Small spacer at bottom
    mainLayout->addStretch();

    // Connect signals
    connect(m_startButton, &QPushButton::clicked,
            this, &MenuScreen::startSortingClicked);
    connect(m_cleanUpAccountsButton, &QPushButton::clicked,
            this, &MenuScreen::cleanUpAccountsClicked);
    connect(m_findDuplicatesButton, &QPushButton::clicked,
            this, &MenuScreen::findDuplicatesClicked);
    connect(m_settingsButton, &QPushButton::clicked,
            this, &MenuScreen::settingsClicked);
    connect(m_configStatusLabel, &QLabel::linkActivated,
            this, &MenuScreen::openSourceFolder);
}

void MenuScreen::refreshConfigStatus() {
    QString sourceFolder = ConfigManager::instance()->sourceFolder();
    QString statusText;
    if (sourceFolder.isEmpty() || !QFileInfo::exists(sourceFolder)) {
        statusText = "⚠ No source folder configured. Click Settings to begin.";
    } else {
        statusText = QString("Source: <a href=\"open://%1\">%2</a>")
                         .arg(sourceFolder, sourceFolder);
    }
    m_configStatusLabel->setText(statusText);
}

void MenuScreen::openSourceFolder() {
    QString sourceFolder = ConfigManager::instance()->sourceFolder();
    if (!sourceFolder.isEmpty() && QFileInfo::exists(sourceFolder)) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(sourceFolder));
    }
}
