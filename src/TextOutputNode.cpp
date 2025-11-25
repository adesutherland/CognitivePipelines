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
#include "TextOutputNode.h"
#include "TextOutputPropertiesWidget.h"

#include <QtConcurrent/QtConcurrent>
#include <QJsonObject>
#include <QMetaObject>
#include <QFuture>
#include <QPromise>
#include <QTextEdit>
#include <QThread>

TextOutputNode::TextOutputNode(QObject* parent)
    : QObject(parent)
{
}

NodeDescriptor TextOutputNode::GetDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("text-output");
    desc.name = QStringLiteral("Text Output");
    desc.category = QStringLiteral("Output");

    // Single input pin consuming text
    PinDefinition in;
    in.direction = PinDirection::Input;
    in.id = QString::fromLatin1(kInputId);
    in.name = QStringLiteral("Text");
    in.type = QStringLiteral("text");
    desc.inputPins.insert(in.id, in);

    // No outputs

    return desc;
}

QWidget* TextOutputNode::createConfigurationWidget(QWidget* parent)
{
    if (!m_propertiesWidget) {
        m_propertiesWidget = new TextOutputPropertiesWidget(parent);
        // If we have a pending value from a previous Execute before the widget existed,
        // apply it immediately. Otherwise, fall back to any text loaded from state.
        if (m_hasPendingText) {
            m_propertiesWidget->onSetText(m_lastText);
            m_hasPendingText = false; // consumed
        } else if (!m_loadedText.isEmpty()) {
            QMetaObject::invokeMethod(m_propertiesWidget, "onSetText", Qt::QueuedConnection,
                                      Q_ARG(QString, m_loadedText));
        }
    } else if (m_propertiesWidget->parent() != parent && parent) {
        m_propertiesWidget->setParent(parent);
    }
    return m_propertiesWidget;
}

QFuture<DataPacket> TextOutputNode::Execute(const DataPacket& inputs)
{
    const QString text = inputs.value(QString::fromLatin1(kInputId)).toString();
    // Remember the last text even if the widget is not created yet (fan-out first run case)
    m_lastText = text;
    m_hasPendingText = (m_propertiesWidget == nullptr);

    // Forward to properties widget on the UI thread safely
    if (m_propertiesWidget) {
        // Block the worker thread until the UI has processed the update to avoid
        // a race where the displayed text lags behind by one execution step.
        // Use a direct call if we're already on the same thread to avoid deadlocks.
        const bool crossThread = QThread::currentThread() != m_propertiesWidget->thread();
        const Qt::ConnectionType type = crossThread
                                        ? Qt::BlockingQueuedConnection
                                        : Qt::DirectConnection;
        QMetaObject::invokeMethod(m_propertiesWidget, "onSetText", type,
                                  Q_ARG(QString, text));
    }

    // Return an immediately-finished future with empty output packet (sink node)
    QPromise<DataPacket> promise;
    promise.addResult(DataPacket{});
    promise.finish();
    return promise.future();
}

QJsonObject TextOutputNode::saveState() const
{
    // TextOutputNode is a display-only sink node that shows runtime results.
    // It should never persist its content to saved files.
    // Always return empty state.
    QJsonObject obj;
    obj.insert(QStringLiteral("text"), QString());
    return obj;
}

void TextOutputNode::loadState(const QJsonObject& data)
{
    m_loadedText = data.value(QStringLiteral("text")).toString();

    if (m_propertiesWidget) {
        QMetaObject::invokeMethod(m_propertiesWidget, "onSetText", Qt::QueuedConnection,
                                  Q_ARG(QString, m_loadedText));
    }
}

void TextOutputNode::clearOutput()
{
    // Clear all internal state
    m_lastText.clear();
    m_loadedText.clear();
    m_hasPendingText = false;

    // Clear the widget display if it exists
    // Use immediate invocation to ensure the widget is cleared before saveState() is called
    if (m_propertiesWidget) {
        const bool crossThread = QThread::currentThread() != m_propertiesWidget->thread();
        const Qt::ConnectionType type = crossThread
                                        ? Qt::BlockingQueuedConnection
                                        : Qt::DirectConnection;
        QMetaObject::invokeMethod(m_propertiesWidget, "onSetText", type,
                                  Q_ARG(QString, QString()));
    }
}
