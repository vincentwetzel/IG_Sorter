#include "ui/AddPersonDialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

AddPersonDialog::AddPersonDialog(const QString& personName, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Add Person to Database");
    setMinimumWidth(400);
    setupUI();
    m_irlNameEdit->setText(personName);
    m_irlNameEdit->selectAll();
    m_irlNameEdit->setFocus();
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
    m_irlNameEdit->setPlaceholderText("IRL name (required)");
    formLayout->addRow("Name:", m_irlNameEdit);

    m_accountEdit = new QLineEdit(this);
    m_accountEdit->setPlaceholderText("e.g. zane_carter — leave blank if unknown");
    formLayout->addRow("Instagram account (optional):", m_accountEdit);

    m_layout->addLayout(formLayout);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_okButton = new QPushButton("Add & Sort", this);
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
