#pragma once

#include "IToolNode.h"

#include <QObject>

class SetOutputNode : public QObject, public IToolNode {
    Q_OBJECT
    Q_INTERFACES(IToolNode)

public:
    explicit SetOutputNode(QObject* parent = nullptr);
    ~SetOutputNode() override = default;

    NodeDescriptor getDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    bool isReady(const QVariantMap& inputs, int incomingConnectionsCount) const override;
    TokenList execute(const TokenList& incomingTokens) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;

    static constexpr const char* kInputAcceptedId = "accepted";
    static constexpr const char* kInputContextId = "context";
    static constexpr const char* kInputErrorId = "error";
    static constexpr const char* kInputMessageId = "message";
    static constexpr const char* kInputNextInputId = "next_input";
    static constexpr const char* kInputOutputId = "output";
    static constexpr const char* kOutputBodyResultId = "_transform_output";
};
