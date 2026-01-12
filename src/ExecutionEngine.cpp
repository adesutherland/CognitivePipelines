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

#include "Logger.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QtConcurrent/QtConcurrent>
#include <QThread>
#include <QTimer>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QRegularExpression>
#include <algorithm>

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

// Thread-local execution context to expose current node id/uuid to node implementations for logging
// Defined here and referenced as 'extern' in control-flow node translation units.
thread_local QtNodes::NodeId g_CurrentNodeId = QtNodes::InvalidNodeId;
thread_local QUuid g_CurrentNodeUuid = QUuid();

// Helper to stringify QVariant for logging, truncating long strings and escaping newlines
QString ExecutionEngine::truncateAndEscape(const QVariant& v)
{
    QString s;
    if (v.typeId() == QMetaType::QVariantList || v.typeId() == QMetaType::QStringList || v.typeId() == QMetaType::QVariantMap) {
        s = QJsonDocument::fromVariant(v).toJson(QJsonDocument::Compact);
    } else {
        s = v.toString();
    }
    s.replace('\n', QStringLiteral("\\n"));
    if (s.size() > 100) {
        s = s.left(100) + QStringLiteral("…(truncated)");
    }
    return s;
}

void ExecutionEngine::setProjectName(const QString& name)
{
    m_projectName = name;
}

QString ExecutionEngine::getNodeOutputDir(const QString& nodeId, int runIndex) const
{
    QString sanitizedProject = m_projectName;
    // Sanitization: replace spaces/special chars with underscores
    // Valid characters: alphanumeric, underscore, hyphen
    static const QRegularExpression re("[^a-zA-Z0-9_-]");
    sanitizedProject.replace(re, QStringLiteral("_"));
    if (sanitizedProject.isEmpty()) {
        sanitizedProject = QStringLiteral("Untitled");
    }

    QString base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (base.isEmpty()) {
        base = QDir::homePath() + QStringLiteral("/Documents");
    }
    
    QString path = base + QStringLiteral("/CognitivePipelineOutput/") 
                 + sanitizedProject + QStringLiteral("/")
                 + QStringLiteral("Node_") + nodeId + QStringLiteral("/")
                 + QStringLiteral("Run_") + QString::number(runIndex) + QStringLiteral("/");
    
    return QDir::cleanPath(path);
}

ExecutionEngine::ExecutionEngine(NodeGraphModel* model, QObject* parent)
    : QObject(parent)
    , _graphModel(model)
{
    // Dispatcher throttling timer runs in the engine's thread (main/UI).
    // It sequences task launches at a fixed cadence to provide reliable slow-motion
    // and to avoid simultaneous sleeps across worker threads.
    m_throttler = new QTimer(this);
    m_throttler->setSingleShot(false);
    connect(m_throttler, &QTimer::timeout, this, &ExecutionEngine::onThrottleTimeout);

    m_finalizeTimer = new QTimer(this);
    m_finalizeTimer->setSingleShot(true);
    connect(m_finalizeTimer, &QTimer::timeout, this, &ExecutionEngine::onFinalizeTimeout);
}

ExecutionEngine::~ExecutionEngine()
{
}

void ExecutionEngine::run()
{
    runPipeline({});
}

