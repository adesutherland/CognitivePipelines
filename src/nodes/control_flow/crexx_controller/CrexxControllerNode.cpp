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

#include "CrexxControllerNode.h"

#include "ExecutionScriptHost.h"
#include "IScriptHost.h"
#include "UniversalScriptPropertiesWidget.h"

#include <QFormLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QMutexLocker>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QVBoxLayout>

#include <memory>

namespace {
constexpr int kMaxIterationsLowerBound = 1;
constexpr int kMaxIterationsUpperBound = 100;

bool variantHasText(const QVariant& value)
{
    return value.isValid() && !value.isNull() && !value.toString().trimmed().isEmpty();
}

QVariantList outputAsList(const QVariant& value)
{
    if (value.typeId() == QMetaType::QVariantList || value.typeId() == QMetaType::QStringList) {
        return value.toList();
    }
    if (value.isValid() && !value.isNull()) {
        return QVariantList{value};
    }
    return {};
}

QString quickJsControllerScript()
{
    return QStringLiteral(
        "// JavaScript controller policy.\n"
        "// Expected loop: Start -> Creator -> Validator -> Done or Creator retry.\n"
        "// Put your validation/routing rules in the validator phase below.\n"
        "const phase = pipeline.input(\"phase\");\n"
        "\n"
        "if (phase === \"start\") {\n"
        "  const brief = pipeline.input(\"start\");\n"
        "  pipeline.output(\"route\", \"creator\");\n"
        "  pipeline.output(\"to_creator\", brief);\n"
        "  pipeline.output(\"status\", \"Initial brief sent to creator\");\n"
        "  return;\n"
        "}\n"
        "\n"
        "if (phase === \"from_creator\") {\n"
        "  const draft = pipeline.input(\"from_creator\");\n"
        "  pipeline.output(\"route\", \"validator\");\n"
        "  pipeline.output(\"to_validator\", draft);\n"
        "  pipeline.output(\"status\", \"Creator result sent to validator\");\n"
        "  return;\n"
        "}\n"
        "\n"
        "const validatorFeedback = String(pipeline.input(\"from_validator\") || \"\");\n"
        "const verdict = validatorFeedback.toUpperCase();\n"
        "\n"
        "// Put validator acceptance rules here.\n"
        "const accepted = verdict.includes(\"PASS\") || verdict.includes(\"OK\") || verdict.includes(\"APPROVED\");\n"
        "if (accepted) {\n"
        "  pipeline.output(\"route\", \"done\");\n"
        "  pipeline.output(\"done\", pipeline.input(\"last_creator_result\") || pipeline.input(\"from_validator\"));\n"
        "  pipeline.output(\"status\", \"Accepted by validator\");\n"
        "  return;\n"
        "}\n"
        "\n"
        "// Put retry prompt/rules here. This payload goes back to the creator.\n"
        "const retryPrompt = [\n"
        "  \"Revise the previous output using this validator feedback:\",\n"
        "  validatorFeedback,\n"
        "  \"\",\n"
        "  \"Previous creator output:\",\n"
        "  pipeline.input(\"last_creator_result\") || \"\"\n"
        "].join(\"\\n\");\n"
        "pipeline.output(\"route\", \"creator\");\n"
        "pipeline.output(\"to_creator\", retryPrompt);\n"
        "pipeline.output(\"status\", \"Validator requested another creator attempt\");\n");
}

QString controllerScriptForEngine(const QString& engineId)
{
    if (engineId.compare(QStringLiteral("quickjs"), Qt::CaseInsensitive) == 0) {
        return quickJsControllerScript();
    }
    return CrexxControllerNode::defaultScript();
}

bool isManagedControllerScript(const QString& script)
{
    const QString normalized = script.trimmed();
    return normalized.isEmpty() ||
           normalized == CrexxControllerNode::defaultScript().trimmed() ||
           normalized == quickJsControllerScript().trimmed();
}
} // namespace

CrexxControllerNode::CrexxControllerNode(QObject* parent)
    : QObject(parent)
    , m_scriptCode(defaultScript())
{
}

