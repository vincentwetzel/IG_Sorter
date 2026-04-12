#include "ui/SortPanel.h"
#include "utils/ConfigManager.h"
#include <algorithm>
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
#include <QPainter>
#include <QStyle>
#include <QStyleOptionFrame>
#include <QAbstractItemView>
#include <QDesktopServices>
#include <QUrl>
#include <QTimer>

// Custom completer that strips "(account)" suffix on insertion
// and treats the entire input as a single token (no word splitting)
class PersonCompleter : public QCompleter {
public:
    using QCompleter::QCompleter;

    QString pathFromIndex(const QModelIndex& index) const override {
        QString text = QCompleter::pathFromIndex(index);
        int parenIdx = text.indexOf('(');
        if (parenIdx > 0) {
            return text.left(parenIdx).trimmed();
        }
        return text;
    }

    // Don't split on underscores — treat entire input as one token
    QStringList splitPath(const QString& path) const override {
        return QStringList(path);
    }
};

// ─── GhostLineEdit ──────────────────────────────────────────────────────────

GhostLineEdit::GhostLineEdit(QWidget* parent)
    : QLineEdit(parent)
{
}

void GhostLineEdit::setGhostText(const QString& ghost) {
    if (m_ghost == ghost) return;
    m_ghost = ghost;
    updateGhostGeometry();
    update();
}

void GhostLineEdit::clearGhost() {
    if (!m_ghost.isEmpty()) {
        m_ghost.clear();
        m_ghostRect = QRect();
        update();
    }
}

void GhostLineEdit::updateGhostGeometry() {
    if (m_ghost.isEmpty() || text().isEmpty()) {
        m_ghostRect = QRect();
        return;
    }

    QFontMetrics fm(font());
    int textWidth = fm.horizontalAdvance(text());

    QStyleOptionFrame opt;
    initStyleOption(&opt);
    QRect textRect = style()->subElementRect(QStyle::SE_LineEditContents, &opt, this);

    int x = textRect.x() + textWidth + 4;
    int y = textRect.y();
    int h = textRect.height();

    m_ghostRect = QRect(x, y, fm.horizontalAdvance(m_ghost) + 4, h);
}

bool GhostLineEdit::event(QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
            emit tabPressed();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Escape) {
            emit escapePressed();
            return true;
        }
    }
    return QLineEdit::event(event);
}

void GhostLineEdit::paintEvent(QPaintEvent* event) {
    QLineEdit::paintEvent(event);

    if (m_ghost.isEmpty() || text().isEmpty() || m_ghostRect.isEmpty()) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QFont ghostFont = font();
    ghostFont.setItalic(true);
    painter.setFont(ghostFont);
    painter.setPen(QColor("#999999"));

    painter.drawText(m_ghostRect, Qt::AlignVCenter | Qt::AlignLeft, m_ghost);
}

void GhostLineEdit::resizeEvent(QResizeEvent* event) {
    QLineEdit::resizeEvent(event);
    if (!m_ghost.isEmpty()) {
        updateGhostGeometry();
    }
}

