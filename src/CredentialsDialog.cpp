//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include "CredentialsDialog.h"
#include "core/LLMProviderRegistry.h"
#include "backends/ILLMBackend.h"

#include <QVBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QLabel>
#include <QDialogButtonBox>
#include <QPushButton>

CredentialsDialog::CredentialsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("API Credentials"));
    resize(500, 200);

    auto *mainLayout = new QVBoxLayout(this);
    auto *formLayout = new QFormLayout();
    formLayout->setSpacing(10);

    // Dynamically build rows for each registered backend
    const auto backends = LLMProviderRegistry::instance().allBackends();
    for (ILLMBackend* backend : backends) {
        if (!backend) continue;

        QString id = backend->id();
        QString name = backend->name();
        
        auto *lineEdit = new QLineEdit(this);
        lineEdit->setEchoMode(QLineEdit::Password);
        lineEdit->setPlaceholderText(tr("Enter %1 API Key...").arg(name));
        
        // Pre-fill with existing credential (if any)
        lineEdit->setText(LLMProviderRegistry::instance().getCredential(id));
        
        formLayout->addRow(tr("%1 API Key:").arg(name), lineEdit);
        m_edits.insert(id, lineEdit);
    }

    mainLayout->addLayout(formLayout);
    mainLayout->addStretch();

    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    mainLayout->addWidget(buttonBox);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &CredentialsDialog::onSaveClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

void CredentialsDialog::onSaveClicked()
{
    QMap<QString, QString> credentials;
    for (auto it = m_edits.begin(); it != m_edits.end(); ++it) {
        credentials.insert(it.key(), it.value()->text().trimmed());
    }

    if (LLMProviderRegistry::instance().saveCredentials(credentials)) {
        accept();
    } else {
        // Error already logged in LLMProviderRegistry
    }
}
