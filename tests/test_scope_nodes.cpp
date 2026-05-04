#include <gtest/gtest.h>

#include <QApplication>
#include <QJsonObject>
#include <QSignalSpy>

#include <memory>

#include "ExecutionIdUtils.h"
#include "ExecutionState.h"
#include "GetInputNode.h"
#include "GetItemNode.h"
#include "IteratorScopeNode.h"
#include "NodeGraphModel.h"
#include "PromptBuilderNode.h"
#include "SetItemResultNode.h"
#include "SetOutputNode.h"
#include "TextChunkerNode.h"
#include "ToolNodeDelegate.h"
#include "TransformScopeNode.h"
#include "test_app.h"

using namespace QtNodes;

namespace {
QApplication* ensureScopeApp()
{
    return sharedTestApp();
}

PortIndex portIndexFor(NodeGraphModel& model, NodeId nodeId, PortType portType, const QString& pinId)
{
    auto* delegate = model.delegateModel<ToolNodeDelegate>(nodeId);
    if (!delegate) {
        return InvalidPortIndex;
    }
    const unsigned int count = delegate->nPorts(portType);
    for (PortIndex i = 0; i < count; ++i) {
        if (delegate->pinIdForIndex(portType, i) == pinId) {
            return i;
        }
    }
    return InvalidPortIndex;
}
}

TEST(ScopeNodesTest, GetInputEmitsTransformFrame)
{
    ensureScopeApp();

    GetInputNode node;
    ExecutionToken in;
    in.data.insert(QStringLiteral("_transform_input"), QStringLiteral("seed"));
    in.data.insert(QStringLiteral("_transform_attempt"), 2);
    in.data.insert(QStringLiteral("_transform_previous_output"), QStringLiteral("draft"));
    in.data.insert(QStringLiteral("_scope_context"), QVariantMap{{QStringLiteral("topic"), QStringLiteral("demo")}});

    const TokenList out = node.execute(TokenList{in});
    ASSERT_EQ(out.size(), 1u);
    const auto packet = out.front().data;
    EXPECT_EQ(packet.value(QString::fromLatin1(GetInputNode::kOutputInputId)).toString(), QStringLiteral("seed"));
    EXPECT_EQ(packet.value(QString::fromLatin1(GetInputNode::kOutputTextId)).toString(), QStringLiteral("seed"));
    EXPECT_EQ(packet.value(QString::fromLatin1(GetInputNode::kOutputAttemptId)).toInt(), 2);
    EXPECT_EQ(packet.value(QString::fromLatin1(GetInputNode::kOutputPreviousOutputId)).toString(), QStringLiteral("draft"));
    EXPECT_EQ(packet.value(QString::fromLatin1(GetInputNode::kOutputContextId)).toMap().value(QStringLiteral("topic")).toString(),
              QStringLiteral("demo"));
}

TEST(ScopeNodesTest, SetOutputDefaultsAccepted)
{
    ensureScopeApp();

    SetOutputNode node;
    ExecutionToken in;
    in.data.insert(QString::fromLatin1(SetOutputNode::kInputOutputId), QStringLiteral("final"));

    const TokenList out = node.execute(TokenList{in});
    ASSERT_EQ(out.size(), 1u);
    const QVariantMap result = out.front().data.value(QString::fromLatin1(SetOutputNode::kOutputBodyResultId)).toMap();
    EXPECT_EQ(result.value(QStringLiteral("output")).toString(), QStringLiteral("final"));
    EXPECT_TRUE(result.value(QStringLiteral("accepted")).toBool());
}

TEST(ScopeNodesTest, GetItemEmitsIteratorFrame)
{
    ensureScopeApp();

    GetItemNode node;
    ExecutionToken in;
    in.data.insert(QStringLiteral("_iterator_item"), QStringLiteral("chunk"));
    in.data.insert(QStringLiteral("_iterator_index"), 3);
    in.data.insert(QStringLiteral("_iterator_count"), 10);

    const TokenList out = node.execute(TokenList{in});
    ASSERT_EQ(out.size(), 1u);
    const auto packet = out.front().data;
    EXPECT_EQ(packet.value(QString::fromLatin1(GetItemNode::kOutputItemId)).toString(), QStringLiteral("chunk"));
    EXPECT_EQ(packet.value(QString::fromLatin1(GetItemNode::kOutputTextId)).toString(), QStringLiteral("chunk"));
    EXPECT_EQ(packet.value(QString::fromLatin1(GetItemNode::kOutputIndexId)).toInt(), 3);
    EXPECT_EQ(packet.value(QString::fromLatin1(GetItemNode::kOutputCountId)).toInt(), 10);
}

