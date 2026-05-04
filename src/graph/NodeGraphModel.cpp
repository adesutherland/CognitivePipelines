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
#include "NodeGraphModel.h"
#include <QtNodes/NodeDelegateModelRegistry>

#include "PromptBuilderNode.h"
#include "TextChunkerNode.h"
#include "ToolNodeDelegate.h"
#include "TextInputNode.h"
#include "IngestInputNode.h"
#include "ImageNode.h"
#include "MermaidNode.h"
#include "ProcessNode.h"
#include "UniversalLLMNode.h"
#include "ImageGenNode.h"
#include "PythonScriptNode.h"
#include "DatabaseNode.h"
#include "TextOutputNode.h"
#include "VaultOutputNode.h"
#include "HumanInputNode.h"
#include "PdfToImageNode.h"
#include "RagIndexerNode.h"
#include "RagQueryNode.h"
#include "ConditionalRouterNode.h"
#include "GetInputNode.h"
#include "GetItemNode.h"
#include "IteratorScopeNode.h"
#include "ScopeSubgraphExecutor.h"
#include "SetItemResultNode.h"
#include "SetOutputNode.h"
#include "TransformScopeNode.h"
#include "UniversalScriptNode.h"
#include "QuickJSRuntime.h"
#if CP_HAS_CREXX
#include "CrexxRuntime.h"
#endif
#include "ExecutionIdUtils.h"

#include <QJsonObject>
#include <QReadLocker>
#include <QWriteLocker>

namespace {
QString graphKindToString(NodeGraphModel::GraphKind kind)
{
    switch (kind) {
    case NodeGraphModel::GraphKind::TransformBody:
        return QStringLiteral("transform_body");
    case NodeGraphModel::GraphKind::IteratorBody:
        return QStringLiteral("iterator_body");
    case NodeGraphModel::GraphKind::Root:
    default:
        return QStringLiteral("root");
    }
}

NodeGraphModel::GraphKind graphKindFromString(const QString& value,
                                              NodeGraphModel::GraphKind fallback = NodeGraphModel::GraphKind::TransformBody)
{
    const QString v = value.trimmed().toLower();
    if (v == QStringLiteral("root")) {
        return NodeGraphModel::GraphKind::Root;
    }
    if (v == QStringLiteral("iterator_body")) {
        return NodeGraphModel::GraphKind::IteratorBody;
    }
    if (v == QStringLiteral("transform_body")) {
        return NodeGraphModel::GraphKind::TransformBody;
    }
    return fallback;
}
}

