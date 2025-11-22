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
#include "ImagePopupDialog.h"
#include <QLabel>
#include <QScrollArea>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QWheelEvent>
#include <QShowEvent>

ImagePopupDialog::ImagePopupDialog(const QPixmap& pixmap, QWidget* parent)
    : QDialog(parent)
{
    setupUI(pixmap);
}

ImagePopupDialog::ImagePopupDialog(const QString& imagePath, QWidget* parent)
    : QDialog(parent)
{
    QPixmap pixmap(imagePath);
    setupUI(pixmap);
}

void ImagePopupDialog::setupUI(const QPixmap& pixmap)
{
    setWindowTitle(tr("Image Viewer"));
    
    // Store the original pixmap
    m_originalPixmap = pixmap;
    
    // Create main layout
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(8);
    
    // Create zoom toolbar
    auto* toolbarLayout = new QHBoxLayout();
    toolbarLayout->setSpacing(4);
    
    auto* zoomInBtn = new QPushButton(tr("Zoom In (+)"), this);
    auto* zoomOutBtn = new QPushButton(tr("Zoom Out (-)"), this);
    auto* fitBtn = new QPushButton(tr("Fit to Window"), this);
    auto* fitWidthBtn = new QPushButton(tr("Fit to Width"), this);
    auto* normalBtn = new QPushButton(tr("100%"), this);
    
    toolbarLayout->addWidget(zoomInBtn);
    toolbarLayout->addWidget(zoomOutBtn);
    toolbarLayout->addWidget(fitBtn);
    toolbarLayout->addWidget(fitWidthBtn);
    toolbarLayout->addWidget(normalBtn);
    toolbarLayout->addStretch();
    
    mainLayout->addLayout(toolbarLayout);
    
    // Create scroll area
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(false);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    
    // Create label to hold the image
    m_imageLabel = new QLabel(m_scrollArea);
    m_imageLabel->setScaledContents(true);  // Enable scaling for zoom
    m_imageLabel->setAlignment(Qt::AlignCenter);
    
    // Add label to scroll area
    m_scrollArea->setWidget(m_imageLabel);
    mainLayout->addWidget(m_scrollArea);
    
    // Create button box with Close button
    m_buttonBox = new QDialogButtonBox(QDialogButtonBox::Close, this);
    mainLayout->addWidget(m_buttonBox);
    
    // Connect zoom buttons to slots
    connect(zoomInBtn, &QPushButton::clicked, this, &ImagePopupDialog::zoomIn);
    connect(zoomOutBtn, &QPushButton::clicked, this, &ImagePopupDialog::zoomOut);
    connect(fitBtn, &QPushButton::clicked, this, &ImagePopupDialog::zoomFit);
    connect(fitWidthBtn, &QPushButton::clicked, this, &ImagePopupDialog::zoomFitToWidth);
    connect(normalBtn, &QPushButton::clicked, this, &ImagePopupDialog::normalSize);
    
    // Connect Close button to dialog rejection
    connect(m_buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    
    // Set reasonable default size (800x600)
    resize(800, 600);
    
    // Set the pixmap (initial zoom will be done in showEvent)
    if (pixmap.isNull()) {
        m_imageLabel->setText(tr("Failed to load image"));
    }
}

void ImagePopupDialog::updateImageDisplay()
{
    if (m_originalPixmap.isNull() || !m_imageLabel) {
        return;
    }
    
    // Calculate new size based on scale factor
    QSize newSize = m_originalPixmap.size() * m_scaleFactor;
    
    // Resize the label (Qt will scale the pixmap automatically due to setScaledContents(true))
    m_imageLabel->setPixmap(m_originalPixmap);
    m_imageLabel->resize(newSize);
}

void ImagePopupDialog::zoomIn()
{
    m_scaleFactor *= 1.25;
    updateImageDisplay();
}

void ImagePopupDialog::zoomOut()
{
    m_scaleFactor *= 0.8;
    updateImageDisplay();
}

void ImagePopupDialog::normalSize()
{
    m_scaleFactor = 1.0;
    updateImageDisplay();
}

void ImagePopupDialog::zoomFit()
{
    if (m_originalPixmap.isNull() || !m_scrollArea) {
        return;
    }
    
    // Get the viewport size (visible area of scroll area)
    QSize viewportSize = m_scrollArea->viewport()->size();
    QSize pixmapSize = m_originalPixmap.size();
    
    // Calculate the scale factor to fit the image in the viewport
    double widthRatio = static_cast<double>(viewportSize.width()) / pixmapSize.width();
    double heightRatio = static_cast<double>(viewportSize.height()) / pixmapSize.height();
    
    // Use the smaller ratio to ensure the entire image fits
    m_scaleFactor = qMin(widthRatio, heightRatio);
    
    updateImageDisplay();
}

void ImagePopupDialog::zoomFitToWidth()
{
    if (m_originalPixmap.isNull() || !m_scrollArea) {
        return;
    }
    
    // Get the viewport width (visible area of scroll area)
    QSize viewportSize = m_scrollArea->viewport()->size();
    QSize pixmapSize = m_originalPixmap.size();
    
    // Calculate the scale factor to fit the image width to viewport width
    m_scaleFactor = static_cast<double>(viewportSize.width()) / pixmapSize.width();
    
    updateImageDisplay();
}

void ImagePopupDialog::wheelEvent(QWheelEvent* event)
{
    // Check if Ctrl key is pressed
    if (event->modifiers() & Qt::ControlModifier) {
        // Get the wheel delta
        int delta = event->angleDelta().y();
        
        if (delta > 0) {
            // Scroll up = Zoom in
            zoomIn();
        } else if (delta < 0) {
            // Scroll down = Zoom out
            zoomOut();
        }
        
        event->accept();
    } else {
        // Let the base class handle normal scrolling
        QDialog::wheelEvent(event);
    }
}

void ImagePopupDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    
    // Perform initial zoom to fit width only on first show
    if (!m_initialZoomDone && !m_originalPixmap.isNull()) {
        m_initialZoomDone = true;
        zoomFitToWidth();
    }
}
