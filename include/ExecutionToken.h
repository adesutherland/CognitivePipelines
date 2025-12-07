#pragma once

#include <QUuid>
#include <QVariantMap>

// ExecutionToken represents a single unit of data and control flow
// in the event-driven execution engine (V3).
struct ExecutionToken
{
    // Identifier of the node that produced this token.
    QUuid sourceNodeId;

    // Identifier of the connection this token is traveling along.
    QUuid connectionId;

    // Data payload associated with this token (node output).
    QVariantMap data;
};
