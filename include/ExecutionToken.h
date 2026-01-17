#pragma once

#include <QUuid>
#include <QString>
#include <QVariantMap>

// ExecutionToken represents a single unit of data and control flow
// in the event-driven execution engine (V3).
struct ExecutionToken
{
    // Unique identifier for this specific token instance.
    // Used to detect stale vs. fresh tokens in control-flow nodes.
    QUuid tokenId;

    // Identifier of the node that produced this token.
    QUuid sourceNodeId;

    // Identifier of the connection this token is traveling along.
    QUuid connectionId;

    // The pin ID that triggered this token's creation (i.e., the pin that received
    // a fresh value). Other pins in the data payload may contain stale values from
    // the data lake snapshot. Control-flow nodes can use this to distinguish fresh
    // vs. stale input values.
    QString triggeringPinId;

    // Data payload associated with this token (node output).
    QVariantMap data;

    // If true, bypasses the ExecutionEngine's deduplication logic.
    bool forceExecution = false;
};