SortPanel::SortPanel(QWidget* parent)
    : QWidget(parent), m_isKnown(true), m_accountType(AccountType::Personal),
      m_db(nullptr), m_allSelected(false), m_suppressGhost(false)
{
    m_mainLayout = new QVBoxLayout(this);
    m_mainLayout->setSpacing(8);
    m_mainLayout->setContentsMargins(8, 8, 8, 8);

    // Name completer — QStringListModel with "Name (Account)" format
    // PersonCompleter automatically strips "(account)" on insertion
    m_completerModel = new QStringListModel(this);
    m_completer = new PersonCompleter(m_completerModel, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    m_completer->setWrapAround(false);
    // Substring matching (not just prefix)
    m_completer->setFilterMode(Qt::MatchContains);

    // Skip and Delete buttons side by side with selected count
    auto* actionRow = new QHBoxLayout();
    actionRow->setSpacing(8);

    actionRow->addStretch();

    m_skipButton = new QPushButton("Skip Batch", this);
    m_skipButton->setMinimumHeight(36);
    m_skipButton->setFixedWidth(140);
    QFont btnFont = m_skipButton->font();
    btnFont.setPointSize(11);
    m_skipButton->setFont(btnFont);
    actionRow->addWidget(m_skipButton);

    connect(m_skipButton, &QPushButton::clicked,
            this, &SortPanel::skipClicked);

    m_selectAllButton = new QPushButton("Select All", this);
    m_selectAllButton->setMinimumHeight(36);
    m_selectAllButton->setFixedWidth(140);
    m_selectAllButton->setFont(btnFont);
    actionRow->addWidget(m_selectAllButton);

    connect(m_selectAllButton, &QPushButton::clicked, this, [this]() {
        // Use m_allSelected which is updated externally via updateSelectAllButtonText
        emit selectAllClicked(!m_allSelected);
    });

    m_deleteButton = new QPushButton("Delete Selected", this);
    m_deleteButton->setMinimumHeight(36);
    m_deleteButton->setFixedWidth(140);
    m_deleteButton->setFont(btnFont);
    m_deleteButton->setObjectName("deleteButton");
    actionRow->addWidget(m_deleteButton);

    connect(m_deleteButton, &QPushButton::clicked,
            this, &SortPanel::deleteSelectedClicked);

    actionRow->addStretch();

    m_mainLayout->addLayout(actionRow);

    // IRL name display row — label centered on full screen width.
    // The Instagram button sits on the right but does NOT affect the label's centering.
    auto* irlGridLayout = new QGridLayout();
    irlGridLayout->setSpacing(10);
    irlGridLayout->setColumnStretch(0, 1);
    irlGridLayout->setColumnStretch(2, 1);

    m_irlNameLabel = new QLabel(this);
    m_irlNameLabel->setAlignment(Qt::AlignCenter);
    QFont irlFont = m_irlNameLabel->font();
    irlFont.setPointSize(14);
    irlFont.setBold(true);
    m_irlNameLabel->setFont(irlFont);
    // Label spans all 3 columns so it can be centered on the full width
    irlGridLayout->addWidget(m_irlNameLabel, 0, 0, 1, 3);

    // Instagram button in the rightmost cell — doesn't affect label centering
    m_openInstagramButton = new QPushButton("Open Instagram", this);
    irlGridLayout->addWidget(m_openInstagramButton, 0, 2, Qt::AlignRight);
    m_openInstagramButton->hide();
    m_mainLayout->addLayout(irlGridLayout);

    // Unknown account widget
    m_unknownAccountWidget = new QWidget(this);
    auto* unknownLayout = new QHBoxLayout(m_unknownAccountWidget);
    unknownLayout->setContentsMargins(0, 0, 0, 0);

    m_unknownAccountLabel = new QLabel("Unknown account:", m_unknownAccountWidget);
    unknownLayout->addWidget(m_unknownAccountLabel);

    m_unknownNameEdit = new GhostLineEdit(m_unknownAccountWidget);
    m_unknownNameEdit->setPlaceholderText("Enter IRL name...");
    m_unknownNameEdit->setCompleter(m_completer);
    unknownLayout->addWidget(m_unknownNameEdit, 1);

    // Update ghost text on any text change
    connect(m_unknownNameEdit, &QLineEdit::textChanged,
            this, &SortPanel::updateGhostText);

    // Tab accepted — commit ghost completion
    connect(m_unknownNameEdit, &GhostLineEdit::tabPressed, this, [this]() {
        QString ghost = m_unknownNameEdit->ghostText();
        if (!ghost.isEmpty()) {
            m_suppressGhost = true;
            QString currentText = m_unknownNameEdit->text().toLower();
            QString fullName;
            for (const auto& entry : m_allCompleterEntries) {
                int parenIdx = entry.indexOf('(');
                QString irlName = (parenIdx > 0)
                    ? entry.left(parenIdx).trimmed()
                    : entry;
                if (irlName.toLower() == currentText + ghost.toLower()) {
                    fullName = irlName;
                    break;
                }
            }
            if (fullName.isEmpty()) {
                fullName = currentText + ghost;
            }
            m_unknownNameEdit->setText(fullName);
            m_unknownNameEdit->setCursorPosition(fullName.length());
            m_unknownNameEdit->clearGhost();
            m_unknownNameEdit->completer()->popup()->hide();  // close dropdown
            m_suppressGhost = false;
        }
    });

    // Escape — dismiss ghost suggestion
    connect(m_unknownNameEdit, &GhostLineEdit::escapePressed, this, [this]() {
        m_unknownNameEdit->clearGhost();
    });

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

        if (m_accountType == AccountType::Curator || m_accountType == AccountType::IrlOnly) {
            // Curator or download source (Twitter, TikTok, etc.) — resolve name,
            // let SortingScreen handle DB prompt if needed
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

    // Quick-fill name buttons (favorites) for IrlOnly sources
    m_favoritesWidget = new QWidget(this);
    m_favoritesLayout = new QHBoxLayout(m_favoritesWidget);
    m_favoritesLayout->setContentsMargins(0, 0, 0, 0);
    m_favoritesLayout->setSpacing(6);
    m_favoritesWidget->hide();
    m_mainLayout->addWidget(m_favoritesWidget);

    // Folder buttons (SFW, MSFW, NSFW) — at the bottom below name/favorites
    m_folderButtonsLayout = new QHBoxLayout();
    m_folderButtonsLayout->setSpacing(20);
    m_folderButtonsLayout->addStretch();
    m_mainLayout->addLayout(m_folderButtonsLayout);

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
        m_allCompleterEntries.clear();
        m_allIrlNames.clear();
        m_completerModel->setStringList(QStringList());
        return;
    }

    // Build entries per-account so every account handle is searchable
    QList<QPair<QString, QString>> pairs;
    for (const auto& entry : m_db->allEntries()) {
        if (entry.irlName.isEmpty()) continue;

        QString display;
        if (!entry.account.isEmpty()) {
            display = QString("%1 (%2)").arg(entry.irlName, entry.account);
        } else {
            display = entry.irlName;
        }
        pairs.append(qMakePair(display, entry.irlName));
    }

    // Sort by display text (case-insensitive)
    std::sort(pairs.begin(), pairs.end(),
        [](const QPair<QString, QString>& a, const QPair<QString, QString>& b) {
            return a.first.compare(b.first, Qt::CaseInsensitive) < 0;
        });

    m_allCompleterEntries.clear();
    m_allIrlNames.clear();
    for (const auto& p : pairs) {
        m_allCompleterEntries.append(p.first);
        m_allIrlNames.append(p.second);
    }

    m_completerModel->setStringList(m_allCompleterEntries);
}

bool SortPanel::eventFilter(QObject* watched, QEvent* event) {
    return QWidget::eventFilter(watched, event);
}

void SortPanel::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    // Re-evaluate favorite buttons visibility on resize
    if (!m_favoriteButtons.isEmpty()) {
        trimOverflowFavoriteButtons();
    }
    // Reposition ghost if visible
    if (!m_unknownNameEdit->ghostText().isEmpty()) {
        m_unknownNameEdit->updateGhostGeometry();
    }
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
        m_irlNameLabel->show();
        m_unknownAccountWidget->show();
        m_unknownAccountLabel->setText("Who is in these photos?");
        m_unknownNameEdit->setPlaceholderText("Enter IRL name for output files...");
        // Clear the text field — for Curator accounts, the name field is for the MODEL,
        // not the curator. Keep button as "Add Person" so user can add new models to DB.
        m_unknownNameEdit->clear();
        m_unknownTypeCombo->hide();
        m_openInstagramButton->show();
        m_unknownAddButton->setText("Add Person");
        m_unknownAddButton->setObjectName("addPersonButton");
    } else if (type == AccountType::IrlOnly) {
        // IrlOnly source type (TikTok, Twitter, etc.) — the account is the photographer,
        // but the name field is for the MODEL in the photos (changes per batch).
        // NEVER pre-fill the name — always let user enter the model.
        m_irlNameLabel->hide();
        m_unknownAccountWidget->show();
        m_unknownAccountLabel->setText("Who is in these photos?");
        m_unknownNameEdit->setPlaceholderText("Enter IRL name...");
        if (isKnown && !irlName.isEmpty()) {
            // Account is known and name provided — use it for this batch
            m_unknownNameEdit->setText(irlName);
            m_unknownNameEdit->clearGhost();
        } else {
            // Unknown account or no name — clear field for user input
            m_unknownNameEdit->clear();
        }
        m_unknownAddButton->setText("Enter");
        m_unknownAddButton->setObjectName("curatorSortButton");
        m_unknownTypeCombo->hide();
        m_openInstagramButton->hide();
    } else if (isKnown && !irlName.isEmpty()) {
        // Personal account with known name — pre-fill the name field
        m_irlNameLabel->setText(accountHandle);
        m_irlNameLabel->show();
        m_unknownAccountWidget->show();
        m_unknownAccountLabel->setText("Who is in these photos?");
        m_unknownNameEdit->setPlaceholderText("Enter IRL name...");
        m_unknownNameEdit->setText(irlName);
        m_unknownNameEdit->clearGhost();
        m_unknownTypeCombo->hide();
        m_unknownAddButton->setText("Sort");
        m_unknownAddButton->setObjectName("curatorSortButton");
        m_openInstagramButton->show();
    } else {
        // Unknown personal account — user must add to database
        m_irlNameLabel->setText("Unknown account: " + accountHandle);
        m_irlNameLabel->show();
        m_unknownAccountWidget->show();
        m_unknownAccountLabel->setText("Who is in these photos?");
        m_unknownNameEdit->setPlaceholderText("Enter IRL name...");
        m_unknownTypeCombo->show();
        m_unknownAddButton->setText("Add Person");
        m_unknownAddButton->setObjectName("addPersonButton");
        m_openInstagramButton->show();
    }
}

