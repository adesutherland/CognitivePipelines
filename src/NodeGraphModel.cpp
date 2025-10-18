#include "NodeGraphModel.h"
#include <QtNodes/NodeDelegateModelRegistry>

NodeGraphModel::NodeGraphModel(QObject* parent)
    : QtNodes::DataFlowGraphModel(std::make_shared<QtNodes::NodeDelegateModelRegistry>())
{
    Q_UNUSED(parent);
    // Constructor body is empty for now.
    // We will register our node types (e.G., LLMConnector) with the registry later.
}