void ExecutionEngine::runPipeline(const QList<QUuid>& specificEntryPoints)
{
    if (!_graphModel) {
        CP_WARN << "ExecutionEngine: No graph model available.";
        return;
    }

    // Clear global state
    {
        QWriteLocker stateLock(&m_dataLock);
        m_dataLake.clear();
    }
    {
        QMutexLocker qlock(&m_queueMutex);
        m_activeTasks = 0;
        m_finalized = false;
        m_lastInputSignature.clear();
        m_hardError = false;
    }

    // Stop throttler and clear dispatch queue for a clean run
    if (m_throttler) m_throttler->stop();
    m_dispatchQueue.clear();
    m_perNodeQueues.clear();
    m_nodeInFlight.clear();

    // New Run ID for this pipeline execution to guard against zombie threads
    m_currentRunId = QUuid::createUuid();

    // Reset node run counters for this session/run
    m_nodeRunCounters.clear();

    emit executionStarted();

    // Reset activity timestamp
    m_lastActivityMs = QDateTime::currentMSecsSinceEpoch();

    // Reset canvas to Idle before starting execution
    const auto nodeIds = _graphModel->allNodeIds();
    {
        for (auto nodeId : nodeIds) {
            emit nodeStatusChanged(nodeUuidForId(nodeId), static_cast<int>(ExecutionState::Idle));
        }
        std::unordered_set<QtNodes::ConnectionId> resetConnections;
        for (auto nodeId : nodeIds) {
            const auto conns = _graphModel->allConnectionIds(nodeId);
            resetConnections.insert(conns.begin(), conns.end());
        }
        for (const auto& cid : resetConnections) {
            emit connectionStatusChanged(connectionUuidForId(cid), static_cast<int>(ExecutionState::Idle));
        }
    }

    // Seed initial tasks
    if (specificEntryPoints.isEmpty()) {
        // Seed with all source nodes (nodes with no incoming edges)
        for (auto nodeId : nodeIds) {
            bool hasIncoming = false;
            const auto attached = _graphModel->allConnectionIds(nodeId);
            for (const auto& cid : attached) {
                if (cid.inNodeId == nodeId) { hasIncoming = true; break; }
            }
            if (!hasIncoming) {
                ExecutionTask task;
                task.nodeId = nodeId;
                task.nodeUuid = nodeUuidForId(nodeId);
                // Empty inputs are acceptable for source nodes
                dispatchTask(task);
            }
        }
    } else {
        // Seed only the specified entry point nodes
        QSet<QUuid> wanted;
        for (const auto& u : specificEntryPoints) wanted.insert(u);
        for (auto nodeId : nodeIds) {
            const QUuid uuid = nodeUuidForId(nodeId);
            if (!wanted.contains(uuid)) continue;
            ExecutionTask task;
            task.nodeId = nodeId;
            task.nodeUuid = uuid;
            dispatchTask(task);
        }
    }

    // In case there are no source nodes or all tasks were skipped, attempt finalization now
    tryFinalize();
}

void ExecutionEngine::dispatchTask(const ExecutionTask& task)
{
    // Assign run identity at scheduling time
    ExecutionTask toSchedule = task;
    toSchedule.runId = m_currentRunId;

    QMutexLocker locker(&m_queueMutex);
    if (m_hardError) return;

    if (m_executionDelay > 0) {
        // Allow independent source nodes to start in parallel even under slow-motion
        if (isSourceNode(toSchedule.nodeId)) {
            locker.unlock();
            launchTask(toSchedule);
            return;
        }
        // Throttle non-source launches: enqueue globally. Only trigger throttling
        // when the queue transitions from empty -> non-empty. This avoids flooding
        // the event loop with immediate dispatches which would effectively bypass
        // the pacing timer when many tasks are queued in rapid succession.
        m_dispatchQueue.enqueue(toSchedule);
        const bool isFirst = (m_dispatchQueue.size() == 1);
        locker.unlock();
        if (isFirst) {
            // Start pacing timer; first dispatch will occur on the first tick to
            // honor slow-motion for the initial emission as well.
            QMetaObject::invokeMethod(m_throttler, "start", Qt::QueuedConnection,
                                      Q_ARG(int, std::max(1, m_executionDelay)));
        }
        return;
    }

    // No delay: launch immediately
    // Per-node serialization: if a task is already running for this node, queue it
    const int inflight = m_nodeInFlight.value(toSchedule.nodeUuid, 0);
    if (inflight > 0) {
        m_perNodeQueues[toSchedule.nodeUuid].enqueue(toSchedule);
        return;
    }
    m_nodeInFlight.insert(toSchedule.nodeUuid, 1);
    locker.unlock();
    launchTask(toSchedule);
}

