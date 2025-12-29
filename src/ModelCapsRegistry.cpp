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
#include "ModelCapsRegistry.h"

#include <QDebug>
#include <QLoggingCategory>
#include "logging_categories.h"
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>

using namespace ModelCapsTypes;

namespace {


const QRegularExpression trailingNegativeLookaheadRegex(QStringLiteral("\\(\\?!([^)]*)\\)\\s*$"));

QString normalizeEnumString(const QString& value)
{
    QString normalized = value.trimmed().toLower();
    normalized.remove(QLatin1Char('_'));
    normalized.remove(QLatin1Char('-'));
    normalized.remove(QLatin1Char(' '));
    return normalized;
}

std::optional<RoleMode> roleModeFromString(const QString& value)
{
    const QString normalized = normalizeEnumString(value);
    if (normalized == QStringLiteral("system")) {
        return RoleMode::System;
    }
    if (normalized == QStringLiteral("developer")) {
        return RoleMode::Developer;
    }
    if (normalized == QStringLiteral("systeminstruction")) {
        return RoleMode::SystemInstruction;
    }

    return std::nullopt;
}

std::optional<Capability> capabilityFromString(const QString& value)
{
    const QString normalized = normalizeEnumString(value);
    if (normalized == QStringLiteral("vision")) {
        return Capability::Vision;
    }
    if (normalized == QStringLiteral("reasoning")) {
        return Capability::Reasoning;
    }
    if (normalized == QStringLiteral("tooluse")) {
        return Capability::ToolUse;
    }
    if (normalized == QStringLiteral("longcontext")) {
        return Capability::LongContext;
    }
    if (normalized == QStringLiteral("audio")) {
        return Capability::Audio;
    }
    if (normalized == QStringLiteral("image")) {
        return Capability::Image;
    }
    if (normalized == QStringLiteral("structuredoutput")) {
        return Capability::StructuredOutput;
    }

    return std::nullopt;
}

std::optional<EndpointMode> endpointModeFromString(const QString& value)
{
    const QString normalized = normalizeEnumString(value);
    if (normalized == QStringLiteral("chat")) {
        return EndpointMode::Chat;
    }
    if (normalized == QStringLiteral("completion")) {
        return EndpointMode::Completion;
    }
    if (normalized == QStringLiteral("assistant") || normalized == QStringLiteral("assistants")) {
        return EndpointMode::Assistant;
    }
    return std::nullopt;
}

} // namespace

ModelCapsRegistry& ModelCapsRegistry::instance()
{
    static ModelCapsRegistry registry;
    return registry;
}

