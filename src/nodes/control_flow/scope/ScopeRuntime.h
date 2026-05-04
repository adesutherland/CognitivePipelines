#pragma once

#include "CommonDataTypes.h"

#include <QVariant>
#include <QVariantList>
#include <QVariantMap>
#include <QUuid>
#include <QString>

#include <functional>

enum class ScopeBodyKind {
    Transform,
    Iterator
};

struct ScopeFrame {
    QString bodyId;
    ScopeBodyKind kind {ScopeBodyKind::Transform};
    QUuid activationId;
    int attempt {0};
    int index {-1};
    int count {-1};
    QVariant input;
    QVariant item;
    QVariant previousOutput;
    QVariantMap context;
    QVariantList history;
};

struct ScopeBodyResult {
    bool ok {false};
    QVariant output;
    bool accepted {true};
    QVariant nextInput;
    bool skip {false};
    QVariantMap context;
    QString status;
    QString message;
    QString error;
    QVariantMap raw;
    DataPacket lastPacket;
};

using ScopeBodyRunner = std::function<ScopeBodyResult(const QString& bodyId,
                                                      ScopeBodyKind kind,
                                                      const ScopeFrame& frame,
                                                      const DataPacket& parentInputs)>;

QString scopeBodyKindToString(ScopeBodyKind kind);
ScopeBodyKind scopeBodyKindFromString(const QString& value,
                                      ScopeBodyKind fallback = ScopeBodyKind::Transform);
QVariantMap scopeVariantToMap(const QVariant& value);
QVariantList scopeVariantToList(const QVariant& value);
bool scopeVariantTruthy(const QVariant& value);
QVariant scopePreferredValue(const QVariant& primary, const QVariant& fallback);
void scopeMergeMap(QVariantMap& target, const QVariantMap& delta);
