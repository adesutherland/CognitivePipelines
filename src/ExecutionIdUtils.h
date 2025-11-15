//
// Cognitive Pipeline Application
//
// Deterministic QUuid helpers for nodes and connections used for execution-state signaling
//
// Copyright (c) 2025 Adrian Sutherland
//
// MIT License
//
#pragma once

#include <QUuid>
#include <QByteArray>
#include <QtNodes/internal/Definitions.hpp>

namespace ExecIds {

// Stable namespaces (UUID v5) used for generating deterministic ids
inline QUuid nodeNamespace()
{
    // DNS namespace as a stable base
    return QUuid("{6ba7b810-9dad-11d1-80b4-00c04fd430c8}");
}

inline QUuid connectionNamespace()
{
    // Another stable base
    return QUuid("{6ba7b811-9dad-11d1-80b4-00c04fd430c8}");
}

inline QUuid nodeUuid(QtNodes::NodeId n)
{
    const QByteArray key = QByteArray::number(static_cast<qulonglong>(n));
    return QUuid::createUuidV5(nodeNamespace(), key);
}

inline QUuid connectionUuid(const QtNodes::ConnectionId& c)
{
    const QByteArray key = QByteArray::number(static_cast<qulonglong>(c.outNodeId)) + '/' +
                           QByteArray::number(static_cast<qulonglong>(c.outPortIndex)) + '>' +
                           QByteArray::number(static_cast<qulonglong>(c.inNodeId)) + '/' +
                           QByteArray::number(static_cast<qulonglong>(c.inPortIndex));
    return QUuid::createUuidV5(connectionNamespace(), key);
}

} // namespace ExecIds