// Call this to explicitly clear the name input (used when loading new batches)
void SortPanel::clearNameInput() {
    m_unknownNameEdit->clear();
    m_unknownNameEdit->clearGhost();
}

// Set the name input field to a specific value
void SortPanel::setNameInput(const QString& name) {
    m_unknownNameEdit->setText(name);
    m_unknownNameEdit->clearGhost();
}

void SortPanel::clearSelections() {
    for (auto* btn : m_folderButtons) {
        btn->setChecked(false);
    }
}

void SortPanel::updateSelectedCount(int count) {
    Q_UNUSED(count);
    // No-op — counter label removed
}

void SortPanel::updateSelectAllButtonText(bool allSelected) {
    m_allSelected = allSelected;
    if (allSelected) {
        m_selectAllButton->setText("Deselect All");
    } else {
        m_selectAllButton->setText("Select All");
    }
}

bool SortPanel::isCuratorNameResolved() const {
    return (m_accountType == AccountType::Curator || m_accountType == AccountType::IrlOnly)
        && !m_unknownNameEdit->text().trimmed().isEmpty();
}

QString SortPanel::getCuratorResolvedName() const {
    return m_unknownNameEdit->text().trimmed();
}

int SortPanel::getUnknownAccountTypeIndex() const {
    return m_unknownTypeCombo->currentIndex();
}