QString CrexxControllerNode::defaultScript()
{
    return QStringLiteral(
        "/* cREXX controller policy.\n"
        "   Expected loop: Start -> Creator -> Validator -> Done or Creator retry.\n"
        "   Put your validation/routing rules in the validator phase below.\n"
        "\n"
        "   Named pin API:\n"
        "     address pipeline \"GET phase INTO :phase\"\n"
        "     address pipeline \"SET to_creator :payload\"\n"
        "     address pipeline \"SET route creator\"\n"
        "\n"
        "   Inputs: phase, start, from_creator, from_validator,\n"
        "           iteration, max_iterations, last_creator_payload,\n"
        "           last_creator_result, last_validator_result.\n"
        "   Outputs: route plus to_creator, to_validator, done, status.\n"
        "*/\n"
        "\n"
        "phase = \"\"\n"
        "address pipeline \"GET phase INTO :phase\"\n"
        "\n"
        "if phase = \"start\" then do\n"
        "    brief = \"\"\n"
        "    address pipeline \"GET start INTO :brief\"\n"
        "    address pipeline \"SET route creator\"\n"
        "    address pipeline \"SET to_creator :brief\"\n"
        "    address pipeline \"SET status Initial brief sent to creator\"\n"
        "    return 0\n"
        "end\n"
        "\n"
        "if phase = \"from_creator\" then do\n"
        "    brief = \"\"\n"
        "    draft = \"\"\n"
        "    address pipeline \"GET start INTO :brief\"\n"
        "    address pipeline \"GET from_creator INTO :draft\"\n"
        "    payload = \"Question is \" || brief || x2c(\"0A0A\") || \"Answer was \" || draft\n"
        "    address pipeline \"SET route validator\"\n"
        "    address pipeline \"SET to_validator :payload\"\n"
        "    address pipeline \"SET status Creator result sent to validator\"\n"
        "    return 0\n"
        "end\n"
        "\n"
        "validator_feedback = \"\"\n"
        "address pipeline \"GET from_validator INTO :validator_feedback\"\n"
        "verdict = upper(validator_feedback)\n"
        "accepted = 0\n"
        "\n"
        "/* Put validator acceptance rules here. */\n"
        "if pos(\"PASS\", verdict) > 0 then accepted = 1\n"
        "if pos(\"OK\", verdict) > 0 then accepted = 1\n"
        "if pos(\"APPROVED\", verdict) > 0 then accepted = 1\n"
        "\n"
        "if accepted = 1 then do\n"
        "    payload = \"\"\n"
        "    address pipeline \"GET last_creator_result INTO :payload\"\n"
        "    if payload = \"\" then do\n"
        "        address pipeline \"GET from_validator INTO :payload\"\n"
        "    end\n"
        "    address pipeline \"SET route done\"\n"
        "    address pipeline \"SET done :payload\"\n"
        "    address pipeline \"SET status Accepted by validator\"\n"
        "    return 0\n"
        "end\n"
        "\n"
        "/* Put retry prompt/rules here. This payload goes back to the creator. */\n"
        "previous = \"\"\n"
        "address pipeline \"GET last_creator_result INTO :previous\"\n"
        "payload = \"Revise the previous output using this validator feedback:\" || x2c(\"0A\") || validator_feedback || x2c(\"0A0A\") || \"Previous creator output:\" || x2c(\"0A\") || previous\n"
        "address pipeline \"SET route creator\"\n"
        "address pipeline \"SET to_creator :payload\"\n"
        "address pipeline \"SET status Validator requested another creator attempt\"\n");
}

