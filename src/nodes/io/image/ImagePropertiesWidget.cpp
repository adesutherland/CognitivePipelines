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
#include "ImagePropertiesWidget.h"
#include "ImagePopupDialog.h"
#include <QFileDialog>
#include <QPixmap>
#include <QEvent>
#include <QResizeEvent>
#include <QShowEvent>
#include "Logger.h"

ImagePropertiesWidget::ImagePropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    // Initialize layout ready flag
    m_isLayoutReady = false;
    
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    // Preview label - configured for image display
    m_previewLabel = new QLabel(this);
    m_previewLabel->setMinimumSize(200, 200);
    // Set size policy: Ignored (horizontal) and Fixed (vertical)
    // This decouples the label's content size from the parent widget's size
    m_previewLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_previewLabel->setMinimumHeight(1);
    m_previewLabel->setMaximumHeight(300);
    m_previewLabel->setScaledContents(false);
    m_previewLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_previewLabel->setFrameStyle(QFrame::Box | QFrame::Sunken);
    m_previewLabel->setText(tr("No image selected"));
    m_previewLabel->installEventFilter(this);
    layout->addWidget(m_previewLabel);

    // Path label - displays the filename/path
    m_pathLabel = new QLabel(this);
    m_pathLabel->setWordWrap(true);
    m_pathLabel->setText(tr("No file selected"));
    layout->addWidget(m_pathLabel);

    // Select button
    m_selectButton = new QPushButton(tr("Select Image..."), this);
    layout->addWidget(m_selectButton);

    // Connect button click handler
    connect(m_selectButton, &QPushButton::clicked, this, [this]() {
        QString fileName = QFileDialog::getOpenFileName(
            this,
            tr("Select Image"),
            QString(),
            tr("Image Files (*.png *.jpg *.jpeg *.bmp *.webp)")
        );
        
        if (!fileName.isEmpty()) {
            // Update UI immediately
            setImagePath(fileName);
            // Notify the Node
            emit imagePathChanged(fileName);
        }
    });

    // View Full Size button
    m_viewFullSizeButton = new QPushButton(tr("View Full Size"), this);
    layout->addWidget(m_viewFullSizeButton);

    // Connect view full size button
    connect(m_viewFullSizeButton, &QPushButton::clicked, this, &ImagePropertiesWidget::onViewFullSize);

    layout->addStretch();
}

void ImagePropertiesWidget::setImagePath(const QString& path)
{
    // Store the current path
    m_currentPath = path;
    
    // Update the path label
    if (m_pathLabel) {
        if (path.isEmpty()) {
            m_pathLabel->setText(tr("No file selected"));
        } else {
            m_pathLabel->setText(path);
        }
    }
    
    // Load the image into m_originalPixmap
    if (path.isEmpty()) {
        m_originalPixmap = QPixmap();
    } else {
        m_originalPixmap.load(path);
        // Verify that the pixmap loaded successfully
        if (m_originalPixmap.isNull()) {
            CP_WARN << "ImagePropertiesWidget::setImagePath: Failed to load image from path:" << path;
        }
    }
    
    // Update the preview
    updatePreview();
}

QString ImagePropertiesWidget::imagePath() const
{
    if (m_pathLabel && m_pathLabel->text() != tr("No file selected")) {
        return m_pathLabel->text();
    }
    return QString();
}

void ImagePropertiesWidget::updatePreview()
{
    if (!m_previewLabel) {
        CP_WARN << "[ImagePropertiesWidget::updatePreview] Aborting: m_previewLabel is null";
        return;
    }
    
    if (m_originalPixmap.isNull()) {
        // Clear preview
        m_previewLabel->clear();
        m_previewLabel->setText(tr("No image selected"));
    } else {
        // Calculate available width by subtracting layout margins from widget width
        auto* layout = qobject_cast<QVBoxLayout*>(this->layout());
        if (!layout) {
            CP_WARN << "[ImagePropertiesWidget::updatePreview] Aborting: Could not cast layout to QVBoxLayout";
            return;
        }
        
        QMargins margins = layout->contentsMargins();
        int availableWidth = this->width() - margins.left() - margins.right();
        
        // Sanity check: Ensure we have positive width before scaling
        if (availableWidth <= 0) {
            CP_WARN << "[ImagePropertiesWidget::updatePreview] Aborting: availableWidth <= 0 (width =" << availableWidth << ")";
            return;
        }
        
        // Step 1: Scale the image to fit the available width exactly, maintaining aspect ratio
        // Critical: Use scaledToWidth() to ensure the image fills the panel width
        // The height will scale proportionally, and if it exceeds 300px, it will be cropped below
        QPixmap scaled = m_originalPixmap.scaledToWidth(
            availableWidth,
            Qt::SmoothTransformation
        );
        
        // Step 2: If the scaled height exceeds our maximum, crop it in memory
        // This prevents the layout system from trying to accommodate oversized content
        const int maxHeight = 300;
        if (scaled.height() > maxHeight) {
            // Create a cropped pixmap containing only the top portion
            QPixmap cropped = scaled.copy(0, 0, scaled.width(), maxHeight);
            m_previewLabel->setPixmap(cropped);
        } else {
            // Image fits within height constraint, use as-is
            m_previewLabel->setPixmap(scaled);
        }
    }
}

bool ImagePropertiesWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_previewLabel && event->type() == QEvent::MouseButtonRelease) {
        if (!m_currentPath.isEmpty()) {
            emit galleryRequested(m_currentPath);
        }
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void ImagePropertiesWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Guard: Don't update preview until layout is ready
    if (!m_isLayoutReady) {
        return;
    }
    updatePreview();
}

void ImagePropertiesWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    m_isLayoutReady = true;
    updatePreview();
}

void ImagePropertiesWidget::onViewFullSize()
{
    // Check if we have a valid image
    if (m_currentPath.isEmpty() || m_originalPixmap.isNull()) {
        CP_WARN << "ImagePropertiesWidget::onViewFullSize: No valid image to display";
        return;
    }
    
    // Create and show the popup dialog
    ImagePopupDialog dialog(m_originalPixmap, this);
    dialog.exec();
}
