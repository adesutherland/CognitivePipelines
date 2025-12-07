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

#include "ExecutionEngine.h"

#include <QDebug>
#include <QtConcurrent/QtConcurrent>
#include <QThread>

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/internal/Definitions.hpp>

#include "NodeGraphModel.h"
#include "ToolNodeDelegate.h"
#include "RagIndexerNode.h"

#include <unordered_set>

namespace {

// Stable namespaces used to derive deterministic UUIDs for nodes/connections.
const QUuid kNodeNamespace("{6ba7b810-9dad-11d1-80b4-00c04fd430c8}");
const QUuid kConnectionNamespace("{6ba7b811-9dad-11d1-80b4-00c04fd430c8}");

QUuid nodeUuidForId(QtNodes::NodeId nodeId)
{
    QByteArray key = QByteArray::number(static_cast<qulonglong>(nodeId));
    return QUuid::createUuidV5(kNodeNamespace, key);
}

QUuid connectionUuidForId(const QtNodes::ConnectionId& c)
{
    QByteArray key = QByteArray::number(static_cast<qulonglong>(c.outNodeId)) + '/' +
                     QByteArray::number(static_cast<qulonglong>(c.outPortIndex)) + '>' +
                     QByteArray::number(static_cast<qulonglong>(c.inNodeId)) + '/' +
                     QByteArray::number(static_cast<qulonglong>(c.inPortIndex));
    return QUuid::createUuidV5(kConnectionNamespace, key);
}

} // namespace

ExecutionEngine::ExecutionEngine(NodeGraphModel* model, QObject* parent)
    : QObject(parent)
    , _graphModel(model)
{
}

void ExecutionEngine::run()
{
    runPipeline();
}

void ExecutionEngine::runPipeline()
{
    if (!_graphModel) {
        qWarning() << "ExecutionEngine: No graph model available.";
        return;
    }

    // Clear global engine state
    {
        QMutexLocker locker(&m_stateMutex);
        m_dataLake.clear();
        m_pendingTokens.clear();
        m_nodesRunning.clear();
    }

    // Reset canvas to Idle before starting execution
    const auto nodeIds = _graphModel->allNodeIds();
    {
        // Nodes to Idle
        for (auto nodeId : nodeIds) {
            emit nodeStatusChanged(nodeUuidForId(nodeId), static_cast<int>(ExecutionState::Idle));
        }

        // Connections to Idle
        std::unordered_set<QtNodes::ConnectionId> resetConnections;
        for (auto nodeId : nodeIds) {
            const auto conns = _graphModel->allConnectionIds(nodeId);
            resetConnections.insert(conns.begin(), conns.end());
        }
        for (const auto& cid : resetConnections) {
            emit connectionStatusChanged(connectionUuidForId(cid), static_cast<int>(ExecutionState::Idle));
        }
    }

    // Initialize pending tokens: each node starts with the set of input pins
    // that still require tokens based on the current graph wiring.
    {
        QMutexLocker locker(&m_stateMutex);

        for (auto nodeId : nodeIds) {
            const QUuid nodeUuid = nodeUuidForId(nodeId);

            // Ensure every node has an entry, even if it has no inputs.
            QSet<PinId> pendingPins;

            auto* delegate = _graphModel->delegateModel<ToolNodeDelegate>(nodeId);
            if (!delegate) {
                // Nodes without a delegate are treated as having no required inputs.
                m_pendingTokens.insert(nodeUuid, pendingPins);
                continue;
            }

            const auto attached = _graphModel->allConnectionIds(nodeId);
            for (const auto& conn : attached) {
                if (conn.inNodeId != nodeId) {
                    continue; // consider only incoming edges
                }

                // Map the inbound port index to the logical PinId.
                const PinId pinId = delegate->pinIdForIndex(QtNodes::PortType::In, conn.inPortIndex);
                if (!pinId.isEmpty()) {
                    pendingPins.insert(pinId);
                }
            }

            m_pendingTokens.insert(nodeUuid, pendingPins);
        }
    }

    // Kick off the first scheduling pass.
    processTokens();
}

