#pragma once

#include "IToolNode.h"

#include <QObject>
#include <QString>

class TextChunkerNode : public QObject, public IToolNode {
    Q_OBJECT
    Q_INTERFACES(IToolNode)

public:
    explicit TextChunkerNode(QObject* parent = nullptr);
    ~TextChunkerNode() override = default;

    NodeDescriptor getDescriptor() const override;
    QWidget* createConfigurationWidget(QWidget* parent) override;
    bool isReady(const QVariantMap& inputs, int incomingConnectionsCount) const override;
    TokenList execute(const TokenList& incomingTokens) override;
    QJsonObject saveState() const override;
    void loadState(const QJsonObject& data) override;

    int chunkSize() const { return m_chunkSize; }
    int chunkOverlap() const { return m_chunkOverlap; }
    QString fileType() const { return m_fileType; }

    static constexpr const char* kInputTextId = "text";
    static constexpr const char* kOutputChunksId = "chunks";
    static constexpr const char* kOutputCountId = "count";
    static constexpr const char* kOutputSummaryId = "summary";
    static constexpr const char* kOutputTextId = "text";

public slots:
    void setChunkSize(int value);
    void setChunkOverlap(int value);
    void setFileType(const QString& fileType);

signals:
    void chunkSizeChanged(int value);
    void chunkOverlapChanged(int value);
    void fileTypeChanged(const QString& fileType);

private:
    int m_chunkSize {1000};
    int m_chunkOverlap {100};
    QString m_fileType {QStringLiteral("plain")};
};