void SortPanel::updateGhostText() {
    if (m_suppressGhost) return;

    QString currentText = m_unknownNameEdit->text();
    if (currentText.isEmpty()) {
        m_unknownNameEdit->clearGhost();
        return;
    }

    QString lowerText = currentText.toLower();
    for (const auto& entry : m_allCompleterEntries) {
        int parenIdx = entry.indexOf('(');
        QString irlName = (parenIdx > 0)
            ? entry.left(parenIdx).trimmed()
            : entry;

        if (irlName.toLower().startsWith(lowerText) && irlName.length() > currentText.length()) {
            QString ghost = irlName.mid(currentText.length());
            m_unknownNameEdit->setGhostText(ghost);
            return;
        }
    }

    m_unknownNameEdit->clearGhost();
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

void SortPanel::setQuickFillNames(const QStringList& names) {
    // Always clear existing buttons and stretches first
    for (auto* btn : m_favoriteButtons) {
        delete btn;
    }
    m_favoriteButtons.clear();
    for (int i = m_favoritesLayout->count() - 1; i >= 0; --i) {
        QLayoutItem* item = m_favoritesLayout->takeAt(i);
        if (!item->widget()) delete item;  // stretch
    }

    if (names.isEmpty()) {
        m_favoritesWidget->hide();
        m_favoritesWidget->adjustSize();
        if (m_mainLayout) m_mainLayout->invalidate();
        return;
    }

    // Create all buttons first
    for (const QString& name : names) {
        auto* btn = new QPushButton(name, m_favoritesWidget);
        QFont font = btn->font();
        font.setPointSize(9);
        btn->setFont(font);
        btn->setMinimumHeight(32);
        btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
        btn->setCheckable(true);
        connect(btn, &QPushButton::clicked, this, [this, name, btn]() {
            m_unknownNameEdit->setText(name);
            m_unknownNameEdit->setCursorPosition(name.length());
            m_unknownNameEdit->clearGhost();
        });
        m_favoriteButtons.append(btn);
        m_favoritesLayout->addWidget(btn);
    }
    m_favoritesLayout->addStretch();

    // Trim overflow buttons after layout completes
    QTimer::singleShot(0, this, [this]() { trimOverflowFavoriteButtons(); });

    m_favoritesWidget->show();
}

void SortPanel::trimOverflowFavoriteButtons() {
    int availableWidth = this->width() - m_favoritesLayout->contentsMargins().left()
                       - m_favoritesLayout->contentsMargins().right();
    int spacing = m_favoritesLayout->spacing();
    int usedWidth = 0;
    int visibleCount = 0;

    for (int i = 0; i < m_favoriteButtons.size(); ++i) {
        auto* btn = m_favoriteButtons[i];
        int btnWidth = btn->sizeHint().width();
        if (i > 0) usedWidth += spacing;
        if (usedWidth + btnWidth <= availableWidth) {
            btn->show();
            usedWidth += btnWidth;
            visibleCount++;
        } else {
            btn->hide();
        }
    }
}

void SortPanel::rebuildFavoriteButtons() {
    // Placeholder — called when favorites need refreshing
    // Actual data is managed externally via setQuickFillNames()
}