void ExecutionEngine::processTokens()
{
    if (!_graphModel) {
        return;
    }

    // Determine which nodes are currently Ready-to-Run.
    QVector<std::pair<QtNodes::NodeId, QUuid>> readyNodes;

    {
        QMutexLocker locker(&m_stateMutex);

        const auto nodeIds = _graphModel->allNodeIds();
        for (auto nodeId : nodeIds) {
            const QUuid nodeUuid = nodeUuidForId(nodeId);

            // Nodes that are no longer tracked in m_pendingTokens are
            // considered finished for this run.
            if (!m_pendingTokens.contains(nodeUuid)) {
                continue;
            }

            const QSet<PinId> pendingPins = m_pendingTokens.value(nodeUuid);
            if (!pendingPins.isEmpty()) {
                continue; // still waiting for at least one token
            }

            if (m_nodesRunning.contains(nodeUuid)) {
                continue; // already executing
            }

            // This node has no pending tokens and is not currently running.
            m_nodesRunning.insert(nodeUuid);
            m_pendingTokens.remove(nodeUuid);
            readyNodes.append({nodeId, nodeUuid});
        }
    }

    if (readyNodes.isEmpty()) {
        return; // nothing to schedule right now
    }

    // Launch each Ready-to-Run node asynchronously on the global QtConcurrent pool.
    for (const auto& pair : readyNodes) {
        const QtNodes::NodeId nodeId = pair.first;
        const QUuid nodeUuid = pair.second;

        QPointer<NodeGraphModel> graphModel(_graphModel);

        // Keep the returned QFuture to satisfy [[nodiscard]] on QtConcurrent::run.
        // We intentionally don't use it further because completion is observed
        // via signals and internal engine state.
        [[maybe_unused]] auto future = QtConcurrent::run([this, graphModel, nodeId, nodeUuid]() {
            if (!graphModel) {
                qWarning() << "ExecutionEngine: Graph model deleted during execution.";
                return;
            }

            auto* delegate = graphModel->delegateModel<ToolNodeDelegate>(nodeId);
            if (!delegate) {
                qWarning() << "ExecutionEngine: No delegate for node" << nodeId << ". Skipping.";

                // Mark as completed in terms of engine state.
                handleNodeCompleted(nodeId, nodeUuid, TokenList{});
                return;
            }

            const auto connector = delegate->connector();
            if (!connector) {
                qWarning() << "ExecutionEngine: No connector for node" << nodeId << ". Skipping.";
                handleNodeCompleted(nodeId, nodeUuid, TokenList{});
                return;
            }

            const NodeDescriptor descriptor = connector->getDescriptor();
            const QString nodeName = descriptor.name;

            // Emit Running state for the node and all of its incoming connections.
            emit nodeStatusChanged(nodeUuid, static_cast<int>(ExecutionState::Running));
            const auto attached = graphModel->allConnectionIds(nodeId);
            for (const auto& cid : attached) {
                if (cid.inNodeId == nodeId) {
                    emit connectionStatusChanged(connectionUuidForId(cid), static_cast<int>(ExecutionState::Running));
                }
            }

            // Build input tokens from the data lake based on the node's incoming
            // connections. For each input pin, we look up the upstream node and
            // output pin, fetch the corresponding value from m_dataLake, and
            // construct an ExecutionToken carrying that payload.
            TokenList incomingTokens;

            // First, collect all inbound connections along with their resolved
            // source node UUIDs and source pin ids. We keep this step outside
            // of the engine state mutex since it only touches the graph model.
            struct InboundEdge
            {
                QUuid                 sourceNodeUuid;
                PinId                 sourcePinId;
                PinId                 targetPinId;   // logical input pin id on the current node
                QtNodes::ConnectionId connectionId;
            };

            QVector<InboundEdge> inboundEdges;
            const auto inboundConnections = graphModel->allConnectionIds(nodeId);
            for (const auto& cid : inboundConnections) {
                if (cid.inNodeId != nodeId) {
                    continue; // only consider edges that terminate at this node
                }

                auto* sourceDelegate = graphModel->delegateModel<ToolNodeDelegate>(cid.outNodeId);
                if (!sourceDelegate) {
                    continue;
                }

                const PinId sourcePinId = sourceDelegate->pinIdForIndex(QtNodes::PortType::Out,
                                                                         cid.outPortIndex);
                if (sourcePinId.isEmpty()) {
                    continue;
                }

                auto* targetDelegate = graphModel->delegateModel<ToolNodeDelegate>(cid.inNodeId);
                if (!targetDelegate) {
                    continue;
                }

                InboundEdge edge;
                edge.sourceNodeUuid = nodeUuidForId(cid.outNodeId);
                edge.sourcePinId = sourcePinId;
                edge.targetPinId = targetDelegate->pinIdForIndex(QtNodes::PortType::In,
                                                                  cid.inPortIndex);
                edge.connectionId = cid;
                inboundEdges.push_back(edge);
            }

            {
                // Now, under the engine state mutex, translate the collected
                // inbound edges into concrete execution tokens using the
                // values already present in the data lake.
                QMutexLocker locker(&m_stateMutex);

                for (const auto& edge : inboundEdges) {
                    auto bucketIt = m_dataLake.constFind(edge.sourceNodeUuid);
                    if (bucketIt == m_dataLake.cend()) {
                        continue; // upstream node has not produced data yet
                    }

                    const QVariant value = bucketIt->value(edge.sourcePinId);
                    if (!value.isValid()) {
                        continue; // no value stored for this pin
                    }

                    ExecutionToken token;
                    token.sourceNodeId = edge.sourceNodeUuid;
                    token.connectionId = connectionUuidForId(edge.connectionId);
                    // IMPORTANT: route payload using the *target* pin id so
                    // that the receiving node sees values under its own
                    // logical input pin names (e.g. "input" for
                    // PromptBuilderNode), restoring the original V2
                    // semantics expected by the tests.
                    const QString payloadKey = edge.targetPinId.isEmpty()
                                              ? edge.sourcePinId
                                              : edge.targetPinId;
                    token.data.insert(payloadKey, value);

                    incomingTokens.push_back(std::move(token));
                }
            }

            emit nodeLog(QString::fromLatin1("Executing Node: %1 %2")
                             .arg(QString::number(nodeId)).arg(nodeName));

            // For long-running nodes like RagIndexerNode, listen for mid-run progress
            // updates and surface them via nodeOutputChanged.
            RagIndexerNode* ragIndexer = dynamic_cast<RagIndexerNode*>(connector.get());
            QMetaObject::Connection progressConn;
            if (ragIndexer) {
                progressConn = QObject::connect(ragIndexer, &RagIndexerNode::progressUpdated,
                                                this, [this, nodeId, nodeUuid](const DataPacket& progressPacket) {
                    QVariantMap variantMap;
                    for (auto it = progressPacket.cbegin(); it != progressPacket.cend(); ++it) {
                        variantMap.insert(it.key(), it.value());
                    }

                    {
                        QMutexLocker locker(&m_stateMutex);
                        m_dataLake[nodeUuid] = variantMap;
                    }

                    emit nodeOutputChanged(nodeId);
                });
            }

            TokenList outputTokens;
            try {
                outputTokens = connector->execute(incomingTokens);
            } catch (const std::exception& ex) {
                emit nodeLog(QString::fromLatin1("ExecutionEngine: Exception in node %1 %2: %3")
                                 .arg(QString::number(nodeId)).arg(nodeName).arg(ex.what()));
                emit nodeStatusChanged(nodeUuid, static_cast<int>(ExecutionState::Error));
                for (const auto& cid : attached) {
                    if (cid.inNodeId == nodeId) {
                        emit connectionStatusChanged(connectionUuidForId(cid), static_cast<int>(ExecutionState::Error));
                    }
                }

                if (ragIndexer) {
                    QObject::disconnect(progressConn);
                }

                // Mark node as no longer running in the engine state.
                handleNodeCompleted(nodeId, nodeUuid, TokenList{});
                return;
            } catch (...) {
                emit nodeLog(QString::fromLatin1("ExecutionEngine: Unknown exception in node %1 %2")
                                 .arg(QString::number(nodeId)).arg(nodeName));
                emit nodeStatusChanged(nodeUuid, static_cast<int>(ExecutionState::Error));
                for (const auto& cid : attached) {
                    if (cid.inNodeId == nodeId) {
                        emit connectionStatusChanged(connectionUuidForId(cid), static_cast<int>(ExecutionState::Error));
                    }
                }

                if (ragIndexer) {
                    QObject::disconnect(progressConn);
                }

                handleNodeCompleted(nodeId, nodeUuid, TokenList{});
                return;
            }

            if (ragIndexer) {
                QObject::disconnect(progressConn);
            }

            // Success path: mark as Finished and update engine state with produced tokens.
            emit nodeLog(QString::fromLatin1("Node Finished: %1 %2")
                             .arg(QString::number(nodeId)).arg(nodeName));

            handleNodeCompleted(nodeId, nodeUuid, outputTokens);

            emit nodeStatusChanged(nodeUuid, static_cast<int>(ExecutionState::Finished));
            for (const auto& cid : attached) {
                if (cid.inNodeId == nodeId) {
                    emit connectionStatusChanged(connectionUuidForId(cid), static_cast<int>(ExecutionState::Finished));
                }
            }

            if (m_executionDelay > 0) {
                QThread::msleep(static_cast<unsigned long>(m_executionDelay));
            }
        });
    }
}

