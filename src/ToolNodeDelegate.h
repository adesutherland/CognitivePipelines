#pragma once

#include <memory>
#include <vector>
#include <unordered_map>

#include <QtNodes/NodeDelegateModel>
#include <QtNodes/NodeData>

#include <QObject>
#include <QPointer>
#include <QVariant>

#include "IToolConnector.h"
#include "CommonDataTypes.h"

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
    void triggerExecutionIfReady();

    // Helpers to translate between port index and our pin ids
    QString inputPinIdForIndex(QtNodes::PortIndex idx) const;
    QString outputPinIdForIndex(QtNodes::PortIndex idx) const;

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
};
