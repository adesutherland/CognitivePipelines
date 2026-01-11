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
#include "PdfToImageNode.h"
#include "PdfToImagePropertiesWidget.h"
#include "Logger.h"

#include <QtConcurrent/QtConcurrent>
#include <QJsonObject>
#include <QPdfDocument>
#include <QPainter>
#include <QImage>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>
#include <QUrl>
#include <QSizeF>
#include <QRectF>
#include <QFileInfo>

PdfToImageNode::PdfToImageNode(QObject* parent)
    : QObject(parent)
{
}

NodeDescriptor PdfToImageNode::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("pdf-to-image");
    desc.name = QStringLiteral("PDF to Image");
    desc.category = QStringLiteral("Input / Output");

    // Input pin: PDF File
    PinDefinition inPin;
    inPin.direction = PinDirection::Input;
    inPin.id = QString::fromLatin1(kPdfPathPinId);
    inPin.name = QStringLiteral("PDF File");
    inPin.type = QStringLiteral("text");
    desc.inputPins.insert(inPin.id, inPin);

    // Output pin: Image
    PinDefinition outPin;
    outPin.direction = PinDirection::Output;
    outPin.id = QString::fromLatin1(kImagePathPinId);
    outPin.name = QStringLiteral("Image");
    outPin.type = QStringLiteral("image");
    desc.outputPins.insert(outPin.id, outPin);

    return desc;
}

QWidget* PdfToImageNode::createConfigurationWidget(QWidget* parent)
{
    if (!m_widget) {
        m_widget = new PdfToImagePropertiesWidget(parent);
        
        // Connect widget signal to update internal state
        connect(m_widget, &PdfToImagePropertiesWidget::pdfPathChanged, this, [this](const QString& path) {
            m_pdfPath = path;
        });
        
        // Initialize widget with current state
        if (!m_pdfPath.isEmpty()) {
            m_widget->setPdfPath(m_pdfPath);
        }
    }
    return m_widget;
}