NodeDescriptor CrexxControllerNode::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("crexx-controller");
    desc.name = QStringLiteral("cREXX Controller");
    desc.category = QStringLiteral("Control Flow");

    PinDefinition start;
    start.direction = PinDirection::Input;
    start.id = QString::fromLatin1(kInputStartId);
    start.name = start.id;
    start.type = QStringLiteral("text");
    desc.inputPins.insert(start.id, start);
    desc.inputPinOrder << start.id;

    PinDefinition feedback;
    feedback.direction = PinDirection::Input;
    feedback.id = QString::fromLatin1(kInputCreatorResultId);
    feedback.name = feedback.id;
    feedback.type = QStringLiteral("text");
    desc.inputPins.insert(feedback.id, feedback);
    desc.inputPinOrder << feedback.id;

    PinDefinition validatorFeedback;
    validatorFeedback.direction = PinDirection::Input;
    validatorFeedback.id = QString::fromLatin1(kInputValidatorResultId);
    validatorFeedback.name = validatorFeedback.id;
    validatorFeedback.type = QStringLiteral("text");
    desc.inputPins.insert(validatorFeedback.id, validatorFeedback);
    desc.inputPinOrder << validatorFeedback.id;

    PinDefinition creator;
    creator.direction = PinDirection::Output;
    creator.id = QString::fromLatin1(kOutputCreatorId);
    creator.name = creator.id;
    creator.type = QStringLiteral("text");
    desc.outputPins.insert(creator.id, creator);
    desc.outputPinOrder << creator.id;

    PinDefinition validator;
    validator.direction = PinDirection::Output;
    validator.id = QString::fromLatin1(kOutputValidatorId);
    validator.name = validator.id;
    validator.type = QStringLiteral("text");
    desc.outputPins.insert(validator.id, validator);
    desc.outputPinOrder << validator.id;

    PinDefinition done;
    done.direction = PinDirection::Output;
    done.id = QString::fromLatin1(kOutputDoneId);
    done.name = done.id;
    done.type = QStringLiteral("text");
    desc.outputPins.insert(done.id, done);
    desc.outputPinOrder << done.id;

    return desc;
}

QWidget* CrexxControllerNode::createConfigurationWidget(QWidget* parent)
{
    auto* root = new QWidget(parent);
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(8);

    auto* help = new QLabel(root);
    help->setWordWrap(true);
    help->setText(tr("The controller script runs on start, creator result, and validator result. "
                     "It can route to creator, validator, done, or error."));
    layout->addWidget(help);

    auto* form = new QFormLayout();
    auto* maxSpin = new QSpinBox(root);
    maxSpin->setRange(kMaxIterationsLowerBound, kMaxIterationsUpperBound);
    maxSpin->setValue(maxIterations());
    form->addRow(tr("Max iterations"), maxSpin);
    layout->addLayout(form);

    auto* scriptWidget = new UniversalScriptPropertiesWidget(root);
    scriptWidget->setScript(scriptCode());
    scriptWidget->setEngineId(engineId());
    scriptWidget->setFanOut(false);
    scriptWidget->setFanOutVisible(false);
    scriptWidget->setSyntaxHighlighting(m_enableSyntaxHighlighting);
    scriptWidget->setPinEditorsVisible(false);
    layout->addWidget(scriptWidget);

    auto* exampleButton = new QPushButton(tr("Use Example"), root);
    layout->addWidget(exampleButton);

    connect(maxSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &CrexxControllerNode::setMaxIterations);
    connect(this, &CrexxControllerNode::maxIterationsChanged, maxSpin, [maxSpin](int value) {
        QSignalBlocker blocker(maxSpin);
        maxSpin->setValue(value);
    });

    connect(scriptWidget, &UniversalScriptPropertiesWidget::scriptChanged,
            this, &CrexxControllerNode::setScriptCode);
    connect(scriptWidget, &UniversalScriptPropertiesWidget::engineChanged,
            this, &CrexxControllerNode::setEngineId);
    connect(scriptWidget, &UniversalScriptPropertiesWidget::syntaxHighlightingChanged,
            this, [this](bool enabled) { m_enableSyntaxHighlighting = enabled; });
    connect(this, &CrexxControllerNode::scriptCodeChanged,
            scriptWidget, &UniversalScriptPropertiesWidget::setScript);
    connect(this, &CrexxControllerNode::engineIdChanged,
            scriptWidget, &UniversalScriptPropertiesWidget::setEngineId);
    connect(exampleButton, &QPushButton::clicked, this, [this]() {
        setScriptCode(controllerScriptForEngine(engineId()));
    });

    return root;
}

bool CrexxControllerNode::isReady(const QVariantMap& inputs, int incomingConnectionsCount) const
{
    Q_UNUSED(incomingConnectionsCount);
    return inputs.contains(QString::fromLatin1(kInputStartId)) ||
           inputs.contains(QString::fromLatin1(kInputCreatorResultId)) ||
           inputs.contains(QString::fromLatin1(kInputValidatorResultId));
}

