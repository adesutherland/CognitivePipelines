//
// Cognitive Pipeline Application - PromptDialog implementation
//
#include "PromptDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QTextEdit>
#include <QPushButton>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QApplication>
#include <QDir>

namespace {
static QString tryReadApiKeyFromJson(const QByteArray& data) {
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return {};
    const QJsonObject root = doc.object();
    const QJsonValue accountsVal = root.value(QStringLiteral("accounts"));
    if (!accountsVal.isArray()) return {};
    const QJsonArray accounts = accountsVal.toArray();
    if (accounts.isEmpty()) return {};
    const QJsonValue first = accounts.at(0);
    if (!first.isObject()) return {};
    const QJsonObject firstObj = first.toObject();
    const QString key = firstObj.value(QStringLiteral("api_key")).toString();
    return key;
}
}

PromptDialog::PromptDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Interactive Prompt"));
    resize(700, 560);

    auto* mainLayout = new QVBoxLayout(this);

    // API Key (read-only)
    mainLayout->addWidget(new QLabel(tr("API Key:"), this));
    apiKeyEdit_ = new QLineEdit(this);
    apiKeyEdit_->setReadOnly(true);
    apiKeyEdit_->setPlaceholderText(tr("Loaded from accounts.json"));
    mainLayout->addWidget(apiKeyEdit_);

    // Prompt input
    mainLayout->addWidget(new QLabel(tr("Prompt:"), this));
    promptEdit_ = new QTextEdit(this);
    promptEdit_->setPlaceholderText(tr("Enter your prompt here..."));
    promptEdit_->setAcceptRichText(false);
    mainLayout->addWidget(promptEdit_);

    // Send button
    sendButton_ = new QPushButton(tr("Send"), this);
    mainLayout->addWidget(sendButton_);
    connect(sendButton_, &QPushButton::clicked, this, &PromptDialog::onSendClicked);

    // Response display
    mainLayout->addWidget(new QLabel(tr("LLM Response:"), this));
    responseEdit_ = new QTextEdit(this);
    responseEdit_->setReadOnly(true);
    mainLayout->addWidget(responseEdit_);

    // Load API key from accounts.json
    const bool ok = loadApiKeyFromAccountsJson();
    if (!ok) {
        QMessageBox::critical(this, tr("API Key Error"), tr("Could not load API key from accounts.json. Please create it at the project root using accounts.json.example."));
        sendButton_->setEnabled(false);
    }
}

QString PromptDialog::locateAccountsJson() const {
    // Try common relative locations from the running binary / current dir.
    const QStringList candidates = {
        QStringLiteral("accounts.json"),
        QStringLiteral("../accounts.json"),
        QStringLiteral("../../accounts.json"),
        QStringLiteral("../../../accounts.json"),
        QStringLiteral("../../../../accounts.json")
    };
    for (const QString& rel : candidates) {
        const QString path = QDir::current().absoluteFilePath(rel);
        if (QFileInfo::exists(path)) return path;
    }
    // Also try relative to the application dir
    const QDir appDir(QCoreApplication::applicationDirPath());
    for (const QString& rel : candidates) {
        const QString path = appDir.absoluteFilePath(rel);
        if (QFileInfo::exists(path)) return path;
    }
    return {};
}

bool PromptDialog::loadApiKeyFromAccountsJson() {
    const QString path = locateAccountsJson();
    if (path.isEmpty()) return false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return false;
    const QByteArray data = f.readAll();
    f.close();
    const QString key = tryReadApiKeyFromJson(data);
    if (key.isEmpty()) return false;
    apiKeyEdit_->setText(key);
    return true;
}

void PromptDialog::onSendClicked() {
    const QString apiKey = apiKeyEdit_->text().trimmed();
    const QString prompt = promptEdit_->toPlainText().trimmed();

    if (apiKey.isEmpty()) {
        QMessageBox::critical(this, tr("Missing API Key"), tr("API key is empty. Please configure accounts.json."));
        return;
    }
    if (prompt.isEmpty()) {
        QMessageBox::warning(this, tr("Empty Prompt"), tr("Please enter a prompt before sending."));
        return;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);
    const std::string response = client_.sendPrompt(apiKey.toStdString(), prompt.toStdString());
    QApplication::restoreOverrideCursor();

    responseEdit_->setPlainText(QString::fromStdString(response));
}
