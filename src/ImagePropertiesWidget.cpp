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
#include <QFileDialog>
#include <QPixmap>

ImagePropertiesWidget::ImagePropertiesWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    // Preview label - configured for image display
    m_previewLabel = new QLabel(this);
    m_previewLabel->setMinimumSize(200, 200);
    m_previewLabel->setScaledContents(true);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setFrameStyle(QFrame::Box | QFrame::Sunken);
    m_previewLabel->setText(tr("No image selected"));
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

    layout->addStretch();
}

void ImagePropertiesWidget::setImagePath(const QString& path)
{
    // Update the path label
    if (m_pathLabel) {
        if (path.isEmpty()) {
            m_pathLabel->setText(tr("No file selected"));
        } else {
            m_pathLabel->setText(path);
        }
    }
    
    // Attempt to load and display the image
    if (m_previewLabel) {
        if (path.isEmpty()) {
            // Clear preview
            m_previewLabel->clear();
            m_previewLabel->setText(tr("No image selected"));
        } else {
            QPixmap pixmap(path);
            if (!pixmap.isNull()) {
                // Successfully loaded - display scaled version
                m_previewLabel->setPixmap(pixmap.scaled(
                    m_previewLabel->size(),
                    Qt::KeepAspectRatio,
                    Qt::SmoothTransformation
                ));
            } else {
                // Failed to load
                m_previewLabel->clear();
                m_previewLabel->setText(tr("Failed to load image"));
            }
        }
    }
}

QString ImagePropertiesWidget::imagePath() const
{
    if (m_pathLabel && m_pathLabel->text() != tr("No file selected")) {
        return m_pathLabel->text();
    }
    return QString();
}
