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

#include <QObject>
#include <QString>
#include <QVector>
#include <QReadWriteLock>

#include "ModelCaps.h"

class ModelCapsRegistry {
public:
    static ModelCapsRegistry& instance();

    struct ResolvedCaps {
        ModelCapsTypes::ModelCaps caps;
        QString ruleId; // empty if no explicit id on rule
    };

    bool loadFromFile(const QString& path);
    std::optional<ResolvedCaps> resolveWithRule(const QString& modelId, const QString& backendId = {}) const;
    std::optional<ModelCapsTypes::ModelCaps> resolve(const QString& modelId, const QString& backendId = {}) const;

private:
    ModelCapsRegistry() = default;
    Q_DISABLE_COPY(ModelCapsRegistry)

    QVector<ModelCapsTypes::ModelRule> rules_;
    mutable QReadWriteLock lock_;
};

// Endpoint routing metadata is now defined in ModelCaps.h under ModelCapsTypes.