TEST(ScopeNodesTest, SetItemResultDefaults)
{
    ensureScopeApp();

    SetItemResultNode node;
    ExecutionToken in;
    in.data.insert(QString::fromLatin1(SetItemResultNode::kInputResultId), QStringLiteral("mapped"));

    const TokenList out = node.execute(TokenList{in});
    ASSERT_EQ(out.size(), 1u);
    const QVariantMap result = out.front().data.value(QString::fromLatin1(SetItemResultNode::kOutputBodyResultId)).toMap();
    EXPECT_EQ(result.value(QStringLiteral("output")).toString(), QStringLiteral("mapped"));
    EXPECT_FALSE(result.value(QStringLiteral("skip")).toBool());
}

TEST(ScopeNodesTest, TransformScopeRetriesUntilAccepted)
{
    ensureScopeApp();

    TransformScopeNode scope;
    scope.setMode(QStringLiteral("retry_until_accepted"));
    scope.setMaxAttempts(5);

    int calls = 0;
    scope.setBodyRunner([&](const QString&, ScopeBodyKind kind, const ScopeFrame& frame, const DataPacket&) {
        EXPECT_EQ(kind, ScopeBodyKind::Transform);
        ++calls;
        ScopeBodyResult result;
        result.ok = true;
        result.accepted = calls >= 3;
        result.output = QStringLiteral("candidate-%1").arg(calls);
        result.nextInput = QStringLiteral("next-%1").arg(frame.attempt);
        return result;
    });

    ExecutionToken in;
    in.data.insert(QString::fromLatin1(TransformScopeNode::kInputInputId), QStringLiteral("seed"));

    const TokenList out = scope.execute(TokenList{in});
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(calls, 3);
    EXPECT_EQ(out.front().data.value(QString::fromLatin1(TransformScopeNode::kOutputOutputId)).toString(),
              QStringLiteral("candidate-3"));
    EXPECT_EQ(out.front().data.value(QString::fromLatin1(TransformScopeNode::kOutputStatusId)).toString(),
              QStringLiteral("accepted"));
}

TEST(ScopeNodesTest, IteratorScopeMapsList)
{
    ensureScopeApp();

    IteratorScopeNode scope;
    int calls = 0;
    scope.setBodyRunner([&](const QString&, ScopeBodyKind kind, const ScopeFrame& frame, const DataPacket&) {
        EXPECT_EQ(kind, ScopeBodyKind::Iterator);
        ++calls;
        ScopeBodyResult result;
        result.ok = true;
        result.output = QStringLiteral("out-%1-%2").arg(frame.index).arg(frame.item.toString());
        return result;
    });

    ExecutionToken in;
    in.data.insert(QString::fromLatin1(IteratorScopeNode::kInputItemsId),
                   QVariantList{QStringLiteral("a"), QStringLiteral("b")});

    const TokenList out = scope.execute(TokenList{in});
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(calls, 2);
    const QVariantList results = out.front().data.value(QString::fromLatin1(IteratorScopeNode::kOutputResultsId)).toList();
    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results.at(0).toString(), QStringLiteral("out-0-a"));
    EXPECT_EQ(results.at(1).toString(), QStringLiteral("out-1-b"));
}

TEST(ScopeNodesTest, TextChunkerNodeProducesListForIteratorInput)
{
    ensureScopeApp();

    TextChunkerNode node;
    node.setChunkSize(12);
    node.setChunkOverlap(0);

    ExecutionToken in;
    in.data.insert(QString::fromLatin1(TextChunkerNode::kInputTextId),
                   QStringLiteral("alpha beta gamma delta"));

    const TokenList out = node.execute(TokenList{in});
    ASSERT_EQ(out.size(), 1u);
    const QVariantList chunks = out.front().data.value(QString::fromLatin1(TextChunkerNode::kOutputChunksId)).toList();
    ASSERT_GT(chunks.size(), 1);
    EXPECT_EQ(out.front().data.value(QString::fromLatin1(TextChunkerNode::kOutputCountId)).toInt(), chunks.size());
}

