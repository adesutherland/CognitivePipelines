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
#include "ImageNode.h"
#include "ImagePropertiesWidget.h"

#include <QtConcurrent/QtConcurrent>
#include <QJsonObject>

ImageNode::ImageNode(QObject* parent)
    : QObject(parent)
{
}

void ImageNode::setImagePath(const QString& path)
{
    if (m_imagePath == path) return;
    m_imagePath = path;
    emit imagePathChanged(m_imagePath);
}

NodeDescriptor ImageNode::GetDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("image-node");
    desc.name = QStringLiteral("Image");
    desc.category = QStringLiteral("Input / Output");

    // One input pin
    PinDefinition in;
    in.direction = PinDirection::Input;
    in.id = QString::fromLatin1(kImagePinId);
    in.name = QStringLiteral("Input");
    in.type = QStringLiteral("image");
    desc.inputPins.insert(in.id, in);

    // One output pin
    PinDefinition out;
    out.direction = PinDirection::Output;
    out.id = QString::fromLatin1(kImagePinId);
    out.name = QStringLiteral("Output");
    out.type = QStringLiteral("image");
    desc.outputPins.insert(out.id, out);

    return desc;
}

QWidget* ImageNode::createConfigurationWidget(QWidget* parent)
{
    auto* w = new ImagePropertiesWidget(parent);
    // Store widget pointer for thread-safe UI updates from Execute
    m_widget = w;
    
    // Initialize from current state
    // Prefer m_lastExecutedPath if Execute has already run, otherwise use m_imagePath
    QString initialPath = m_lastExecutedPath.isEmpty() ? m_imagePath : m_lastExecutedPath;
    w->setImagePath(initialPath);

    // UI -> Node (live updates)
    QObject::connect(w, &ImagePropertiesWidget::imagePathChanged,
                     this, &ImageNode::setImagePath);

    // Node -> UI (reflect programmatic changes)
    QObject::connect(this, &ImageNode::imagePathChanged,
                     w, &ImagePropertiesWidget::setImagePath);

    return w;
}

QFuture<DataPacket> ImageNode::Execute(const DataPacket& inputs)
{
    const QString internalPath = m_imagePath;
    QPointer<ImagePropertiesWidget> widget = m_widget;
    QPointer<ImageNode> self(this);
    
    return QtConcurrent::run([inputs, internalPath, widget, self]() -> DataPacket {
        DataPacket output;
        
        // Step 1: Resolve Path
        // Check if input pin has data (Viewer Mode)
        const QString pinId = QString::fromLatin1(kImagePinId);
        QString resolvedPath;
        
        if (inputs.contains(pinId) && inputs.value(pinId).isValid()) {
            QVariant inputValue = inputs.value(pinId);
            // Viewer Mode: use input from upstream node
            if (inputValue.canConvert<QString>()) {
                resolvedPath = inputValue.toString();
            }
        }
        
        // If no valid input, use internal path (Source Mode)
        if (resolvedPath.isEmpty()) {
            resolvedPath = internalPath;
        }
        
        // Step 2: Store the resolved path for late widget initialization
        // If the widget is created AFTER Execute runs, it needs to know the last executed path
        if (self && !resolvedPath.isEmpty()) {
            QMetaObject::invokeMethod(self, [self, resolvedPath]() {
                if (self) {
                    self->m_lastExecutedPath = resolvedPath;
                }
            }, Qt::QueuedConnection);
        }
        
        // Step 3: Update UI (Thread-Safe)
        // The Execute method runs on a background thread, so we must use
        // QMetaObject::invokeMethod to call setImagePath on the main thread
        if (widget && !resolvedPath.isEmpty()) {
            QMetaObject::invokeMethod(widget, "setImagePath",
                                     Qt::QueuedConnection,
                                     Q_ARG(QString, resolvedPath));
        }
        
        // Step 3: Output
        // Send the resolved path to the output pin
        output.insert(pinId, resolvedPath);
        
        return output;
    });
}

QJsonObject ImageNode::saveState() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("imagePath"), m_imagePath);
    return obj;
}

void ImageNode::loadState(const QJsonObject& data)
{
    if (data.contains(QStringLiteral("imagePath"))) {
        setImagePath(data.value(QStringLiteral("imagePath")).toString());
    }
}