NodeGraphModel::NodeGraphModel(QObject* parent, GraphKind kind, const QString& executionScopeKey)
    : QtNodes::DataFlowGraphModel(std::make_shared<QtNodes::NodeDelegateModelRegistry>())
    , m_graphKind(kind)
    , m_executionScopeKey(executionScopeKey.trimmed().isEmpty() ? QStringLiteral("root") : executionScopeKey.trimmed())
{
    Q_UNUSED(parent);
    
    // Register default script engines
    ScriptEngineRegistry::instance().registerEngine(QStringLiteral("quickjs"), []() {
        return std::make_unique<QuickJSRuntime>();
    });
#if CP_HAS_CREXX
    ScriptEngineRegistry::instance().registerEngine(QStringLiteral("crexx"), []() {
        return std::make_unique<CrexxRuntime>();
    });
#endif

    // IMPORTANT: Disable reactive propagation by default.
    // Our ExecutionEngine is the only mechanism that should trigger execution.
    // By overriding setPortData below to return false without forwarding to NodeDelegateModel,
    // we prevent QtNodes from calling ToolNodeDelegate::setInData during connection changes/load.

    auto registry = dataModelRegistry();

    // Register PromptBuilderNode via the generic ToolNodeDelegate adapter
    registry->registerModel([this]() {
        auto tool = std::make_shared<PromptBuilderNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Text Utilities"));

    registry->registerModel([this]() {
        auto tool = std::make_shared<TextChunkerNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Text Utilities"));

    // Register TextInputNode via the generic ToolNodeDelegate adapter
    registry->registerModel([this]() {
        auto tool = std::make_shared<TextInputNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Input / Output"));

    registry->registerModel([this]() {
        auto tool = std::make_shared<IngestInputNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Input / Output"));

    // Register ImageNode under the "Input / Output" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto tool = std::make_shared<ImageNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Input / Output"));

    // Register MermaidNode under the "Visualization" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto tool = std::make_shared<MermaidNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Visualization"));

    // Register PdfToImageNode under the "Input / Output" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto tool = std::make_shared<PdfToImageNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Input / Output"));

    // Register TextOutputNode under the "Input / Output" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto tool = std::make_shared<TextOutputNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Input / Output"));

    registry->registerModel([this]() {
        auto tool = std::make_shared<VaultOutputNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Input / Output"));

    // Register ProcessNode under the "External Tools" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto node = std::make_shared<ProcessNode>();
        return std::make_unique<ToolNodeDelegate>(node);
    }, QStringLiteral("External Tools"));

    // Register UniversalLLMNode under the "AI Services" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto node = std::make_shared<UniversalLLMNode>();
        return std::make_unique<ToolNodeDelegate>(node);
    }, QStringLiteral("AI Services"));

    // Register ImageGenNode under the "AI Services" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto tool = std::make_shared<ImageGenNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("AI Services"));

    // Register PythonScriptNode under the "External Tools" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto node = std::make_shared<PythonScriptNode>();
        return std::make_unique<ToolNodeDelegate>(node);
    }, QStringLiteral("External Tools"));

    // Register DatabaseNode under the "Persistence" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto node = std::make_shared<DatabaseNode>();
        return std::make_unique<ToolNodeDelegate>(node);
    }, QStringLiteral("Persistence"));

    // Register RagIndexerNode under the "Persistence" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto tool = std::make_shared<RagIndexerNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Persistence"));

    // Register RagQueryNode under the "Retrieval" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto tool = std::make_shared<RagQueryNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Retrieval"));

    // Register HumanInputNode under the "Input / Output" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto tool = std::make_shared<HumanInputNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Input / Output"));

    // Register ConditionalRouterNode under the "Control Flow" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto tool = std::make_shared<ConditionalRouterNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Control Flow"));

    // Register scope parents in every graph kind so scopes can be nested.
    registry->registerModel([this]() {
        auto tool = std::make_shared<TransformScopeNode>();
        tool->setBodyRunner([this](const QString& bodyId,
                                   ScopeBodyKind kind,
                                   const ScopeFrame& frame,
                                   const DataPacket& parentInputs) {
            return ScopeSubgraphExecutor::run(ensureSubgraph(bodyId, GraphKind::TransformBody),
                                              kind,
                                              frame,
                                              parentInputs);
        });
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Control Flow"));

    registry->registerModel([this]() {
        auto tool = std::make_shared<IteratorScopeNode>();
        tool->setBodyRunner([this](const QString& bodyId,
                                   ScopeBodyKind kind,
                                   const ScopeFrame& frame,
                                   const DataPacket& parentInputs) {
            return ScopeSubgraphExecutor::run(ensureSubgraph(bodyId, GraphKind::IteratorBody),
                                              kind,
                                              frame,
                                              parentInputs);
        });
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Control Flow"));

    if (m_graphKind == GraphKind::TransformBody) {
        registry->registerModel([this]() {
            auto tool = std::make_shared<GetInputNode>();
            return std::make_unique<ToolNodeDelegate>(tool);
        }, QStringLiteral("Control Flow"));

        registry->registerModel([this]() {
            auto tool = std::make_shared<SetOutputNode>();
            return std::make_unique<ToolNodeDelegate>(tool);
        }, QStringLiteral("Control Flow"));
    } else if (m_graphKind == GraphKind::IteratorBody) {
        registry->registerModel([this]() {
            auto tool = std::make_shared<GetItemNode>();
            return std::make_unique<ToolNodeDelegate>(tool);
        }, QStringLiteral("Control Flow"));

        registry->registerModel([this]() {
            auto tool = std::make_shared<SetItemResultNode>();
            return std::make_unique<ToolNodeDelegate>(tool);
        }, QStringLiteral("Control Flow"));
    }

    // Register UniversalScriptNode under the "Scripting" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto node = std::make_shared<UniversalScriptNode>();
        return std::make_unique<ToolNodeDelegate>(node);
    }, QStringLiteral("Scripting"));
}

void NodeGraphModel::clear()
{
    clearNodeExecutionOutputs();

    // Collect node IDs first to avoid iterator invalidation
    const auto idsSet = allNodeIds();
    QList<QtNodes::NodeId> ids;
    ids.reserve(static_cast<int>(idsSet.size()));
    for (auto id : idsSet) ids.append(id);
    for (auto id : ids) {
        deleteNode(id);
    }
    m_subgraphs.clear();
}

QJsonObject NodeGraphModel::save() const
{
    QJsonObject root = DataFlowGraphModel::save();
    root.insert(QStringLiteral("_graph_kind"), graphKindToString(m_graphKind));
    QJsonObject subgraphs;
    for (auto it = m_subgraphs.cbegin(); it != m_subgraphs.cend(); ++it) {
        if (it->second) {
            subgraphs.insert(it->first, it->second->save());
        }
    }
    if (!subgraphs.isEmpty()) {
        root.insert(QStringLiteral("subgraphs"), subgraphs);
    }
    return root;
}

void NodeGraphModel::load(const QJsonObject& json)
{
    clear();
    DataFlowGraphModel::load(json);

    const QJsonObject subgraphs = json.value(QStringLiteral("subgraphs")).toObject();
    for (auto it = subgraphs.constBegin(); it != subgraphs.constEnd(); ++it) {
        const QJsonObject childJson = it.value().toObject();
        const GraphKind kind = graphKindFromString(childJson.value(QStringLiteral("_graph_kind")).toString());
        auto child = std::make_unique<NodeGraphModel>(nullptr, kind, it.key());
        child->load(childJson);
        m_subgraphs.emplace(it.key(), std::move(child));
    }
}

NodeGraphModel* NodeGraphModel::ensureSubgraph(const QString& subgraphId, GraphKind kind)
{
    const QString id = subgraphId.trimmed();
    if (id.isEmpty()) {
        return nullptr;
    }
    auto it = m_subgraphs.find(id);
    if (it != m_subgraphs.end()) {
        return it->second.get();
    }

    auto child = std::make_unique<NodeGraphModel>(nullptr, kind, id);
    NodeGraphModel* ptr = child.get();
    m_subgraphs.emplace(id, std::move(child));
    return ptr;
}

NodeGraphModel* NodeGraphModel::ensureSubgraph(const QString& subgraphId)
{
    return ensureSubgraph(subgraphId, GraphKind::TransformBody);
}

NodeGraphModel* NodeGraphModel::subgraph(const QString& subgraphId) const
{
    auto it = m_subgraphs.find(subgraphId);
    return it == m_subgraphs.cend() ? nullptr : it->second.get();
}

QList<NodeGraphModel*> NodeGraphModel::subgraphModels() const
{
    QList<NodeGraphModel*> models;
    for (auto it = m_subgraphs.cbegin(); it != m_subgraphs.cend(); ++it) {
        if (it->second) {
            models.append(it->second.get());
        }
    }
    return models;
}

QList<QPair<QUuid, QString>> NodeGraphModel::getEntryPoints() const
{
    QList<QPair<QUuid, QString>> result;
    const auto idsSet = allNodeIds();
    for (auto nodeId : idsSet) {
        bool hasIncomingOnPort0 = false;
        const auto conns = allConnectionIds(nodeId);
        for (const auto& cid : conns) {
            if (cid.inNodeId == nodeId && cid.inPortIndex == 0) { hasIncomingOnPort0 = true; break; }
        }
        if (hasIncomingOnPort0) continue;

        // Resolve label
        QString label;
        if (auto* del = const_cast<NodeGraphModel*>(this)->delegateModel<ToolNodeDelegate>(nodeId)) {
            const QString desc = del->description();
            if (!desc.isEmpty()) {
                label = desc;
            } else {
                if (auto node = del->node()) {
                    const NodeDescriptor d = node->getDescriptor();
                    label = d.name;
                }
            }
        }
        if (label.isEmpty()) {
            label = QStringLiteral("Node %1").arg(QString::number(nodeId));
        }

        result.append(qMakePair(ExecIds::nodeUuid(m_executionScopeKey, nodeId), label));
    }
    return result;
}

DataPacket NodeGraphModel::nodeOutput(QtNodes::NodeId nodeId) const
{
    QReadLocker locker(&m_nodeOutputsLock);
    return m_nodeOutputs.value(nodeId);
}

void NodeGraphModel::clearNodeExecutionOutputs()
{
    {
        QWriteLocker locker(&m_nodeOutputsLock);
        m_nodeOutputs.clear();
    }

    for (NodeGraphModel* child : subgraphModels()) {
        child->clearNodeExecutionOutputs();
    }
}

void NodeGraphModel::reportNodeExecutionStatus(QtNodes::NodeId nodeId, int state)
{
    emit executionNodeStatusChanged(ExecIds::nodeUuid(m_executionScopeKey, nodeId), state);
}

void NodeGraphModel::reportConnectionExecutionStatus(const QtNodes::ConnectionId& connectionId, int state)
{
    emit executionConnectionStatusChanged(ExecIds::connectionUuid(m_executionScopeKey, connectionId), state);
}

void NodeGraphModel::reportNodeOutput(QtNodes::NodeId nodeId, const DataPacket& packet)
{
    {
        QWriteLocker locker(&m_nodeOutputsLock);
        if (packet.isEmpty()) {
            m_nodeOutputs.remove(nodeId);
        } else {
            m_nodeOutputs.insert(nodeId, packet);
        }
    }
    emit executionNodeOutputChanged(nodeId);
}

QtNodes::NodeId NodeGraphModel::addNode(QString const nodeType)
{
    // Call base class to create the node
    QtNodes::NodeId nodeId = DataFlowGraphModel::addNode(nodeType);
    
    if (nodeId != QtNodes::InvalidNodeId) {
        connectNodeSignals(nodeId);
    }
    
    return nodeId;
}

bool NodeGraphModel::deleteNode(QtNodes::NodeId const nodeId)
{
    QString childBodyId;
    if (auto* delegate = delegateModel<ToolNodeDelegate>(nodeId)) {
        if (auto scope = std::dynamic_pointer_cast<TransformScopeNode>(delegate->node())) {
            childBodyId = scope->bodyId();
        } else if (auto iterator = std::dynamic_pointer_cast<IteratorScopeNode>(delegate->node())) {
            childBodyId = iterator->bodyId();
        }
    }

    const bool deleted = DataFlowGraphModel::deleteNode(nodeId);
    if (deleted && !childBodyId.isEmpty()) {
        m_subgraphs.erase(childBodyId);
    }
    return deleted;
}

void NodeGraphModel::loadNode(QJsonObject const &nodeJson)
{
    // Get the current number of nodes before loading
    const size_t nodeCountBefore = allNodeIds().size();
    
    // Call base class to actually load and create the node
    // This will create the node and call ToolNodeDelegate::load() which restores state
    DataFlowGraphModel::loadNode(nodeJson);
    
    // Get the current number of nodes after loading
    const auto allIds = allNodeIds();
    const size_t nodeCountAfter = allIds.size();
    
    // If a new node was created, find it and connect signals + trigger initial geometry update
    if (nodeCountAfter > nodeCountBefore) {
        // The new node is in the set. We need to find which one is new.
        // Since allNodeIds() returns an unordered_set, we can't reliably get "the last one"
        // Instead, we'll iterate and connect signals to ALL nodes (safe, as Qt ignores duplicate connections)
        for (auto nodeId : allIds) {
            connectNodeSignals(nodeId);
            
            // Emit nodeUpdated for the newly loaded node to trigger initial geometry calculation
            // This is necessary because ToolNodeDelegate::load() may have emitted embeddedWidgetSizeUpdated()
            // BEFORE we connected the signals above, so the initial size change was missed
            if (nodeCountAfter == nodeCountBefore + 1) {
                // Only one new node - emit update for it
                Q_EMIT nodeUpdated(nodeId);
            }
        }
    }
}

void NodeGraphModel::connectNodeSignals(QtNodes::NodeId nodeId)
{
    // Get the delegate model for the node
    auto model = delegateModel<QtNodes::NodeDelegateModel>(nodeId);
    if (model) {
        // Connect port change signals to our slots
        // When ports are inserted/deleted, we need to emit nodeUpdated(nodeId)
        // to trigger geometry recalculation in BasicGraphicsScene
        connect(model, &QtNodes::NodeDelegateModel::portsInserted,
                this, &NodeGraphModel::onNodePortsInserted);
        connect(model, &QtNodes::NodeDelegateModel::portsDeleted,
                this, &NodeGraphModel::onNodePortsDeleted);
        
        // Also connect embeddedWidgetSizeUpdated to trigger geometry recalculation
        // This is needed for embedded widgets like NodeInfoWidget to resize the node
        connect(model, &QtNodes::NodeDelegateModel::embeddedWidgetSizeUpdated,
                this, &NodeGraphModel::onNodePortsInserted);

        if (auto* toolDelegate = qobject_cast<ToolNodeDelegate*>(model)) {
            if (auto scope = std::dynamic_pointer_cast<TransformScopeNode>(toolDelegate->node())) {
                connect(scope.get(), &TransformScopeNode::openBodyRequested,
                        this, &NodeGraphModel::requestTransformBodyOpen,
                        Qt::UniqueConnection);
            } else if (auto iterator = std::dynamic_pointer_cast<IteratorScopeNode>(toolDelegate->node())) {
                connect(iterator.get(), &IteratorScopeNode::openBodyRequested,
                        this, &NodeGraphModel::requestIteratorBodyOpen,
                        Qt::UniqueConnection);
            }
        }
    }
}

void NodeGraphModel::requestTransformBodyOpen(const QString& bodyId, const QString& title)
{
    ensureSubgraph(bodyId, GraphKind::TransformBody);
    emit childGraphOpenRequested(bodyId, title, static_cast<int>(GraphKind::TransformBody));
}

void NodeGraphModel::requestIteratorBodyOpen(const QString& bodyId, const QString& title)
{
    ensureSubgraph(bodyId, GraphKind::IteratorBody);
    emit childGraphOpenRequested(bodyId, title, static_cast<int>(GraphKind::IteratorBody));
}

void NodeGraphModel::onNodePortsInserted()
{
    notifyNodeGeometryChanged();
}

void NodeGraphModel::onNodePortsDeleted()
{
    notifyNodeGeometryChanged();
}

void NodeGraphModel::notifyNodeGeometryChanged()
{
    // Find which node emitted the signal by checking if the sender is one of the delegates
    auto senderModel = qobject_cast<QtNodes::NodeDelegateModel*>(sender());
    if (!senderModel) {
        return;
    }

    // Search all nodes to find which one owns this delegate
    for (auto nodeId : allNodeIds()) {
        // Get the delegate model for this node
        auto model = delegateModel<QtNodes::NodeDelegateModel>(nodeId);
        if (model == senderModel) {
            // Found the node! Emit nodeUpdated to trigger geometry recalculation
            Q_EMIT nodeUpdated(nodeId);
            return;
        }
    }
}

bool NodeGraphModel::setPortData(QtNodes::NodeId nodeId,
                                 QtNodes::PortType portType,
                                 QtNodes::PortIndex portIndex,
                                 QVariant const &value,
                                 QtNodes::PortRole role)
{
    Q_UNUSED(nodeId);
    Q_UNUSED(portType);
    Q_UNUSED(portIndex);
    Q_UNUSED(value);
    Q_UNUSED(role);

    // Intentionally do nothing to prevent QtNodes' reactive data propagation.
    // Execution is controlled exclusively by our ExecutionEngine.
    return false;
}