TEST(ScopeNodesTest, TransformBodySubgraphExecutesPromptBuilder)
{
    ensureScopeApp();

    NodeGraphModel root;
    const NodeId scopeId = root.addNode(QStringLiteral("transform-scope"));
    ASSERT_NE(scopeId, InvalidNodeId);

    auto* scopeDelegate = root.delegateModel<ToolNodeDelegate>(scopeId);
    ASSERT_NE(scopeDelegate, nullptr);
    auto scope = std::dynamic_pointer_cast<TransformScopeNode>(scopeDelegate->node());
    ASSERT_TRUE(scope);

    NodeGraphModel* body = root.ensureSubgraph(scope->bodyId(), NodeGraphModel::GraphKind::TransformBody);
    ASSERT_NE(body, nullptr);

    const NodeId inputId = body->addNode(QStringLiteral("scope-get-input"));
    const NodeId promptId = body->addNode(QStringLiteral("prompt-builder"));
    const NodeId outputId = body->addNode(QStringLiteral("scope-set-output"));
    ASSERT_NE(inputId, InvalidNodeId);
    ASSERT_NE(promptId, InvalidNodeId);
    ASSERT_NE(outputId, InvalidNodeId);

    auto* promptDelegate = body->delegateModel<ToolNodeDelegate>(promptId);
    ASSERT_NE(promptDelegate, nullptr);
    auto prompt = std::dynamic_pointer_cast<PromptBuilderNode>(promptDelegate->node());
    ASSERT_TRUE(prompt);
    prompt->setTemplateText(QStringLiteral("done {input}"));

    const PortIndex inputOut = portIndexFor(*body, inputId, PortType::Out, QString::fromLatin1(GetInputNode::kOutputInputId));
    const PortIndex promptIn = portIndexFor(*body, promptId, PortType::In, QStringLiteral("input"));
    const PortIndex promptOut = portIndexFor(*body, promptId, PortType::Out, QString::fromLatin1(PromptBuilderNode::kOutputId));
    const PortIndex setOutputIn = portIndexFor(*body, outputId, PortType::In, QString::fromLatin1(SetOutputNode::kInputOutputId));
    ASSERT_NE(inputOut, InvalidPortIndex);
    ASSERT_NE(promptIn, InvalidPortIndex);
    ASSERT_NE(promptOut, InvalidPortIndex);
    ASSERT_NE(setOutputIn, InvalidPortIndex);

    body->addConnection(ConnectionId{inputId, inputOut, promptId, promptIn});
    body->addConnection(ConnectionId{promptId, promptOut, outputId, setOutputIn});

    ExecutionToken in;
    in.data.insert(QString::fromLatin1(TransformScopeNode::kInputInputId), QStringLiteral("seed"));
    const TokenList out = scope->execute(TokenList{in});
    ASSERT_EQ(out.size(), 1u);
    EXPECT_FALSE(out.front().data.contains(QStringLiteral("__error")))
        << out.front().data.value(QStringLiteral("__error")).toString().toStdString();
    EXPECT_EQ(out.front().data.value(QString::fromLatin1(TransformScopeNode::kOutputOutputId)).toString(),
              QStringLiteral("done seed"));
}

