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
#include "PromptBuilderNode.h"
#include "PromptBuilderPropertiesWidget.h"

#include <QtConcurrent/QtConcurrent>
#include <QJsonObject>

PromptBuilderNode::PromptBuilderNode(QObject* parent)
    : QObject(parent)
{
}

void PromptBuilderNode::setTemplateText(const QString& text)
{
    if (m_templateText == text) return;
    m_templateText = text;
    emit templateTextChanged(m_templateText);
}

NodeDescriptor PromptBuilderNode::GetDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("prompt-builder");
    desc.name = QStringLiteral("Prompt Builder");
    desc.category = QStringLiteral("Text");

    PinDefinition in;
    in.direction = PinDirection::Input;
    in.id = QString::fromLatin1(kInputId);
    in.name = QStringLiteral("Input");
    in.type = QStringLiteral("text");
    desc.inputPins.insert(in.id, in);

    PinDefinition out;
    out.direction = PinDirection::Output;
    out.id = QString::fromLatin1(kOutputId);
    out.name = QStringLiteral("Prompt");
    out.type = QStringLiteral("text");
    desc.outputPins.insert(out.id, out);

    return desc;
}

QWidget* PromptBuilderNode::createConfigurationWidget(QWidget* parent)
{
    auto* w = new PromptBuilderPropertiesWidget(parent);
    // Initialize from current state
    w->setTemplateText(m_templateText);

    // UI -> Node (live updates)
    QObject::connect(w, &PromptBuilderPropertiesWidget::templateChanged,
                     this, &PromptBuilderNode::setTemplateText);
    // Node -> UI (reflect programmatic changes)
    QObject::connect(this, &PromptBuilderNode::templateTextChanged,
                     w, &PromptBuilderPropertiesWidget::setTemplateText);

    return w;
}

QFuture<DataPacket> PromptBuilderNode::Execute(const DataPacket& inputs)
{
    // Capture state for background execution
    const QString tpl = m_templateText;
    const QString inputText = inputs.value(QString::fromLatin1(kInputId)).toString();

    return QtConcurrent::run([tpl, inputText]() -> DataPacket {
        DataPacket output;
        QString result = tpl;
        // simple replacement; if no placeholder, just append? requirement: replace {input}
        result.replace(QStringLiteral("{input}"), inputText);
        output.insert(QString::fromLatin1(kOutputId), result);
        return output;
    });
}


QJsonObject PromptBuilderNode::saveState() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("template"), m_templateText);
    return obj;
}

void PromptBuilderNode::loadState(const QJsonObject& data)
{
    if (data.contains(QStringLiteral("template"))) {
        setTemplateText(data.value(QStringLiteral("template")).toString());
    }
}