TokenList CrexxControllerNode::execute(const TokenList& incomingTokens)
{
    QMutexLocker locker(&m_mutex);

    const QString startKey = QString::fromLatin1(kInputStartId);
    const QString creatorKey = QString::fromLatin1(kInputCreatorResultId);
    const QString validatorKey = QString::fromLatin1(kInputValidatorResultId);
    TokenList outputs;

    for (const auto& token : incomingTokens) {
        if (isStartTrigger(token, startKey)) {
            m_startQueue.push_back(token.data.value(startKey));
        }
    }

    if (m_isProcessing) {
        for (const auto& token : incomingTokens) {
            const bool creatorTriggered = isCreatorTrigger(token, creatorKey);
            const bool validatorTriggered = isValidatorTrigger(token, validatorKey);
            if (!creatorTriggered && !validatorTriggered) continue;

            QString phase;
            QVariant creatorResult;
            QVariant validatorResult;
            if (creatorTriggered) {
                phase = QString::fromLatin1(kInputCreatorResultId);
                creatorResult = token.data.value(creatorKey);
                m_lastCreatorResult = creatorResult;
            } else {
                phase = QString::fromLatin1(kInputValidatorResultId);
                validatorResult = token.data.value(validatorKey);
                m_lastValidatorResult = validatorResult;
                ++m_iterationCount;
            }

            ControllerDecision decision = runControllerScript(phase,
                                                              m_currentStart,
                                                              creatorResult,
                                                              validatorResult,
                                                              m_iterationCount,
                                                              m_lastCreatorPayload);

            if (validatorTriggered &&
                decision.decision == QStringLiteral("creator") &&
                m_iterationCount >= m_maxIterations) {
                decision.decision = QStringLiteral("done");
                decision.payload = variantHasText(m_lastCreatorResult)
                    ? m_lastCreatorResult
                    : m_lastValidatorResult;
                if (decision.status.trimmed().isEmpty()) {
                    decision.status = QStringLiteral("Max iterations reached");
                } else {
                    decision.status += QStringLiteral("; max iterations reached");
                }
            }

            if (decision.decision == QStringLiteral("creator")) {
                m_lastCreatorPayload = payloadFallback(decision,
                                                       variantHasText(m_lastValidatorResult)
                                                           ? m_lastValidatorResult
                                                           : m_lastCreatorPayload);
                decision.payload = m_lastCreatorPayload;
                outputs.push_back(makeOutputToken(decision, true));
            } else if (decision.decision == QStringLiteral("validator")) {
                decision.payload = payloadFallback(decision, m_lastCreatorResult);
                outputs.push_back(makeOutputToken(decision, true));
            } else if (decision.decision == QStringLiteral("done")) {
                decision.payload = payloadFallback(decision,
                                                   variantHasText(m_lastCreatorResult)
                                                       ? m_lastCreatorResult
                                                       : m_lastValidatorResult);
                outputs.push_back(makeOutputToken(decision, false));
                m_isProcessing = false;
                m_iterationCount = 0;
                m_currentStart = QVariant();
                m_lastCreatorPayload = QVariant();
                m_lastCreatorResult = QVariant();
                m_lastValidatorResult = QVariant();
            } else {
                outputs.push_back(makeOutputToken(decision, false));
                m_isProcessing = false;
                m_iterationCount = 0;
                m_currentStart = QVariant();
                m_lastCreatorPayload = QVariant();
                m_lastCreatorResult = QVariant();
                m_lastValidatorResult = QVariant();
            }
            break;
        }
    }

    if (!m_isProcessing && !m_startQueue.empty()) {
        m_currentStart = m_startQueue.front();
        m_startQueue.pop_front();
        m_lastCreatorPayload = m_currentStart;
        m_lastCreatorResult = QVariant();
        m_lastValidatorResult = QVariant();
        m_iterationCount = 0;
        m_isProcessing = true;

        ControllerDecision decision = runControllerScript(QStringLiteral("start"),
                                                          m_currentStart,
                                                          QVariant(),
                                                          QVariant(),
                                                          m_iterationCount,
                                                          m_lastCreatorPayload);
        if (decision.decision == QStringLiteral("done")) {
            decision.payload = payloadFallback(decision, m_currentStart);
            outputs.push_back(makeOutputToken(decision, false));
            m_isProcessing = false;
            m_currentStart = QVariant();
            m_lastCreatorPayload = QVariant();
        } else if (decision.decision == QStringLiteral("error")) {
            outputs.push_back(makeOutputToken(decision, false));
            m_isProcessing = false;
            m_currentStart = QVariant();
            m_lastCreatorPayload = QVariant();
        } else {
            if (decision.decision != QStringLiteral("validator")) {
                decision.decision = QStringLiteral("creator");
            }
            const QVariant fallback = decision.decision == QStringLiteral("validator") ? m_lastCreatorResult : m_currentStart;
            m_lastCreatorPayload = payloadFallback(decision, fallback);
            decision.payload = m_lastCreatorPayload;
            outputs.push_back(makeOutputToken(decision, true));
        }
    }

    return outputs;
}

