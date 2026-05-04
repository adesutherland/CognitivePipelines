#pragma once

#include "IToolNode.h"

#include <QObject>

class GetItemNode : public QObject, public IToolNode {
    Q_OBJECT
    Q_INTERFACES(IToolNode)

public:
    explicit GetItemNode(QObject* parent = nullptr);
    ~GetItemNode() override = default;

    NodeDescriptor getDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    TokenList execute(const TokenList& incomingTokens) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;

    static constexpr const char* kOutputContextId = "context";
    static constexpr const char* kOutputCountId = "count";
    static constexpr const char* kOutputHistoryId = "history";
    static constexpr const char* kOutputIndexId = "index";
    static constexpr const char* kOutputItemId = "item";
    static constexpr const char* kOutputTextId = "text";
};
