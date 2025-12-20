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
#include "about_dialog.h"

#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QDialogButtonBox>
#include <QApplication>

#ifndef APP_VERSION
#define APP_VERSION "unknown"
#endif

#ifndef GIT_COMMIT_HASH
#define GIT_COMMIT_HASH "unknown"
#endif

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("About"));
    setModal(true);
    resize(600, 500);

    auto* mainLayout = new QVBoxLayout(this);

    // ====== HEADER LAYOUT (Horizontal: Icon on left, Info on right) ======
    auto* headerLayout = new QHBoxLayout();

    // Left: Application Icon
    auto* iconLabel = new QLabel(this);
    QIcon appIcon = QApplication::windowIcon();
    if (!appIcon.isNull()) {
        QPixmap iconPixmap = appIcon.pixmap(128, 128);
        iconLabel->setPixmap(iconPixmap);
    } else {
        iconLabel->setText(tr("[Icon]"));
    }
    iconLabel->setAlignment(Qt::AlignTop);
    headerLayout->addWidget(iconLabel);

    // Right: Vertical layout with app name, version, build info, Qt version, contact
    auto* infoLayout = new QVBoxLayout();

    // App Name (Bold, larger font)
    auto* nameLabel = new QLabel(tr("<b>Cognitive Pipeline Application</b>"), this);
    nameLabel->setTextFormat(Qt::RichText);
    QFont nameFont = nameLabel->font();
    nameFont.setPointSize(16);
    nameLabel->setFont(nameFont);
    infoLayout->addWidget(nameLabel);

    // Version
    auto* versionLabel = new QLabel(tr("Version: %1").arg(QString::fromUtf8(APP_VERSION)), this);
    infoLayout->addWidget(versionLabel);

    // Git hash
    auto* gitLabel = new QLabel(tr("Git Hash: %1").arg(QString::fromUtf8(GIT_COMMIT_HASH)), this);
    infoLayout->addWidget(gitLabel);

    // Build date/time
    auto* buildLabel = new QLabel(tr("Build Date: %1 %2").arg(QString::fromUtf8(__DATE__), QString::fromUtf8(__TIME__)), this);
    infoLayout->addWidget(buildLabel);

    // Qt runtime version
    auto* qtLabel = new QLabel(tr("Qt Runtime: %1").arg(QString::fromUtf8(qVersion())), this);
    infoLayout->addWidget(qtLabel);

    // Contact
    auto* contactLabel = new QLabel(tr("Contact: <a href=\"mailto:adrian@sutherlandonline.org\">adrian@sutherlandonline.org</a>"), this);
    contactLabel->setTextFormat(Qt::RichText);
    contactLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    contactLabel->setOpenExternalLinks(true);
    infoLayout->addWidget(contactLabel);

    infoLayout->addStretch();
    headerLayout->addLayout(infoLayout);

    mainLayout->addLayout(headerLayout);

    // ====== LICENSE AREA ======
    auto* licenseEdit = new QTextEdit(this);
    licenseEdit->setReadOnly(true);
    licenseEdit->setMinimumHeight(250);

    QString licenseText = tr(
        "MIT License\n"
        "\n"
        "Copyright (c) 2025 Adrian Sutherland\n"
        "\n"
        "Permission is hereby granted, free of charge, to any person obtaining a copy\n"
        "of this software and associated documentation files (the \"Software\"), to deal\n"
        "in the Software without restriction, including without limitation the rights\n"
        "to use, copy, modify, merge, publish, distribute, sublicense, and/or sell\n"
        "copies of the Software, and to permit persons to whom the Software is\n"
        "furnished to do so, subject to the following conditions:\n"
        "\n"
        "The above copyright notice and this permission notice shall be included in all\n"
        "copies or substantial portions of the Software.\n"
        "\n"
        "THE SOFTWARE IS PROVIDED \"AS IS\", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR\n"
        "IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,\n"
        "FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE\n"
        "AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER\n"
        "LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,\n"
        "OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE\n"
        "SOFTWARE.\n"
        "\n"
        "================================================================================\n"
        "\n"
        "Qt Framework Attribution\n"
        "\n"
        "This application uses the Qt Toolkit under the terms of the LGPLv3.\n"
        "\n"
        "Qt is a cross-platform C++ application framework developed by The Qt Company.\n"
        "For more information about Qt and its licensing, please visit:\n"
        "https://www.qt.io/licensing/\n"
        "\n"
        "The Qt Toolkit is licensed under the GNU Lesser General Public License (LGPL)\n"
        "version 3. Under the LGPL, you have the right to use, modify, and distribute\n"
        "this application. The Qt libraries remain under the LGPL, and any modifications\n"
        "to Qt itself must be made available under the same license.\n"
        "\n"
        "MermaidJS\n"
        "Copyright (c) 2014-2025 Knut Sveidqvist\n"
        "Licensed under the MIT License.\n"
        "https://mermaid.js.org/\n"
    );

    licenseEdit->setPlainText(licenseText);
    mainLayout->addWidget(licenseEdit);

    // ====== FOOTER (Close button) ======
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    mainLayout->addWidget(buttons);
}
