#include "ScopeSubgraphExecutor.h"

#include "NodeGraphModel.h"
#include "SetItemResultNode.h"
#include "SetOutputNode.h"
#include "ToolNodeDelegate.h"
#include "ExecutionState.h"

#include <QtNodes/Definitions>

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QQueue>

namespace {
struct QueuedExecution {
    QtNodes::NodeId nodeId {QtNodes::InvalidNodeId};
    TokenList inputs;
};

struct Edge {
    QtNodes::ConnectionId cid;
    PinId sourcePin;
    PinId targetPin;
};

QByteArray inputSignature(const QVariantMap& input)
{
    return QCryptographicHash::hash(QJsonDocument::fromVariant(input).toJson(QJsonDocument::Compact),
                                    QCryptographicHash::Sha256);
}

DataPacket systemFramePacket(const ScopeFrame& frame, const DataPacket& parentInputs)
{
    DataPacket packet;
    packet.insert(QStringLiteral("_scope_body_id"), frame.bodyId);
    packet.insert(QStringLiteral("_scope_kind"), scopeBodyKindToString(frame.kind));
    packet.insert(QStringLiteral("_scope_activation_id"), frame.activationId);
    packet.insert(QStringLiteral("_scope_context"), frame.context);
    packet.insert(QStringLiteral("_scope_history"), frame.history);

    packet.insert(QStringLiteral("_transform_input"), frame.input);
    packet.insert(QStringLiteral("_transform_attempt"), frame.attempt);
    packet.insert(QStringLiteral("_transform_previous_output"), frame.previousOutput);

    packet.insert(QStringLiteral("_iterator_item"), frame.item);
    packet.insert(QStringLiteral("_iterator_index"), frame.index);
    packet.insert(QStringLiteral("_iterator_count"), frame.count);

    for (auto it = parentInputs.cbegin(); it != parentInputs.cend(); ++it) {
        packet.insert(QStringLiteral("_parent_%1").arg(it.key()), it.value());
    }
    return packet;
}

bool hasIncoming(NodeGraphModel* graph, QtNodes::NodeId nodeId)
{
    const auto connections = graph->allConnectionIds(nodeId);
    for (const auto& cid : connections) {
        if (cid.inNodeId == nodeId) {
            return true;
        }
    }
    return false;
}

QVector<Edge> outgoingEdges(NodeGraphModel* graph, QtNodes::NodeId nodeId)
{
    QVector<Edge> edges;
    const auto connections = graph->allConnectionIds(nodeId);
    for (const auto& cid : connections) {
        if (cid.outNodeId != nodeId) {
            continue;
        }
        auto* srcDel = graph->delegateModel<ToolNodeDelegate>(cid.outNodeId);
        auto* dstDel = graph->delegateModel<ToolNodeDelegate>(cid.inNodeId);
        if (!srcDel || !dstDel) {
            continue;
        }
        const PinId sourcePin = srcDel->pinIdForIndex(QtNodes::PortType::Out, cid.outPortIndex);
        const PinId targetPin = dstDel->pinIdForIndex(QtNodes::PortType::In, cid.inPortIndex);
        if (sourcePin.isEmpty() || targetPin.isEmpty()) {
            continue;
        }
        edges.push_back({cid, sourcePin, targetPin});
    }
    return edges;
}

QVector<Edge> incomingEdges(NodeGraphModel* graph, QtNodes::NodeId nodeId)
{
    QVector<Edge> edges;
    const auto connections = graph->allConnectionIds(nodeId);
    for (const auto& cid : connections) {
        if (cid.inNodeId != nodeId) {
            continue;
        }
        auto* srcDel = graph->delegateModel<ToolNodeDelegate>(cid.outNodeId);
        auto* dstDel = graph->delegateModel<ToolNodeDelegate>(cid.inNodeId);
        if (!srcDel || !dstDel) {
            continue;
        }
        const PinId sourcePin = srcDel->pinIdForIndex(QtNodes::PortType::Out, cid.outPortIndex);
        const PinId targetPin = dstDel->pinIdForIndex(QtNodes::PortType::In, cid.inPortIndex);
        if (sourcePin.isEmpty() || targetPin.isEmpty()) {
            continue;
        }
        edges.push_back({cid, sourcePin, targetPin});
    }
    return edges;
}

void reportNode(NodeGraphModel* graph, QtNodes::NodeId nodeId, ExecutionState state)
{
    if (graph) {
        graph->reportNodeExecutionStatus(nodeId, static_cast<int>(state));
    }
}

void reportIncomingConnections(NodeGraphModel* graph, QtNodes::NodeId nodeId, ExecutionState state)
{
    if (!graph) {
        return;
    }
    const auto connections = graph->allConnectionIds(nodeId);
    for (const auto& cid : connections) {
        if (cid.inNodeId == nodeId) {
            graph->reportConnectionExecutionStatus(cid, static_cast<int>(state));
        }
    }
}

void reportConnection(NodeGraphModel* graph, const QtNodes::ConnectionId& cid, ExecutionState state)
{
    if (graph) {
        graph->reportConnectionExecutionStatus(cid, static_cast<int>(state));
    }
}

ScopeBodyResult transformResultFromMap(const QVariantMap& map)
{
    ScopeBodyResult result;
    result.ok = true;
    result.raw = map;
    result.output = map.value(QStringLiteral("output"));
    result.accepted = map.contains(QStringLiteral("accepted"))
        ? scopeVariantTruthy(map.value(QStringLiteral("accepted")))
        : true;
    result.nextInput = map.value(QStringLiteral("next_input"));
    result.context = scopeVariantToMap(map.value(QStringLiteral("context")));
    result.message = map.value(QStringLiteral("message")).toString();
    result.error = map.value(QStringLiteral("error")).toString().trimmed();
    result.status = result.accepted ? QStringLiteral("accepted") : QStringLiteral("retry");
    if (!result.error.isEmpty()) {
        result.ok = false;
        result.status = QStringLiteral("error");
    }
    return result;
}

ScopeBodyResult iteratorResultFromMap(const QVariantMap& map)
{
    ScopeBodyResult result;
    result.ok = true;
    result.raw = map;
    result.output = map.value(QStringLiteral("output"));
    result.skip = scopeVariantTruthy(map.value(QStringLiteral("skip")));
    result.context = scopeVariantToMap(map.value(QStringLiteral("context")));
    result.message = map.value(QStringLiteral("message")).toString();
    result.error = map.value(QStringLiteral("error")).toString().trimmed();
    result.status = result.skip ? QStringLiteral("skipped") : QStringLiteral("completed");
    if (!result.error.isEmpty()) {
        result.ok = false;
        result.status = QStringLiteral("error");
    }
    return result;
}
}

