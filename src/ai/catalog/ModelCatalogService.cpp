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
#include "ModelCatalogService.h"

#include "ModelCapsRegistry.h"
#include "ai/backends/ILLMBackend.h"
#include "ai/registry/LLMProviderRegistry.h"

#include <QSet>
#include <QtConcurrent>

#include <algorithm>

namespace {

bool isLocalProvider(const QString& providerId)
{
    return providerId.compare(QStringLiteral("ollama"), Qt::CaseInsensitive) == 0;
}

int providerOrder(const QString& providerId)
{
    if (providerId == QStringLiteral("openai")) {
        return 0;
    }
    if (providerId == QStringLiteral("anthropic")) {
        return 1;
    }
    if (providerId == QStringLiteral("google")) {
        return 2;
    }
    if (providerId == QStringLiteral("ollama")) {
        return 3;
    }
    return 100;
}

int providerStatusOrder(const ProviderCatalogEntry& entry)
{
    if (!entry.isUsable) {
        return 3;
    }
    if (entry.requiresCredential && entry.hasCredential) {
        return 0;
    }
    if (!entry.requiresCredential) {
        return 1;
    }
    return 2;
}

QString defaultChatDriver(const QString& providerId)
{
    if (providerId == QStringLiteral("openai")) {
        return QStringLiteral("openai-chat-completions");
    }
    if (providerId == QStringLiteral("anthropic")) {
        return QStringLiteral("anthropic-messages");
    }
    if (providerId == QStringLiteral("google")) {
        return QStringLiteral("google-generate-content");
    }
    if (providerId == QStringLiteral("ollama")) {
        return QStringLiteral("ollama-chat");
    }
    return QStringLiteral("%1-chat").arg(providerId);
}

QString defaultImageDriver(const QString& providerId)
{
    if (providerId == QStringLiteral("openai")) {
        return QStringLiteral("openai-images");
    }
    return QStringLiteral("%1-images").arg(providerId);
}

QString resolvedDriverProfileId(const ModelCapsRegistry::ResolvedCaps& resolved,
                                const QString& fallbackDriver)
{
    return resolved.driverProfileId.trimmed().isEmpty()
               ? fallbackDriver
               : resolved.driverProfileId.trimmed();
}

bool supportsChatModel(const QString& providerId, const QString& modelId, QString* driverProfileId)
{
    const auto resolved = ModelCapsRegistry::instance().resolveWithRule(modelId, providerId);
    if (resolved.has_value()) {
        const bool isChat = resolved->caps.capabilities.isEmpty()
                            || resolved->caps.hasCapability(ModelCapsTypes::Capability::Chat);
        if (!isChat) {
            return false;
        }
        if (driverProfileId) {
            *driverProfileId = resolvedDriverProfileId(*resolved, defaultChatDriver(providerId));
        }
        return true;
    }

    if (providerId == QStringLiteral("ollama")
        && !modelId.contains(QStringLiteral("embed"), Qt::CaseInsensitive)) {
        if (driverProfileId) {
            *driverProfileId = defaultChatDriver(providerId);
        }
        return true;
    }

    return false;
}

QString hiddenReasonForChat(const QString& providerId, const QString& modelId)
{
    if (const auto resolved = ModelCapsRegistry::instance().resolveWithRule(modelId, providerId)) {
        if (resolved->caps.hasCapability(ModelCapsTypes::Capability::Image)) {
            return QStringLiteral("Image model, not a chat/completion model");
        }
        if (resolved->caps.hasCapability(ModelCapsTypes::Capability::Embedding)) {
            return QStringLiteral("Embedding model, not a chat/completion model");
        }
    }
    if (modelId.contains(QStringLiteral("embedding"), Qt::CaseInsensitive)) {
        return QStringLiteral("Embedding model, not a chat/completion model");
    }
    if (modelId.contains(QStringLiteral("image"), Qt::CaseInsensitive)
        || modelId.contains(QStringLiteral("imagen"), Qt::CaseInsensitive)
        || modelId.contains(QStringLiteral("dall-e"), Qt::CaseInsensitive)) {
        return QStringLiteral("Image model, not a chat/completion model");
    }
    return QStringLiteral("No capability rule or driver profile matched");
}

bool supportsEmbeddingModel(const QString& providerId, const QString& modelId, QString* driverProfileId)
{
    const auto resolved = ModelCapsRegistry::instance().resolveWithRule(modelId, providerId);
    if (resolved.has_value() && resolved->caps.hasCapability(ModelCapsTypes::Capability::Embedding)) {
        if (driverProfileId) {
            *driverProfileId = resolvedDriverProfileId(*resolved, QStringLiteral("%1-embeddings").arg(providerId));
        }
        return true;
    }

    return false;
}

QString defaultEmbeddingDriver(const QString& providerId)
{
    if (providerId == QStringLiteral("openai")) {
        return QStringLiteral("openai-embeddings");
    }
    if (providerId == QStringLiteral("ollama")) {
        return QStringLiteral("ollama-embed");
    }
    return QStringLiteral("%1-embeddings").arg(providerId);
}

bool providerHasImplementedEmbeddings(const QString& providerId)
{
    return providerId == QStringLiteral("openai") || providerId == QStringLiteral("ollama");
}

bool providerHasImplementedImageGeneration(const QString& providerId)
{
    return providerId == QStringLiteral("openai");
}

bool supportsImageModel(const QString& providerId, const QString& modelId, QString* driverProfileId)
{
    const auto resolved = ModelCapsRegistry::instance().resolveWithRule(modelId, providerId);
    if (resolved.has_value() && resolved->caps.hasCapability(ModelCapsTypes::Capability::Image)) {
        if (driverProfileId) {
            *driverProfileId = resolvedDriverProfileId(*resolved, defaultImageDriver(providerId));
        }
        return true;
    }

    return false;
}

int modelProbeMaxTokens(const QString& providerId)
{
    if (providerId == QStringLiteral("google")) {
        return 64;
    }
    return 32;
}

void appendEntryOnce(QList<ModelCatalogEntry>& entries, QSet<QString>& seen, ModelCatalogEntry entry)
{
    const QString key = QStringLiteral("%1:%2:%3")
                            .arg(entry.providerId,
                                 QString::number(static_cast<int>(entry.kind)),
                                 entry.id);
    if (seen.contains(key)) {
        return;
    }

    seen.insert(key);
    entries.append(std::move(entry));
}

QList<ModelCatalogEntry> buildChatEntries(const QString& providerId,
                                          const QStringList& modelIds,
                                          bool dynamic)
{
    QList<ModelCatalogEntry> entries;
    QSet<QString> seen;
    QSet<QString> aliasIds;

    const auto aliases = ModelCapsRegistry::instance().virtualModelsForBackend(providerId);
    for (const auto& alias : aliases) {
        if (!alias.backend.isEmpty() && alias.backend != providerId) {
            continue;
        }

        QString driverProfileId;
        const bool supported = supportsChatModel(providerId, alias.target, &driverProfileId);

        ModelCatalogEntry entry;
        entry.providerId = providerId;
        entry.id = alias.id;
        entry.displayName = alias.name;
        entry.description = QStringLiteral("Alias for %1").arg(alias.target);
        entry.driverProfileId = driverProfileId;
        entry.kind = ModelCatalogKind::Chat;
        entry.visibility = supported ? ModelCatalogVisibility::Recommended : ModelCatalogVisibility::Hidden;
        entry.isAlias = true;
        entry.isDynamic = false;
        if (!supported) {
            entry.filterReason = QStringLiteral("Alias target has no capability rule or driver profile");
        }

        aliasIds.insert(alias.id);
        appendEntryOnce(entries, seen, std::move(entry));
    }

    QStringList sortedModels = modelIds;
    sortedModels.removeDuplicates();
    std::sort(sortedModels.begin(), sortedModels.end(), [](const QString& lhs, const QString& rhs) {
        return lhs.localeAwareCompare(rhs) < 0;
    });

    for (const QString& modelId : sortedModels) {
        if (modelId.trimmed().isEmpty() || aliasIds.contains(modelId)) {
            continue;
        }

        QString driverProfileId;
        const bool supported = supportsChatModel(providerId, modelId, &driverProfileId);

        ModelCatalogEntry entry;
        entry.providerId = providerId;
        entry.id = modelId;
        entry.displayName = modelId;
        entry.driverProfileId = driverProfileId;
        entry.kind = ModelCatalogKind::Chat;
        entry.visibility = supported ? ModelCatalogVisibility::Available : ModelCatalogVisibility::Hidden;
        entry.isAlias = false;
        entry.isDynamic = dynamic;
        if (!supported) {
            entry.filterReason = hiddenReasonForChat(providerId, modelId);
        }

        appendEntryOnce(entries, seen, std::move(entry));
    }

    return entries;
}

QList<ModelCatalogEntry> buildEmbeddingEntries(const QString& providerId, const QStringList& modelIds)
{
    QList<ModelCatalogEntry> entries;
    QSet<QString> seen;
    const bool implemented = providerHasImplementedEmbeddings(providerId);

    QStringList sortedModels = modelIds;
    sortedModels.removeDuplicates();
    std::sort(sortedModels.begin(), sortedModels.end(), [](const QString& lhs, const QString& rhs) {
        return lhs.localeAwareCompare(rhs) < 0;
    });

    for (int i = 0; i < sortedModels.size(); ++i) {
        const QString& modelId = sortedModels.at(i);
        if (modelId.trimmed().isEmpty()) {
            continue;
        }

        QString driverProfileId;
        const bool supported = supportsEmbeddingModel(providerId, modelId, &driverProfileId);

        ModelCatalogEntry entry;
        entry.providerId = providerId;
        entry.id = modelId;
        entry.displayName = modelId;
        entry.driverProfileId = driverProfileId.isEmpty() ? defaultEmbeddingDriver(providerId) : driverProfileId;
        entry.kind = ModelCatalogKind::Embedding;
        entry.visibility = implemented && supported
                               ? (i == 0 ? ModelCatalogVisibility::Recommended
                                         : ModelCatalogVisibility::Available)
                               : ModelCatalogVisibility::Hidden;
        if (!implemented) {
            entry.filterReason = QStringLiteral("Embedding driver is not implemented for this provider");
        } else if (!supported) {
            entry.filterReason = QStringLiteral("No embedding capability rule matched");
        }

        appendEntryOnce(entries, seen, std::move(entry));
    }

    if (providerId == QStringLiteral("ollama") && entries.isEmpty()) {
        ModelCatalogEntry entry;
        entry.providerId = providerId;
        entry.id = QStringLiteral("nomic-embed-text");
        entry.displayName = QStringLiteral("nomic-embed-text");
        entry.driverProfileId = QStringLiteral("ollama-embed");
        entry.description = QStringLiteral("Common local embedding model");
        entry.kind = ModelCatalogKind::Embedding;
        entry.visibility = ModelCatalogVisibility::Recommended;
        appendEntryOnce(entries, seen, std::move(entry));
    }

    return entries;
}

QList<ModelCatalogEntry> buildImageEntries(const QString& providerId,
                                           const QStringList& modelIds,
                                           bool dynamic)
{
    QList<ModelCatalogEntry> entries;
    QSet<QString> seen;
    const bool implemented = providerHasImplementedImageGeneration(providerId);

    QStringList sortedModels = modelIds;
    sortedModels.removeDuplicates();
    std::sort(sortedModels.begin(), sortedModels.end(), [](const QString& lhs, const QString& rhs) {
        return lhs.localeAwareCompare(rhs) < 0;
    });

    for (int i = 0; i < sortedModels.size(); ++i) {
        const QString& modelId = sortedModels.at(i);
        if (modelId.trimmed().isEmpty()) {
            continue;
        }

        QString driverProfileId;
        const bool supported = supportsImageModel(providerId, modelId, &driverProfileId);

        ModelCatalogEntry entry;
        entry.providerId = providerId;
        entry.id = modelId;
        entry.displayName = modelId;
        entry.driverProfileId = driverProfileId.isEmpty() ? defaultImageDriver(providerId) : driverProfileId;
        entry.kind = ModelCatalogKind::Image;
        entry.visibility = implemented && supported
                               ? (i == 0 ? ModelCatalogVisibility::Recommended
                                         : ModelCatalogVisibility::Available)
                               : ModelCatalogVisibility::Hidden;
        entry.isDynamic = dynamic;
        if (!implemented) {
            entry.filterReason = QStringLiteral("Image generation driver is not implemented for this provider");
        } else if (!supported) {
            entry.filterReason = QStringLiteral("No image-generation capability rule matched");
        }

        appendEntryOnce(entries, seen, std::move(entry));
    }

    return entries;
}

QList<ModelCatalogEntry> buildEntries(const QString& providerId,
                                      ModelCatalogKind kind,
                                      const QStringList& modelIds,
                                      bool dynamic)
{
    if (kind == ModelCatalogKind::Embedding) {
        return buildEmbeddingEntries(providerId, modelIds);
    }
    if (kind == ModelCatalogKind::Image) {
        return buildImageEntries(providerId, modelIds, dynamic);
    }
    return buildChatEntries(providerId, modelIds, dynamic);
}

} // namespace

