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
#include <QRegularExpression>
#include <QSet>

PromptBuilderNode::PromptBuilderNode(QObject* parent)
    : QObject(parent)
{
}

void PromptBuilderNode::setTemplateText(const QString& text)
{
    if (m_template == text) {
        return;
    }

    // Parse variables from the provided template and route through the main slot
    static const QRegularExpression re(QStringLiteral("\\{([^{}]+)\\}"));
    QStringList vars;
    QSet<QString> seen;
    auto it = re.globalMatch(text);
    while (it.hasNext()) {
        const auto m = it.next();
        const QString var = m.captured(1).trimmed();
        if (!var.isEmpty() && !seen.contains(var)) {
            seen.insert(var);
            vars.append(var);
        }
    }
    if (vars.isEmpty()) {
        // Keep a convenient default variable for quick usage
        vars.append(QString::fromLatin1(kInputId));
    }
    onTemplateChanged(text, vars);
}

void PromptBuilderNode::onTemplateChanged(const QString& newTemplate, const QStringList& newVariables)
{
    // Notify delegate to update ports first
    emit inputPinsUpdateRequested(newVariables);

    // Update internal state and notify UI
    m_template = newTemplate;
    m_variables = newVariables;
    emit templateTextChanged(m_template);
}

NodeDescriptor PromptBuilderNode::GetDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("prompt-builder");
    desc.name = QStringLiteral("Prompt Builder");
    desc.category = QStringLiteral("Text");

    // Only declare the static output pin here. Inputs are dynamic and managed via ToolNodeDelegate.
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
    w->setTemplateText(m_template);

    // UI -> Node (live updates)
    QObject::connect(w, &PromptBuilderPropertiesWidget::templateChanged,
                     this, &PromptBuilderNode::onTemplateChanged);
    // Node -> UI (reflect programmatic changes)
    QObject::connect(this, &PromptBuilderNode::templateTextChanged,
                     w, &PromptBuilderPropertiesWidget::setTemplateText);

    return w;
}

QFuture<DataPacket> PromptBuilderNode::Execute(const DataPacket& inputs)
{
    // Capture state for background execution
    const QString tpl = m_template;
    const QStringList vars = m_variables;

    return QtConcurrent::run([tpl, vars, inputs]() -> DataPacket {
        DataPacket output;
        QString result = tpl;
        for (const QString& var : vars) {
            const QString key = QStringLiteral("{") + var + QStringLiteral("}");
            const QString value = inputs.value(var).toString();
            result.replace(key, value);
        }
        output.insert(QString::fromLatin1(kOutputId), result);
        return output;
    });
}


QJsonObject PromptBuilderNode::saveState() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("template"), m_template);
    return obj;
}

void PromptBuilderNode::loadState(const QJsonObject& data)
{
    if (data.contains(QStringLiteral("template"))) {
        setTemplateText(data.value(QStringLiteral("template")).toString());
    }
}
