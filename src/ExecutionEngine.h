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
#include <QHash>
#include <QSet>
#include <QMutex>
#include <QList>
#include <QReadWriteLock>
#include <QQueue>

#include "CommonDataTypes.h"
#include "ExecutionState.h"
#include "IToolConnector.h"

namespace QtNodes { class DataFlowGraphModel; using NodeId = unsigned int; }

class NodeGraphModel;
class QTimer;

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

    // Emitted whenever a node's output data packet is updated (including mid-run progress
    // updates for long-running nodes like RagIndexerNode). The UI can listen to this to
    // refresh the Stage Output view for the currently selected node.
    void nodeOutputChanged(QtNodes::NodeId nodeId);

public slots:
    void run();
    void setExecutionDelay(int ms);

public:
    // Thread-safe accessor to retrieve output data for a specific node
    DataPacket nodeOutput(QtNodes::NodeId nodeId) const;

private:
    // V3.1 task-queue based execution engine state ------------------------

    // A single unit of scheduled work for a node.
    struct ExecutionTask {
        QtNodes::NodeId nodeId {0};
        QUuid            nodeUuid;
        TokenList        inputs;   // snapshot of ready-to-use input packets
        QUuid            runId;    // run identifier for safety across restarts
    };

    // Global Data Lake: for each node UUID we store a QVariantMap of its
    // successfully produced outputs, keyed by pin name
    QHash<QUuid, QVariantMap> m_dataLake;

    // For deduplicating repeated executions with identical inputs (e.g., when
    // multiple upstream pins trigger separately but resolve to the same full
    // input set). Keyed by target node UUID.
    QHash<QUuid, QByteArray> m_lastInputSignature;

    // Simple task queue mutex used for counters and guarding concurrent scheduling
    mutable QMutex       m_queueMutex;
    int                  m_activeTasks {0};
    bool                 m_finalized {false};

    // Protects access to the data lake (readers/writers)
    mutable QReadWriteLock m_dataLock;

    NodeGraphModel* _graphModel {nullptr};

    int m_executionDelay = 0;

    // Hard error flag to stop further scheduling when a node reports an error
    bool m_hardError = false;

    // Internal helpers for the token-based scheduler
    void runPipeline();

    // Dispatch helpers
    void dispatchTask(const ExecutionTask& task);
    void launchTask(const ExecutionTask& task);
    void tryFinalize();
    void handleTaskCompleted(QtNodes::NodeId nodeId,
                             const QUuid& nodeUuid,
                             const TokenList& outputTokens,
                             const QUuid& runId);
    bool isSourceNode(QtNodes::NodeId nodeId) const;

signals:
    // Global execution lifecycle
    void executionStarted();
    void executionFinished();

private slots:
    void onThrottleTimeout();
    void onFinalizeTimeout();

private:
    // Run identity used to guard against zombie threads from previous runs
    QUuid m_currentRunId;

    // Dispatcher throttling: main-thread timer launching tasks at a fixed cadence
    QQueue<ExecutionTask> m_dispatchQueue;
    QTimer* m_throttler {nullptr};

    // Per-node serialization to preserve in-order execution for the same target
    QHash<QUuid, int> m_nodeInFlight; // 0 or 1 per nodeUuid
    QHash<QUuid, QQueue<ExecutionTask>> m_perNodeQueues;

    // Finalization delay to satisfy slow-motion elapsed timing semantics
    QTimer* m_finalizeTimer {nullptr};
    qint64  m_lastActivityMs {0};
};
