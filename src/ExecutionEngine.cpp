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
#include <QFuture>
#include <QFutureWatcher>
#include <QtConcurrent/QtConcurrent>
#include <QThread>

#include <QtNodes/DataFlowGraphModel>
#include <QtNodes/internal/Definitions.hpp>

#include "NodeGraphModel.h"
#include "ToolNodeDelegate.h"
#include "IToolConnector.h"

#include <unordered_set>
#include <queue>

ExecutionEngine::ExecutionEngine(NodeGraphModel* model, QObject* parent)
    : QObject(parent)
    , _graphModel(model)
{
}

void ExecutionEngine::run()
{
    // Clear any previous graph representation and outputs
    _dag.clear();
    _nodeOutputs.clear();

    if (!_graphModel) {
        qWarning() << "ExecutionEngine: No graph model available.";
        return;
    }

    // Deterministic UUID helpers for nodes and connections (used for UI status updates)
    const QUuid nodeNs("{6ba7b810-9dad-11d1-80b4-00c04fd430c8}"); // DNS namespace as a stable base
    const QUuid connNs("{6ba7b811-9dad-11d1-80b4-00c04fd430c8}"); // another stable base
    auto nodeUuid = [nodeNs](QtNodes::NodeId n) {
        QByteArray key = QByteArray::number(static_cast<qulonglong>(n));
        return QUuid::createUuidV5(nodeNs, key);
    };
    auto connectionUuid = [connNs](const QtNodes::ConnectionId& c) {
        QByteArray key = QByteArray::number(static_cast<qulonglong>(c.outNodeId)) + '/' +
                         QByteArray::number(static_cast<qulonglong>(c.outPortIndex)) + '>' +
                         QByteArray::number(static_cast<qulonglong>(c.inNodeId)) + '/' +
                         QByteArray::number(static_cast<qulonglong>(c.inPortIndex));
        return QUuid::createUuidV5(connNs, key);
    };

    // Reset canvas to Idle on main thread before launching worker
    {
        const auto resetNodeIds = _graphModel->allNodeIds();
        // Nodes to Idle
        for (auto n : resetNodeIds) {
            emit nodeStatusChanged(nodeUuid(n), static_cast<int>(ExecutionState::Idle));
        }
        // Connections to Idle
        std::unordered_set<QtNodes::ConnectionId> resetConnections;
        for (auto n : resetNodeIds) {
            auto conns = _graphModel->allConnectionIds(n);
            resetConnections.insert(conns.begin(), conns.end());
        }
        for (const auto &cid : resetConnections) {
            emit connectionStatusChanged(connectionUuid(cid), static_cast<int>(ExecutionState::Idle));
        }
    }

    // Ensure all nodes exist in the adjacency map (even if they have no outgoing edges)
    const auto nodeIds = _graphModel->allNodeIds();
    for (auto nodeId : nodeIds) {
        if (!_dag.contains(nodeId)) {
            _dag.insert(nodeId, {});
        }
    }

    // Collect all unique connections across the graph
    std::unordered_set<QtNodes::ConnectionId> allConnections;
    for (auto nodeId : nodeIds) {
        auto conns = _graphModel->allConnectionIds(nodeId);
        allConnections.insert(conns.begin(), conns.end());
    }

    // Build adjacency list: from outNodeId -> inNodeId
    for (const auto &conn : allConnections) {
        // Add edge only if both nodes are valid
        if (conn.outNodeId != QtNodes::InvalidNodeId && conn.inNodeId != QtNodes::InvalidNodeId) {
            // Ensure keys exist
            if (!_dag.contains(conn.outNodeId)) _dag.insert(conn.outNodeId, {});
            if (!_dag.contains(conn.inNodeId)) _dag.insert(conn.inNodeId, {});

            auto &neighbors = _dag[conn.outNodeId];
            neighbors.insert(conn.inNodeId);
        }
    }

    // Compute in-degree for Kahn's algorithm and collect roots
    QMap<QtNodes::NodeId, int> indegree;
    for (auto it = _dag.cbegin(); it != _dag.cend(); ++it) {
        const auto from = it.key();
        if (!indegree.contains(from)) indegree[from] = 0;
        for (auto toIt = it.value().cbegin(); toIt != it.value().cend(); ++toIt) {
            indegree[*toIt] = indegree.value(*toIt, 0) + 1;
        }
    }

    QList<QtNodes::NodeId> roots;
    for (auto it = indegree.cbegin(); it != indegree.cend(); ++it) {
        if (it.value() == 0) roots.append(it.key());
    }
    std::sort(roots.begin(), roots.end());

    // Kahn's topological sort producing a linear order
    QList<QtNodes::NodeId> topoOrder;
    std::queue<QtNodes::NodeId> q;
    for (auto id : roots) q.push(id);

    auto localIndegree = indegree; // copy
    while (!q.empty()) {
        auto u = q.front(); q.pop();
        topoOrder.append(u);
        const auto neighbors = _dag.value(u);
        for (const auto &v : neighbors) {
            localIndegree[v] = localIndegree.value(v, 0) - 1;
            if (localIndegree[v] == 0) {
                q.push(v);
            }
        }
    }

    if (topoOrder.size() != indegree.size()) {
        qWarning() << "ExecutionEngine: Cycle detected in graph or disconnected nodes not accounted for. Aborting execution.";
        return;
    }

    if (topoOrder.isEmpty()) {
        qWarning() << "ExecutionEngine: Graph is empty. Nothing to execute.";
        return;
    }

    // Helper to translate port index to our pin id using the delegate's dynamic mapping
    auto pinIdForIndex = [](ToolNodeDelegate* del, QtNodes::PortType portType, QtNodes::PortIndex idx) -> QString {
        if (!del) return {};
        return del->pinIdForIndex(portType, idx);
    };

    // Run the pipeline in a worker thread and emit status signals from there
    auto order = topoOrder;
    QPointer<NodeGraphModel> graphModel = _graphModel;

    [[maybe_unused]] QFuture<void> __execFuture = QtConcurrent::run([this, order, graphModel, pinIdForIndex, nodeUuid, connectionUuid]() mutable {
        if (!graphModel) {
            qWarning() << "ExecutionEngine: Graph model deleted during execution.";
            return;
        }

        bool aborted = false;
        DataPacket mainPacket;
        // Ensure the cumulative packet starts clean at the beginning of each run
        mainPacket.clear();

        // Iterate through nodes in topological order
        for (int idx = 0; idx < order.size(); ++idx) {
            if (!graphModel) { aborted = true; break; }
            const auto nodeId = order.at(idx);

            auto *delegate = graphModel->delegateModel<ToolNodeDelegate>(nodeId);
            if (!delegate) {
                qWarning() << "ExecutionEngine: No delegate for node" << nodeId << ". Skipping.";
                continue;
            }
            const auto connector = delegate->connector();
            if (!connector) {
                qWarning() << "ExecutionEngine: No connector for node" << nodeId << ". Skipping.";
                continue;
            }

            // Emit Running state for the node and its incoming connections
            emit nodeStatusChanged(nodeUuid(nodeId), static_cast<int>(ExecutionState::Running));
            const auto attached = graphModel->allConnectionIds(nodeId);
            for (const auto &cid : attached) {
                if (cid.inNodeId == nodeId) {
                    emit connectionStatusChanged(connectionUuid(cid), static_cast<int>(ExecutionState::Running));
                }
            }

            // Build input packet from main (cumulative) packet
            // New scheme: namespace values by source Node UUID and provide both current and old values.
            DataPacket inputPacket;
            for (const auto &connId : attached) {
                if (connId.inNodeId != nodeId) continue; // only incoming

                auto *srcDelegate = graphModel->delegateModel<ToolNodeDelegate>(connId.outNodeId);
                const QString srcPinId = pinIdForIndex(srcDelegate, QtNodes::PortType::Out, connId.outPortIndex);
                const QString dstPinId = pinIdForIndex(delegate, QtNodes::PortType::In, connId.inPortIndex);

                // Compute namespaced keys for source node's output pins
                const QUuid srcNodeUuid = nodeUuid(connId.outNodeId);
                const QString nsPrefix = srcNodeUuid.toString();
                const QString currentKey = nsPrefix + QLatin1String(".") + srcPinId;
                const QString oldKey = nsPrefix + QLatin1String(".old.") + srcPinId;

                // Look up Current and Old values from the cumulative packet
                QVariant currentVal = mainPacket.value(currentKey);
                const QVariant oldVal = mainPacket.value(oldKey);

                // Backward compatibility: if namespaced current is missing or empty, fall back to plain key
                const bool currentMissing = !currentVal.isValid() || (currentVal.metaType().id() == QMetaType::QString && currentVal.toString().isEmpty());
                if (currentMissing && mainPacket.contains(srcPinId)) {
                    currentVal = mainPacket.value(srcPinId);
                }

                inputPacket.insert(dstPinId, currentVal);
                inputPacket.insert(dstPinId + QLatin1String(".old"), oldVal);
            }

            const QString nodeName = connector->GetDescriptor().name;
            QString inputsStr;
            for (auto it = inputPacket.cbegin(); it != inputPacket.cend(); ++it) {
                if (!inputsStr.isEmpty()) inputsStr += ", ";
                inputsStr += it.key() + QLatin1String("=") + it.value().toString();
            }
            emit nodeLog(QString::fromLatin1("Executing Node: %1 %2 with INPUT: {%3}")
                             .arg(QString::number(nodeId)).arg(nodeName).arg(inputsStr));

            // Execute and wait for result in this worker thread
            DataPacket result;
            try {
                QFuture<DataPacket> future = connector->Execute(inputPacket);
                result = future.result();
            } catch (const std::exception &ex) {
                emit nodeLog(QString::fromLatin1("ExecutionEngine: Exception in node %1 %2: %3")
                                 .arg(QString::number(nodeId)).arg(nodeName).arg(ex.what()));
                emit nodeStatusChanged(nodeUuid(nodeId), static_cast<int>(ExecutionState::Error));
                for (const auto &cid : attached) {
                    if (cid.inNodeId == nodeId) {
                        emit connectionStatusChanged(connectionUuid(cid), static_cast<int>(ExecutionState::Error));
                    }
                }
                aborted = true;
                break;
            } catch (...) {
                emit nodeLog(QString::fromLatin1("ExecutionEngine: Unknown exception in node %1 %2")
                                 .arg(QString::number(nodeId)).arg(nodeName));
                emit nodeStatusChanged(nodeUuid(nodeId), static_cast<int>(ExecutionState::Error));
                for (const auto &cid : attached) {
                    if (cid.inNodeId == nodeId) {
                        emit connectionStatusChanged(connectionUuid(cid), static_cast<int>(ExecutionState::Error));
                    }
                }
                aborted = true;
                break;
            }

            _nodeOutputs[nodeId] = result;

            // Merge output into the main (cumulative) packet using namespaced CURRENT/OLD keys per pin
            const QUuid thisNodeUuid = nodeUuid(nodeId);
            const QString thisPrefix = thisNodeUuid.toString();
            for (auto it = result.cbegin(); it != result.cend(); ++it) {
                const QString &outputKey = it.key();
                const QVariant &newVal = it.value();

                const QString currentKey = thisPrefix + QLatin1String(".") + outputKey;
                const QString oldKey = thisPrefix + QLatin1String(".old.") + outputKey;

                // Swap: move existing current to old, then write new to current
                const QVariant previousVal = mainPacket.value(currentKey);
                mainPacket.insert(oldKey, previousVal);
                mainPacket.insert(currentKey, newVal);

                // Maintain non-namespaced key for compatibility with existing nodes/tests
                mainPacket.insert(outputKey, newVal);
            }

            // Error handling: check for error _before_ emitting any Finished states
            const QString errorMsg = result.value(QStringLiteral("__error")).toString();
            if (!errorMsg.trimmed().isEmpty()) {
                emit nodeLog(QString::fromLatin1("ExecutionEngine: Error reported by node %1 %2: %3")
                                 .arg(QString::number(nodeId)).arg(nodeName).arg(errorMsg));
                // Ensure error flag is visible in main packet as well
                mainPacket.insert(QStringLiteral("__error"), errorMsg);

                // Mark node and its incoming connections as Error
                emit nodeStatusChanged(nodeUuid(nodeId), static_cast<int>(ExecutionState::Error));
                for (const auto &cid : attached) {
                    if (cid.inNodeId == nodeId) {
                        emit connectionStatusChanged(connectionUuid(cid), static_cast<int>(ExecutionState::Error));
                    }
                }

                aborted = true;
                break;
            }

            // Success path: log and mark as Finished
            QString outputsStr;
            for (auto it = result.cbegin(); it != result.cend(); ++it) {
                if (!outputsStr.isEmpty()) outputsStr += ", ";
                outputsStr += it.key() + QLatin1String("=") + it.value().toString();
            }
            emit nodeLog(QString::fromLatin1("Node Finished: %1 %2 with OUTPUT: {%3}")
                             .arg(QString::number(nodeId)).arg(nodeName).arg(outputsStr));

            // Emit Finished for the node and its incoming connections
            emit nodeStatusChanged(nodeUuid(nodeId), static_cast<int>(ExecutionState::Finished));
            for (const auto &cid : attached) {
                if (cid.inNodeId == nodeId) {
                    emit connectionStatusChanged(connectionUuid(cid), static_cast<int>(ExecutionState::Finished));
                }
            }

            // Execution delay if configured
            if (m_executionDelay > 0) {
                QThread::msleep(static_cast<unsigned long>(m_executionDelay));
            }
        }

        // Emit chain finished/pipeline result
        emit nodeLog(QStringLiteral("ExecutionEngine: Chain finished."));
        emit pipelineFinished(mainPacket);
    });
}


void ExecutionEngine::setExecutionDelay(int ms)
{
    m_executionDelay = ms;
}
