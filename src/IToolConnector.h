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

#include <QString>
#include <QList>
#include <QVariant>
#include <QWidget>
#include <QFuture>
#include <QMetaType>

/**
 * @file IToolConnector.h
 * @brief Defines the IToolConnector interface for executable nodes in a cognitive pipeline.
 *
 * The IToolConnector interface is the contract that every executable node ("tool")
 * in the CognitivePipelines application must implement. It exposes:
 *  - Tool metadata for the UI and graph editor (name, description, pin schemas).
 *  - A configuration widget suitable for embedding in a properties panel.
 *  - An asynchronous execution API that consumes input data and produces output data.
 *
 * Implementations are expected to be lightweight, reusable components that
 * encapsulate a single unit of processing and can be orchestrated by the host
 * pipeline runtime.
 */

/**
 * @brief Describes the schema of a single input or output pin.
 */
struct PinDefinition {
    /**
     * @brief Stable, unique identifier for the pin (used by the runtime/serialization).
     */
    QString id;

    /**
     * @brief Human-readable pin name for the UI.
     */
    QString name;

    /**
     * @brief Short description shown as tooltip or in documentation.
     */
    QString description;

    /**
     * @brief The expected Qt metatype of the data carried by this pin.
     *
     * Use values from QMetaType::Type for built-in types, or register
     * custom types with Q_DECLARE_METATYPE and qRegisterMetaType.
     */
    QMetaType::Type type = QMetaType::UnknownType;

    /**
     * @brief Whether this pin is optional (true) or required (false).
     */
    bool optional = false;
};

/**
 * @brief Aggregates tool-level metadata consumed by the UI and the runtime.
 */
struct ToolMetadata {
    /**
     * @brief Display name of the tool (e.g., "Image Blur").
     */
    QString name;

    /**
     * @brief Concise description of what the tool does.
     */
    QString description;

    /**
     * @brief Definitions of all input pins accepted by this tool.
     */
    QList<PinDefinition> inputs;

    /**
     * @brief Definitions of all output pins produced by this tool.
     */
    QList<PinDefinition> outputs;
};

/**
 * @brief Abstract base class (interface) for executable tools in the pipeline.
 *
 * Key design notes:
 *  - Ownership of the configuration widget remains with the caller when a
 *    parent widget is supplied; implementers should pass the provided parent
 *    to ensure correct lifetime management.
 *  - Asynchronous execution returns a QFuture that resolves to a QVariantMap
 *    mapping output pin IDs to values. Implementers may use QPromise/QFuture,
 *    QtConcurrent, or custom threading to fulfill the future.
 *  - All heavy work must be performed off the GUI thread. Only the
 *    configuration widget interacts with the UI thread.
 */
class IToolConnector {
public:
    using InputMap = QVariantMap;   ///< Map of input pin IDs to values.
    using OutputMap = QVariantMap;  ///< Map of output pin IDs to values.

    virtual ~IToolConnector() = default;

    /**
     * @brief Returns static metadata describing this tool.
     *
     * The host uses this to populate catalogs, tooltips, and to validate graph
     * connections. The pin IDs provided here must match the keys expected in
     * executeAsync() inputs and produced in its outputs.
     */
    virtual ToolMetadata metadata() const = 0;

    /**
     * @brief Creates (or returns) a QWidget for configuring this tool.
     *
     * The returned widget is intended to be embedded inside the host's
     * properties panel. Implementations may lazily create and cache the widget.
     *
     * Ownership and lifetime:
     *  - The caller provides a parent and takes ownership via Qt's parent-child
     *    mechanism. Implementers should set the given parent on creation.
     *  - The widget must be thread-affine to the GUI thread.
     *
     * @param parent Parent widget to use for proper ownership.
     * @return QWidget* A pointer to the configuration widget (never nullptr on success).
     */
    virtual QWidget* createConfigurationWidget(QWidget* parent) = 0;

    /**
     * @brief Executes the tool's logic asynchronously.
     *
     * The host provides a map of input values keyed by input pin IDs declared
     * in metadata(). The implementation performs its work off the GUI thread
     * and completes the returned future with a map of output values keyed by
     * output pin IDs declared in metadata().
     *
     * Error handling:
     *  - On failure, the future should be completed with an empty map and the
     *    error can be reported via logging or by placing an error description
     *    under a designated output pin (e.g., "error"). Alternatively,
     *    implementations may throw inside the worker; the host should treat
     *    that as a failed future.
     *
     * Cancellation (optional):
     *  - Implementations may support cancellation via custom means; this
     *    minimal interface does not impose a standardized cancellation token.
     *
     * Threading guidance:
     *  - Use QtConcurrent::run, QThreadPool with QRunnable, or QPromise/QFuture
     *    to produce the result without blocking the GUI thread.
     *
     * @param inputs Map of input pin IDs to their values as QVariants.
     * @return QFuture<OutputMap> Future that resolves to the output values.
     */
    virtual QFuture<OutputMap> executeAsync(const InputMap& inputs) = 0;
};
