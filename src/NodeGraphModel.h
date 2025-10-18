#pragma once

#include <QtNodes/DataFlowGraphModel>

#include <QObject>

class NodeGraphModel : public QtNodes::DataFlowGraphModel
{
    Q_OBJECT

public:
    explicit NodeGraphModel(QObject* parent = nullptr);
};
