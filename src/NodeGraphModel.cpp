#include "NodeGraphModel.h"
#include <QtNodes/NodeDelegateModelRegistry>

#include "LLMConnector.h"
#include "ToolNodeDelegate.h"

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
}