bool ModelCapsRegistry::loadFromFile(const QString& path)
{
    QWriteLocker writeLocker(&lock_);
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "ModelCapsRegistry: unable to open file" << path;
        return false;
    }

    const QByteArray payload = file.readAll();

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning() << "ModelCapsRegistry: failed to parse JSON" << parseError.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "ModelCapsRegistry: root JSON is not an object";
        return false;
    }

    const QJsonObject root = doc.object();
    const QJsonValue rulesValue = root.value(QStringLiteral("rules"));

    if (!rulesValue.isArray()) {
        qWarning() << "ModelCapsRegistry: 'rules' is missing or not an array";
        return false;
    }

    const QJsonArray rulesArray = rulesValue.toArray();
    QVector<ModelRule> parsedRules;
    parsedRules.reserve(rulesArray.size());

    for (const auto& ruleValue : rulesArray) {
        if (!ruleValue.isObject()) {
            qWarning() << "ModelCapsRegistry: skipping non-object rule entry";
            continue;
        }

        const QJsonObject ruleObj = ruleValue.toObject();

        const QJsonValue patternValue = ruleObj.value(QStringLiteral("pattern"));
        if (!patternValue.isString()) {
            qWarning() << "ModelCapsRegistry: skipping rule without string pattern";
            continue;
        }

        const QString patternString = patternValue.toString();
        QRegularExpression regex(patternString);
        if (!regex.isValid()) {
            qWarning() << "ModelCapsRegistry: invalid regex pattern" << patternString << "-" << regex.errorString();
            continue;
        }

        std::optional<QRegularExpression> trailingNegativeLookahead;
        if (const auto negativeMatch = trailingNegativeLookaheadRegex.match(patternString); negativeMatch.hasMatch()) {
            const QString negativePatternText = negativeMatch.captured(1);
            QRegularExpression negativeRegex(negativePatternText);

            if (!negativeRegex.isValid()) {
                qWarning() << "ModelCapsRegistry: invalid trailing negative lookahead" << negativePatternText << "-" << negativeRegex.errorString();
            } else {
                trailingNegativeLookahead = negativeRegex;
            }
        }

        ModelCaps caps;

        const auto roleModeValue = [ruleObj]() -> QJsonValue {
            const QJsonValue camel = ruleObj.value(QStringLiteral("roleMode"));
            if (camel.isUndefined()) {
                return ruleObj.value(QStringLiteral("role_mode"));
            }
            return camel;
        }();

        if (roleModeValue.isString()) {
            if (const auto role = roleModeFromString(roleModeValue.toString())) {
                caps.roleMode = *role;
            } else {
                qWarning() << "ModelCapsRegistry: unknown roleMode" << roleModeValue.toString();
            }
        }

        if (const QJsonValue capabilitiesValue = ruleObj.value(QStringLiteral("capabilities")); capabilitiesValue.isArray()) {
            QSet<Capability> capabilitySet;
            for (const auto& capVal : capabilitiesValue.toArray()) {
                if (!capVal.isString()) {
                    continue;
                }

                if (const auto cap = capabilityFromString(capVal.toString())) {
                    capabilitySet.insert(*cap);
                } else {
                    qWarning() << "ModelCapsRegistry: unknown capability" << capVal.toString();
                    qWarning().noquote() << QStringLiteral("Unknown capability string: %1").arg(capVal.toString());
                }
            }

            caps.capabilities = std::move(capabilitySet);
        }

        if (const QJsonValue disabledValue = ruleObj.value(QStringLiteral("disabledCapabilities")); disabledValue.isArray()) {
            for (const auto& capVal : disabledValue.toArray()) {
                if (!capVal.isString()) {
                    continue;
                }

                if (const auto cap = capabilityFromString(capVal.toString())) {
                    caps.capabilities.remove(*cap);
                } else {
                    qWarning() << "ModelCapsRegistry: unknown disabled capability" << capVal.toString();
                    qWarning().noquote() << QStringLiteral("Unknown capability string: %1").arg(capVal.toString());
                }
            }
        }

        const auto parseConstraints = [&caps](const QJsonObject& constraintsObj) {
            if (const QJsonValue maxInputTokens = constraintsObj.value(QStringLiteral("maxInputTokens")); maxInputTokens.isDouble()) {
                caps.constraints.maxInputTokens = maxInputTokens.toInt();
            }

            if (const QJsonValue maxOutputTokens = constraintsObj.value(QStringLiteral("maxOutputTokens")); maxOutputTokens.isDouble()) {
                caps.constraints.maxOutputTokens = maxOutputTokens.toInt();
            }

            if (const QJsonValue temperatureValue = constraintsObj.value(QStringLiteral("temperature")); temperatureValue.isObject()) {
                const QJsonObject tempObj = temperatureValue.toObject();
                TemperatureConstraint temp;

                if (const auto defaultVal = tempObj.value(QStringLiteral("default")); defaultVal.isDouble()) {
                    temp.defaultValue = defaultVal.toDouble();
                }
                if (const auto minVal = tempObj.value(QStringLiteral("min")); minVal.isDouble()) {
                    temp.min = minVal.toDouble();
                }
                if (const auto maxVal = tempObj.value(QStringLiteral("max")); maxVal.isDouble()) {
                    temp.max = maxVal.toDouble();
                }

                caps.constraints.temperature = temp;
            }

            if (const QJsonValue reasoningValue = constraintsObj.value(QStringLiteral("reasoning_effort")); reasoningValue.isObject()) {
                const QJsonObject reasoningObj = reasoningValue.toObject();
                ReasoningEffortConstraint re;

                if (const auto defaultVal = reasoningObj.value(QStringLiteral("default")); defaultVal.isString()) {
                    re.defaultValue = defaultVal.toString();
                }

                if (const auto allowedVal = reasoningObj.value(QStringLiteral("allowed")); allowedVal.isArray()) {
                    for (const auto& v : allowedVal.toArray()) {
                        if (v.isString()) {
                            re.allowed.push_back(v.toString());
                        }
                    }
                }

                caps.constraints.reasoningEffort = re;
            }

            // New hints for backend parameter shaping
            if (const QJsonValue omitTempVal = constraintsObj.value(QStringLiteral("omitTemperature")); omitTempVal.isBool()) {
                caps.constraints.omitTemperature = omitTempVal.toBool();
            }
            if (const QJsonValue tokenFieldVal = constraintsObj.value(QStringLiteral("tokenFieldName")); tokenFieldVal.isString()) {
                caps.constraints.tokenFieldName = tokenFieldVal.toString();
            }
        };

        if (const QJsonValue constraintsValue = ruleObj.value(QStringLiteral("constraints")); constraintsValue.isObject()) {
            parseConstraints(constraintsValue.toObject());
        }

        if (const QJsonValue paramConstraintsValue = ruleObj.value(QStringLiteral("parameter_constraints")); paramConstraintsValue.isObject()) {
            parseConstraints(paramConstraintsValue.toObject());
        }

        // Endpoint routing mode (safe default Chat if missing/invalid)
        if (const QJsonValue endpointVal = ruleObj.value(QStringLiteral("endpoint")); endpointVal.isString()) {
            if (const auto em = endpointModeFromString(endpointVal.toString())) {
                caps.endpointMode = *em;
            } else {
                caps.endpointMode = EndpointMode::Chat; // safe fallback
            }
        } else {
            caps.endpointMode = EndpointMode::Chat; // default
        }

        ModelRule rule;
        if (const QJsonValue idValue = ruleObj.value(QStringLiteral("id")); idValue.isString()) {
            rule.id = idValue.toString();
        }
        rule.pattern = regex;
        rule.caps = caps;

        if (const QJsonValue backendValue = ruleObj.value(QStringLiteral("backend")); backendValue.isString()) {
            rule.backend = backendValue.toString();
        }

        if (const QJsonValue priorityValue = ruleObj.value(QStringLiteral("priority")); priorityValue.isDouble()) {
            rule.priority = priorityValue.toInt();
        }

        rule.trailingNegativeLookahead = std::move(trailingNegativeLookahead);

        // Diagnostic: per‑rule details are useful only in debug logging.
        const int capsCount = caps.capabilities.size();
        const QString backendForLog = rule.backend.isEmpty() ? QStringLiteral("(any)") : rule.backend;
        const QString idForLog = rule.id.isEmpty() ? QStringLiteral("(no-id)") : rule.id;
        const char* endpointStr = (caps.endpointMode == EndpointMode::Chat)
                                      ? "chat"
                                      : (caps.endpointMode == EndpointMode::Completion ? "completion" : "assistant");
        qCDebug(cp_registry).noquote() << QStringLiteral("Loaded Rule [%1]: Pattern='%2', Backend='%3', Caps Count=%4, Endpoint=%5")
                                 .arg(idForLog, patternString, backendForLog, QString::number(capsCount), QString::fromLatin1(endpointStr));

        parsedRules.push_back(std::move(rule));
    }

    std::stable_sort(parsedRules.begin(), parsedRules.end(), [](const ModelRule& lhs, const ModelRule& rhs) {
        return lhs.priority > rhs.priority;
    });

    // Commit parsed rules
    rules_ = std::move(parsedRules);

    // Emit a concise summary at info level (categorized so it can be filtered via QT_LOGGING_RULES)
    int total = rules_.size();
    QHash<QString,int> perBackend;
    for (const auto& r : rules_) {
        perBackend[r.backend] += 1;
    }
    QStringList parts;
    for (auto it = perBackend.constBegin(); it != perBackend.constEnd(); ++it) {
        const QString b = it.key().isEmpty() ? QStringLiteral("(any)") : it.key();
        parts << QStringLiteral("%1=%2").arg(b).arg(it.value());
    }
    qCInfo(cp_registry).noquote() << QStringLiteral("ModelCapsRegistry: loaded %1 rules (%2)")
                             .arg(QString::number(total), parts.join(QStringLiteral(", ")));
    return true;
}

