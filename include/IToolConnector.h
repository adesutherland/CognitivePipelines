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

#include <QWidget>
#include <QFuture>
#include <QObject>
#include <QJsonObject>
#include <QVariant>
#include <list>

#include "CommonDataTypes.h"
#include "ExecutionToken.h"

/**
 * @file IToolConnector.h
 * @brief Finalized abstract interface for executable nodes (tools) in the pipeline.
 */

using TokenList = std::list<ExecutionToken>;
using PinId = QString;

class IToolConnector {
public:
    virtual ~IToolConnector() = default;

    // Blueprint-aligned API
    // Returns the static descriptor for this node/tool.
    virtual NodeDescriptor getDescriptor() const = 0;

    // Creates (or returns) a QWidget used to configure this tool instance.
    virtual QWidget* createConfigurationWidget(QWidget* parent) = 0;

    // Executes the tool with the given incoming execution tokens and returns
    // the list of output tokens produced by this node.
    virtual TokenList execute(const TokenList& incomingTokens) = 0;

    // Serialization of node-specific state (properties).
    virtual QJsonObject saveState() const = 0;
    virtual void loadState(const QJsonObject& data) = 0;

    // Scheduling predicate: by default, requires all inbound pins to be present (strict AND).
    // Node implementations may override to relax readiness (e.g., OR semantics for partial inputs).
    // Default keeps backward compatibility with existing nodes.
    virtual bool isReady(const QVariantMap& inputs, int incomingConnectionsCount) const {
        Q_UNUSED(incomingConnectionsCount);
        // Default AND logic: ready when the number of provided inputs equals the number of inbound connections
        return static_cast<int>(inputs.size()) == incomingConnectionsCount;
    }
};

// Declare the Qt interface IID for IToolConnector so Q_INTERFACES can resolve it
Q_DECLARE_INTERFACE(IToolConnector, "org.cognitivepipelines.IToolConnector/1.0")