QJsonObject CrexxControllerNode::saveState() const
{
    QMutexLocker locker(&m_mutex);
    QJsonObject obj;
    obj.insert(QStringLiteral("scriptCode"), m_scriptCode);
    obj.insert(QStringLiteral("engineId"), m_engineId);
    obj.insert(QStringLiteral("enableSyntaxHighlighting"), m_enableSyntaxHighlighting);
    obj.insert(QStringLiteral("maxIterations"), m_maxIterations);
    return obj;
}

void CrexxControllerNode::loadState(const QJsonObject& data)
{
    if (data.contains(QStringLiteral("engineId"))) {
        setEngineId(data.value(QStringLiteral("engineId")).toString());
    }
    if (data.contains(QStringLiteral("scriptCode"))) {
        setScriptCode(data.value(QStringLiteral("scriptCode")).toString());
    } else if (data.contains(QStringLiteral("script"))) {
        setScriptCode(data.value(QStringLiteral("script")).toString());
    }
    if (data.contains(QStringLiteral("enableSyntaxHighlighting"))) {
        m_enableSyntaxHighlighting = data.value(QStringLiteral("enableSyntaxHighlighting")).toBool(true);
    }
    if (data.contains(QStringLiteral("maxIterations"))) {
        setMaxIterations(data.value(QStringLiteral("maxIterations")).toInt());
    }
}

QString CrexxControllerNode::scriptCode() const
{
    QMutexLocker locker(&m_mutex);
    return m_scriptCode;
}

void CrexxControllerNode::setScriptCode(const QString& scriptCode)
{
    QString normalized = scriptCode;
    if (normalized.trimmed().isEmpty()) {
        normalized = defaultScript();
    }

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_scriptCode != normalized) {
            m_scriptCode = normalized;
            changed = true;
        }
    }

    if (changed) {
        emit scriptCodeChanged(normalized);
    }
}

QString CrexxControllerNode::engineId() const
{
    QMutexLocker locker(&m_mutex);
    return m_engineId;
}

void CrexxControllerNode::setEngineId(const QString& engineId)
{
    const QString normalized = engineId.trimmed().isEmpty() ? QStringLiteral("crexx") : engineId.trimmed();
    QString nextScript;
    bool engineChanged = false;
    bool scriptChanged = false;

    {
        QMutexLocker locker(&m_mutex);
        if (m_engineId == normalized) {
            return;
        }

        const bool shouldReplaceScript = isManagedControllerScript(m_scriptCode);
        m_engineId = normalized;
        engineChanged = true;

        if (shouldReplaceScript) {
            nextScript = controllerScriptForEngine(m_engineId);
            if (m_scriptCode != nextScript) {
                m_scriptCode = nextScript;
                scriptChanged = true;
            }
        }
    }

    if (engineChanged) {
        emit engineIdChanged(normalized);
    }
    if (scriptChanged) {
        emit scriptCodeChanged(nextScript);
    }
}

int CrexxControllerNode::maxIterations() const
{
    QMutexLocker locker(&m_mutex);
    return m_maxIterations;
}

