#include "ui/MenuScreen.h"
#include "utils/ConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileInfo>

MenuScreen::MenuScreen(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(40, 60, 40, 40);

    // Spacer at top
    mainLayout->addStretch();

    // Title
    m_titleLabel = new QLabel("IG Sorter", this);
    m_titleLabel->setAlignment(Qt::AlignCenter);
    m_titleLabel->setObjectName("menuTitle");
    QFont titleFont = m_titleLabel->font();
    titleFont.setPointSize(28);
    titleFont.setBold(true);
    m_titleLabel->setFont(titleFont);
    mainLayout->addWidget(m_titleLabel);

    // Subtitle
    m_subtitleLabel = new QLabel("Organize your image collection", this);
    m_subtitleLabel->setAlignment(Qt::AlignCenter);
    m_subtitleLabel->setObjectName("menuSubtitle");
    QFont subFont = m_subtitleLabel->font();
    subFont.setPointSize(14);
    m_subtitleLabel->setFont(subFont);
    mainLayout->addWidget(m_subtitleLabel);

    // Spacer
    mainLayout->addSpacing(40);

    // Config status
    m_configStatusLabel = new QLabel(this);
    m_configStatusLabel->setAlignment(Qt::AlignCenter);
    QFont configFont = m_configStatusLabel->font();
    configFont.setPointSize(11);
    m_configStatusLabel->setFont(configFont);
    mainLayout->addWidget(m_configStatusLabel);

    // Initial refresh
    refreshConfigStatus();

    // Spacer
    mainLayout->addSpacing(40);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(20);

    m_startButton = new QPushButton("Start Sorting", this);
    m_startButton->setMinimumSize(200, 50);
    QFont btnFont = m_startButton->font();
    btnFont.setPointSize(14);
    m_startButton->setFont(btnFont);
    buttonLayout->addWidget(m_startButton);

    m_settingsButton = new QPushButton("Settings", this);
    m_settingsButton->setMinimumSize(200, 50);
    m_settingsButton->setFont(btnFont);
    buttonLayout->addWidget(m_settingsButton);

    mainLayout->addLayout(buttonLayout);

    // Spacer at bottom
    mainLayout->addStretch();

    // Connect signals
    connect(m_startButton, &QPushButton::clicked,
            this, &MenuScreen::startSortingClicked);
    connect(m_settingsButton, &QPushButton::clicked,
            this, &MenuScreen::settingsClicked);
}

void MenuScreen::refreshConfigStatus() {
    QString sourceFolder = ConfigManager::instance()->sourceFolder();
    QString statusText;
    if (sourceFolder.isEmpty() || !QFileInfo::exists(sourceFolder)) {
        statusText = "⚠ No source folder configured. Click Settings to begin.";
    } else {
        statusText = QString("Source: %1").arg(sourceFolder);
    }
    m_configStatusLabel->setText(statusText);
}