void ExecutionEngine::launchTask(const ExecutionTask& task)
{
    QString outputDir;
    {
        QMutexLocker locker(&m_queueMutex);
        if (m_hardError) return;
        ++m_activeTasks;

        const QString nodeIdStr = QString::number(task.nodeId);
        int runIndex = m_nodeRunCounters.value(nodeIdStr, 0);
        m_nodeRunCounters.insert(nodeIdStr, runIndex + 1);

        outputDir = getNodeOutputDir(nodeIdStr, runIndex);
        if (!QDir().mkpath(outputDir)) {
            emit nodeLog(QStringLiteral("FAILED to create output directory: %1").arg(outputDir));
        }
    }

    QPointer<NodeGraphModel> graphModel(_graphModel);
    // Launch concurrently (discard the QFuture as we don't need to track it)
    (void)QtConcurrent::run([this, task, graphModel, outputDir]() {
        // Update last activity time when a task actually begins its work
        m_lastActivityMs = QDateTime::currentMSecsSinceEpoch();
        // Worker Guard: if runId is stale, abandon work immediately
        if (task.runId != m_currentRunId) {
            QMutexLocker locker(&m_queueMutex);
            --m_activeTasks;
            tryFinalize();
            return;
        }
        if (!graphModel) {
            QMutexLocker locker(&m_queueMutex);
            --m_activeTasks;
            tryFinalize();
            return;
        }

        auto* delegate = graphModel->delegateModel<ToolNodeDelegate>(task.nodeId);
        if (!delegate) {
            handleTaskCompleted(task.nodeId, task.nodeUuid, TokenList{}, task.runId);
            QMutexLocker locker(&m_queueMutex);
            --m_activeTasks;
            tryFinalize();
            return;
        }

        const auto connector = delegate->connector();
        if (!connector) {
            handleTaskCompleted(task.nodeId, task.nodeUuid, TokenList{}, task.runId);
            QMutexLocker locker(&m_queueMutex);
            --m_activeTasks;
            tryFinalize();
            return;
        }

        const NodeDescriptor descriptor = connector->getDescriptor();
        const QString nodeName = descriptor.name;

        // Mark node and incoming connections Running
        emit nodeStatusChanged(task.nodeUuid, static_cast<int>(ExecutionState::Running));
        const auto attached = graphModel->allConnectionIds(task.nodeId);
        for (const auto& cid : attached) {
            if (cid.inNodeId == task.nodeId) {
                emit connectionStatusChanged(connectionUuidForId(cid), static_cast<int>(ExecutionState::Running));
            }
        }

        // Attempt to retrieve user-defined description/caption for better identification
        QString userCaption;
        if (delegate) {
            userCaption = delegate->description();
            if (userCaption.trimmed().isEmpty()) {
                userCaption = delegate->caption();
            }
        }

        emit nodeLog(QString::fromLatin1("Node Started: id=%1, type=%2, caption=\"%3\"")
                         .arg(QString::number(task.nodeId))
                         .arg(nodeName)
                         .arg(userCaption));
        // Backward-compatibility for existing tests/tools expecting this legacy prefix
        emit nodeLog(QString::fromLatin1("Executing Node: %1 %2")
                         .arg(QString::number(task.nodeId))
                         .arg(nodeName));

        // For long-running nodes like RagIndexerNode, forward progress updates
        RagIndexerNode* ragIndexer = dynamic_cast<RagIndexerNode*>(connector.get());
        QMetaObject::Connection progressConn;
        if (ragIndexer) {
            progressConn = QObject::connect(ragIndexer, &RagIndexerNode::progressUpdated,
                                            this, [this, nid = task.nodeId, uuid = task.nodeUuid, runId = task.runId](const DataPacket& progressPacket) {
                if (runId != m_currentRunId) return; // Run ID guard for progress
                QVariantMap variantMap;
                for (auto it = progressPacket.cbegin(); it != progressPacket.cend(); ++it) {
                    variantMap.insert(it.key(), it.value());
                }
                {
                    if (runId != m_currentRunId) return;
                    QWriteLocker locker(&m_dataLock);
                    m_dataLake[uuid] = variantMap;
                }
                // Deliver snapshot synchronously to receivers in the UI/main thread
                if (runId != m_currentRunId) return;
                QMetaObject::invokeMethod(this, "nodeOutputChanged",
                                          Qt::BlockingQueuedConnection,
                                          Q_ARG(QtNodes::NodeId, nid));
            });
        }

        TokenList outputTokens;
        try {
            // Set thread-local context for node-level logging
            g_CurrentNodeId = task.nodeId;
            g_CurrentNodeUuid = task.nodeUuid;

            // Inject system tokens (e.g., persistent node-specific output directory)
            TokenList effectiveInputs = task.inputs;
            if (!outputDir.isEmpty()) {
                ExecutionToken sysToken;
                sysToken.data.insert(QStringLiteral("_sys_node_output_dir"), outputDir);
                effectiveInputs.push_back(std::move(sysToken));
            }

            outputTokens = connector->execute(effectiveInputs);
        } catch (const std::exception& ex) {
            if (task.runId != m_currentRunId) {
                if (ragIndexer) QObject::disconnect(progressConn);
                QMutexLocker locker(&m_queueMutex);
                --m_activeTasks;
                tryFinalize();
                return;
            }
            emit nodeLog(QString::fromLatin1("ExecutionEngine: Exception in node %1 %2: %3")
                             .arg(QString::number(task.nodeId)).arg(nodeName).arg(ex.what()));
            emit nodeStatusChanged(task.nodeUuid, static_cast<int>(ExecutionState::Error));
            for (const auto& cid : attached) {
                if (cid.inNodeId == task.nodeId) {
                    emit connectionStatusChanged(connectionUuidForId(cid), static_cast<int>(ExecutionState::Error));
                }
            }
            // Clear thread-local context
            g_CurrentNodeId = QtNodes::InvalidNodeId;
            g_CurrentNodeUuid = QUuid();
            if (ragIndexer) QObject::disconnect(progressConn);
            handleTaskCompleted(task.nodeId, task.nodeUuid, TokenList{}, task.runId);
            QMutexLocker locker(&m_queueMutex);
            --m_activeTasks;
            tryFinalize();
            return;
        } catch (...) {
            if (task.runId != m_currentRunId) {
                if (ragIndexer) QObject::disconnect(progressConn);
                QMutexLocker locker(&m_queueMutex);
                --m_activeTasks;
                tryFinalize();
                return;
            }
            emit nodeLog(QString::fromLatin1("ExecutionEngine: Unknown exception in node %1 %2")
                             .arg(QString::number(task.nodeId)).arg(nodeName));
            emit nodeStatusChanged(task.nodeUuid, static_cast<int>(ExecutionState::Error));
            for (const auto& cid : attached) {
                if (cid.inNodeId == task.nodeId) {
                    emit connectionStatusChanged(connectionUuidForId(cid), static_cast<int>(ExecutionState::Error));
                }
            }
            // Clear thread-local context
            g_CurrentNodeId = QtNodes::InvalidNodeId;
            g_CurrentNodeUuid = QUuid();
            if (ragIndexer) QObject::disconnect(progressConn);
            handleTaskCompleted(task.nodeId, task.nodeUuid, TokenList{}, task.runId);
            QMutexLocker locker(&m_queueMutex);
            --m_activeTasks;
            tryFinalize();
            return;
        }

        if (ragIndexer) QObject::disconnect(progressConn);

        if (task.runId != m_currentRunId) {
            QMutexLocker locker(&m_queueMutex);
            --m_activeTasks;
            tryFinalize();
            return;
        }

        // Log completion and dump output DataPacket key/value pairs
        emit nodeLog(QString::fromLatin1("Node Finished: id=%1, type=%2")
                         .arg(QString::number(task.nodeId)).arg(nodeName));
        // Dump each key/value from produced tokens in a normalized, single-line format
        int tokenIndex = 0;
        for (const auto& tok : outputTokens) {
            for (auto it = tok.data.cbegin(); it != tok.data.cend(); ++it) {
                const QString key = it.key();
                const QString val = ExecutionEngine::truncateAndEscape(it.value());
                emit nodeLog(QString::fromLatin1("  Output[%1] %2 = \"%3\"")
                                 .arg(QString::number(tokenIndex))
                                 .arg(key)
                                 .arg(val));
            }
            ++tokenIndex;
        }

        // Mark finished and propagate
        handleTaskCompleted(task.nodeId, task.nodeUuid, outputTokens, task.runId);

        // Clear thread-local context after successful execution
        g_CurrentNodeId = QtNodes::InvalidNodeId;
        g_CurrentNodeUuid = QUuid();

        emit nodeStatusChanged(task.nodeUuid, static_cast<int>(ExecutionState::Finished));
        for (const auto& cid : attached) {
            if (cid.inNodeId == task.nodeId) {
                emit connectionStatusChanged(connectionUuidForId(cid), static_cast<int>(ExecutionState::Finished));
            }
        }

        QMutexLocker locker(&m_queueMutex);
        --m_activeTasks;
        // Per-node serialization: if queued tasks exist for this node, launch next
        if (m_executionDelay == 0) {
            auto it = m_perNodeQueues.find(task.nodeUuid);
            if (it != m_perNodeQueues.end() && !it->isEmpty()) {
                ExecutionTask nextTask = it->dequeue();
                // keep node marked inflight
                locker.unlock();
                launchTask(nextTask);
                return;
            } else {
                m_nodeInFlight.insert(task.nodeUuid, 0);
            }
        }
        tryFinalize();
    });
}