void CrexxControllerNode::setMaxIterations(int maxIterations)
{
    int bounded = maxIterations;
    if (bounded < kMaxIterationsLowerBound) {
        bounded = kMaxIterationsLowerBound;
    }
    if (bounded > kMaxIterationsUpperBound) {
        bounded = kMaxIterationsUpperBound;
    }

    bool changed = false;
    {
        QMutexLocker locker(&m_mutex);
        if (m_maxIterations != bounded) {
            m_maxIterations = bounded;
            changed = true;
        }
    }

    if (changed) {
        emit maxIterationsChanged(bounded);
    }
}

CrexxControllerNode::ControllerDecision CrexxControllerNode::runControllerScript(const QString& phase,
                                                                                 const QVariant& start,
                                                                                 const QVariant& creatorResult,
                                                                                 const QVariant& validatorResult,
                                                                                 int iteration,
                                                                                 const QVariant& lastCreatorPayload) const
{
    ControllerDecision decision;

    const QString script = m_scriptCode.trimmed().isEmpty() ? controllerScriptForEngine(m_engineId) : m_scriptCode;
    std::unique_ptr<IScriptEngine> engine = ScriptEngineRegistry::instance().createEngine(m_engineId);
    if (!engine) {
        decision.decision = QStringLiteral("error");
        decision.error = QStringLiteral("Script engine is not available: %1").arg(m_engineId);
        decision.status = decision.error;
        return decision;
    }

    DataPacket input;
    input.insert(QStringLiteral("phase"), phase);
    input.insert(QStringLiteral("start"), start);
    input.insert(QStringLiteral("from_creator"), creatorResult);
    input.insert(QStringLiteral("from_validator"), validatorResult);
    input.insert(QStringLiteral("iteration"), iteration);
    input.insert(QStringLiteral("max_iterations"), m_maxIterations);
    input.insert(QStringLiteral("last_creator_payload"), lastCreatorPayload);
    input.insert(QStringLiteral("last_creator_result"), m_lastCreatorResult);
    input.insert(QStringLiteral("last_validator_result"), m_lastValidatorResult);

    DataPacket output;
    QList<QString> logs;
    ExecutionScriptHost host(input, output, logs);
    const bool success = engine->execute(script, &host);

    decision.logs = output.value(QStringLiteral("logs")).toString();
    if (!success) {
        decision.decision = QStringLiteral("error");
        decision.error = output.value(QStringLiteral("__error"),
                                      QStringLiteral("cREXX controller script failed.")).toString();
        decision.status = decision.error;
        return decision;
    }

    const QVariantList values = outputAsList(output.value(QStringLiteral("output")));
    decision.decision = normalizedRoute(output.value(QStringLiteral("route"),
                                                     output.value(QStringLiteral("decision"),
                                                                  values.value(0))).toString());
    if (values.size() > 1) {
        decision.payload = values.value(1);
    }
    if (output.contains(QStringLiteral("payload"))) {
        decision.payload = output.value(QStringLiteral("payload"));
    }
    if (output.contains(QString::fromLatin1(kOutputCreatorId))) {
        decision.decision = QStringLiteral("creator");
        decision.payload = output.value(QString::fromLatin1(kOutputCreatorId));
    }
    if (output.contains(QString::fromLatin1(kOutputValidatorId))) {
        decision.decision = QStringLiteral("validator");
        decision.payload = output.value(QString::fromLatin1(kOutputValidatorId));
    }
    if (output.contains(QString::fromLatin1(kOutputDoneId))) {
        decision.decision = QStringLiteral("done");
        decision.payload = output.value(QString::fromLatin1(kOutputDoneId));
    }
    if (values.size() > 2) {
        decision.status = values.value(2).toString();
    }
    if (output.contains(QStringLiteral("_status"))) {
        decision.status = output.value(QStringLiteral("_status")).toString();
    } else if (output.contains(QStringLiteral("status"))) {
        decision.status = output.value(QStringLiteral("status")).toString();
    }

    if (decision.decision.isEmpty()) {
        decision.decision = QStringLiteral("creator");
    }
    if (decision.decision == QStringLiteral("error") && decision.error.trimmed().isEmpty()) {
        decision.error = decision.status.trimmed().isEmpty()
            ? QStringLiteral("Controller script returned error.")
            : decision.status;
    }

    return decision;
}