TEST(ScopeNodesTest, ScopeBodyExecutionEmitsNodeStatusForHighlighting)
{
    ensureScopeApp();

    NodeGraphModel root;
    const NodeId scopeId = root.addNode(QStringLiteral("transform-scope"));
    ASSERT_NE(scopeId, InvalidNodeId);

    auto* scopeDelegate = root.delegateModel<ToolNodeDelegate>(scopeId);
    ASSERT_NE(scopeDelegate, nullptr);
    auto scope = std::dynamic_pointer_cast<TransformScopeNode>(scopeDelegate->node());
    ASSERT_TRUE(scope);

    NodeGraphModel* body = root.ensureSubgraph(scope->bodyId(), NodeGraphModel::GraphKind::TransformBody);
    ASSERT_NE(body, nullptr);

    const NodeId inputId = body->addNode(QStringLiteral("scope-get-input"));
    const NodeId outputId = body->addNode(QStringLiteral("scope-set-output"));
    ASSERT_NE(inputId, InvalidNodeId);
    ASSERT_NE(outputId, InvalidNodeId);

    const PortIndex inputOut = portIndexFor(*body, inputId, PortType::Out, QString::fromLatin1(GetInputNode::kOutputInputId));
    const PortIndex setOutputIn = portIndexFor(*body, outputId, PortType::In, QString::fromLatin1(SetOutputNode::kInputOutputId));
    ASSERT_NE(inputOut, InvalidPortIndex);
    ASSERT_NE(setOutputIn, InvalidPortIndex);
    body->addConnection(ConnectionId{inputId, inputOut, outputId, setOutputIn});

    QSignalSpy spy(body, &NodeGraphModel::executionNodeStatusChanged);
    QSignalSpy outputSpy(body, &NodeGraphModel::executionNodeOutputChanged);

    ExecutionToken in;
    in.data.insert(QString::fromLatin1(TransformScopeNode::kInputInputId), QStringLiteral("seed"));
    const TokenList out = scope->execute(TokenList{in});
    ASSERT_EQ(out.size(), 1u);

    const QUuid inputUuid = ExecIds::nodeUuid(body->executionScopeKey(), inputId);
    const QUuid outputUuid = ExecIds::nodeUuid(body->executionScopeKey(), outputId);
    bool sawInputFinished = false;
    bool sawOutputFinished = false;

    for (const QList<QVariant>& args : spy) {
        const QUuid uuid = args.at(0).toUuid();
        const int state = args.at(1).toInt();
        if (uuid == inputUuid && state == static_cast<int>(ExecutionState::Finished)) {
            sawInputFinished = true;
        }
        if (uuid == outputUuid && state == static_cast<int>(ExecutionState::Finished)) {
            sawOutputFinished = true;
        }
    }

    EXPECT_TRUE(sawInputFinished);
    EXPECT_TRUE(sawOutputFinished);
    EXPECT_GE(outputSpy.count(), 2);
    EXPECT_EQ(body->nodeOutput(inputId).value(QString::fromLatin1(GetInputNode::kOutputInputId)).toString(),
              QStringLiteral("seed"));
    EXPECT_TRUE(body->nodeOutput(outputId).contains(QString::fromLatin1(SetOutputNode::kOutputBodyResultId)));
}

TEST(ScopeNodesTest, IteratorBodySubgraphExecutesPromptBuilder)
{
    ensureScopeApp();

    NodeGraphModel root;
    const NodeId scopeId = root.addNode(QStringLiteral("iterator-scope"));
    ASSERT_NE(scopeId, InvalidNodeId);

    auto* scopeDelegate = root.delegateModel<ToolNodeDelegate>(scopeId);
    ASSERT_NE(scopeDelegate, nullptr);
    auto scope = std::dynamic_pointer_cast<IteratorScopeNode>(scopeDelegate->node());
    ASSERT_TRUE(scope);

    NodeGraphModel* body = root.ensureSubgraph(scope->bodyId(), NodeGraphModel::GraphKind::IteratorBody);
    ASSERT_NE(body, nullptr);

    const NodeId itemId = body->addNode(QStringLiteral("iterator-get-item"));
    const NodeId promptId = body->addNode(QStringLiteral("prompt-builder"));
    const NodeId resultId = body->addNode(QStringLiteral("iterator-set-result"));
    ASSERT_NE(itemId, InvalidNodeId);
    ASSERT_NE(promptId, InvalidNodeId);
    ASSERT_NE(resultId, InvalidNodeId);

    auto* promptDelegate = body->delegateModel<ToolNodeDelegate>(promptId);
    ASSERT_NE(promptDelegate, nullptr);
    auto prompt = std::dynamic_pointer_cast<PromptBuilderNode>(promptDelegate->node());
    ASSERT_TRUE(prompt);
    prompt->setTemplateText(QStringLiteral("done {item}"));

    const PortIndex itemOut = portIndexFor(*body, itemId, PortType::Out, QString::fromLatin1(GetItemNode::kOutputItemId));
    const PortIndex promptIn = portIndexFor(*body, promptId, PortType::In, QStringLiteral("item"));
    const PortIndex promptOut = portIndexFor(*body, promptId, PortType::Out, QString::fromLatin1(PromptBuilderNode::kOutputId));
    const PortIndex resultIn = portIndexFor(*body, resultId, PortType::In, QString::fromLatin1(SetItemResultNode::kInputResultId));
    ASSERT_NE(itemOut, InvalidPortIndex);
    ASSERT_NE(promptIn, InvalidPortIndex);
    ASSERT_NE(promptOut, InvalidPortIndex);
    ASSERT_NE(resultIn, InvalidPortIndex);

    body->addConnection(ConnectionId{itemId, itemOut, promptId, promptIn});
    body->addConnection(ConnectionId{promptId, promptOut, resultId, resultIn});

    ExecutionToken in;
    in.data.insert(QString::fromLatin1(IteratorScopeNode::kInputItemsId),
                   QVariantList{QStringLiteral("a"), QStringLiteral("b")});
    const TokenList out = scope->execute(TokenList{in});
    ASSERT_EQ(out.size(), 1u);
    EXPECT_FALSE(out.front().data.contains(QStringLiteral("__error")))
        << out.front().data.value(QStringLiteral("__error")).toString().toStdString();
    const QVariantList results = out.front().data.value(QString::fromLatin1(IteratorScopeNode::kOutputResultsId)).toList();
    ASSERT_EQ(results.size(), 2);
    EXPECT_EQ(results.at(0).toString(), QStringLiteral("done a"));
    EXPECT_EQ(results.at(1).toString(), QStringLiteral("done b"));
}