std::optional<ModelCapsRegistry::ResolvedCaps> ModelCapsRegistry::resolveWithRule(const QString& modelId, const QString& backendId) const
{
    QReadLocker readLocker(&lock_);
    // Instrumentation: introspect modelId at resolve entry to detect quoting issues
    const auto firstChar = modelId.isEmpty() ? QStringLiteral("∅") : modelId.left(1);
    const auto lastChar = modelId.isEmpty() ? QStringLiteral("∅") : modelId.right(1);
    qCDebug(cp_registry).noquote() << "[ModelLifecycle] Registry::resolve entry -> backend='" << (backendId.isEmpty()?QStringLiteral("(any)"):backendId)
                       << "' model='" << modelId << "' len=" << modelId.size()
                       << " first='" << firstChar << "' last='" << lastChar << "'";
    for (const auto& rule : rules_) {
        if (!backendId.isEmpty() && !rule.backend.isEmpty() && rule.backend != backendId) {
            continue;
        }

        const auto match = rule.pattern.match(modelId);
        if (match.hasMatch()) {
            if (rule.trailingNegativeLookahead.has_value()) {
                if (const auto& negative = *rule.trailingNegativeLookahead; negative.isValid() && negative.match(modelId).hasMatch()) {
                    continue;
                }
            }
            const bool hasVision = rule.caps.capabilities.contains(Capability::Vision);
            const bool hasReasoning = rule.caps.capabilities.contains(Capability::Reasoning);
            const QString idForLog = rule.id.isEmpty() ? QStringLiteral("(no-id)") : rule.id;
            qCDebug(cp_registry).noquote() << QStringLiteral("RESOLVE: Model '%1' matched Rule '%2' (Priority %3). Capabilities: Vision=%4, Reasoning=%5")
                                     .arg(modelId, idForLog, QString::number(rule.priority),
                                          hasVision ? QStringLiteral("T") : QStringLiteral("F"),
                                          hasReasoning ? QStringLiteral("T") : QStringLiteral("F"));
            ResolvedCaps rc { rule.caps, rule.id };
            return rc;
        }
    }
    qCDebug(cp_registry).noquote() << QStringLiteral("RESOLVE: Model '%1' hit FALLBACK.").arg(modelId);
    return std::nullopt;
}

std::optional<ModelCapsTypes::ModelCaps> ModelCapsRegistry::resolve(const QString& modelId, const QString& backendId) const
{
    if (const auto rc = resolveWithRule(modelId, backendId)) {
        return rc->caps;
    }
    return std::nullopt;
}
