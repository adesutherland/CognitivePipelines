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
#include "TextInputNode.h"
#include "TextInputPropertiesWidget.h"

#include <QtConcurrent/QtConcurrent>

TextInputNode::TextInputNode(QObject* parent)
    : QObject(parent)
{
}

void TextInputNode::setText(const QString& text)
{
    if (m_text == text) return;
    m_text = text;
    emit textChanged(m_text);
}

NodeDescriptor TextInputNode::GetDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("text-input");
    desc.name = QStringLiteral("Text Input");
    desc.category = QStringLiteral("Inputs");

    // No input pins

    // One output pin
    PinDefinition out;
    out.direction = PinDirection::Output;
    out.id = QString::fromLatin1(kOutputId);
    out.name = QStringLiteral("Text");
    out.type = QStringLiteral("text");
    desc.outputPins.insert(out.id, out);

    return desc;
}

QWidget* TextInputNode::createConfigurationWidget(QWidget* parent)
{
    auto* w = new TextInputPropertiesWidget(parent);
    // Initialize from current state
    w->setText(m_text);

    // UI -> Node (live updates)
    QObject::connect(w, &TextInputPropertiesWidget::textChanged,
                     this, &TextInputNode::setText);

    // Node -> UI (reflect programmatic changes)
    QObject::connect(this, &TextInputNode::textChanged,
                     w, &TextInputPropertiesWidget::setText);

    return w;
}

QFuture<DataPacket> TextInputNode::Execute(const DataPacket& /*inputs*/)
{
    const QString value = m_text;
    return QtConcurrent::run([value]() -> DataPacket {
        DataPacket output;
        output.insert(QString::fromLatin1(kOutputId), value);
        return output;
    });
}
