#include "ui/SortPanel.h"
#include "utils/ConfigManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLayoutItem>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QCompleter>
#include <QStringListModel>
#include <QKeyEvent>
#include <QDesktopServices>
#include <QUrl>

SortPanel::SortPanel(QWidget* parent)
    : QWidget(parent), m_isKnown(true), m_accountType(AccountType::Personal),
      m_db(nullptr)
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(10);
    m_mainLayout->setContentsMargins(10, 10, 10, 10);

    // Name completer
    m_completerModel = new QStringListModel(this);
    m_completer = new QCompleter(m_completerModel, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setWrapAround(false);

    // Selected count
    m_selectedCountLabel = new QLabel("0 files selected", this);
    m_selectedCountLabel->setAlignment(Qt::AlignCenter);
    QFont countFont = m_selectedCountLabel->font();
    countFont.setPointSize(12);
    m_selectedCountLabel->setFont(countFont);
    m_mainLayout->addWidget(m_selectedCountLabel);

    // Folder buttons
    m_folderButtonsLayout = new QHBoxLayout();
    m_folderButtonsLayout->setSpacing(20);
    m_folderButtonsLayout->addStretch();
    m_mainLayout->addLayout(m_folderButtonsLayout);

    // Skip button
    m_skipButton = new QPushButton("Skip Batch", this);
    m_skipButton->setMinimumHeight(40);
    QFont btnFont = m_skipButton->font();
    btnFont.setPointSize(12);
    m_skipButton->setFont(btnFont);
    m_mainLayout->addWidget(m_skipButton);

    connect(m_skipButton, &QPushButton::clicked,
            this, &SortPanel::skipClicked);

    // IRL name display row (label + Open Instagram button)
    auto* irlRowLayout = new QHBoxLayout();
    irlRowLayout->setSpacing(10);
    m_irlNameLabel = new QLabel(this);
    m_irlNameLabel->setAlignment(Qt::AlignCenter);
    QFont irlFont = m_irlNameLabel->font();
    irlFont.setPointSize(16);
    irlFont.setBold(true);
    m_irlNameLabel->setFont(irlFont);
    irlRowLayout->addWidget(m_irlNameLabel, 1);

    m_openInstagramButton = new QPushButton("Open Instagram", this);
    irlRowLayout->addWidget(m_openInstagramButton);
    m_openInstagramButton->hide();
    m_mainLayout->addLayout(irlRowLayout);

    // Unknown account widget
    m_unknownAccountWidget = new QWidget(this);
    auto* unknownLayout = new QHBoxLayout(m_unknownAccountWidget);
    unknownLayout->setContentsMargins(0, 0, 0, 0);

    m_unknownAccountLabel = new QLabel("Unknown account:", m_unknownAccountWidget);
    unknownLayout->addWidget(m_unknownAccountLabel);

    m_unknownNameEdit = new QLineEdit(m_unknownAccountWidget);
    m_unknownNameEdit->setPlaceholderText("Enter IRL name...");
    m_unknownNameEdit->setCompleter(m_completer);
    unknownLayout->addWidget(m_unknownNameEdit);

    // Install event filter for Tab completion
    m_unknownNameEdit->installEventFilter(this);

    m_unknownTypeCombo = new QComboBox(m_unknownAccountWidget);
    m_unknownTypeCombo->addItem("Personal");
    m_unknownTypeCombo->addItem("Curator");
    m_unknownTypeCombo->addItem("IRL Only");
    unknownLayout->addWidget(m_unknownTypeCombo);

    m_unknownAddButton = new QPushButton("Add", m_unknownAccountWidget);
    unknownLayout->addWidget(m_unknownAddButton);

    m_mainLayout->addWidget(m_unknownAccountWidget);

    connect(m_unknownAddButton, &QPushButton::clicked, this, [this]() {
        QString irlName = m_unknownNameEdit->text().trimmed();
        if (irlName.isEmpty()) return;

        if (m_accountType == AccountType::Curator) {
            // Curator: just use this name for the current batch — no DB change
            emit curatorResolvedName(irlName);
        } else {
            // Unknown personal: add to database
            AccountType type = AccountType::Personal;
            switch (m_unknownTypeCombo->currentIndex()) {
            case 1: type = AccountType::Curator; break;
            case 2: type = AccountType::IrlOnly; break;
            default: type = AccountType::Personal; break;
            }
            emit addUnknownAccount(m_accountHandle, irlName, type);
        }
    });

    connect(m_openInstagramButton, &QPushButton::clicked, this, [this]() {
        if (!m_accountHandle.isEmpty()) {
            QString url = "https://www.instagram.com/" + m_accountHandle;
            QDesktopServices::openUrl(QUrl(url));
        }
    });

    // Initially hide unknown widget
    m_unknownAccountWidget->hide();
}

