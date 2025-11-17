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

#include "LLMConnector.h"
#include "PromptBuilderNode.h"
#include "ToolNodeDelegate.h"
#include "TextInputNode.h"
#include "ProcessConnector.h"
#include "GoogleLLMConnector.h"
#include "PythonScriptConnector.h"
#include "DatabaseConnector.h"
#include "TextOutputNode.h"
#include "HumanInputNode.h"

NodeGraphModel::NodeGraphModel(QObject* parent)
    : QtNodes::DataFlowGraphModel(std::make_shared<QtNodes::NodeDelegateModelRegistry>())
{
    Q_UNUSED(parent);

    // IMPORTANT: Disable reactive propagation by default.
    // Our ExecutionEngine is the only mechanism that should trigger execution.
    // By overriding setPortData below to return false without forwarding to NodeDelegateModel,
    // we prevent QtNodes from calling ToolNodeDelegate::setInData during connection changes/load.

    // Register LLMConnector via the generic ToolNodeDelegate adapter
    auto registry = dataModelRegistry();

    registry->registerModel([this]() {
        // Create connector and wrap into ToolNodeDelegate
        auto connector = std::make_shared<LLMConnector>();
        return std::make_unique<ToolNodeDelegate>(connector);
    }, QStringLiteral("Generative AI"));

    // Register PromptBuilderNode via the generic ToolNodeDelegate adapter
    registry->registerModel([this]() {
        auto tool = std::make_shared<PromptBuilderNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Text"));

    // Register TextInputNode via the generic ToolNodeDelegate adapter
    registry->registerModel([this]() {
        auto tool = std::make_shared<TextInputNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Inputs"));

    // Register TextOutputNode under the "Output" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto tool = std::make_shared<TextOutputNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("Output"));

    // Register ProcessConnector under the "Connectors" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto connector = std::make_shared<ProcessConnector>();
        return std::make_unique<ToolNodeDelegate>(connector);
    }, QStringLiteral("Connectors"));

    // Register GoogleLLMConnector under the "Connectors" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto connector = std::make_shared<GoogleLLMConnector>();
        return std::make_unique<ToolNodeDelegate>(connector);
    }, QStringLiteral("Connectors"));

    // Register PythonScriptConnector under the "Scripting" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto connector = std::make_shared<PythonScriptConnector>();
        return std::make_unique<ToolNodeDelegate>(connector);
    }, QStringLiteral("Scripting"));

    // Register DatabaseConnector under the "Data" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto connector = std::make_shared<DatabaseConnector>();
        return std::make_unique<ToolNodeDelegate>(connector);
    }, QStringLiteral("Data"));

    // Register HumanInputNode under the "I/O" category via ToolNodeDelegate
    registry->registerModel([this]() {
        auto tool = std::make_shared<HumanInputNode>();
        return std::make_unique<ToolNodeDelegate>(tool);
    }, QStringLiteral("I/O"));
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

QtNodes::NodeId NodeGraphModel::addNode(QString const nodeType)
{
    // Call base class to create the node
    QtNodes::NodeId nodeId = DataFlowGraphModel::addNode(nodeType);
    
    if (nodeId != QtNodes::InvalidNodeId) {
        // Get the delegate model for the new node
        auto model = delegateModel<QtNodes::NodeDelegateModel>(nodeId);
        if (model) {
            // Connect port change signals to our slots
            // When ports are inserted/deleted, we need to emit nodeUpdated(nodeId)
            // to trigger geometry recalculation in BasicGraphicsScene
            connect(model, &QtNodes::NodeDelegateModel::portsInserted,
                    this, &NodeGraphModel::onNodePortsInserted);
            connect(model, &QtNodes::NodeDelegateModel::portsDeleted,
                    this, &NodeGraphModel::onNodePortsDeleted);
        }
    }
    
    return nodeId;
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
