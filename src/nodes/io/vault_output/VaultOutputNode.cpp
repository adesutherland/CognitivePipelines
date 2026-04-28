#include "VaultOutputNode.h"

#include "VaultOutputPropertiesWidget.h"
#include "DocumentLoader.h"
#include "ModelCapsRegistry.h"
#include "ai/backends/ILLMBackend.h"
#include "ai/catalog/ModelCatalogService.h"
#include "ai/registry/LLMProviderRegistry.h"
#include "Logger.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QMimeDatabase>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>

namespace {

QString defaultRoutingPrompt()
{
    return QStringLiteral(
        "Choose the best relative subfolder and filename for this markdown note inside the vault. "
        "Prefer an existing folder when it already matches the topic. "
        "Return strict JSON with keys subfolder, filename, and reason. "
        "subfolder must be relative to the vault root. filename should not include the .md extension.");
}

QString trimForPrompt(QString text, int maxChars)
{
    if (text.size() <= maxChars) {
        return text;
    }
    text.truncate(maxChars);
    text += QStringLiteral("\n...[truncated]");
    return text;
}

QString firstMeaningfulLine(const QString& text)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (const QString& rawLine : lines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }
        if (line.startsWith(QStringLiteral("#"))) {
            line.remove(QRegularExpression(QStringLiteral("^#+\\s*")));
        }
        if (!line.isEmpty()) {
            return line;
        }
    }
    return {};
}

QString buildVaultSummary(const QString& rootPath)
{
    QDir root(rootPath);
    if (!root.exists()) {
        return QStringLiteral("Vault does not exist yet and will be created on first save.");
    }

    QStringList directories;
    directories << QStringLiteral(".");
    QDirIterator dirIt(rootPath, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (dirIt.hasNext() && directories.size() < 128) {
        dirIt.next();
        directories << root.relativeFilePath(dirIt.filePath());
    }

    const QStringList markdownFiles = DocumentLoader::scanDirectory(
        rootPath,
        {QStringLiteral("*.md"), QStringLiteral("*.markdown")});

    QStringList fileSummaries;
    int fileCount = 0;
    for (const QString& filePath : markdownFiles) {
        if (fileCount >= 160) {
            break;
        }
        QFile file(filePath);
        QString title;
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            stream.setEncoding(QStringConverter::Utf8);
            title = firstMeaningfulLine(stream.read(600));
            file.close();
        }
        const QString relPath = root.relativeFilePath(filePath);
        if (title.isEmpty()) {
            fileSummaries << relPath;
        } else {
            fileSummaries << QStringLiteral("%1 | %2").arg(relPath, title);
        }
        ++fileCount;
    }

    QString summary = QStringLiteral("Existing folders:\n- %1").arg(directories.join(QStringLiteral("\n- ")));
    if (!fileSummaries.isEmpty()) {
        summary += QStringLiteral("\n\nExisting markdown notes:\n- %1").arg(fileSummaries.join(QStringLiteral("\n- ")));
    } else {
        summary += QStringLiteral("\n\nExisting markdown notes:\n- <none>");
    }
    if (markdownFiles.size() > fileCount) {
        summary += QStringLiteral("\n- ... %1 more note(s) omitted").arg(markdownFiles.size() - fileCount);
    }
    return summary;
}

QString sanitizePathSegment(QString segment)
{
    segment = segment.trimmed();
    if (segment == QStringLiteral(".") || segment == QStringLiteral("..")) {
        return {};
    }

    segment.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    segment.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    while (segment.startsWith(QLatin1Char('.'))) {
        segment.remove(0, 1);
    }
    return segment.trimmed();
}

QString sanitizeRelativeSubfolder(QString raw)
{
    raw.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const QStringList segments = raw.split(QLatin1Char('/'), Qt::SkipEmptyParts);
    QStringList cleanSegments;
    for (const QString& segment : segments) {
        const QString clean = sanitizePathSegment(segment);
        if (!clean.isEmpty()) {
            cleanSegments << clean;
        }
    }
    return cleanSegments.join(QLatin1Char('/'));
}

