#include "ui/AddPersonDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>

AddPersonDialog::AddPersonDialog(const QString& personName, const QString& accountHandle,
                                 AccountType suggestedType, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Add Person to Database");
    setMinimumWidth(400);
    setupUI();

    // Pre-select the suggested type from the dropdown
    switch (suggestedType) {
    case AccountType::Curator:  m_typeCombo->setCurrentIndex(1); break;
    case AccountType::IrlOnly:  m_typeCombo->setCurrentIndex(2); break;
    default:                    m_typeCombo->setCurrentIndex(0); break;
    }

    m_irlNameEdit->setText(personName);
    m_irlNameEdit->selectAll();
    if (!accountHandle.isEmpty()) {
        m_accountEdit->setText(accountHandle);
    } else {
        m_accountEdit->setFocus();
    }
}

void AddPersonDialog::setupUI() {
    m_layout = new QVBoxLayout(this);
    m_layout->setSpacing(15);

    m_messageLabel = new QLabel(
        "This name is not in the database. Would you like to add this person?", this);
    m_messageLabel->setWordWrap(true);
    m_layout->addWidget(m_messageLabel);

    auto* formLayout = new QFormLayout();
    formLayout->setSpacing(10);

    m_irlNameEdit = new QLineEdit(this);
    m_irlNameEdit->setPlaceholderText("IRL name (optional)");
    formLayout->addRow("Name:", m_irlNameEdit);

    m_accountEdit = new QLineEdit(this);
    m_accountEdit->setPlaceholderText("e.g. zane_carter — leave blank if unknown");
    formLayout->addRow("Instagram account:", m_accountEdit);

    m_typeCombo = new QComboBox(this);
    m_typeCombo->addItem("Personal");
    m_typeCombo->addItem("Curator");
    m_typeCombo->addItem("IRL Only");
    formLayout->addRow("Account type:", m_typeCombo);

    m_layout->addLayout(formLayout);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_okButton = new QPushButton("Add", this);
    m_cancelButton = new QPushButton("Cancel", this);
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    m_layout->addLayout(buttonLayout);

    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

QString AddPersonDialog::irlName() const {
    return m_irlNameEdit->text().trimmed();
}

QString AddPersonDialog::accountHandle() const {
    return m_accountEdit->text().trimmed();
}

AccountType AddPersonDialog::accountType() const {
    switch (m_typeCombo->currentIndex()) {
    case 1: return AccountType::Curator;
    case 2: return AccountType::IrlOnly;
    default: return AccountType::Personal;
    }
}
