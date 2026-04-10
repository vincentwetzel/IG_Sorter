#pragma once

#include <QDialog>
#include "core/DatabaseManager.h"

class QVBoxLayout;
class QHBoxLayout;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;

// Dialog that appears when a user enters a name that isn't in the database
// during curator batch sorting. Prompts for optional Instagram account.
class AddPersonDialog : public QDialog {
    Q_OBJECT
public:
    explicit AddPersonDialog(const QString& personName, const QString& accountHandle = QString(),
                             AccountType suggestedType = AccountType::Personal,
                             QWidget* parent = nullptr);

    QString irlName() const;
    QString accountHandle() const;  // may be empty — user can skip it
    AccountType accountType() const;

private:
    void setupUI();

    QVBoxLayout*   m_layout;
    QLabel*        m_messageLabel;
    QLineEdit*     m_irlNameEdit;
    QLineEdit*     m_accountEdit;
    QComboBox*     m_typeCombo;
    QPushButton*   m_okButton;
    QPushButton*   m_cancelButton;
};