TEST(ScopeNodesTest, BodyOnlyNodesArePaletteRegisteredInMatchingBodies)
{
    ensureScopeApp();

    NodeGraphModel root;
    EXPECT_EQ(root.addNode(QStringLiteral("scope-get-input")), InvalidNodeId);
    EXPECT_EQ(root.addNode(QStringLiteral("scope-set-output")), InvalidNodeId);
    EXPECT_EQ(root.addNode(QStringLiteral("iterator-get-item")), InvalidNodeId);
    EXPECT_EQ(root.addNode(QStringLiteral("iterator-set-result")), InvalidNodeId);
    EXPECT_NE(root.addNode(QStringLiteral("text-chunker")), InvalidNodeId);
    EXPECT_NE(root.addNode(QStringLiteral("transform-scope")), InvalidNodeId);
    EXPECT_NE(root.addNode(QStringLiteral("iterator-scope")), InvalidNodeId);

    NodeGraphModel* transformBody = root.ensureSubgraph(QStringLiteral("body-transform"), NodeGraphModel::GraphKind::TransformBody);
    ASSERT_NE(transformBody, nullptr);
    EXPECT_EQ(transformBody->graphKind(), NodeGraphModel::GraphKind::TransformBody);
    EXPECT_NE(transformBody->addNode(QStringLiteral("scope-get-input")), InvalidNodeId);
    EXPECT_NE(transformBody->addNode(QStringLiteral("scope-set-output")), InvalidNodeId);
    EXPECT_EQ(transformBody->addNode(QStringLiteral("iterator-get-item")), InvalidNodeId);

    NodeGraphModel* iteratorBody = root.ensureSubgraph(QStringLiteral("body-iterator"), NodeGraphModel::GraphKind::IteratorBody);
    ASSERT_NE(iteratorBody, nullptr);
    EXPECT_EQ(iteratorBody->graphKind(), NodeGraphModel::GraphKind::IteratorBody);
    EXPECT_NE(iteratorBody->addNode(QStringLiteral("iterator-get-item")), InvalidNodeId);
    EXPECT_NE(iteratorBody->addNode(QStringLiteral("iterator-set-result")), InvalidNodeId);
    EXPECT_EQ(iteratorBody->addNode(QStringLiteral("scope-get-input")), InvalidNodeId);
}

TEST(ScopeNodesTest, SubgraphsPersistWithGraphKind)
{
    ensureScopeApp();

    NodeGraphModel root;
    NodeGraphModel* body = root.ensureSubgraph(QStringLiteral("body-a"), NodeGraphModel::GraphKind::IteratorBody);
    ASSERT_NE(body, nullptr);
    const NodeId itemId = body->addNode(QStringLiteral("iterator-get-item"));
    ASSERT_NE(itemId, InvalidNodeId);

    const QJsonObject saved = root.save();
    ASSERT_TRUE(saved.contains(QStringLiteral("subgraphs")));
    ASSERT_TRUE(saved.value(QStringLiteral("subgraphs")).toObject().contains(QStringLiteral("body-a")));

    NodeGraphModel loaded;
    loaded.load(saved);
    NodeGraphModel* restored = loaded.subgraph(QStringLiteral("body-a"));
    ASSERT_NE(restored, nullptr);
    EXPECT_EQ(restored->graphKind(), NodeGraphModel::GraphKind::IteratorBody);
    EXPECT_NE(restored->addNode(QStringLiteral("iterator-set-result")), InvalidNodeId);
    EXPECT_EQ(restored->addNode(QStringLiteral("scope-set-output")), InvalidNodeId);
}