QString sanitizeFilenameBase(QString raw, const QString& fallbackTitle)
{
    raw = raw.trimmed();
    if (raw.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive)) {
        raw.chop(3);
    }
    raw = sanitizePathSegment(raw);
    if (raw.isEmpty()) {
        raw = sanitizePathSegment(fallbackTitle);
    }
    if (raw.isEmpty()) {
        raw = QStringLiteral("note_%1").arg(QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    }
    return raw;
}

QString uniqueMarkdownPath(const QString& directoryPath, const QString& fileBase)
{
    const QString base = fileBase.isEmpty() ? QStringLiteral("note") : fileBase;
    QString candidate = QDir(directoryPath).filePath(base + QStringLiteral(".md"));
    int suffix = 2;
    while (QFileInfo::exists(candidate)) {
        candidate = QDir(directoryPath).filePath(QStringLiteral("%1-%2.md").arg(base).arg(suffix));
        ++suffix;
    }
    return candidate;
}

QString extractFirstJsonObjectText(const QString& text)
{
    int start = -1;
    int depth = 0;
    bool inString = false;
    bool escaping = false;

    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text[i];

        if (start < 0) {
            if (ch == QLatin1Char('{')) {
                start = i;
                depth = 1;
                inString = false;
                escaping = false;
            }
            continue;
        }

        if (escaping) {
            escaping = false;
            continue;
        }
        if (ch == QLatin1Char('\\')) {
            escaping = true;
            continue;
        }
        if (ch == QLatin1Char('"')) {
            inString = !inString;
            continue;
        }
        if (inString) {
            continue;
        }
        if (ch == QLatin1Char('{')) {
            ++depth;
            continue;
        }
        if (ch == QLatin1Char('}')) {
            --depth;
            if (depth == 0) {
                return text.mid(start, i - start + 1);
            }
        }
    }

    return {};
}

QJsonObject parseDecisionObject(const QString& responseText)
{
    auto tryParse = [](const QString& text) -> QJsonObject {
        QJsonParseError error;
        const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &error);
        if (error.error == QJsonParseError::NoError && doc.isObject()) {
            return doc.object();
        }
        return {};
    };

    QJsonObject parsed = tryParse(responseText.trimmed());
    if (!parsed.isEmpty()) {
        return parsed;
    }

    parsed = tryParse(extractFirstJsonObjectText(responseText));
    return parsed;
}

} // namespace

VaultOutputNode::VaultOutputNode(QObject* parent)
    : QObject(parent)
    , m_routingPrompt(defaultRoutingPrompt())
{
    m_providerId = ModelCatalogService::instance().defaultProvider(ModelCatalogKind::Chat);
    if (!m_providerId.isEmpty()) {
        const auto models = ModelCatalogService::instance().fallbackModels(m_providerId, ModelCatalogKind::Chat);
        for (const auto& model : models) {
            if (model.visibility != ModelCatalogVisibility::Hidden) {
                m_modelId = model.id;
                break;
            }
        }
    }
}

