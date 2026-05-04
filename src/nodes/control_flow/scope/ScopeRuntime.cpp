#include "ScopeRuntime.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaType>

QString scopeBodyKindToString(ScopeBodyKind kind)
{
    switch (kind) {
    case ScopeBodyKind::Iterator:
        return QStringLiteral("iterator");
    case ScopeBodyKind::Transform:
    default:
        return QStringLiteral("transform");
    }
}

ScopeBodyKind scopeBodyKindFromString(const QString& value, ScopeBodyKind fallback)
{
    const QString v = value.trimmed().toLower();
    if (v == QStringLiteral("iterator")) {
        return ScopeBodyKind::Iterator;
    }
    if (v == QStringLiteral("transform")) {
        return ScopeBodyKind::Transform;
    }
    return fallback;
}

QVariantMap scopeVariantToMap(const QVariant& value)
{
    if (!value.isValid() || value.isNull()) {
        return {};
    }

    if (value.typeId() == QMetaType::QVariantMap) {
        return value.toMap();
    }

    if (value.canConvert<QJsonObject>()) {
        return value.value<QJsonObject>().toVariantMap();
    }

    const QString text = value.toString().trimmed();
    if (text.isEmpty()) {
        return {};
    }

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(text.toUtf8(), &error);
    if (error.error == QJsonParseError::NoError && doc.isObject()) {
        return doc.object().toVariantMap();
    }

    return {};
}

QVariantList scopeVariantToList(const QVariant& value)
{
    if (!value.isValid() || value.isNull()) {
        return {};
    }

    if (value.typeId() == QMetaType::QVariantList) {
        return value.toList();
    }

    if (value.typeId() == QMetaType::QStringList) {
        QVariantList list;
        const auto strings = value.toStringList();
        list.reserve(strings.size());
        for (const QString& item : strings) {
            list.append(item);
        }
        return list;
    }

    if (value.canConvert<QJsonArray>()) {
        return value.value<QJsonArray>().toVariantList();
    }

    const QString text = value.toString();
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return {};
    }

    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &error);
    if (error.error == QJsonParseError::NoError && doc.isArray()) {
        return doc.array().toVariantList();
    }

    if (text.contains(QLatin1Char('\n'))) {
        QVariantList lines;
        const QStringList split = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        lines.reserve(split.size());
        for (const QString& line : split) {
            const QString item = line.trimmed();
            if (!item.isEmpty()) {
                lines.append(item);
            }
        }
        return lines;
    }

    return QVariantList{value};
}

bool scopeVariantTruthy(const QVariant& value)
{
    if (!value.isValid() || value.isNull()) {
        return false;
    }

    if (value.typeId() == QMetaType::Bool) {
        return value.toBool();
    }
    if (value.canConvert<double>() && value.typeId() != QMetaType::QString) {
        return value.toDouble() != 0.0;
    }

    const QString v = value.toString().trimmed().toLower();
    if (v.isEmpty() ||
        v == QStringLiteral("false") ||
        v == QStringLiteral("0") ||
        v == QStringLiteral("no") ||
        v == QStringLiteral("n") ||
        v == QStringLiteral("off") ||
        v == QStringLiteral("null")) {
        return false;
    }

    return true;
}

QVariant scopePreferredValue(const QVariant& primary, const QVariant& fallback)
{
    return primary.isValid() && !primary.isNull() ? primary : fallback;
}

void scopeMergeMap(QVariantMap& target, const QVariantMap& delta)
{
    for (auto it = delta.cbegin(); it != delta.cend(); ++it) {
        target.insert(it.key(), it.value());
    }
}