ExecutionToken CrexxControllerNode::makeOutputToken(const ControllerDecision& decision, bool forceNext) const
{
    ExecutionToken token;
    token.forceExecution = forceNext;
    token.data.insert(QStringLiteral("_decision"), decision.decision);
    token.data.insert(QStringLiteral("_iteration"), m_iterationCount);
    if (!decision.status.trimmed().isEmpty()) {
        token.data.insert(QStringLiteral("_status"), decision.status);
    }
    if (!decision.logs.trimmed().isEmpty()) {
        token.data.insert(QStringLiteral("logs"), decision.logs);
    }

    if (decision.decision == QStringLiteral("done")) {
        token.data.insert(QString::fromLatin1(kOutputDoneId), decision.payload);
        token.data.insert(QStringLiteral("text"), decision.payload);
    } else if (decision.decision == QStringLiteral("validator")) {
        token.data.insert(QString::fromLatin1(kOutputValidatorId), decision.payload);
        token.data.insert(QStringLiteral("text"), decision.payload);
    } else if (decision.decision == QStringLiteral("creator")) {
        token.data.insert(QString::fromLatin1(kOutputCreatorId), decision.payload);
        token.data.insert(QStringLiteral("text"), decision.payload);
    } else {
        token.data.insert(QStringLiteral("__error"), decision.error.trimmed().isEmpty()
            ? QStringLiteral("cREXX controller error.")
            : decision.error);
        token.data.insert(QStringLiteral("text"), decision.error);
    }

    return token;
}

QVariant CrexxControllerNode::payloadFallback(const ControllerDecision& decision, const QVariant& fallback) const
{
    if (variantHasText(decision.payload)) {
        return decision.payload;
    }
    return fallback;
}

QString CrexxControllerNode::variantToText(const QVariant& value)
{
    if (!value.isValid() || value.isNull()) {
        return QString();
    }
    if (value.typeId() == QMetaType::QVariantMap ||
        value.typeId() == QMetaType::QVariantList ||
        value.typeId() == QMetaType::QStringList) {
        return QString::fromUtf8(QJsonDocument::fromVariant(value).toJson(QJsonDocument::Compact));
    }
    return value.toString();
}

QString CrexxControllerNode::normalizedRoute(const QString& value)
{
    const QString trimmed = value.trimmed().toLower();
    if (trimmed == QStringLiteral("done") ||
        trimmed == QStringLiteral("finish") ||
        trimmed == QStringLiteral("finished") ||
        trimmed == QStringLiteral("stop") ||
        trimmed == QStringLiteral("pass") ||
        trimmed == QStringLiteral("ok")) {
        return QStringLiteral("done");
    }
    if (trimmed == QStringLiteral("error") ||
        trimmed == QStringLiteral("fail") ||
        trimmed == QStringLiteral("failed")) {
        return QStringLiteral("error");
    }
    if (trimmed == QStringLiteral("next") ||
        trimmed == QStringLiteral("retry") ||
        trimmed == QStringLiteral("continue") ||
        trimmed == QStringLiteral("again") ||
        trimmed == QStringLiteral("creator") ||
        trimmed == QStringLiteral("generate") ||
        trimmed == QStringLiteral("generator")) {
        return QStringLiteral("creator");
    }
    if (trimmed == QStringLiteral("validator") ||
        trimmed == QStringLiteral("validate") ||
        trimmed == QStringLiteral("checker") ||
        trimmed == QStringLiteral("check")) {
        return QStringLiteral("validator");
    }
    return trimmed;
}

bool CrexxControllerNode::isStartTrigger(const ExecutionToken& token, const QString& startKey)
{
    return token.triggeringPinId == startKey ||
           (token.triggeringPinId.isEmpty() && token.data.contains(startKey));
}

bool CrexxControllerNode::isCreatorTrigger(const ExecutionToken& token, const QString& creatorKey)
{
    return token.triggeringPinId == creatorKey ||
           (token.triggeringPinId.isEmpty() && token.data.contains(creatorKey));
}

bool CrexxControllerNode::isValidatorTrigger(const ExecutionToken& token, const QString& validatorKey)
{
    return token.triggeringPinId == validatorKey ||
           (token.triggeringPinId.isEmpty() && token.data.contains(validatorKey));
}