void ExecutionEngine::tryFinalize()
{
    if (m_finalized) return;
    if (m_activeTasks != 0) return;
    if (!m_dispatchQueue.isEmpty()) return;
    // Ensure all per-node queues are empty
    for (auto it = m_perNodeQueues.cbegin(); it != m_perNodeQueues.cend(); ++it) {
        if (!it.value().isEmpty()) return;
    }

    // If slow-motion is enabled, enforce a minimum delay since last activity
    if (m_executionDelay > 0) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qint64 elapsed = now - m_lastActivityMs;
        const int remaining = m_executionDelay - static_cast<int>(elapsed);
        if (remaining > 0) {
            // Schedule finalize on engine thread regardless of caller thread
            QTimer::singleShot(remaining, this, &ExecutionEngine::onFinalizeTimeout);
            return;
        }
    }

    // Build final packet
    DataPacket finalPacket;
    bool hasError = false;
    {
        QReadLocker locker(&m_dataLock);
        for (auto it = m_dataLake.cbegin(); it != m_dataLake.cend(); ++it) {
            const QVariantMap& bucket = it.value();
            auto errIt = bucket.constFind(QStringLiteral("__error"));
            if (errIt != bucket.cend() && !errIt->toString().trimmed().isEmpty()) {
                hasError = true; break;
            }
        }
    }

    if (hasError) {
        QReadLocker locker(&m_dataLock);
        for (auto it = m_dataLake.cbegin(); it != m_dataLake.cend(); ++it) {
            const QVariantMap& bucket = it.value();
            for (auto vit = bucket.cbegin(); vit != bucket.cend(); ++vit) {
                finalPacket.insert(vit.key(), vit.value());
            }
        }
    } else if (_graphModel) {
        const auto allNodes = _graphModel->allNodeIds();
        QSet<QtNodes::NodeId> hasOutgoing;
        for (auto nid : allNodes) {
            const auto attached = _graphModel->allConnectionIds(nid);
            for (const auto& cid : attached) if (cid.outNodeId == nid) hasOutgoing.insert(nid);
        }
        QReadLocker locker(&m_dataLock);
        for (auto nid : allNodes) {
            if (hasOutgoing.contains(nid)) continue;
            const QUuid uuid = nodeUuidForId(nid);
            const QVariantMap bucket = m_dataLake.value(uuid);
            for (auto it = bucket.cbegin(); it != bucket.cend(); ++it) {
                finalPacket.insert(it.key(), it.value());
            }
        }
    }

    m_finalized = true;
    emit nodeLog(QStringLiteral("ExecutionEngine: Chain finished."));
    emit pipelineFinished(finalPacket);
    emit executionFinished();
}