NodeDescriptor VaultOutputNode::getDescriptor() const
{
    NodeDescriptor desc;
    desc.id = QStringLiteral("vault-output");
    desc.name = QStringLiteral("Vault Output");
    desc.category = QStringLiteral("Input / Output");

    desc.inputPins.insert(QString::fromLatin1(kInputMarkdownId),
                          PinDefinition{PinDirection::Input,
                                        QString::fromLatin1(kInputMarkdownId),
                                        QStringLiteral("Markdown"),
                                        QStringLiteral("text")});
    desc.inputPins.insert(QString::fromLatin1(kInputVaultRootId),
                          PinDefinition{PinDirection::Input,
                                        QString::fromLatin1(kInputVaultRootId),
                                        QStringLiteral("Vault Root"),
                                        QStringLiteral("text")});
    desc.inputPins.insert(QString::fromLatin1(kInputPromptId),
                          PinDefinition{PinDirection::Input,
                                        QString::fromLatin1(kInputPromptId),
                                        QStringLiteral("Prompt"),
                                        QStringLiteral("text")});

    desc.outputPins.insert(QString::fromLatin1(kOutputSavedPathId),
                           PinDefinition{PinDirection::Output,
                                         QString::fromLatin1(kOutputSavedPathId),
                                         QStringLiteral("Saved Path"),
                                         QStringLiteral("text")});
    desc.outputPins.insert(QString::fromLatin1(kOutputSubfolderId),
                           PinDefinition{PinDirection::Output,
                                         QString::fromLatin1(kOutputSubfolderId),
                                         QStringLiteral("Subfolder"),
                                         QStringLiteral("text")});
    desc.outputPins.insert(QString::fromLatin1(kOutputFilenameId),
                           PinDefinition{PinDirection::Output,
                                         QString::fromLatin1(kOutputFilenameId),
                                         QStringLiteral("Filename"),
                                         QStringLiteral("text")});
    desc.outputPins.insert(QString::fromLatin1(kOutputDecisionId),
                           PinDefinition{PinDirection::Output,
                                         QString::fromLatin1(kOutputDecisionId),
                                         QStringLiteral("Decision"),
                                         QStringLiteral("json")});

    return desc;
}

QWidget* VaultOutputNode::createConfigurationWidget(QWidget* parent)
{
    if (!m_widget) {
        auto* widget = new VaultOutputPropertiesWidget(parent);
        widget->setVaultRoot(m_vaultRoot);
        widget->setProvider(m_providerId);
        widget->setModel(m_modelId);
        widget->setRoutingPrompt(m_routingPrompt);
        widget->setTemperature(m_temperature);
        widget->setMaxTokens(m_maxTokens);

        connect(widget, &VaultOutputPropertiesWidget::vaultRootChanged,
                this, &VaultOutputNode::setVaultRoot);
        connect(widget, &VaultOutputPropertiesWidget::providerChanged,
                this, &VaultOutputNode::setProviderId);
        connect(widget, &VaultOutputPropertiesWidget::modelChanged,
                this, &VaultOutputNode::setModelId);
        connect(widget, &VaultOutputPropertiesWidget::routingPromptChanged,
                this, &VaultOutputNode::setRoutingPrompt);
        connect(widget, &VaultOutputPropertiesWidget::temperatureChanged,
                this, &VaultOutputNode::setTemperature);
        connect(widget, &VaultOutputPropertiesWidget::maxTokensChanged,
                this, &VaultOutputNode::setMaxTokens);

        m_widget = widget;
    } else if (parent && m_widget->parent() != parent) {
        m_widget->setParent(parent);
    }

    return m_widget;
}