ModelCatalogService& ModelCatalogService::instance()
{
    static ModelCatalogService service;
    return service;
}

bool ModelCatalogService::providerRequiresCredential(const QString& providerId)
{
    if (const auto settings = ModelCapsRegistry::instance().providerSettings(providerId)) {
        return settings->requiresCredential;
    }
    return !isLocalProvider(providerId);
}

QList<ProviderCatalogEntry> ModelCatalogService::providers(ModelCatalogKind kind) const
{
    QList<ProviderCatalogEntry> entries;
    const auto backends = LLMProviderRegistry::instance().allBackends();
    entries.reserve(backends.size());

    for (ILLMBackend* backend : backends) {
        if (!backend) {
            continue;
        }

        const auto settings = ModelCapsRegistry::instance().providerSettings(backend->id());
        if (settings.has_value() && !settings->enabled) {
            continue;
        }

        ProviderCatalogEntry entry;
        entry.id = backend->id();
        entry.name = settings.has_value() && !settings->name.trimmed().isEmpty()
                         ? settings->name.trimmed()
                         : backend->name();
        entry.requiresCredential = providerRequiresCredential(entry.id);
        entry.hasCredential = !entry.requiresCredential
                                  || !LLMProviderRegistry::instance().getCredential(entry.id).trimmed().isEmpty();
        entry.isLocal = isLocalProvider(entry.id);
        entry.isUsable = !entry.requiresCredential || entry.hasCredential;
        if (kind == ModelCatalogKind::Embedding && !providerHasImplementedEmbeddings(entry.id)) {
            entry.isUsable = false;
            entry.statusText = QStringLiteral("No embedding driver");
        } else if (kind == ModelCatalogKind::Image && !providerHasImplementedImageGeneration(entry.id)) {
            entry.isUsable = false;
            entry.statusText = QStringLiteral("No image driver");
        } else if (entry.isLocal) {
            if (settings.has_value() && !settings->baseUrl.trimmed().isEmpty()) {
                entry.statusText = QStringLiteral("Local %1").arg(settings->baseUrl.trimmed());
            } else {
                entry.statusText = QStringLiteral("Local");
            }
        } else if (entry.hasCredential) {
            entry.statusText = QStringLiteral("Configured");
        } else {
            entry.statusText = QStringLiteral("Missing key");
        }

        entries.append(std::move(entry));
    }

    std::sort(entries.begin(), entries.end(), [](const ProviderCatalogEntry& lhs,
                                                 const ProviderCatalogEntry& rhs) {
        const int lhsStatus = providerStatusOrder(lhs);
        const int rhsStatus = providerStatusOrder(rhs);
        if (lhsStatus != rhsStatus) {
            return lhsStatus < rhsStatus;
        }

        const int lhsOrder = providerOrder(lhs.id);
        const int rhsOrder = providerOrder(rhs.id);
        if (lhsOrder != rhsOrder) {
            return lhsOrder < rhsOrder;
        }

        return lhs.name.localeAwareCompare(rhs.name) < 0;
    });

    return entries;
}

