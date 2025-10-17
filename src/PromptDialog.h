//
// Cognitive Pipeline Application - PromptDialog
//
// A dialog to enter a prompt, load API key from accounts.json, send via LlmApiClient,
// and display the response.
//
#pragma once

#include <QDialog>
#include <QPointer>

class QLabel;
class QLineEdit;
class QTextEdit;
class QPushButton;

#include "llm_api_client.h"

class PromptDialog : public QDialog {
    Q_OBJECT
public:
    explicit PromptDialog(QWidget* parent = nullptr);
    ~PromptDialog() override = default;

private slots:
    void onSendClicked();

private:
    bool loadApiKeyFromAccountsJson();
    QString locateAccountsJson() const;

    // UI widgets
    QLineEdit* apiKeyEdit_ {nullptr};
    QTextEdit* promptEdit_ {nullptr};
    QTextEdit* responseEdit_ {nullptr};
    QPushButton* sendButton_ {nullptr};

    // Client
    LlmApiClient client_;
};
