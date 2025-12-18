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
#include "ToolNodeDelegate.h"
#include "TextInputNode.h"
#include "ImageNode.h"
#include "MermaidNode.h"
#include "ProcessConnector.h"
#include "UniversalLLMNode.h"
#include "ImageGenNode.h"
#include "PythonScriptConnector.h"
#include "DatabaseConnector.h"
#include "TextOutputNode.h"
#include "HumanInputNode.h"
#include "PdfToImageNode.h"
#include "RagIndexerNode.h"
#include "RagQueryNode.h"
#include "ConditionalRouterNode.h"
#include "LoopNode.h"
#include "LoopUntilNode.h"
#include "ExecutionIdUtils.h"

NodeGraphModel::NodeGraphModel(QObject* parent)
    : QtNodes::DataFlowGraphModel(std::make_shared<QtNodes::NodeDelegateModelRegistry>())
{
    Q_UNUSED(parent);

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

    // Register TextInputNode via the generic ToolNodeDelegate adapter
    registry->registerModel([this]() {
        auto tool = std::make_shared<TextInputNode>();
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

    // Register ProcessConnector under the "External Tools" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto connector = std::make_shared<ProcessConnector>();
        return std::make_unique<ToolNodeDelegate>(connector);
    }, QStringLiteral("External Tools"));

    // Register UniversalLLMNode under the "AI Services" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto connector = std::make_shared<UniversalLLMNode>();
        return std::make_unique<ToolNodeDelegate>(connector);
    }, QStringLiteral("AI Services"));

    // Register ImageGenNode under the "AI Services" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto tool = std::make_shared<ImageGenNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("AI Services"));

    // Register PythonScriptConnector under the "External Tools" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto connector = std::make_shared<PythonScriptConnector>();
        return std::make_unique<ToolNodeDelegate>(connector);
    }, QStringLiteral("External Tools"));

    // Register DatabaseConnector under the "Persistence" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto connector = std::make_shared<DatabaseConnector>();
        return std::make_unique<ToolNodeDelegate>(connector);
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

    // Register LoopNode under the "Control Flow" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto tool = std::make_shared<LoopNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Control Flow"));

    // Register LoopUntilNode under the "Control Flow" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto tool = std::make_shared<LoopUntilNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Control Flow"));
}

void NodeGraphModel::clear()
{
    // Collect node IDs first to avoid iterator invalidation
    const auto idsSet = allNodeIds();
    QList<QtNodes::NodeId> ids;
    ids.reserve(static_cast<int>(idsSet.size()));
    for (auto id : idsSet) ids.append(id);
    for (auto id : ids) {
        deleteNode(id);
    }
}

QList<QPair<QUuid, QString>> NodeGraphModel::getEntryPoints() const
{
    QList<QPair<QUuid, QString>> result;
    const auto idsSet = allNodeIds();
    for (auto nodeId : idsSet) {
        bool hasIncoming = false;
        const auto conns = allConnectionIds(nodeId);
        for (const auto& cid : conns) {
            if (cid.inNodeId == nodeId) { hasIncoming = true; break; }
        }
        if (hasIncoming) continue;

        // Resolve label
        QString label;
        if (auto* del = const_cast<NodeGraphModel*>(this)->delegateModel<ToolNodeDelegate>(nodeId)) {
            const QString desc = del->description();
            if (!desc.isEmpty()) {
                label = desc;
            } else {
                if (auto conn = del->connector()) {
                    const NodeDescriptor d = conn->getDescriptor();
                    label = d.name;
                }
            }
        }
        if (label.isEmpty()) {
            label = QStringLiteral("Node %1").arg(QString::number(nodeId));
        }

        result.append(qMakePair(ExecIds::nodeUuid(nodeId), label));
    }
    return result;
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
    }
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