void ExecutionEngine::handleNodeCompleted(QtNodes::NodeId nodeId,
                                          const QUuid& nodeUuid,
                                          const TokenList& outputTokens)
{
    bool shouldFinalize = false;

    // Precompute which downstream pins become unblocked as a result of this
    // node finishing. We do this before touching the shared engine state so
    // that we don't hold m_stateMutex while querying the graph model.
    QVector<QPair<QUuid, PinId>> unlockedPins; // (targetNodeUuid, targetPinId)

    if (_graphModel) {
        const auto attached = _graphModel->allConnectionIds(nodeId);
        for (const auto& cid : attached) {
            if (cid.outNodeId != nodeId) {
                continue; // only consider edges originating at this node
            }

            auto* targetDelegate = _graphModel->delegateModel<ToolNodeDelegate>(cid.inNodeId);
            if (!targetDelegate) {
                continue;
            }

            const PinId targetPinId = targetDelegate->pinIdForIndex(QtNodes::PortType::In,
                                                                     cid.inPortIndex);
            if (targetPinId.isEmpty()) {
                continue;
            }

            const QUuid targetUuid = nodeUuidForId(cid.inNodeId);
            unlockedPins.append(qMakePair(targetUuid, targetPinId));
        }
    }

    {
        QMutexLocker locker(&m_stateMutex);

        // Mark node as no longer running.
        m_nodesRunning.remove(nodeUuid);

        // Merge all produced tokens into the global data lake.
        bool thisNodeReportedError = false;
        for (const auto& token : outputTokens) {
            const QUuid producer = token.sourceNodeId.isNull() ? nodeUuid : token.sourceNodeId;
            QVariantMap& bucket = m_dataLake[producer];
            for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
                bucket.insert(it.key(), it.value());
            }

            // Detect a logical error flag emitted by the node. Any non-empty
            // "__error" field is treated as a hard pipeline error and will
            // short‑circuit further scheduling.
            const auto errIt = token.data.constFind(QStringLiteral("__error"));
            if (errIt != token.data.cend() && !errIt->toString().trimmed().isEmpty()) {
                thisNodeReportedError = true;
            }
        }

        if (thisNodeReportedError) {
            // Hard error: stop scheduling any further work. We keep the
            // data lake as‑is so that the final packet can surface the
            // error payload, but clear all pending tokens and running
            // nodes to force quiescence.
            m_pendingTokens.clear();
            m_nodesRunning.clear();
            shouldFinalize = true;
        } else {
            // Normal success path: unlock downstream dependencies. For each
            // connection that originates from this node's outputs, mark the
            // corresponding target pin as having received its token by
            // removing it from that node's pending set.
            for (const auto& pair : unlockedPins) {
                const QUuid& targetUuid = pair.first;
                const PinId& targetPinId = pair.second;

                auto it = m_pendingTokens.find(targetUuid);
                if (it == m_pendingTokens.end()) {
                    continue; // target node is either already running/finished or not tracked
                }

                it.value().remove(targetPinId);
            }

            // Quiescence detection: when there are no running nodes and no nodes
            // still waiting on tokens, the pipeline run is considered finished.
            if (m_nodesRunning.isEmpty() && m_pendingTokens.isEmpty()) {
                shouldFinalize = true;
            }
        }
    }

    // Notify UI that this node's output snapshot has changed.
    emit nodeOutputChanged(nodeId);

    // After updating state, attempt another scheduling pass to pick up any
    // nodes that became ready-to-run.
    processTokens();

    if (shouldFinalize) {
        // Build the final output packet for the whole pipeline.
        //
        // Historical behavior (relied upon by the unit and integration
        // tests) is that the final packet exposes plain pin IDs such as
        // "text", "prompt" or "response" — NOT the internal
        // "nodeUuid.pin" form used inside the data lake. Regressing to
        // namespaced keys broke several tests. To restore the original
        // contract we flatten the data lake back to pin IDs here.
        //
        // If any node reported a logical error (non‑empty "__error"
        // field), the final packet is built from all node buckets so
        // that the error payload is always visible. Otherwise we only
        // aggregate outputs from terminal nodes (those with no outgoing
        // edges), which naturally surfaces the final stage(s) of the
        // pipeline.

        DataPacket finalPacket;

        bool hasError = false;
        {
            QMutexLocker locker(&m_stateMutex);
            for (auto it = m_dataLake.cbegin(); it != m_dataLake.cend(); ++it) {
                const QVariantMap& bucket = it.value();
                auto errIt = bucket.constFind(QStringLiteral("__error"));
                if (errIt != bucket.cend() && !errIt->toString().trimmed().isEmpty()) {
                    hasError = true;
                    break;
                }
            }
        }

        if (hasError) {
            // Error case: flatten every node's bucket directly into the
            // final packet using plain pin IDs.
            QMutexLocker locker(&m_stateMutex);
            for (auto it = m_dataLake.cbegin(); it != m_dataLake.cend(); ++it) {
                const QVariantMap& bucket = it.value();
                for (auto valIt = bucket.cbegin(); valIt != bucket.cend(); ++valIt) {
                    finalPacket.insert(valIt.key(), valIt.value());
                }
            }
        } else {
            // Success case: collect only terminal nodes (no outgoing
            // connections) and flatten their outputs.
            if (_graphModel) {
                const auto allNodes = _graphModel->allNodeIds();

                // Determine which nodes have outgoing edges.
                QSet<QtNodes::NodeId> hasOutgoing;
                for (auto nid : allNodes) {
                    const auto attached = _graphModel->allConnectionIds(nid);
                    for (const auto& cid : attached) {
                        if (cid.outNodeId == nid) {
                            hasOutgoing.insert(nid);
                        }
                    }
                }

                QMutexLocker locker(&m_stateMutex);
                for (auto nid : allNodes) {
                    if (hasOutgoing.contains(nid)) {
                        continue; // not a terminal node
                    }

                    const QUuid uuid = nodeUuidForId(nid);
                    const QVariantMap bucket = m_dataLake.value(uuid);
                    for (auto it = bucket.cbegin(); it != bucket.cend(); ++it) {
                        finalPacket.insert(it.key(), it.value());
                    }
                }
            }
        }

        emit nodeLog(QStringLiteral("ExecutionEngine: Chain finished."));
        emit pipelineFinished(finalPacket);
    }
}

void ExecutionEngine::setExecutionDelay(int ms)
{
    m_executionDelay = ms;
}

DataPacket ExecutionEngine::nodeOutput(QtNodes::NodeId nodeId) const
{
    QMutexLocker locker(&m_stateMutex);
    const QUuid nodeUuid = nodeUuidForId(nodeId);

    const QVariantMap map = m_dataLake.value(nodeUuid);
    DataPacket result;
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        result.insert(it.key(), it.value());
    }
    return result;
}
