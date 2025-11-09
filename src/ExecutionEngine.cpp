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

    // Set up asynchronous chained execution over topoOrder
    struct ChainState {
        QList<QtNodes::NodeId> order;
        int index {0};
        QPointer<NodeGraphModel> graphModel;
    };

    auto state = new ChainState{topoOrder, 0, _graphModel};

    // Helper to translate port index to our pin id based on connector descriptor
    auto pinIdForIndex = [](ToolNodeDelegate* del, QtNodes::PortType portType, QtNodes::PortIndex idx) -> QString {
        if (!del) return {};
        auto conn = del->connector();
        if (!conn) return {};
        NodeDescriptor desc = conn->GetDescriptor();
        if (portType == QtNodes::PortType::In) {
            int i = 0;
            for (auto it = desc.inputPins.constBegin(); it != desc.inputPins.constEnd(); ++it, ++i) {
                if (static_cast<int>(idx) == i) return it.key();
            }
        } else if (portType == QtNodes::PortType::Out) {
            int i = 0;
            for (auto it = desc.outputPins.constBegin(); it != desc.outputPins.constEnd(); ++it, ++i) {
                if (static_cast<int>(idx) == i) return it.key();
            }
        }
        return {};
    };

    // Define a lambda to execute the next node in the chain using indirection to avoid self-capture warning
    auto runNextPtr = std::make_shared<std::function<void()>>();
    *runNextPtr = [this, state, runNextPtr, pinIdForIndex]() mutable {
        if (!state->graphModel) {
            qWarning() << "ExecutionEngine: Graph model deleted during execution.";
            delete state;
            return;
        }
        if (state->index >= state->order.size()) {
            // Locate the last node that actually produced an output
            DataPacket finalOutput;
            for (int i = state->order.size() - 1; i >= 0; --i) {
                auto id = state->order.at(i);
                if (_nodeOutputs.contains(id)) { finalOutput = _nodeOutputs.value(id); break; }
            }
            emit nodeLog(QStringLiteral("ExecutionEngine: Chain finished."));
            emit pipelineFinished(finalOutput);
            delete state;
            return;
        }

        const auto nodeId = state->order.at(state->index);

        // Fetch the ToolNodeDelegate for this node
        auto *delegate = state->graphModel->delegateModel<ToolNodeDelegate>(nodeId);
        if (!delegate) {
            qWarning() << "ExecutionEngine: No delegate for node" << nodeId << ". Skipping.";
            state->index++;
            QMetaObject::invokeMethod(this, [runNextPtr]() { (*runNextPtr)(); }, Qt::QueuedConnection);
            return;
        }

        const auto connector = delegate->connector();
        if (!connector) {
            qWarning() << "ExecutionEngine: No connector for node" << nodeId << ". Skipping.";
            state->index++;
            QMetaObject::invokeMethod(this, [runNextPtr]() { (*runNextPtr)(); }, Qt::QueuedConnection);
            return;
        }

        // Build input packet for this node from upstream outputs via pin-to-pin mapping
        DataPacket inputPacket;
        const auto attached = state->graphModel->allConnectionIds(nodeId);
        for (const auto &connId : attached) {
            if (connId.inNodeId != nodeId) continue; // only incoming
            auto *srcDelegate = state->graphModel->delegateModel<ToolNodeDelegate>(connId.outNodeId);

            const QString srcPinId = pinIdForIndex(srcDelegate, QtNodes::PortType::Out, connId.outPortIndex);
            const QString dstPinId = pinIdForIndex(delegate, QtNodes::PortType::In, connId.inPortIndex);

            QVariant v;
            if (_nodeOutputs.contains(connId.outNodeId)) {
                const auto &srcPacket = _nodeOutputs.value(connId.outNodeId);
                v = srcPacket.value(srcPinId);
            }
            inputPacket[dstPinId] = v;
        }

        const QString nodeName = connector->GetDescriptor().name;
        QString inputsStr;
        for (auto it = inputPacket.cbegin(); it != inputPacket.cend(); ++it) {
            if (!inputsStr.isEmpty()) inputsStr += ", ";
            inputsStr += it.key() + QLatin1String("=") + it.value().toString();
        }
        emit nodeLog(QString::fromLatin1("Executing Node: %1 %2 with INPUT: {%3}")
                         .arg(QString::number(nodeId)).arg(nodeName).arg(inputsStr));

        // Execute asynchronously with the correctly built input packet
        QFuture<DataPacket> future = connector->Execute(inputPacket);

        auto *watcher = new QFutureWatcher<DataPacket>(this);
        connect(watcher, &QFutureWatcher<DataPacket>::finished, this, [this, state, watcher, runNextPtr, nodeId, nodeName]() mutable {
            const DataPacket result = watcher->result();
            // Store this node's output packet for downstream nodes
            _nodeOutputs[nodeId] = result;

            QString outputsStr;
            for (auto it = result.cbegin(); it != result.cend(); ++it) {
                if (!outputsStr.isEmpty()) outputsStr += ", ";
                outputsStr += it.key() + QLatin1String("=") + it.value().toString();
            }
            emit nodeLog(QString::fromLatin1("Node Finished: %1 %2 with OUTPUT: {%3}")
                             .arg(QString::number(nodeId)).arg(nodeName).arg(outputsStr));

            // If the node reported an error via the special key, stop the pipeline here
            const QString errorMsg = result.value(QStringLiteral("__error")).toString();
            if (!errorMsg.trimmed().isEmpty()) {
                emit nodeLog(QString::fromLatin1("ExecutionEngine: Error reported by node %1 %2: %3")
                                 .arg(QString::number(nodeId)).arg(nodeName).arg(errorMsg));
                watcher->deleteLater();
                emit pipelineFinished(result);
                delete state;
                return;
            }

            watcher->deleteLater();
            state->index++;
            // Queue next execution to avoid deep recursion and keep UI responsive
            QMetaObject::invokeMethod(this, [runNextPtr]() { (*runNextPtr)(); }, Qt::QueuedConnection);
        });
        watcher->setFuture(future);
    };

    // Kick off the chain asynchronously
    QMetaObject::invokeMethod(this, [runNextPtr]() { (*runNextPtr)(); }, Qt::QueuedConnection);
}
