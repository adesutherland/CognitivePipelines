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

#include "LLMConnector.h"

#include <QtConcurrent/QtConcurrent>
#include <QPointer>
#include <QProcessEnvironment>

#include "llm_api_client.h"

LLMConnector::LLMConnector(QObject* parent)
    : QObject(parent) {
}

NodeDescriptor LLMConnector::GetDescriptor() const {
    NodeDescriptor desc;
    desc.id = QStringLiteral("llm-connector");
    desc.name = QStringLiteral("LLM Connector");
    desc.category = QStringLiteral("Generative AI");

    PinDefinition inPrompt;
    inPrompt.direction = PinDirection::Input;
    inPrompt.id = QString::fromLatin1(kInputPromptId);
    inPrompt.name = QStringLiteral("Prompt");
    inPrompt.type = QStringLiteral("text");
    desc.inputPins.insert(inPrompt.id, inPrompt);

    PinDefinition outResponse;
    outResponse.direction = PinDirection::Output;
    outResponse.id = QString::fromLatin1(kOutputResponseId);
    outResponse.name = QStringLiteral("Response");
    outResponse.type = QStringLiteral("text");
    desc.outputPins.insert(outResponse.id, outResponse);

    return desc;
}

QWidget* LLMConnector::createConfigurationWidget(QWidget* parent) {
    Q_UNUSED(parent);
    return nullptr; // Configuration UI will be implemented in a later story
}

QFuture<DataPacket> LLMConnector::Execute(const DataPacket& inputs) {
    // Extract prompt from inputs
    const QVariant promptVar = inputs.value(QString::fromLatin1(kInputPromptId));
    const QString prompt = promptVar.toString();

    // Capture copies for background thread
    return QtConcurrent::run([prompt]() -> DataPacket {
        DataPacket output;

        // Read API key from environment
        QString apiKey = qEnvironmentVariable("OPENAI_API_KEY");
        if (apiKey.isEmpty()) {
            // Fallback placeholder (discouraged in production). Keep empty to signal error.
            // apiKey = QStringLiteral("REPLACE_WITH_API_KEY");
        }

        if (apiKey.isEmpty()) {
            output.insert(QString::fromLatin1(kOutputResponseId), QVariant(QStringLiteral("ERROR: OPENAI_API_KEY not set.")));
            return output;
        }

        LlmApiClient client;
        const std::string response = client.sendPrompt(apiKey.toStdString(), prompt.toStdString());
        output.insert(QString::fromLatin1(kOutputResponseId), QString::fromStdString(response));
        return output;
    });
}