void SortPanel::setOutputFolders(const QVector<OutputFolderConfig>& folders) {
    m_outputFolders = folders;
    rebuildFolderButtons();
}

void SortPanel::setDatabaseManager(DatabaseManager* db) {
    m_db = db;
    refreshCompleter();
}

void SortPanel::refreshCompleter() {
    if (!m_db) {
        m_completerModel->setStringList(QStringList());
        return;
    }

    QStringList names;
    for (const auto& entry : m_db->allEntries()) {
        if (!entry.irlName.isEmpty() && !names.contains(entry.irlName, Qt::CaseInsensitive)) {
            names.append(entry.irlName);
        }
    }
    names.sort(Qt::CaseInsensitive);
    m_completerModel->setStringList(names);
}

bool SortPanel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == m_unknownNameEdit && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Tab) {
            // Accept the first completion if available
            QStringListModel* model = qobject_cast<QStringListModel*>(m_completer->completionModel());
            if (model && model->rowCount() > 0) {
                // Pick the first match from the filtered completion model
                m_unknownNameEdit->setText(model->stringList().first());
                m_unknownNameEdit->setCursorPosition(m_unknownNameEdit->text().length());
            }
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void SortPanel::setAccountInfo(const QString& accountHandle,
                               const QString& irlName,
                               bool isKnown,
                               AccountType type) {
    m_accountHandle = accountHandle;
    m_irlName = irlName;
    m_isKnown = isKnown;
    m_accountType = type;

    if (type == AccountType::Curator) {
        // Curator accounts post photos of many people — user must identify each batch
        m_irlNameLabel->setText("Curator: " + accountHandle);
        m_unknownAccountWidget->show();
        m_unknownAccountLabel->setText("Who is in these photos?");
        m_unknownNameEdit->setPlaceholderText("Enter IRL name for output files...");
        m_unknownNameEdit->clear();
        m_unknownTypeCombo->hide();
        m_openInstagramButton->show();
        m_unknownAddButton->setText("Sort");
        m_unknownAddButton->setObjectName("curatorSortButton");
    } else if (isKnown && !irlName.isEmpty()) {
        // Personal account with known name — no identification needed
        m_irlNameLabel->setText(irlName);
        m_unknownAccountWidget->hide();
        m_openInstagramButton->hide();
    } else {
        // Unknown personal account — user must add to database
        m_irlNameLabel->setText("Unknown account: " + accountHandle);
        m_unknownAccountWidget->show();
        m_unknownAccountLabel->setText("Unknown: " + accountHandle);
        m_unknownNameEdit->setPlaceholderText("Enter IRL name...");
        m_unknownNameEdit->clear();
        m_unknownTypeCombo->show();
        m_unknownAddButton->setText("Add");
        m_unknownAddButton->setObjectName("");
        m_openInstagramButton->show();
    }
}

void SortPanel::clearSelections() {
    for (auto* btn : m_folderButtons) {
        btn->setChecked(false);
    }
    m_selectedCountLabel->setText("0 files selected");
}

void SortPanel::updateSelectedCount(int count) {
    m_selectedCountLabel->setText(QString("%1 files selected").arg(count));
}

bool SortPanel::isCuratorNameResolved() const {
    return m_accountType == AccountType::Curator && !m_unknownNameEdit->text().trimmed().isEmpty();
}

QString SortPanel::getCuratorResolvedName() const {
    return m_unknownNameEdit->text().trimmed();
}

void SortPanel::rebuildFolderButtons() {
    // Clear existing buttons
    for (auto* btn : m_folderButtons) {
        delete btn;
    }
    m_folderButtons.clear();

    // Clear existing stretches from the layout
    for (int i = m_folderButtonsLayout->count() - 1; i >= 0; --i) {
        QLayoutItem* item = m_folderButtonsLayout->itemAt(i);
        if (item && !item->widget()) {
            // It's a stretch or spacer
            delete m_folderButtonsLayout->takeAt(i);
        }
    }

    m_folderButtonsLayout->addStretch();

    for (int i = 0; i < m_outputFolders.size(); ++i) {
        auto* btn = new QPushButton(m_outputFolders[i].name, this);
        btn->setMinimumHeight(45);
        QFont btnFont = btn->font();
        btnFont.setPointSize(12);
        btn->setFont(btnFont);
        m_folderButtonsLayout->addWidget(btn);
        m_folderButtons.append(btn);

        connect(btn, &QPushButton::clicked, this, [this, i]() {
            emit sortToFolderClicked(i);
        });
    }

    m_folderButtonsLayout->addStretch();
}