ScopeBodyResult ScopeSubgraphExecutor::run(NodeGraphModel* graph,
                                           ScopeBodyKind kind,
                                           const ScopeFrame& frame,
                                           const DataPacket& parentInputs)
{
    ScopeBodyResult result;
    if (!graph) {
        result.error = QStringLiteral("Scope body graph is not available.");
        return result;
    }
    if (graph->allNodeIds().empty()) {
        result.error = kind == ScopeBodyKind::Iterator
            ? QStringLiteral("Iterator body is empty. Add Get Item, body work, and Set Item Result.")
            : QStringLiteral("Transform body is empty. Add Get Input, body work, and Set Output.");
        return result;
    }

    const DataPacket framePacket = systemFramePacket(frame, parentInputs);
    QQueue<QueuedExecution> queue;
    QMap<QtNodes::NodeId, QVariantMap> dataLake;
    QMap<QtNodes::NodeId, QByteArray> lastSignature;

    const auto nodeIds = graph->allNodeIds();
    for (auto nodeId : nodeIds) {
        reportNode(graph, nodeId, ExecutionState::Idle);
        graph->reportNodeOutput(nodeId, DataPacket{});
        const auto connections = graph->allConnectionIds(nodeId);
        for (const auto& cid : connections) {
            if (cid.outNodeId == nodeId) {
                reportConnection(graph, cid, ExecutionState::Idle);
            }
        }
    }

    for (auto nodeId : nodeIds) {
        if (!hasIncoming(graph, nodeId)) {
            ExecutionToken seed;
            seed.data = framePacket;
            queue.enqueue({nodeId, TokenList{std::move(seed)}});
        }
    }

    if (queue.isEmpty()) {
        result.error = QStringLiteral("Scope body has no source node. Add the matching Get node or another source node.");
        return result;
    }

    int steps = 0;
    constexpr int kMaxSteps = 1000;
    while (!queue.isEmpty()) {
        if (++steps > kMaxSteps) {
            result.error = QStringLiteral("Scope body exceeded the internal step limit.");
            return result;
        }

        QueuedExecution item = queue.dequeue();
        auto* delegate = graph->delegateModel<ToolNodeDelegate>(item.nodeId);
        if (!delegate || !delegate->node()) {
            continue;
        }

        reportNode(graph, item.nodeId, ExecutionState::Running);
        reportIncomingConnections(graph, item.nodeId, ExecutionState::Running);

        const TokenList outputs = delegate->node()->execute(item.inputs);
        if (outputs.empty()) {
            reportNode(graph, item.nodeId, ExecutionState::Finished);
            reportIncomingConnections(graph, item.nodeId, ExecutionState::Finished);
            continue;
        }

        QVariantMap merged;
        for (const auto& token : outputs) {
            for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
                merged.insert(it.key(), it.value());
            }
        }
        result.lastPacket = merged;
        dataLake[item.nodeId] = merged;
        graph->reportNodeOutput(item.nodeId, merged);

        const QVariant terminalValue = kind == ScopeBodyKind::Iterator
            ? merged.value(QString::fromLatin1(SetItemResultNode::kOutputBodyResultId))
            : merged.value(QString::fromLatin1(SetOutputNode::kOutputBodyResultId));
        if (terminalValue.isValid()) {
            result = kind == ScopeBodyKind::Iterator
                ? iteratorResultFromMap(scopeVariantToMap(terminalValue))
                : transformResultFromMap(scopeVariantToMap(terminalValue));
            result.lastPacket = merged;
            const auto state = result.ok ? ExecutionState::Finished : ExecutionState::Error;
            reportNode(graph, item.nodeId, state);
            reportIncomingConnections(graph, item.nodeId, state);
            return result;
        }

        const QString error = merged.value(QStringLiteral("__error")).toString().trimmed();
        if (!error.isEmpty()) {
            result.error = error;
            result.status = QStringLiteral("error");
            reportNode(graph, item.nodeId, ExecutionState::Error);
            reportIncomingConnections(graph, item.nodeId, ExecutionState::Error);
            return result;
        }

        reportNode(graph, item.nodeId, ExecutionState::Finished);
        reportIncomingConnections(graph, item.nodeId, ExecutionState::Finished);

        const QVector<Edge> outEdges = outgoingEdges(graph, item.nodeId);
        for (const auto& token : outputs) {
            for (const Edge& edge : outEdges) {
                const auto outIt = token.data.constFind(edge.sourcePin);
                if (outIt == token.data.cend()) {
                    continue;
                }

                QVariantMap inputPayload;
                inputPayload.insert(edge.targetPin, outIt.value());

                const QVector<Edge> inEdges = incomingEdges(graph, edge.cid.inNodeId);
                for (const Edge& inEdge : inEdges) {
                    if (inEdge.targetPin == edge.targetPin) {
                        continue;
                    }
                    const QVariantMap bucket = dataLake.value(inEdge.cid.outNodeId);
                    const QVariant value = bucket.value(inEdge.sourcePin);
                    if (value.isValid()) {
                        inputPayload.insert(inEdge.targetPin, value);
                    }
                }

                auto* targetDelegate = graph->delegateModel<ToolNodeDelegate>(edge.cid.inNodeId);
                if (!targetDelegate || !targetDelegate->node()) {
                    continue;
                }
                if (!targetDelegate->node()->isReady(inputPayload, inEdges.size())) {
                    continue;
                }

                const QByteArray signature = inputSignature(inputPayload);
                if (lastSignature.value(edge.cid.inNodeId) == signature) {
                    continue;
                }
                lastSignature.insert(edge.cid.inNodeId, signature);

                reportConnection(graph, edge.cid, ExecutionState::Finished);

                ExecutionToken next;
                next.data = inputPayload;
                next.triggeringPinId = edge.targetPin;
                queue.enqueue({edge.cid.inNodeId, TokenList{std::move(next)}});
            }
        }
    }

    result.error = kind == ScopeBodyKind::Iterator
        ? QStringLiteral("Iterator body completed without Set Item Result.")
        : QStringLiteral("Transform body completed without Set Output.");
    return result;
}
