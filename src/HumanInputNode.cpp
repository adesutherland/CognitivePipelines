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
#include "HumanInputNode.h"
#include "HumanInputPropertiesWidget.h"
#include "mainwindow.h"

#include <QtConcurrent/QtConcurrent>
#include <QJsonObject>
#include <QMetaObject>
#include <QFuture>
#include <QPromise>
#include <QTextEdit>
#include <QThread>
#include <QApplication>

HumanInputNode::HumanInputNode(QObject* parent)
    : QObject(parent)
{
}

NodeDescriptor HumanInputNode::GetDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("human-input");
    desc.name = QStringLiteral("Human Input");
    desc.category = QStringLiteral("I/O");

    // Input pin for prompt
    PinDefinition in;
    in.direction = PinDirection::Input;
    in.id = QString::fromLatin1(kInputId);
    in.name = QStringLiteral("Prompt");
    in.type = QStringLiteral("text");
    desc.inputPins.insert(in.id, in);

    // Output pin for text
    PinDefinition out;
    out.direction = PinDirection::Output;
    out.id = QString::fromLatin1(kOutputId);
    out.name = QStringLiteral("Text");
    out.type = QStringLiteral("text");
    desc.outputPins.insert(out.id, out);

    return desc;
}

QWidget* HumanInputNode::createConfigurationWidget(QWidget* parent)
{
    if (!m_propertiesWidget) {
        m_propertiesWidget = new HumanInputPropertiesWidget(parent);
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

QFuture<DataPacket> HumanInputNode::Execute(const DataPacket& inputs)
{
    const QString prompt = inputs.value(QString::fromLatin1(kInputId)).toString();
    // Remember the last prompt even if the widget is not created yet
    m_lastText = prompt;
    m_hasPendingText = (m_propertiesWidget == nullptr);

    // Forward prompt to properties widget on the UI thread safely
    if (m_propertiesWidget) {
        // Block the worker thread until the UI has processed the update to avoid
        // a race where the displayed text lags behind by one execution step.
        // Use a direct call if we're already on the same thread to avoid deadlocks.
        const bool crossThread = QThread::currentThread() != m_propertiesWidget->thread();
        const Qt::ConnectionType type = crossThread
                                        ? Qt::BlockingQueuedConnection
                                        : Qt::DirectConnection;
        QMetaObject::invokeMethod(m_propertiesWidget, "onSetText", type,
                                  Q_ARG(QString, prompt));
    }

    // Execute on worker thread, blocking to wait for user input
    return QtConcurrent::run([prompt]() -> DataPacket {
        // Use default prompt if none provided
        const QString effectivePrompt = prompt.isEmpty() 
                                        ? QStringLiteral("Please provide input:") 
                                        : prompt;

        // Get MainWindow instance
        MainWindow* mainWindow = nullptr;
        QWidget* activeWindow = QApplication::activeWindow();
        if (activeWindow) {
            mainWindow = qobject_cast<MainWindow*>(activeWindow);
        }
        if (!mainWindow) {
            // Fallback: search through top-level widgets
            for (QWidget* widget : QApplication::topLevelWidgets()) {
                mainWindow = qobject_cast<MainWindow*>(widget);
                if (mainWindow) break;
            }
        }

        QString returnedText;
        if (mainWindow) {
            // Call the slot on the main thread using BlockingQueuedConnection
            QMetaObject::invokeMethod(mainWindow, "requestUserInput",
                                     Qt::BlockingQueuedConnection,
                                     Q_RETURN_ARG(QString, returnedText),
                                     Q_ARG(QString, effectivePrompt));
        }

        // Create output packet with the returned text
        DataPacket output;
        output.insert(QString::fromLatin1(kOutputId), returnedText);
        return output;
    });
}

QJsonObject HumanInputNode::saveState() const
{
    QString textToSave = m_loadedText;
    if (!m_propertiesWidget && m_hasPendingText) {
        // If a value was received but the widget hasn't been created yet, prefer that
        textToSave = m_lastText;
    }
    if (m_propertiesWidget) {
        // Try to read the current contents directly from the QTextEdit
        if (auto* edit = m_propertiesWidget->findChild<QTextEdit*>()) {
            textToSave = edit->toPlainText();
        }
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("text"), textToSave);
    return obj;
}

void HumanInputNode::loadState(const QJsonObject& data)
{
    m_loadedText = data.value(QStringLiteral("text")).toString();

    if (m_propertiesWidget) {
        QMetaObject::invokeMethod(m_propertiesWidget, "onSetText", Qt::QueuedConnection,
                                  Q_ARG(QString, m_loadedText));
    }
}
