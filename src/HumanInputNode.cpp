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
        // Load the default prompt into the widget
        m_propertiesWidget->setDefaultPrompt(m_defaultPrompt);
        // Connect signal to update m_defaultPrompt when user edits
        connect(m_propertiesWidget, &HumanInputPropertiesWidget::defaultPromptChanged,
                this, &HumanInputNode::onDefaultPromptChanged);
    } else if (m_propertiesWidget->parent() != parent && parent) {
        m_propertiesWidget->setParent(parent);
    }
    return m_propertiesWidget;
}

QFuture<DataPacket> HumanInputNode::Execute(const DataPacket& inputs)
{
    // Retrieve text from input pin
    const QString inputPrompt = inputs.value(QString::fromLatin1(kInputId)).toString();
    
    // Determine effective prompt: input pin > default prompt > hardcoded fallback
    QString effectivePrompt;
    if (!inputPrompt.isEmpty()) {
        effectivePrompt = inputPrompt;
    } else if (!m_defaultPrompt.isEmpty()) {
        effectivePrompt = m_defaultPrompt;
    } else {
        effectivePrompt = QStringLiteral("Please provide input:");
    }

    // Execute on worker thread, blocking to wait for user input
    return QtConcurrent::run([effectivePrompt]() -> DataPacket {
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
        bool accepted = false;
        if (mainWindow) {
            // Call the slot on the main thread using BlockingQueuedConnection
            QMetaObject::invokeMethod(mainWindow, "requestUserInput",
                                     Qt::BlockingQueuedConnection,
                                     Q_RETURN_ARG(bool, accepted),
                                     Q_ARG(QString, effectivePrompt),
                                     Q_ARG(QString&, returnedText));
        }

        // Create output packet
        DataPacket output;
        
        // If user canceled, return error to stop the pipeline
        if (!accepted) {
            output.insert(QStringLiteral("__error"), QStringLiteral("User canceled input"));
        } else {
            // User accepted: return the text
            output.insert(QString::fromLatin1(kOutputId), returnedText);
        }
        
        return output;
    });
}

QJsonObject HumanInputNode::saveState() const
{
    QString textToSave = m_defaultPrompt;
    
    // If widget exists, read current value from it (user may have edited it)
    if (m_propertiesWidget) {
        textToSave = m_propertiesWidget->defaultPrompt();
    }

    QJsonObject obj;
    obj.insert(QStringLiteral("default_prompt"), textToSave);
    return obj;
}

void HumanInputNode::loadState(const QJsonObject& data)
{
    // Load default prompt with backward compatibility
    if (data.contains(QStringLiteral("default_prompt"))) {
        m_defaultPrompt = data.value(QStringLiteral("default_prompt")).toString();
    } else if (data.contains(QStringLiteral("text"))) {
        // Backward compatibility: migrate old "text" key to new meaning
        m_defaultPrompt = data.value(QStringLiteral("text")).toString();
    }

    // Update widget if it already exists
    if (m_propertiesWidget) {
        m_propertiesWidget->setDefaultPrompt(m_defaultPrompt);
    }
}

void HumanInputNode::onDefaultPromptChanged(const QString& text)
{
    m_defaultPrompt = text;
}
