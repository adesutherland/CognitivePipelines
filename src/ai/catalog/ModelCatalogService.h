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

#include <QFuture>
#include <QList>
#include <QString>
#include <QStringList>

enum class ModelCatalogKind {
    Chat,
    Embedding,
    Image
};

enum class ModelCatalogVisibility {
    Recommended,
    Available,
    Hidden
};

struct ProviderCatalogEntry {
    QString id;
    QString name;
    QString statusText;
    bool requiresCredential {true};
    bool hasCredential {false};
    bool isLocal {false};
    bool isUsable {false};
};

struct ModelCatalogEntry {
    QString providerId;
    QString id;
    QString displayName;
    QString description;
    QString driverProfileId;
    QString filterReason;
    ModelCatalogKind kind {ModelCatalogKind::Chat};
    ModelCatalogVisibility visibility {ModelCatalogVisibility::Available};
    bool isAlias {false};
    bool isDynamic {false};
};

struct ModelTestResult {
    QString providerId;
    QString modelId;
    QString driverProfileId;
    QString message;
    bool success {false};
};

Q_DECLARE_METATYPE(ModelCatalogEntry)
Q_DECLARE_METATYPE(QList<ModelCatalogEntry>)
Q_DECLARE_METATYPE(ModelTestResult)

class ModelCatalogService {
public:
    static ModelCatalogService& instance();

    QList<ProviderCatalogEntry> providers(ModelCatalogKind kind = ModelCatalogKind::Chat) const;
    QString defaultProvider(ModelCatalogKind kind = ModelCatalogKind::Chat) const;
    QList<ModelCatalogEntry> fallbackModels(const QString& providerId,
                                            ModelCatalogKind kind = ModelCatalogKind::Chat) const;
    QFuture<QList<ModelCatalogEntry>> fetchModels(const QString& providerId,
                                                  ModelCatalogKind kind = ModelCatalogKind::Chat);
    QFuture<ModelTestResult> testModel(const QString& providerId,
                                       const QString& modelId,
                                       ModelCatalogKind kind = ModelCatalogKind::Chat);
    QStringList modelIds(const QList<ModelCatalogEntry>& entries,
                         bool includeHidden = false) const;

    static bool providerRequiresCredential(const QString& providerId);

private:
    ModelCatalogService() = default;
};
