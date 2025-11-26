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
#include "ToolNodeDelegate.h"

#include <QtConcurrent/QtConcurrent>
#include <QtNodes/Definitions>
#include <QtNodes/NodeDelegateModelRegistry>

#include <QJsonObject>
#include <QSet>

#include "PromptBuilderNode.h"
#include "NodeInfoWidget.h"

using namespace QtNodes;

ToolNodeDelegate::ToolNodeDelegate(std::shared_ptr<IToolConnector> connector)
    : _connector(std::move(connector))
{
    // If the connector is a PromptBuilderNode that can notify about dynamic input pin changes, listen for updates.
    if (auto* pb = dynamic_cast<PromptBuilderNode*>(_connector.get())) {
        QObject::connect(pb, &PromptBuilderNode::inputPinsUpdateRequested,
                         this, &ToolNodeDelegate::onConnectorInputPinsUpdateRequested);
    }

    // Initialize default dynamic inputs for known dynamic models (e.g., PromptBuilder)
    if (_connector) {
        const auto id = _connector->GetDescriptor().id;
        if (id == QStringLiteral("prompt-builder")) {
            onConnectorInputPinsUpdateRequested(QStringList{ QStringLiteral("input") });
        }
    }
}

QString ToolNodeDelegate::name() const
{
    ensureDescriptorCached();
    return _descriptor.id; // unique name used by registry
}

QString ToolNodeDelegate::caption() const
{
    ensureDescriptorCached();
    return _descriptor.name;
}

unsigned int ToolNodeDelegate::nPorts(PortType portType) const
{
    ensureDescriptorCached();
    if (portType == PortType::In) return static_cast<unsigned int>(_inputOrder.size());
    if (portType == PortType::Out) return static_cast<unsigned int>(_outputOrder.size());
    return 0U;
}

NodeDataType ToolNodeDelegate::dataType(PortType portType, PortIndex portIndex) const
{
    ensureDescriptorCached();
    NodeDataType t;
    if (portType == PortType::In) {
        if (portIndex < _inputOrder.size()) {
            const auto &pinId = _inputOrder[portIndex];
            const auto &pin = _descriptor.inputPins.value(pinId);
            t.id = pin.type;
            t.name = pin.name;
        }
    } else if (portType == PortType::Out) {
        if (portIndex < _outputOrder.size()) {
            const auto &pinId = _outputOrder[portIndex];
            const auto &pin = _descriptor.outputPins.value(pinId);
            t.id = pin.type;
            t.name = pin.name;
        }
    }
    return t;
}

void ToolNodeDelegate::setInData(std::shared_ptr<NodeData> nodeData, PortIndex const portIndex)
{
    ensureDescriptorCached();

    // Extract QVariant from our VariantNodeData, if provided
    QVariant v;
    if (nodeData) {
        // Accept any NodeData; if it's our VariantNodeData, extract value
        if (auto *vd = dynamic_cast<VariantNodeData*>(nodeData.get())) {
            v = vd->value();
        }
        // else: keep it invalid; future: could support type converters
    }

    const QString pinId = inputPinIdForIndex(portIndex);
    _inputs[pinId] = v;

    // Trigger execution when any input is set (simple strategy)
    triggerExecutionIfReady();
}

std::shared_ptr<NodeData> ToolNodeDelegate::outData(PortIndex const port)
{
    ensureDescriptorCached();

    const QString pinId = outputPinIdForIndex(port);
    const QVariant v = _outputs.value(pinId);

    NodeDataType t;
    if (port < _outputOrder.size()) {
        const auto &pin = _descriptor.outputPins.value(pinId);
        t.id = pin.type;
        t.name = pin.name;
    }
    return std::make_shared<VariantNodeData>(t, v);
}

void ToolNodeDelegate::onConnectorInputPinsUpdateRequested(const QStringList& newVariables)
{
    ensureDescriptorCached();

    // If the list is identical, avoid churn to preserve existing connections.
    QStringList current;
    current.reserve(static_cast<int>(_inputOrder.size()));
    for (const auto &id : _inputOrder) current.push_back(id);
    
    if (current == newVariables) {
        return;
    }

    // Compute old and new counts; for simplicity, replace entire input set
    const unsigned int oldCount = static_cast<unsigned int>(_inputOrder.size());
    if (oldCount > 0) {
        emit portsAboutToBeDeleted(PortType::In, 0, static_cast<PortIndex>(oldCount - 1));
        
        _descriptor.inputPins.clear();
        _inputOrder.clear();
        // prune runtime inputs
        QMap<QString, QVariant> newInputs;
        for (const QString& v : newVariables) {
            if (_inputs.contains(v)) newInputs.insert(v, _inputs.value(v));
        }
        _inputs.swap(newInputs);
        
        emit portsDeleted();
    }

    const unsigned int newCount = static_cast<unsigned int>(newVariables.size());
    if (newCount > 0) {
        emit portsAboutToBeInserted(PortType::In, 0, static_cast<PortIndex>(newCount - 1));
        
        for (const QString& var : newVariables) {
            PinDefinition in;
            in.direction = PinDirection::Input;
            in.id = var;
            in.name = var;
            in.type = QStringLiteral("text");
            _descriptor.inputPins.insert(in.id, in);
            _inputOrder.push_back(in.id);
        }
        
        emit portsInserted();
    }
    
    // Trigger node geometry recalculation after port changes
    emit embeddedWidgetSizeUpdated();
}

