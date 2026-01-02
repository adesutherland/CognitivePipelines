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
#pragma once

#include <optional>

#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>

namespace ModelCapsTypes {
Q_NAMESPACE

// Endpoint routing metadata
// -------------------------
// Indicates which HTTP API family a model expects when talking to
// OpenAI-compatible backends.
// JSON (per rule): "endpoint": "chat" | "completion" | "assistant"
// Safe default when missing/invalid: Chat
enum class EndpointMode {
    Chat,       // /v1/chat/completions (default)
    Completion, // /v1/completions
    Assistant   // /v1/assistants (beta header required)
};

enum class RoleMode {
    System,
    Developer,
    SystemInstruction,
    SystemParameter
};
Q_ENUM_NS(RoleMode)

enum class Capability {
    Vision,
    Reasoning,
    ToolUse,
    LongContext,
    Audio,
    Image,
    StructuredOutput
};
Q_ENUM_NS(Capability)

struct TemperatureConstraint {
    std::optional<double> defaultValue;
    std::optional<double> min;
    std::optional<double> max;
};

struct ReasoningEffortConstraint {
    std::optional<QString> defaultValue;
    QStringList allowed;
};

struct ParameterConstraints {
    std::optional<int> maxInputTokens;
    std::optional<int> maxOutputTokens;
    std::optional<TemperatureConstraint> temperature;
    std::optional<ReasoningEffortConstraint> reasoningEffort;
    // Hints for backend parameter shaping
    std::optional<bool> omitTemperature;           // If true, do not send temperature regardless of other flags
    std::optional<QString> tokenFieldName;         // e.g., "max_completion_tokens" or "max_tokens"
};

struct ModelCaps {
    EndpointMode endpointMode { EndpointMode::Chat };
    RoleMode roleMode { RoleMode::System };
    QSet<Capability> capabilities;
    ParameterConstraints constraints {};
    QMap<QString, QString> customHeaders;

    bool hasCapability(Capability c) const;
};

struct ModelRule {
    QString id;
    QRegularExpression pattern;
    ModelCaps caps;
    QString backend;
    int priority { 0 };
    std::optional<QRegularExpression> trailingNegativeLookahead;
};

struct VirtualModel {
    QString id;      // The alias
    QString target;  // The real model ID
    QString backend; // Optional backend filter
    QString name;    // UI Display name
};

} // namespace ModelCapsTypes