TokenList VaultOutputNode::execute(const TokenList& incomingTokens)
{
    DataPacket inputs;
    for (const auto& token : incomingTokens) {
        for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
            inputs.insert(it.key(), it.value());
        }
    }

    const QString markdown = inputs.value(QString::fromLatin1(kInputMarkdownId)).toString();
    const QString vaultRoot = inputs.value(QString::fromLatin1(kInputVaultRootId)).toString().trimmed().isEmpty()
        ? m_vaultRoot.trimmed()
        : inputs.value(QString::fromLatin1(kInputVaultRootId)).toString().trimmed();
    const QString routingPrompt = inputs.value(QString::fromLatin1(kInputPromptId)).toString().trimmed().isEmpty()
        ? m_routingPrompt.trimmed()
        : inputs.value(QString::fromLatin1(kInputPromptId)).toString().trimmed();

    DataPacket output;
    output.insert(QStringLiteral("_provider"), m_providerId);
    output.insert(QStringLiteral("_model"), m_modelId);

    auto fail = [this, &output](const QString& message) {
        output.insert(QStringLiteral("__error"), message);
        setStatusMessage(QStringLiteral("Status: %1").arg(message));
        ExecutionToken token;
        token.data = output;
        return TokenList{token};
    };

    setStatusMessage(QStringLiteral("Status: routing note to vault..."));

    if (markdown.trimmed().isEmpty()) {
        return fail(QStringLiteral("Vault Output requires markdown input."));
    }

    if (vaultRoot.isEmpty()) {
        return fail(QStringLiteral("Vault Output requires a vault root path."));
    }

    if (m_providerId.trimmed().isEmpty() || m_modelId.trimmed().isEmpty()) {
        return fail(QStringLiteral("Vault Output requires a configured provider and model."));
    }

    QDir rootDir(vaultRoot);
    if (!rootDir.exists() && !QDir().mkpath(vaultRoot)) {
        return fail(QStringLiteral("Failed to create vault root directory: %1").arg(vaultRoot));
    }

    ILLMBackend* backend = LLMProviderRegistry::instance().getBackend(m_providerId);
    if (!backend) {
        return fail(QStringLiteral("Vault Output backend '%1' is not available.").arg(m_providerId));
    }

    const QString apiKey = LLMProviderRegistry::instance().getCredential(m_providerId);
    if (apiKey.isEmpty() && ModelCatalogService::providerRequiresCredential(m_providerId)) {
        return fail(QStringLiteral("API key not found for provider '%1'.").arg(m_providerId));
    }

    const QString resolvedModel = ModelCapsRegistry::instance().resolveAlias(m_modelId, m_providerId);
    output.insert(QStringLiteral("_model"), resolvedModel);
    if (const auto resolvedRule = ModelCapsRegistry::instance().resolveWithRule(resolvedModel, m_providerId);
        resolvedRule.has_value() && !resolvedRule->driverProfileId.isEmpty()) {
        output.insert(QStringLiteral("_driver"), resolvedRule->driverProfileId);
    }
    const QString vaultSummary = trimForPrompt(buildVaultSummary(vaultRoot), 14000);
    const QString trimmedMarkdown = trimForPrompt(markdown, 16000);
    const QString systemPrompt = QStringLiteral(
        "You route markdown notes into an existing personal knowledge vault. "
        "Respond with valid JSON only. Never return an absolute path or parent-directory traversal.");
    const QString userPrompt = QStringLiteral(
        "Vault root: %1\n\n"
        "Vault summary:\n%2\n\n"
        "Routing instructions:\n%3\n\n"
        "Incoming markdown note:\n```markdown\n%4\n```")
            .arg(vaultRoot,
                 vaultSummary,
                 routingPrompt.isEmpty() ? defaultRoutingPrompt() : routingPrompt,
                 trimmedMarkdown);

    const LLMResult result = backend->sendPrompt(apiKey,
                                                 resolvedModel,
                                                 m_temperature,
                                                 m_maxTokens,
                                                 systemPrompt,
                                                 userPrompt);

    output.insert(QStringLiteral("_raw_response"), result.rawResponse);
    if (result.hasError) {
        const QString errorForLog = result.errorMsg.isEmpty() ? result.content : result.errorMsg;
        CP_WARN.noquote() << QStringLiteral("VaultOutputNode: backend failure provider=%1 model=%2 message=%3")
                                      .arg(m_providerId, resolvedModel, errorForLog);
        return fail(result.errorMsg.isEmpty() ? result.content : result.errorMsg);
    }

    const QJsonObject decision = parseDecisionObject(result.content);
    const QString fallbackTitle = firstMeaningfulLine(markdown);
    const QString relativeSubfolder = sanitizeRelativeSubfolder(decision.value(QStringLiteral("subfolder")).toString());
    const QString filenameBase = sanitizeFilenameBase(decision.value(QStringLiteral("filename")).toString(),
                                                      fallbackTitle);
    const QString reason = decision.value(QStringLiteral("reason")).toString().trimmed();

    const QString targetDirPath = relativeSubfolder.isEmpty()
        ? rootDir.absolutePath()
        : rootDir.filePath(relativeSubfolder);
    if (!QDir().mkpath(targetDirPath)) {
        return fail(QStringLiteral("Failed to create vault subfolder: %1").arg(targetDirPath));
    }

    const QString targetPath = uniqueMarkdownPath(targetDirPath, filenameBase);
    QSaveFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return fail(QStringLiteral("Failed to open vault output file: %1").arg(targetPath));
    }
    file.write(markdown.toUtf8());
    if (!file.commit()) {
        return fail(QStringLiteral("Failed to save vault output file: %1").arg(targetPath));
    }

    QVariantMap decisionMap = decision.toVariantMap();
    if (!reason.isEmpty() && !decisionMap.contains(QStringLiteral("reason"))) {
        decisionMap.insert(QStringLiteral("reason"), reason);
    }
    decisionMap.insert(QStringLiteral("provider"), m_providerId);
    decisionMap.insert(QStringLiteral("model"), resolvedModel);

    output.insert(QString::fromLatin1(kOutputSavedPathId), QFileInfo(targetPath).absoluteFilePath());
    output.insert(QString::fromLatin1(kOutputSubfolderId), relativeSubfolder);
    output.insert(QString::fromLatin1(kOutputFilenameId), QFileInfo(targetPath).fileName());
    output.insert(QString::fromLatin1(kOutputDecisionId), decisionMap);
    setStatusMessage(QStringLiteral("Status: saved %1").arg(QFileInfo(targetPath).absoluteFilePath()));

    ExecutionToken token;
    token.data = output;
    return TokenList{token};
}

