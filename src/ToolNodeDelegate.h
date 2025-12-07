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

#include <memory>
#include <vector>
#include <unordered_map>

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeData>

#include <QObject>
#include <QPointer>
#include <QVariant>
#include <QJsonObject>

#include "ExecutionToken.h"
#include "IToolConnector.h"
#include "CommonDataTypes.h"

class NodeInfoWidget;

// Generic adapter that bridges IToolConnector to QtNodes::NodeDelegateModel
class ToolNodeDelegate : public QtNodes::NodeDelegateModel {
    Q_OBJECT
public:
    explicit ToolNodeDelegate(std::shared_ptr<IToolConnector> connector);
    ~ToolNodeDelegate() override = default;

    // NodeDelegateModel overrides
    QString name() const override;
    QString caption() const override;
    bool captionVisible() const override { return true; }

    unsigned int nPorts(QtNodes::PortType portType) const override;
    QtNodes::NodeDataType dataType(QtNodes::PortType portType, QtNodes::PortIndex portIndex) const override;

    void setInData(std::shared_ptr<QtNodes::NodeData> nodeData, QtNodes::PortIndex const portIndex) override;
    std::shared_ptr<QtNodes::NodeData> outData(QtNodes::PortIndex const port) override;

    QWidget* embeddedWidget() override;

    // Node persistence
    QJsonObject save() const override;
    void load(QJsonObject const& data) override;

    // Returns the configuration widget for the properties panel (not embedded in node)
    QWidget* configurationWidget();

    // Expose underlying connector for engine/execution control
    std::shared_ptr<IToolConnector> connector() const { return _connector; }

    // Node description accessor methods
    QString description() const { return m_nodeDescription; }
    void setDescription(const QString& desc);

private:
    // Minimal generic NodeData that carries QVariant and a declared type id/name
    class VariantNodeData : public QtNodes::NodeData {
    public:
        VariantNodeData(const QtNodes::NodeDataType& t, const QVariant& v) : _t(t), _v(v) {}
        QtNodes::NodeDataType type() const override { return _t; }
        const QVariant& value() const { return _v; }
    private:
        QtNodes::NodeDataType _t;
        QVariant _v;
    };

    void ensureDescriptorCached() const;

    // Helpers to translate between port index and our pin ids
    QString inputPinIdForIndex(QtNodes::PortIndex idx) const;
    QString outputPinIdForIndex(QtNodes::PortIndex idx) const;

public:
    // Public helper for external components (ExecutionEngine) to map indices to pin ids
    QString pinIdForIndex(QtNodes::PortType portType, QtNodes::PortIndex idx) const;

private Q_SLOTS:
    void onConnectorInputPinsUpdateRequested(const QStringList& newVariables);

private:
    std::shared_ptr<IToolConnector> _connector;

    // Cached descriptor and pin orderings for stable indices
    mutable bool _descriptorCached {false};
    mutable NodeDescriptor _descriptor;
    mutable std::vector<QString> _inputOrder;
    mutable std::vector<QString> _outputOrder;

    // Runtime IO values
    QMap<QString, QVariant> _inputs;   // keyed by pin id
    QMap<QString, QVariant> _outputs;  // keyed by pin id

    // Lazily created config widget
    QPointer<QWidget> _widget;
    
    // Embedded info widget for displaying description inside the node
    QPointer<NodeInfoWidget> m_infoWidget;

    // Node description (generic metadata)
    QString m_nodeDescription;
};