QByteArray ExecutionEngine::computeInputSignature(const QVariantMap& inputPayload) const
{
    QJsonObject jobj = QJsonObject::fromVariantMap(inputPayload);
    const QByteArray json = QJsonDocument(jobj).toJson(QJsonDocument::Compact);
    return QCryptographicHash::hash(json, QCryptographicHash::Sha256);
}

void ExecutionEngine::handleTaskCompleted(QtNodes::NodeId nodeId,
                                          const QUuid& nodeUuid,
                                          const TokenList& outputTokens,
                                          const QUuid& runId)
{
    // Data Guard: if this completion belongs to a stale run, ignore
    if (runId != m_currentRunId) return;

    if (outputTokens.empty()) {
        QMutexLocker ql(&m_queueMutex);
        m_lastInputSignature.remove(nodeUuid);
        return;
    }

    // Update data lake snapshot for this node (merge all produced tokens)
    bool thisNodeReportedError = false;
    {
        QWriteLocker locker(&m_dataLock);
        for (const auto& token : outputTokens) {
            const QUuid producer = token.sourceNodeId.isNull() ? nodeUuid : token.sourceNodeId;
            QVariantMap& bucket = m_dataLake[producer];
            for (auto it = token.data.cbegin(); it != token.data.cend(); ++it) {
                bucket.insert(it.key(), it.value());
            }
            const auto errIt = token.data.constFind(QStringLiteral("__error"));
            if (errIt != token.data.cend() && !errIt->toString().trimmed().isEmpty()) {
                thisNodeReportedError = true;
            }
        }
    }

    // Emit snapshot synchronously so tests observe per-iteration values
    if (runId == m_currentRunId) {
        QMetaObject::invokeMethod(this, "nodeOutputChanged",
                                  Qt::BlockingQueuedConnection,
                                  Q_ARG(QtNodes::NodeId, nodeId));
    }

    // Propagate each produced token to all connected downstream nodes.
    if (!_graphModel) return;

    // If a hard error occurred, stop scheduling new work
    if (thisNodeReportedError) {
        QMutexLocker qlock(&m_queueMutex);
        m_hardError = true;
        return;
    }

    const auto attached = _graphModel->allConnectionIds(nodeId);
    // Build mapping of connection to pin ids
    struct Edge { QtNodes::ConnectionId cid; PinId sourcePinId; PinId targetPinId; };
    QVector<Edge> edges;
    for (const auto& cid : attached) {
        if (cid.outNodeId != nodeId) continue;
        auto* srcDel = _graphModel->delegateModel<ToolNodeDelegate>(cid.outNodeId);
        auto* dstDel = _graphModel->delegateModel<ToolNodeDelegate>(cid.inNodeId);
        if (!srcDel || !dstDel) continue;
        const PinId sPin = srcDel->pinIdForIndex(QtNodes::PortType::Out, cid.outPortIndex);
        const PinId tPin = dstDel->pinIdForIndex(QtNodes::PortType::In, cid.inPortIndex);
        if (sPin.isEmpty() || tPin.isEmpty()) continue;
        edges.push_back({cid, sPin, tPin});
    }

    // Input snapshotting: For each edge that is triggered by a token, immediately
    // build a full input payload for the target node using the triggering token
    // for that target pin and the latest values in the data lake for other pins.
    for (const auto& tok : outputTokens) {
        const QUuid triggerTokenId = tok.tokenId.isNull() ? QUuid::createUuid() : tok.tokenId;
        for (const auto& e : edges) {
            const auto it = tok.data.constFind(e.sourcePinId);
            if (it == tok.data.cend()) continue; // token didn't fire this pin

            const QtNodes::NodeId targetNodeId = e.cid.inNodeId;
            const QUuid targetUuid = nodeUuidForId(targetNodeId);

            // Collect all inbound edges for the target to know required pins
            struct InEdge { QUuid sourceNodeUuid; PinId sourcePin; PinId targetPin; };
            QVector<InEdge> inEdges;
            const auto inboundConnections = _graphModel->allConnectionIds(targetNodeId);
            for (const auto& cid : inboundConnections) {
                if (cid.inNodeId != targetNodeId) continue;
                auto* srcDel = _graphModel->delegateModel<ToolNodeDelegate>(cid.outNodeId);
                auto* dstDel = _graphModel->delegateModel<ToolNodeDelegate>(cid.inNodeId);
                if (!srcDel || !dstDel) continue;
                const PinId sPin = srcDel->pinIdForIndex(QtNodes::PortType::Out, cid.outPortIndex);
                const PinId tPin = dstDel->pinIdForIndex(QtNodes::PortType::In, cid.inPortIndex);
                if (sPin.isEmpty() || tPin.isEmpty()) continue;
                inEdges.push_back({ nodeUuidForId(cid.outNodeId), sPin, tPin });
            }

            QVariantMap inputPayload;
            // Start with the triggering value
            inputPayload.insert(e.targetPinId, it.value());

            // Fill remaining pins from the latest data lake snapshot under a read lock
            {
                QReadLocker rlock(&m_dataLock);
                for (const auto& ie : inEdges) {
                    if (ie.targetPin == e.targetPinId) continue; // already set by triggering token
                    const QVariantMap bucket = m_dataLake.value(ie.sourceNodeUuid);
                    const QVariant v = bucket.value(ie.sourcePin);
                    if (v.isValid()) inputPayload.insert(ie.targetPin, v);
                }
            }

            // Node-negotiated readiness: ask target connector if inputs are sufficient
            auto* targetDel = _graphModel->delegateModel<ToolNodeDelegate>(targetNodeId);
            if (!targetDel) {
                continue;
            }
            auto targetConnector = targetDel->connector();
            if (!targetConnector) {
                continue;
            }
            if (!targetConnector->isReady(inputPayload, inEdges.size())) {
                // Inputs not sufficient per node policy; skip scheduling for now
                continue;
            }

            // Deduplicate: compute signature of inputs and avoid duplicate executions
            const QByteArray signature = computeInputSignature(inputPayload);
            {
                QMutexLocker ql(&m_queueMutex);
                const QByteArray last = m_lastInputSignature.value(targetUuid);
                if (!last.isEmpty() && last == signature) {
                    continue; // same inputs as last execution for this node
                }
                m_lastInputSignature.insert(targetUuid, signature);
            }

            // Create the snapshot TokenList for the target node
            TokenList snap;
            ExecutionToken t;
            t.tokenId = triggerTokenId;  // Preserve triggering token identity
            t.sourceNodeId = nodeUuid;
            t.connectionId = connectionUuidForId(e.cid);
            t.triggeringPinId = e.targetPinId;  // The pin that received a fresh value
            t.data = inputPayload;
            snap.push_back(std::move(t));

            ExecutionTask next;
            next.nodeId = targetNodeId;
            next.nodeUuid = targetUuid;
            next.inputs = std::move(snap);
            if (runId == m_currentRunId) {
                dispatchTask(next);
            }
        }
    }
}

