#include <gtest/gtest.h>

#include "MermaidNode.h"
#include "MermaidRenderService.h"

TEST(MermaidNodeTest, DescriptorMatchesPins)
{
    MermaidNode node;
    const NodeDescriptor desc = node.getDescriptor();

    ASSERT_EQ(desc.id, QStringLiteral("mermaid-node"));
    ASSERT_EQ(desc.name, QStringLiteral("Mermaid Renderer"));

    ASSERT_TRUE(desc.inputPins.contains(QString::fromLatin1(MermaidNode::kInputCode)));
    EXPECT_EQ(desc.inputPins.value(QString::fromLatin1(MermaidNode::kInputCode)).type, QStringLiteral("text"));

    ASSERT_TRUE(desc.outputPins.contains(QString::fromLatin1(MermaidNode::kOutputImage)));
    EXPECT_EQ(desc.outputPins.value(QString::fromLatin1(MermaidNode::kOutputImage)).type, QStringLiteral("image"));
}

TEST(MermaidNodeTest, EmptyCodeProducesError)
{
    MermaidNode node;

    ExecutionToken token;
    token.data.insert(QString::fromLatin1(MermaidNode::kInputCode), QString());

    TokenList inputs;
    inputs.push_back(token);

    const TokenList outputs = node.execute(inputs);
    ASSERT_FALSE(outputs.empty());

    const DataPacket out = outputs.front().data;
    ASSERT_TRUE(out.contains(QStringLiteral("__error")));

    const QString err = out.value(QStringLiteral("__error")).toString();
    EXPECT_FALSE(err.isEmpty());
    EXPECT_TRUE(err.contains(QStringLiteral("empty"), Qt::CaseInsensitive));
    EXPECT_EQ(out.value(QString::fromLatin1(MermaidNode::kOutputImage)).toString(), err);
}

TEST(MermaidNodeTest, ScalePersistsAcrossState)
{
    MermaidNode node;
    QJsonObject state;
    state.insert(QStringLiteral("lastCode"), QStringLiteral("graph TD; A-->B"));
    state.insert(QStringLiteral("scale"), 2.5);

    node.loadState(state);
    const QJsonObject saved = node.saveState();

    EXPECT_DOUBLE_EQ(saved.value(QStringLiteral("scale")).toDouble(), 2.5);
    EXPECT_EQ(saved.value(QStringLiteral("lastCode")).toString(), QStringLiteral("graph TD; A-->B"));
}

TEST(MermaidNodeTest, ScaleClampsToMinimum)
{
    MermaidNode node;
    QJsonObject state;
    state.insert(QStringLiteral("scale"), 0.0);

    node.loadState(state);
    const QJsonObject saved = node.saveState();

    EXPECT_GE(saved.value(QStringLiteral("scale")).toDouble(), 0.1);
}

TEST(MermaidRenderSizingTest, LeavesNormalScaleUnchanged)
{
    const auto sizing = MermaidRenderService::planRenderSizing(800.0, 600.0, 2.0);
    EXPECT_FALSE(sizing.clamped);
    EXPECT_TRUE(sizing.error.isEmpty());
    EXPECT_DOUBLE_EQ(sizing.effectiveScale, 2.0);
}

TEST(MermaidRenderSizingTest, ClampsLargeScaleToAvoidLimits)
{
    const auto sizing = MermaidRenderService::planRenderSizing(12000.0, 9000.0, 3.0);
    EXPECT_TRUE(sizing.clamped);
    EXPECT_TRUE(sizing.error.isEmpty());
    EXPECT_GT(sizing.viewWidth, 0);
    EXPECT_GT(sizing.viewHeight, 0);
    EXPECT_LT(sizing.effectiveScale, 3.0);
}

TEST(MermaidRenderSizingTest, ErrorsWhenScaleIsExtreme)
{
    const auto sizing = MermaidRenderService::planRenderSizing(1'000'000.0, 1'000'000.0, 3.0);
    EXPECT_FALSE(sizing.error.isEmpty());
    EXPECT_EQ(sizing.viewWidth, 0);
    EXPECT_EQ(sizing.viewHeight, 0);
}

TEST(MermaidRenderSizingTest, ClampsTallDiagram)
{
    const auto sizing = MermaidRenderService::planRenderSizing(800.0, 40000.0, 1.5);
    EXPECT_TRUE(sizing.clamped);
    EXPECT_TRUE(sizing.error.isEmpty());
    EXPECT_GT(sizing.viewHeight, 0);
    EXPECT_LT(sizing.viewHeight, 40000);
}

TEST(MermaidRenderSizingTest, ClampsWithHighDevicePixelRatio)
{
    const auto sizing = MermaidRenderService::planRenderSizing(9000.0, 9000.0, 1.5, 2.0);
    EXPECT_TRUE(sizing.clamped);
    EXPECT_TRUE(sizing.error.isEmpty());
    EXPECT_GT(sizing.viewWidth, 0);
    EXPECT_LE(sizing.viewWidth, 8192);
}
