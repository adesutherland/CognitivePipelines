#include "about_dialog.h"

#include <QLabel>
#include <QVBoxLayout>
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

    auto* layout = new QVBoxLayout(this);

    // Application name
    auto* nameLabel = new QLabel(tr("<b>Cognitive Pipeline Application</b>"), this);
    nameLabel->setTextFormat(Qt::RichText);
    layout->addWidget(nameLabel);

    // Version
    auto* versionLabel = new QLabel(tr("Version: %1").arg(QString::fromUtf8(APP_VERSION)), this);
    layout->addWidget(versionLabel);

    // Git hash
    auto* gitLabel = new QLabel(tr("Git Hash: %1").arg(QString::fromUtf8(GIT_COMMIT_HASH)), this);
    layout->addWidget(gitLabel);

    // Build date/time
    auto* buildLabel = new QLabel(tr("Build Date: %1 %2").arg(QString::fromUtf8(__DATE__), QString::fromUtf8(__TIME__)), this);
    layout->addWidget(buildLabel);

    // Qt runtime version
    auto* qtLabel = new QLabel(tr("Qt Runtime: %1").arg(QString::fromUtf8(qVersion())), this);
    layout->addWidget(qtLabel);

    // Spacer and Close button
    layout->addStretch();

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    layout->addWidget(buttons);
}
