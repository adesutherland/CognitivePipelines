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
#include "CredentialsEditorDialog.h"

#include <QDialogButtonBox>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QPushButton>
#include <QDir>
#include <QFileInfo>
#include <QSaveFile>

CredentialsEditorDialog::CredentialsEditorDialog(const QString &filePath, QWidget *parent)
    : QDialog(parent), textEdit(nullptr), buttonBox(nullptr), m_filePath(filePath)
{
    setWindowTitle(QStringLiteral("Edit Credentials"));

    auto *layout = new QVBoxLayout(this);

    textEdit = new QTextEdit(this);
    buttonBox = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);

    layout->addWidget(textEdit);
    layout->addWidget(buttonBox);

    resize(600, 400);

    connect(buttonBox, &QDialogButtonBox::accepted, this, &CredentialsEditorDialog::onSaveClicked);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &CredentialsEditorDialog::reject);

    // Ensure parent directory exists
    const QFileInfo fi(m_filePath);
    const QString dirPath = fi.dir().absolutePath();
    if (!dirPath.isEmpty()) {
        QDir().mkpath(dirPath);
    }

    // Load file contents if present; otherwise prefill with a minimal template
    static const char kTemplateJson[] =
        "{\n"
        "  \"accounts\": [ { \"name\": \"default_openai\", \"api_key\": \"YOUR_API_KEY_HERE\" } ]\n"
        "}\n";

    QFile file(m_filePath);
    if (file.exists() && file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        const QString contents = in.readAll();
        textEdit->setPlainText(contents);
        file.close();
    } else {
        textEdit->setPlainText(QString::fromUtf8(kTemplateJson));
        // Keep Save enabled so the user can create the file
    }
}

void CredentialsEditorDialog::onSaveClicked()
{
    // Atomic save
    QSaveFile sf(m_filePath);
    if (!sf.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this,
                              QStringLiteral("Error"),
                              QStringLiteral("Could not save file: %1").arg(m_filePath));
        return;
    }
    QTextStream out(&sf);
    out << textEdit->toPlainText();
    if (!sf.commit()) {
        QMessageBox::critical(this,
                              QStringLiteral("Error"),
                              QStringLiteral("Failed to finalize save: %1").arg(m_filePath));
        return;
    }
#ifdef Q_OS_UNIX
    QFile::setPermissions(m_filePath, QFileDevice::ReadOwner | QFileDevice::WriteOwner);
#endif
    accept();
}