TokenList PdfToImageNode::execute(const TokenList& incomingTokens)
{
    // Merge incoming tokens into a single DataPacket
    DataPacket inputs;
    for (const auto& token : incomingTokens) {
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            inputs.insert(it.key(), it.value());
        }
    }

    // Step 1: Resolve Temp Directory and Construct Template
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    if (tempDir.isEmpty()) {
        tempDir = QDir::tempPath();
    }
    QString templatePath = tempDir + QDir::separator() + QStringLiteral("pdf_stitched_XXXXXX.png");

    // Create a local temporary file for this execution.
    // setAutoRemove(false) ensures it persists for downstream nodes.
    QTemporaryFile tempFile(templatePath);
    tempFile.setAutoRemove(false);

    // Capture m_pdfPath for use during this execution
    const QString configuredPath = m_pdfPath;

    DataPacket output;

    // Step 2: Extract PDF path from input or use configured path (Source Mode)
    const QString pdfPathPinId = QString::fromLatin1(kPdfPathPinId);
    QString pdfPath;

    // Try to get path from input pin first
    if (inputs.contains(pdfPathPinId) && inputs.value(pdfPathPinId).isValid()) {
        pdfPath = inputs.value(pdfPathPinId).toString();
    }

    // If no input provided, use configured path (Source Mode)
    if (pdfPath.isEmpty()) {
        pdfPath = configuredPath;
    }

    // Instrumentation Logic
    CP_CLOG(PDF_DEBUG) << "Received raw input path:" << pdfPath;

    QFileInfo pdfInfo(pdfPath);
    QString absolutePdfPath = pdfInfo.absoluteFilePath();
    CP_CLOG(PDF_DEBUG) << "Absolute File Path:" << absolutePdfPath;
    CP_CLOG(PDF_DEBUG) << "Exists:" << pdfInfo.exists();
    CP_CLOG(PDF_DEBUG) << "Is Readable:" << pdfInfo.isReadable();
    CP_CLOG(PDF_DEBUG) << "Permissions:" << static_cast<int>(pdfInfo.permissions());

    // If still no path, return empty packet
    if (pdfPath.isEmpty()) {
        ExecutionToken token;
        token.data = output;
        return TokenList{token};
    }

    // Step 3: Load PDF document
    QPdfDocument pdfDoc;
    pdfDoc.load(pdfPath);

    // Step 4: Error handling - check if loading was successful
    if (pdfDoc.status() != QPdfDocument::Status::Ready) {
        // Log Load Result
        CP_CLOG(PDF_DEBUG) << "Failed to load PDF. Status:" << static_cast<int>(pdfDoc.status());
        
        if (pdfDoc.status() == QPdfDocument::Status::Error) {
            CP_CLOG(PDF_DEBUG) << "PDF Error:" << static_cast<int>(pdfDoc.error());
        }

        // Failed to load PDF - return empty packet
        ExecutionToken token;
        token.data = output;
        return TokenList{token};
    }

    const int pageCount = pdfDoc.pageCount();
    if (pageCount == 0) {
        ExecutionToken token;
        token.data = output;
        return TokenList{token};
    }

    // Step 5: Layout calculation - determine dimensions
    qreal totalHeight = 0.0;
    qreal maxWidth = 0.0;

    for (int i = 0; i < pageCount; ++i) {
        QSizeF pageSize = pdfDoc.pagePointSize(i);
        totalHeight += pageSize.height();
        if (pageSize.width() > maxWidth) {
            maxWidth = pageSize.width();
        }
    }

    // Step 6: Create the tall image for stitching
    // Using a reasonable DPI for rendering (72 points = 1 inch, use 2x for better quality)
    const qreal scale = 2.0;
    const int imageWidth = static_cast<int>(maxWidth * scale);
    const int imageHeight = static_cast<int>(totalHeight * scale);

    QImage stitchedImage(imageWidth, imageHeight, QImage::Format_ARGB32);
    stitchedImage.fill(Qt::white);

    // Step 7: Render and stitch pages
    QPainter painter(&stitchedImage);
    qreal currentY = 0.0;

    for (int i = 0; i < pageCount; ++i) {
        QSizeF pageSize = pdfDoc.pagePointSize(i);

        // Calculate image size for this page (in pixels)
        QSize pageImageSize(static_cast<int>(pageSize.width() * scale),
                           static_cast<int>(pageSize.height() * scale));

        // Render the page to a QImage
        QImage pageImage = pdfDoc.render(i, pageImageSize);

        // Draw the page image into the stitched image at the correct vertical offset
        painter.drawImage(QPointF(0, currentY * scale), pageImage);

        // Advance vertical offset
        currentY += pageSize.height();
    }

    painter.end();

    // Step 8: Save to temporary file
    if (!tempFile.open()) {
        CP_CLOG(PDF_DEBUG) << "Failed to open temporary file:" << tempFile.errorString();
        ExecutionToken token;
        token.data = output;
        return TokenList{token};
    }

    // Get the absolute path and close the handle so we can save by path as required
    QString outputPath = tempFile.fileName();
    tempFile.close();

    // Log the fix as required by the Task
    CP_CLOG(PDF_DEBUG) << "Successfully generated output path:" << outputPath;

    // Save the stitched image directly to the absolute path
    if (!stitchedImage.save(outputPath, "PNG")) {
        ExecutionToken token;
        token.data = output;
        return TokenList{token};
    }

    // Step 9: Return output with image path (MUST use absolute path for downstream nodes)
    const QString imagePathPinId = QString::fromLatin1(kImagePathPinId);
    output.insert(imagePathPinId, outputPath);

    ExecutionToken token;
    token.data = output;
    return TokenList{token};
}

QJsonObject PdfToImageNode::saveState() const
{
    QJsonObject obj;
    if (!m_pdfPath.isEmpty()) {
        obj["pdf_path"] = m_pdfPath;
    }
    return obj;
}

void PdfToImageNode::loadState(const QJsonObject& data)
{
    if (data.contains("pdf_path")) {
        m_pdfPath = data["pdf_path"].toString();
        
        // Update widget if it exists
        if (m_widget) {
            m_widget->setPdfPath(m_pdfPath);
        }
    }
}
