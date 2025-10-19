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

#include <QMessageBox>
#include <QApplication>

#include <QtNodes/DataFlowGraphModel>

#include "NodeGraphModel.h"
#include "ToolNodeDelegate.h"
#include "LLMConnector.h"

ExecutionEngine::ExecutionEngine(NodeGraphModel* model, QObject* parent)
    : QObject(parent)
    , _graphModel(model)
{
}

void ExecutionEngine::run()
{
    if (!_graphModel) {
        QMessageBox::warning(QApplication::activeWindow(), QObject::tr("Execution Error"), QObject::tr("No graph model available."));
        return;
    }

    // Find the first LLMConnector node in the graph
    const auto nodeIds = _graphModel->allNodeIds();

    for (auto nodeId : nodeIds) {
        auto* delegate = _graphModel->delegateModel<ToolNodeDelegate>(nodeId);
        if (!delegate) continue;

        // Identify by descriptor id via ToolNodeDelegate name()
        const QString modelId = delegate->name();
        if (modelId == QStringLiteral("llm-connector")) {
            auto connector = delegate->connector();
            if (!connector) break;

            // Execute asynchronously (inputs are not used by LLMConnector for now)
            QFuture<DataPacket> future = connector->Execute(DataPacket{});

            auto* watcher = new QFutureWatcher<DataPacket>(this);
            QObject::connect(watcher, &QFutureWatcher<DataPacket>::finished, this, [watcher]() {
                const DataPacket result = watcher->result();
                const QString key = QString::fromLatin1(LLMConnector::kOutputResponseId);
                const QString response = result.value(key).toString();
                QMessageBox::information(QApplication::activeWindow(), QObject::tr("LLM Response"), response.isEmpty() ? QObject::tr("(Empty response)") : response);
                watcher->deleteLater();
            });
            watcher->setFuture(future);
            return;
        }
    }

    // No LLMConnector found
    QMessageBox::information(QApplication::activeWindow(), QObject::tr("Run"), QObject::tr("Add an LLM Connector node, configure it, then click Run."));
}
