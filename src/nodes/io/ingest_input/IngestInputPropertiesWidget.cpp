#include "IngestInputPropertiesWidget.h"

#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMimeData>
#include <QPixmap>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QUrl>
#include <QVBoxLayout>

IngestInputPropertiesWidget::IngestInputPropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    setAcceptDrops(true);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    m_dropLabel = new QLabel(tr("Drop a file here or use Paste Clipboard."), this);
    m_dropLabel->setAlignment(Qt::AlignCenter);
    m_dropLabel->setMinimumHeight(120);
    m_dropLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    m_dropLabel->setStyleSheet(QStringLiteral("QLabel { border: 2px dashed #8a8a8a; padding: 12px; }"));
    layout->addWidget(m_dropLabel);

    auto* buttonRow = new QHBoxLayout();
    m_selectButton = new QPushButton(tr("Select File..."), this);
    m_pasteButton = new QPushButton(tr("Paste Clipboard"), this);
    buttonRow->addWidget(m_selectButton);
    buttonRow->addWidget(m_pasteButton);
    layout->addLayout(buttonRow);

    m_statusLabel = new QLabel(tr("Nothing ingested yet."), this);
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_runStatusLabel = new QLabel(tr("Status: idle"), this);
    m_runStatusLabel->setWordWrap(true);
    layout->addWidget(m_runStatusLabel);

    m_pathLabel = new QLabel(tr("Source: <none>"), this);
    m_pathLabel->setWordWrap(true);
    layout->addWidget(m_pathLabel);

    m_imagePreviewLabel = new QLabel(this);
    m_imagePreviewLabel->setAlignment(Qt::AlignCenter);
    m_imagePreviewLabel->setMinimumHeight(180);
    m_imagePreviewLabel->setFrameStyle(QFrame::StyledPanel | QFrame::Sunken);
    m_imagePreviewLabel->hide();
    layout->addWidget(m_imagePreviewLabel);

    m_textPreviewEdit = new QPlainTextEdit(this);
    m_textPreviewEdit->setReadOnly(true);
    m_textPreviewEdit->setMaximumBlockCount(200);
    m_textPreviewEdit->setPlaceholderText(tr("Text and markdown previews appear here."));
    m_textPreviewEdit->hide();
    layout->addWidget(m_textPreviewEdit);

    layout->addStretch();

    connect(m_selectButton, &QPushButton::clicked, this, [this]() {
        const QString fileName = QFileDialog::getOpenFileName(
            this,
            tr("Select File"),
            QString(),
            tr("All Supported Files (*.md *.markdown *.txt *.json *.xml *.yaml *.yml *.csv *.html *.htm *.png *.jpg *.jpeg *.bmp *.webp *.gif *.pdf);;All Files (*)"));
        if (!fileName.isEmpty()) {
            emit fileChosen(fileName);
        }
    });

    connect(m_pasteButton, &QPushButton::clicked, this, &IngestInputPropertiesWidget::clipboardPasteRequested);
}

void IngestInputPropertiesWidget::setPayload(const QString& kind,
                                             const QString& sourcePath,
                                             const QString& mimeType,
                                             const QString& previewText,
                                             const QPixmap& previewPixmap)
{
    const QString kindLabel = kind.isEmpty() ? tr("none") : kind;
    const QString mimeLabel = mimeType.isEmpty() ? tr("unknown") : mimeType;
    m_statusLabel->setText(tr("Detected kind: %1\nMIME type: %2").arg(kindLabel, mimeLabel));
    m_pathLabel->setText(tr("Source: %1").arg(sourcePath.isEmpty() ? tr("<none>") : sourcePath));

    if (previewPixmap.isNull()) {
        m_imagePreviewLabel->clear();
    } else {
        m_imagePreviewLabel->setPixmap(previewPixmap.scaled(320, 220, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }

    m_textPreviewEdit->setPlainText(previewText);
    updatePreviewVisibility(!previewText.trimmed().isEmpty(), !previewPixmap.isNull());
}

void IngestInputPropertiesWidget::clearPayload()
{
    m_statusLabel->setText(tr("Nothing ingested yet."));
    setStatusMessage(tr("Status: idle"));
    m_pathLabel->setText(tr("Source: <none>"));
    m_imagePreviewLabel->clear();
    m_textPreviewEdit->clear();
    updatePreviewVisibility(false, false);
}

void IngestInputPropertiesWidget::setStatusMessage(const QString& message)
{
    if (!m_runStatusLabel) {
        return;
    }

    m_runStatusLabel->setText(message.trimmed().isEmpty() ? tr("Status: idle") : message);
}

void IngestInputPropertiesWidget::dragEnterEvent(QDragEnterEvent* event)
{
    if (!event || !event->mimeData()) {
        return;
    }

    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        event->acceptProposedAction();
        return;
    }

    QWidget::dragEnterEvent(event);
}

void IngestInputPropertiesWidget::dropEvent(QDropEvent* event)
{
    if (!event || !event->mimeData()) {
        return;
    }

    const QList<QUrl> urls = event->mimeData()->urls();
    for (const QUrl& url : urls) {
        if (url.isLocalFile()) {
            emit fileChosen(url.toLocalFile());
            event->acceptProposedAction();
            return;
        }
    }

    QWidget::dropEvent(event);
}

void IngestInputPropertiesWidget::updatePreviewVisibility(bool hasTextPreview, bool hasImagePreview)
{
    m_textPreviewEdit->setVisible(hasTextPreview);
    m_imagePreviewLabel->setVisible(hasImagePreview);
}
