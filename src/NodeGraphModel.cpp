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

NodeGraphModel::NodeGraphModel(QObject* parent)
    : QtNodes::DataFlowGraphModel(std::make_shared<QtNodes::NodeDelegateModelRegistry>())
{
    Q_UNUSED(parent);

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
