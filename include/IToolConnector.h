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

#include "CommonDataTypes.h"

/**
 * @file IToolConnector.h
 * @brief Finalized abstract interface for executable nodes (tools) in the pipeline.
 */
class IToolConnector {
public:
    virtual ~IToolConnector() = default;

    // Blueprint-aligned API
    // Returns the static descriptor for this node/tool.
    virtual NodeDescriptor GetDescriptor() const = 0;

    // Creates (or returns) a QWidget used to configure this tool instance.
    virtual QWidget* createConfigurationWidget(QWidget* parent) = 0;

    // Executes the tool asynchronously with given inputs; resolves to output packet.
    virtual QFuture<DataPacket> Execute(const DataPacket& inputs) = 0;
};