void ExecutionEngine::onThrottleTimeout()
{
    if (m_dispatchQueue.isEmpty()) {
        if (m_throttler) m_throttler->stop();
        tryFinalize();
        return;
    }

    // Pop ONE task and launch it
    ExecutionTask task = m_dispatchQueue.dequeue();
    m_lastActivityMs = QDateTime::currentMSecsSinceEpoch();
    launchTask(task);

    if (m_dispatchQueue.isEmpty()) {
        if (m_throttler) m_throttler->stop();
        tryFinalize();
    }
}

bool ExecutionEngine::isSourceNode(QtNodes::NodeId nodeId) const
{
    if (!_graphModel) return false;
    const auto attached = _graphModel->allConnectionIds(nodeId);
    for (const auto& cid : attached) { if (cid.inNodeId == nodeId) return false; }
    return true;
}

void ExecutionEngine::onFinalizeTimeout()
{
    // Timer to delay finalization to satisfy slow-motion elapsed expectations
    tryFinalize();
}

void ExecutionEngine::setExecutionDelay(int ms)
{
    m_executionDelay = ms;
}

DataPacket ExecutionEngine::nodeOutput(QtNodes::NodeId nodeId) const
{
    QReadLocker locker(&m_dataLock);
    const QUuid nodeUuid = nodeUuidForId(nodeId);

    const QVariantMap map = m_dataLake.value(nodeUuid);
    DataPacket result;
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        result.insert(it.key(), it.value());
    }
    return result;
}