QWidget *ToolNodeDelegate::embeddedWidget()
{
    // Lazily create the info widget to display the node description inside the node
    if (!m_infoWidget) {
        m_infoWidget = new NodeInfoWidget();
        // Initialize with current description
        m_infoWidget->setDescription(m_nodeDescription);
    }
    return m_infoWidget;
}

QWidget* ToolNodeDelegate::configurationWidget()
{
    // Lazily create the configuration widget for use in the properties panel only.
    if (!_widget) {
        _widget = _connector ? _connector->createConfigurationWidget(nullptr) : nullptr;
    }
    return _widget;
}

void ToolNodeDelegate::setDescription(const QString& desc)
{
    m_nodeDescription = desc;
    
    // Sync with the embedded info widget if it exists
    if (m_infoWidget) {
        m_infoWidget->setDescription(desc);
    }
    
    // Notify the view that the embedded widget size may have changed
    // This triggers the node to recalculate its geometry
    emit embeddedWidgetSizeUpdated();
}

void ToolNodeDelegate::ensureDescriptorCached() const
{
    if (_descriptorCached || !_connector) return;
    _descriptor = _connector->GetDescriptor();

    _inputOrder.clear();
    _outputOrder.clear();

    // QMap iteration is key-sorted, provides stable order
    for (auto it = _descriptor.inputPins.constBegin(); it != _descriptor.inputPins.constEnd(); ++it) {
        _inputOrder.push_back(it.key());
    }
    for (auto it = _descriptor.outputPins.constBegin(); it != _descriptor.outputPins.constEnd(); ++it) {
        _outputOrder.push_back(it.key());
    }

    _descriptorCached = true;
}

QString ToolNodeDelegate::inputPinIdForIndex(PortIndex idx) const
{
    ensureDescriptorCached();
    if (idx < _inputOrder.size()) return _inputOrder[idx];
    return QString();
}

QString ToolNodeDelegate::outputPinIdForIndex(PortIndex idx) const
{
    ensureDescriptorCached();
    if (idx < _outputOrder.size()) return _outputOrder[idx];
    return QString();
}

QString ToolNodeDelegate::pinIdForIndex(PortType portType, PortIndex idx) const
{
    ensureDescriptorCached();
    if (portType == PortType::In) return inputPinIdForIndex(idx);
    if (portType == PortType::Out) return outputPinIdForIndex(idx);
    return QString();
}

void ToolNodeDelegate::triggerExecutionIfReady()
{
    if (!_connector) return;

    // For now, we execute regardless of whether all inputs are set.
    // In future, enforce required inputs.
    auto future = _connector->Execute(_inputs);

    // When computation starts/finishes
    emit computingStarted();

    // Use watcher to know when finished
    auto watcher = new QFutureWatcher<DataPacket>(this);
    connect(watcher, &QFutureWatcher<DataPacket>::finished, this, [this, watcher]() {
        try {
            const DataPacket result = watcher->result();
            // Update outputs
            for (const auto &key : result.keys()) {
                _outputs[key] = result.value(key);
            }
            // Notify downstream for each output port
            for (PortIndex i = 0; i < _outputOrder.size(); ++i) {
                emit dataUpdated(i);
            }
        } catch (const std::exception &e) {
            qWarning() << "ToolNodeDelegate execution threw exception:" << e.what();
        } catch (...) {
            qWarning() << "ToolNodeDelegate execution threw unknown exception";
        }
        emit computingFinished();
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}


QJsonObject ToolNodeDelegate::save() const
{
    QJsonObject obj;
    // Persist the registry lookup key so loader can instantiate the correct model
    obj.insert(QStringLiteral("model-name"), name());

    // Persist node description
    if (!m_nodeDescription.isEmpty()) {
        obj.insert(QStringLiteral("node-description"), m_nodeDescription);
    }

    // Merge connector-specific state into the internal-data object
    if (_connector) {
        const QJsonObject state = _connector->saveState();
        for (auto it = state.begin(); it != state.end(); ++it) {
            obj.insert(it.key(), it.value());
        }
    }
    return obj;
}

void ToolNodeDelegate::load(QJsonObject const& data)
{
    // Restore node description
    if (data.contains(QStringLiteral("node-description"))) {
        setDescription(data.value(QStringLiteral("node-description")).toString());
    }

    if (_connector) {
        _connector->loadState(data);
    }
}
