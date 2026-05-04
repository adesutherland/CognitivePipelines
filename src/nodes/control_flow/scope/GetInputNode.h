#pragma once

#include "IToolNode.h"

#include <QObject>

class GetInputNode : public QObject, public IToolNode {
    Q_OBJECT
    Q_INTERFACES(IToolNode)

public:
    explicit GetInputNode(QObject* parent = nullptr);
    ~GetInputNode() override = default;

    NodeDescriptor getDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    TokenList execute(const TokenList& incomingTokens) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;

    static constexpr const char* kOutputInputId = "input";
    static constexpr const char* kOutputTextId = "text";
    static constexpr const char* kOutputContextId = "context";
    static constexpr const char* kOutputAttemptId = "attempt";
    static constexpr const char* kOutputPreviousOutputId = "previous_output";
    static constexpr const char* kOutputHistoryId = "history";
};