QJsonObject VaultOutputNode::saveState() const
{
    QJsonObject state;
    state.insert(QStringLiteral("vault_root"), m_vaultRoot);
    state.insert(QStringLiteral("provider_id"), m_providerId);
    state.insert(QStringLiteral("model_id"), m_modelId);
    state.insert(QStringLiteral("routing_prompt"), m_routingPrompt);
    state.insert(QStringLiteral("temperature"), m_temperature);
    state.insert(QStringLiteral("max_tokens"), m_maxTokens);
    return state;
}

void VaultOutputNode::loadState(const QJsonObject& data)
{
    m_vaultRoot = data.value(QStringLiteral("vault_root")).toString(m_vaultRoot);
    m_providerId = data.value(QStringLiteral("provider_id")).toString(m_providerId);
    m_modelId = data.value(QStringLiteral("model_id")).toString(m_modelId);
    m_routingPrompt = data.value(QStringLiteral("routing_prompt")).toString(m_routingPrompt);
    if (data.contains(QStringLiteral("temperature"))) {
        m_temperature = data.value(QStringLiteral("temperature")).toDouble(m_temperature);
    }
    if (data.contains(QStringLiteral("max_tokens"))) {
        m_maxTokens = data.value(QStringLiteral("max_tokens")).toInt(m_maxTokens);
    }

    if (m_widget) {
        m_widget->setVaultRoot(m_vaultRoot);
        m_widget->setProvider(m_providerId);
        m_widget->setModel(m_modelId);
        m_widget->setRoutingPrompt(m_routingPrompt);
        m_widget->setTemperature(m_temperature);
        m_widget->setMaxTokens(m_maxTokens);
    }
}

void VaultOutputNode::setVaultRoot(const QString& path)
{
    m_vaultRoot = path;
}

void VaultOutputNode::setProviderId(const QString& providerId)
{
    m_providerId = providerId.trimmed().toLower();
}

void VaultOutputNode::setModelId(const QString& modelId)
{
    m_modelId = modelId.trimmed();
}

void VaultOutputNode::setRoutingPrompt(const QString& prompt)
{
    m_routingPrompt = prompt;
}

void VaultOutputNode::setTemperature(double value)
{
    m_temperature = value;
}

void VaultOutputNode::setMaxTokens(int value)
{
    m_maxTokens = value;
}

void VaultOutputNode::setStatusMessage(const QString& message)
{
    auto* widget = m_widget.data();
    if (!widget) {
        return;
    }

    QMetaObject::invokeMethod(widget, [widget, message]() {
        widget->setStatusMessage(message);
    }, Qt::QueuedConnection);
}
