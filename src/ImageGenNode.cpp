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
#include "ImageGenNode.h"
#include "ImageGenPropertiesWidget.h"
#include "core/LLMProviderRegistry.h"
#include "backends/ILLMBackend.h"

#include <QJsonObject>
#include <QFileInfo>
#include <QVariant>
#include <QFuture>

ImageGenNode::ImageGenNode(QObject* parent)
    : QObject(parent)
    , m_providerId(QString::fromLatin1(kProviderOpenAI))
    , m_model(QStringLiteral("dall-e-3"))
    , m_size(QStringLiteral("1024x1024"))
    , m_quality(QStringLiteral("standard"))
    , m_style(QStringLiteral("vivid"))
{
}

NodeDescriptor ImageGenNode::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("image-gen");
    desc.name = QStringLiteral("Image Generator");
    desc.category = QStringLiteral("AI Services");

    // Input pin: prompt
    PinDefinition inPin;
    inPin.direction = PinDirection::Input;
    inPin.id = QString::fromLatin1(kInputPromptPinId);
    inPin.name = QStringLiteral("Prompt");
    inPin.type = QStringLiteral("text");
    desc.inputPins.insert(inPin.id, inPin);

    // Output pin: image path
    PinDefinition outPin;
    outPin.direction = PinDirection::Output;
    outPin.id = QString::fromLatin1(kOutputImagePathPinId);
    outPin.name = QStringLiteral("Image Path");
    outPin.type = QStringLiteral("image");
    desc.outputPins.insert(outPin.id, outPin);

    return desc;
}

QWidget* ImageGenNode::createConfigurationWidget(QWidget* parent)
{
    auto* widget = new ImageGenPropertiesWidget(parent);
    m_widget = widget;

    widget->setProvider(m_providerId);
    widget->setModel(m_model);
    widget->setSize(m_size);
    widget->setQuality(m_quality);
    widget->setStyle(m_style);

    connect(widget, &ImageGenPropertiesWidget::configChanged,
            this, &ImageGenNode::handleConfigChanged);

    return widget;
}

TokenList ImageGenNode::execute(const TokenList& incomingTokens)
{
    // Merge incoming tokens
    DataPacket inputs;
    for (const auto& token : incomingTokens) {
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            inputs.insert(it.key(), it.value());
        }
    }

    // Snapshot state for thread safety
    const QString providerId = m_providerId.trimmed().isEmpty()
        ? QString::fromLatin1(kProviderOpenAI)
        : m_providerId.trimmed().toLower();
    const QString model = m_model.trimmed().isEmpty()
        ? QStringLiteral("dall-e-3")
        : m_model.trimmed();
    const QString size = m_size.trimmed().isEmpty()
        ? QStringLiteral("1024x1024")
        : m_size.trimmed();
    const QString quality = m_quality.trimmed().isEmpty()
        ? QStringLiteral("standard")
        : m_quality.trimmed();
    const QString style = m_style.trimmed().isEmpty()
        ? QStringLiteral("vivid")
        : m_style.trimmed();

    const QString prompt = inputs.value(QString::fromLatin1(kInputPromptPinId)).toString().trimmed();
    const QString outputDir = inputs.value(QStringLiteral("_sys_node_output_dir")).toString();

    DataPacket output;
    const QString outputPinId = QString::fromLatin1(kOutputImagePathPinId);
    output.insert(outputPinId, QVariant());

    if (prompt.isEmpty()) {
        const QString err = QStringLiteral("ERROR: Prompt is empty.");
        output.insert(outputPinId, err);
        output.insert(QStringLiteral("__error"), err);

        ExecutionToken token; token.data = output; return TokenList{token};
    }

    ILLMBackend* backend = LLMProviderRegistry::instance().getBackend(providerId);
    if (!backend) {
        const QString err = QStringLiteral("ERROR: Backend '%1' not available.").arg(providerId);
        output.insert(outputPinId, err);
        output.insert(QStringLiteral("__error"), err);

        ExecutionToken token; token.data = output; return TokenList{token};
    }

    QString imagePath;
    try {
        QFuture<QString> future = backend->generateImage(prompt, model, size, quality, style, outputDir);
        imagePath = future.result();
    } catch (const std::exception& e) {
        imagePath = QStringLiteral("ERROR: Exception during image generation: %1").arg(QString::fromUtf8(e.what()));
    } catch (...) {
        imagePath = QStringLiteral("ERROR: Unknown exception during image generation.");
    }

    QFileInfo fileInfo(imagePath);
    if (imagePath.trimmed().isEmpty() || !fileInfo.exists()) {
        const QString err = imagePath.trimmed().isEmpty()
            ? QStringLiteral("ERROR: Image generation failed.")
            : imagePath;
        output.insert(outputPinId, err);
        output.insert(QStringLiteral("__error"), err);
    } else {
        output.insert(outputPinId, fileInfo.absoluteFilePath());
    }

    ExecutionToken token;
    token.data = output;
    return TokenList{token};
}

QJsonObject ImageGenNode::saveState() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("model"), m_model);
    obj.insert(QStringLiteral("size"), m_size);
    obj.insert(QStringLiteral("quality"), m_quality);
    obj.insert(QStringLiteral("style"), m_style);
    return obj;
}

void ImageGenNode::loadState(const QJsonObject& data)
{
    if (data.contains(QStringLiteral("model"))) {
        m_model = data.value(QStringLiteral("model")).toString(m_model);
    }
    if (data.contains(QStringLiteral("size"))) {
        m_size = data.value(QStringLiteral("size")).toString(m_size);
    }
    if (data.contains(QStringLiteral("quality"))) {
        m_quality = data.value(QStringLiteral("quality")).toString(m_quality);
    }
    if (data.contains(QStringLiteral("style"))) {
        m_style = data.value(QStringLiteral("style")).toString(m_style);
    }

    if (m_widget) {
        m_widget->setModel(m_model);
        m_widget->setSize(m_size);
        m_widget->setQuality(m_quality);
        m_widget->setStyle(m_style);
    }
}

void ImageGenNode::handleConfigChanged()
{
    if (!m_widget) return;

    const QString providerName = m_widget->provider().trimmed();
    m_providerId = providerName.isEmpty() ? QString::fromLatin1(kProviderOpenAI) : providerName.toLower();
    m_model = m_widget->model().trimmed();
    m_size = m_widget->size().trimmed();
    m_quality = m_widget->quality().trimmed();
    m_style = m_widget->style().trimmed();
}
