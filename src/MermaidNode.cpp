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
#include "MermaidNode.h"

#include "MermaidPropertiesWidget.h"
#include "MermaidRenderService.h"

#include <QDir>
#include <QFileInfo>
#include "Logger.h"
#include <QMetaObject>
#include <QStandardPaths>
#include <QUuid>

namespace {
QString normalizeCachePath(const QString& path)
{
    const QFileInfo info(path);
    const QString base = info.fileName();
    const QString parent = info.dir().dirName();
    if (!path.isEmpty() && !base.isEmpty() && base == parent) {
        return info.dir().absolutePath();
    }
    return path;
}
}

MermaidNode::MermaidNode(QObject* parent)
    : QObject(parent)
{
}

NodeDescriptor MermaidNode::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("mermaid-node");
    desc.name = QStringLiteral("Mermaid Renderer");
    desc.category = QStringLiteral("Visualization");

    PinDefinition in;
    in.direction = PinDirection::Input;
    in.id = QString::fromLatin1(kInputCode);
    in.name = QStringLiteral("Code");
    in.type = QStringLiteral("text");
    desc.inputPins.insert(in.id, in);

    PinDefinition out;
    out.direction = PinDirection::Output;
    out.id = QString::fromLatin1(kOutputImage);
    out.name = QStringLiteral("Image");
    out.type = QStringLiteral("image");
    desc.outputPins.insert(out.id, out);

    return desc;
}

QWidget* MermaidNode::createConfigurationWidget(QWidget* parent)
{
    auto* widget = new MermaidPropertiesWidget(parent);
    widget->setCode(m_lastCode);
    widget->setScale(m_scaleFactor);
    connect(widget, &MermaidPropertiesWidget::scaleChanged, this, [this](double value) {
        if (value < 0.1) value = 0.1;
        m_scaleFactor = value;
    });
    m_propertiesWidget = widget;
    return widget;
}

TokenList MermaidNode::execute(const TokenList& incomingTokens)
{
    DataPacket inputs;
    for (const auto& token : incomingTokens) {
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            inputs.insert(it.key(), it.value());
        }
    }

    const QString code = inputs.value(QString::fromLatin1(kInputCode)).toString();
    const QString trimmed = code.trimmed();

    DataPacket output;
    const QString outputPinId = QString::fromLatin1(kOutputImage);

    if (trimmed.isEmpty()) {
        const QString err = QStringLiteral("ERROR: Mermaid code is empty.");
        output.insert(outputPinId, err);
        output.insert(QStringLiteral("__error"), err);

        ExecutionToken token;
        token.data = output;
        return TokenList{token};
    }

    QString cacheDir = normalizeCachePath(QStandardPaths::writableLocation(QStandardPaths::CacheLocation));
    if (cacheDir.isEmpty()) {
        cacheDir = QDir::tempPath();
    }

    QDir dir(cacheDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    const QString fileName = QStringLiteral("mermaid_%1.png").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString outputPath = dir.filePath(fileName);

    if (m_scaleFactor < 0.1) {
        m_scaleFactor = 0.1;
    }

    const auto renderResult = MermaidRenderService::instance().renderMermaid(code, outputPath, m_scaleFactor);

    QFileInfo fileInfo(outputPath);
    if (!renderResult.ok || !fileInfo.exists()) {
        const QString detail = renderResult.error.isEmpty() ? QStringLiteral("Mermaid render failed.") : renderResult.error;
        const QString err = QStringLiteral("ERROR: %1").arg(detail);
        output.insert(outputPinId, err);
        output.insert(QStringLiteral("__error"), err);
    } else {
        output.insert(outputPinId, fileInfo.absoluteFilePath());
        if (!renderResult.detail.isEmpty()) {
            output.insert(QStringLiteral("__detail"), renderResult.detail);
        }
        if (renderResult.clamped && !renderResult.detail.isEmpty()) {
            output.insert(QStringLiteral("__warning"), renderResult.detail);
        }
    }

    m_lastCode = code;
    updatePropertiesWidget(code);

    ExecutionToken token;
    token.data = output;
    return TokenList{token};
}

QJsonObject MermaidNode::saveState() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("lastCode"), m_lastCode);
    obj.insert(QStringLiteral("scale"), m_scaleFactor);
    return obj;
}

void MermaidNode::loadState(const QJsonObject& data)
{
    if (data.contains(QStringLiteral("lastCode"))) {
        m_lastCode = data.value(QStringLiteral("lastCode")).toString();
    }
    if (data.contains(QStringLiteral("scale"))) {
        const double scale = data.value(QStringLiteral("scale")).toDouble();
        if (scale > 0.0) {
            m_scaleFactor = scale;
            if (m_scaleFactor < 0.1) {
                m_scaleFactor = 0.1;
            }
        }
    }
    updatePropertiesWidget(m_lastCode);
}

void MermaidNode::updatePropertiesWidget(const QString& code)
{
    auto* widget = m_propertiesWidget.data();
    if (!widget) return;

    const double scale = m_scaleFactor;

    QMetaObject::invokeMethod(widget, [widget, code, scale]() {
        widget->setCode(code);
        widget->setScale(scale);
    }, Qt::QueuedConnection);
}