QString ModelCatalogService::defaultProvider(ModelCatalogKind kind) const
{
    const auto entries = providers(kind);
    for (const auto& entry : entries) {
        if (entry.isUsable) {
            return entry.id;
        }
    }
    return entries.isEmpty() ? QString() : entries.first().id;
}

QList<ModelCatalogEntry> ModelCatalogService::fallbackModels(const QString& providerId,
                                                             ModelCatalogKind kind) const
{
    ILLMBackend* backend = LLMProviderRegistry::instance().getBackend(providerId);
    if (!backend) {
        return {};
    }

    const QStringList models = (kind == ModelCatalogKind::Embedding)
                                   ? backend->availableEmbeddingModels()
                                   : backend->availableModels();
    return buildEntries(providerId, kind, models, false);
}

QFuture<QList<ModelCatalogEntry>> ModelCatalogService::fetchModels(const QString& providerId,
                                                                   ModelCatalogKind kind)
{
    return QtConcurrent::run([providerId, kind]() -> QList<ModelCatalogEntry> {
        ILLMBackend* backend = LLMProviderRegistry::instance().getBackend(providerId);
        if (!backend) {
            return {};
        }

        if (kind == ModelCatalogKind::Embedding && providerId != QStringLiteral("ollama")) {
            return buildEntries(providerId, kind, backend->availableEmbeddingModels(), false);
        }

        QFuture<QStringList> fetchFuture = backend->fetchRawModelList();
        fetchFuture.waitForFinished();

        QStringList models = fetchFuture.result();
        if (models.isEmpty()) {
            models = (kind == ModelCatalogKind::Embedding)
                         ? backend->availableEmbeddingModels()
                         : backend->availableModels();
        }

        if (providerId != QStringLiteral("ollama")) {
            QStringList fallbackModels = (kind == ModelCatalogKind::Embedding)
                                             ? backend->availableEmbeddingModels()
                                             : backend->availableModels();
            for (const QString& fallback : fallbackModels) {
                if (!models.contains(fallback)) {
                    models.append(fallback);
                }
            }
        }

        return buildEntries(providerId, kind, models, true);
    });
}

