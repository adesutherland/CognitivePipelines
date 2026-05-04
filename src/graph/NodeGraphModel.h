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

#include <QtNodes/DataFlowGraphModel>

#include <QObject>
#include <QVariant>
#include <QList>
#include <QPair>
#include <QHash>
#include <QReadWriteLock>
#include <QUuid>

#include <map>
#include <memory>

#include "CommonDataTypes.h"

class NodeGraphModel : public QtNodes::DataFlowGraphModel
{
    Q_OBJECT

public:
    enum class GraphKind {
        Root,
        TransformBody,
        IteratorBody
    };

    explicit NodeGraphModel(QObject* parent = nullptr,
                            GraphKind kind = GraphKind::Root,
                            const QString& executionScopeKey = QStringLiteral("root"));

    // Convenience: remove all nodes and connections from the model
    void clear();

    // Override to connect port change signals when nodes are added
    QtNodes::NodeId addNode(QString const nodeType = QString()) override;
    bool deleteNode(QtNodes::NodeId const nodeId) override;

    // Override to connect port change signals when nodes are loaded from file
    void loadNode(QJsonObject const &nodeJson) override;

    // Persist nested child graphs alongside the visible graph.
    QJsonObject save() const override;
    void load(QJsonObject const &json) override;

    // Disable reactive data propagation from the base model. Our pipelines execute only via ExecutionEngine.
    bool setPortData(QtNodes::NodeId nodeId,
                     QtNodes::PortType portType,
                     QtNodes::PortIndex portIndex,
                     QVariant const &value,
                     QtNodes::PortRole role = QtNodes::PortRole::Data) override;

    // Discover entry points (nodes with no incoming connections).
    // Returns a list of pairs: {NodeUUID, Label}
    // Label resolution: ToolNodeDelegate::description() if non-empty, otherwise node type name from descriptor.
    QList<QPair<QUuid, QString>> getEntryPoints() const;

    NodeGraphModel* ensureSubgraph(const QString& subgraphId, GraphKind kind);
    NodeGraphModel* ensureSubgraph(const QString& subgraphId);
    NodeGraphModel* subgraph(const QString& subgraphId) const;
    QList<NodeGraphModel*> subgraphModels() const;
    GraphKind graphKind() const { return m_graphKind; }
    QString executionScopeKey() const { return m_executionScopeKey; }
    DataPacket nodeOutput(QtNodes::NodeId nodeId) const;
    void clearNodeExecutionOutputs();
    void reportNodeExecutionStatus(QtNodes::NodeId nodeId, int state);
    void reportConnectionExecutionStatus(const QtNodes::ConnectionId& connectionId, int state);
    void reportNodeOutput(QtNodes::NodeId nodeId, const DataPacket& packet);

signals:
    void childGraphOpenRequested(const QString& bodyId, const QString& title, int graphKind);
    void executionNodeStatusChanged(const QUuid& nodeId, int state);
    void executionConnectionStatusChanged(const QUuid& connectionId, int state);
    void executionNodeOutputChanged(QtNodes::NodeId nodeId);

public Q_SLOTS:
    // Slots to handle port changes and trigger geometry recalculation
    void onNodePortsInserted();
    void onNodePortsDeleted();

private:
    void requestTransformBodyOpen(const QString& bodyId, const QString& title);
    void requestIteratorBodyOpen(const QString& bodyId, const QString& title);

    // Helper to emit nodeUpdated for the sender node (triggers geometry recalculation)
    void notifyNodeGeometryChanged();
    
    // Helper to establish signal connections for a node (used by both addNode and loadNode)
    void connectNodeSignals(QtNodes::NodeId nodeId);

    std::map<QString, std::unique_ptr<NodeGraphModel>> m_subgraphs;
    GraphKind m_graphKind {GraphKind::Root};
    QString m_executionScopeKey {QStringLiteral("root")};
    mutable QReadWriteLock m_nodeOutputsLock;
    QHash<QtNodes::NodeId, DataPacket> m_nodeOutputs;
};
