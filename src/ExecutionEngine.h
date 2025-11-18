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

#include <QObject>
#include <QUuid>
#include <QPointer>
#include <QMap>
#include <QSet>
#include <QMutex>

#include "CommonDataTypes.h"
#include "ExecutionState.h"

namespace QtNodes { class DataFlowGraphModel; using NodeId = unsigned int; }

class NodeGraphModel;

class ExecutionEngine : public QObject {
    Q_OBJECT
public:
    explicit ExecutionEngine(NodeGraphModel* model, QObject* parent = nullptr);
    ~ExecutionEngine() override = default;

signals:
    // Emitted once at the very end of a successful run containing only the final DataPacket
    void pipelineFinished(const DataPacket& finalOutput);
    // Emitted for detailed per-node execution logs
    void nodeLog(const QString& message);
    // Emitted when a node's status changes (state is one of ExecutionState)
    void nodeStatusChanged(const QUuid& nodeId, int state);
    // Emitted when a connection's status changes (state is one of ExecutionState)
    void connectionStatusChanged(const QUuid& connectionId, int state);

public slots:
    void run();
    void setExecutionDelay(int ms);

public:
    // Thread-safe accessor to retrieve output data for a specific node
    DataPacket nodeOutput(QtNodes::NodeId nodeId) const;

private:
    // Adjacency list: from nodeId -> set of downstream nodeIds
    QMap<QtNodes::NodeId, QSet<QtNodes::NodeId>> _dag;

    // Stores the output packet produced by each node after it executes
    QMap<QtNodes::NodeId, DataPacket> _nodeOutputs;

    // Protects access to _nodeOutputs (mutable allows locking in const methods)
    mutable QMutex _outputMutex;

    NodeGraphModel* _graphModel {nullptr};

    int m_executionDelay = 0;
};