TEST(ScopeNodesTest, LoadedTransformScopeUsesSavedBodyGraph)
{
    ensureScopeApp();

    NodeGraphModel root;
    const NodeId scopeId = root.addNode(QStringLiteral("transform-scope"));
    ASSERT_NE(scopeId, InvalidNodeId);

    auto* scopeDelegate = root.delegateModel<ToolNodeDelegate>(scopeId);
    ASSERT_NE(scopeDelegate, nullptr);
    auto scope = std::dynamic_pointer_cast<TransformScopeNode>(scopeDelegate->node());
    ASSERT_TRUE(scope);

    NodeGraphModel* body = root.ensureSubgraph(scope->bodyId(), NodeGraphModel::GraphKind::TransformBody);
    ASSERT_NE(body, nullptr);

    const NodeId inputId = body->addNode(QStringLiteral("scope-get-input"));
    const NodeId outputId = body->addNode(QStringLiteral("scope-set-output"));
    ASSERT_NE(inputId, InvalidNodeId);
    ASSERT_NE(outputId, InvalidNodeId);

    const PortIndex inputOut = portIndexFor(*body, inputId, PortType::Out, QString::fromLatin1(GetInputNode::kOutputInputId));
    const PortIndex setOutputIn = portIndexFor(*body, outputId, PortType::In, QString::fromLatin1(SetOutputNode::kInputOutputId));
    ASSERT_NE(inputOut, InvalidPortIndex);
    ASSERT_NE(setOutputIn, InvalidPortIndex);
    body->addConnection(ConnectionId{inputId, inputOut, outputId, setOutputIn});

    const QJsonObject saved = root.save();

    NodeGraphModel loaded;
    loaded.load(saved);
    ASSERT_EQ(loaded.allNodeIds().size(), 1u);

    const NodeId loadedScopeId = *loaded.allNodeIds().begin();
    auto* loadedDelegate = loaded.delegateModel<ToolNodeDelegate>(loadedScopeId);
    ASSERT_NE(loadedDelegate, nullptr);
    auto loadedScope = std::dynamic_pointer_cast<TransformScopeNode>(loadedDelegate->node());
    ASSERT_TRUE(loadedScope);

    NodeGraphModel* loadedBody = loaded.subgraph(loadedScope->bodyId());
    ASSERT_NE(loadedBody, nullptr);
    EXPECT_EQ(loadedBody->graphKind(), NodeGraphModel::GraphKind::TransformBody);
    EXPECT_EQ(loadedBody->allNodeIds().size(), 2u);

    ExecutionToken in;
    in.data.insert(QString::fromLatin1(TransformScopeNode::kInputInputId), QStringLiteral("saved seed"));
    const TokenList out = loadedScope->execute(TokenList{in});
    ASSERT_EQ(out.size(), 1u);
    EXPECT_FALSE(out.front().data.contains(QStringLiteral("__error")))
        << out.front().data.value(QStringLiteral("__error")).toString().toStdString();
    EXPECT_EQ(out.front().data.value(QString::fromLatin1(TransformScopeNode::kOutputOutputId)).toString(),
              QStringLiteral("saved seed"));
}

TEST(ScopeNodesTest, DeletingScopeRemovesOwnedBodyGraph)
{
    ensureScopeApp();

    NodeGraphModel root;
    const NodeId scopeId = root.addNode(QStringLiteral("iterator-scope"));
    ASSERT_NE(scopeId, InvalidNodeId);

    auto* scopeDelegate = root.delegateModel<ToolNodeDelegate>(scopeId);
    ASSERT_NE(scopeDelegate, nullptr);
    auto scope = std::dynamic_pointer_cast<IteratorScopeNode>(scopeDelegate->node());
    ASSERT_TRUE(scope);

    ASSERT_NE(root.ensureSubgraph(scope->bodyId(), NodeGraphModel::GraphKind::IteratorBody), nullptr);
    ASSERT_NE(root.subgraph(scope->bodyId()), nullptr);

    EXPECT_TRUE(root.deleteNode(scopeId));
    EXPECT_EQ(root.subgraph(scope->bodyId()), nullptr);
}