QFuture<ModelTestResult> ModelCatalogService::testModel(const QString& providerId,
                                                        const QString& modelId,
                                                        ModelCatalogKind kind)
{
    return QtConcurrent::run([providerId, modelId, kind]() -> ModelTestResult {
        ModelTestResult result;
        result.providerId = providerId;
        result.modelId = modelId;

        ILLMBackend* backend = LLMProviderRegistry::instance().getBackend(providerId);
        if (!backend) {
            result.message = QStringLiteral("Provider '%1' is not registered").arg(providerId);
            return result;
        }

        const auto entries = buildEntries(providerId, kind, QStringList{modelId}, false);
        if (!entries.isEmpty()) {
            result.driverProfileId = entries.first().driverProfileId;
        }

        const QString apiKey = LLMProviderRegistry::instance().getCredential(providerId);
        if (apiKey.trimmed().isEmpty() && ModelCatalogService::providerRequiresCredential(providerId)) {
            result.message = QStringLiteral("Provider '%1' has no credential configured").arg(providerId);
            return result;
        }

        if (kind == ModelCatalogKind::Embedding) {
            const EmbeddingResult embedding = backend->getEmbedding(apiKey, modelId, QStringLiteral("connection test"));
            if (embedding.hasError || embedding.vector.empty()) {
                result.message = embedding.errorMsg.trimmed().isEmpty()
                                     ? QStringLiteral("Embedding test returned no vector")
                                     : embedding.errorMsg;
                return result;
            }

            result.success = true;
            result.message = QStringLiteral("Embedding test OK (%1 dimensions)")
                                 .arg(static_cast<qulonglong>(embedding.vector.size()));
            return result;
        }

        if (kind == ModelCatalogKind::Image) {
            result.message = QStringLiteral("Image driver selected; live image generation test is not run from the selector");
            result.success = providerHasImplementedImageGeneration(providerId);
            return result;
        }

        const LLMResult response = backend->sendPrompt(apiKey,
                                                       modelId,
                                                       0.0,
                                                       modelProbeMaxTokens(providerId),
                                                       QStringLiteral("You are a connection test."),
                                                       QStringLiteral("Reply with exactly: OK"),
                                                       {});
        if (response.hasError) {
            result.message = response.errorMsg.trimmed().isEmpty()
                                 ? QStringLiteral("Chat test failed without a provider message")
                                 : response.errorMsg;
            return result;
        }

        result.success = true;
        const QString content = response.content.simplified();
        result.message = content.isEmpty()
                             ? QStringLiteral("Chat test OK")
                             : QStringLiteral("Chat test OK: %1").arg(content.left(120));
        return result;
    });
}

QStringList ModelCatalogService::modelIds(const QList<ModelCatalogEntry>& entries,
                                          bool includeHidden) const
{
    QStringList ids;
    for (const auto& entry : entries) {
        if (!includeHidden && entry.visibility == ModelCatalogVisibility::Hidden) {
            continue;
        }
        ids.append(entry.id);
    }
    ids.removeDuplicates();
    return ids;
}
