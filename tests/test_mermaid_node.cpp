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

TEST(MermaidRenderSizingTest, ClampedDetailMentionsScaleAndDpr)
{
    const auto sizing = MermaidRenderService::planRenderSizing(12000.0, 12000.0, 3.5, 2.0);
    ASSERT_TRUE(sizing.clamped);
    ASSERT_FALSE(sizing.detail.isEmpty());
    EXPECT_NE(sizing.detail.indexOf(QStringLiteral("3.50")), -1);
    EXPECT_NE(sizing.detail.indexOf(QStringLiteral("dpr 2.00")), -1);
}

TEST(MermaidRenderSizingTest, ClampsWhenTileBudgetExceededWithoutDimensionClamp)
{
    // Sized to avoid dimension clamp but exceed tile memory budget at high DPR
    const auto sizing = MermaidRenderService::planRenderSizing(2600.0, 2600.0, 3.0, 2.0);
    EXPECT_TRUE(sizing.clamped);
    EXPECT_TRUE(sizing.error.isEmpty());
    EXPECT_LT(sizing.effectiveScale, 3.0);
    EXPECT_GT(sizing.effectiveScale, 0.5);
}

TEST(MermaidRenderSizingTest, ClampsScaleAboveThreeAtHighDpr)
{
    // Regression: large diagrams at high scale should clamp before rendering to avoid tile truncation
    const auto sizing = MermaidRenderService::planRenderSizing(1400.0, 1200.0, 3.5, 2.0);
    EXPECT_TRUE(sizing.clamped);
    EXPECT_TRUE(sizing.error.isEmpty());
    EXPECT_LT(sizing.effectiveScale, 3.5);
    EXPECT_GT(sizing.effectiveScale, 0.5);
}

TEST(MermaidRenderDetailTest, FormatsClampDetailInOrder)
{
    const QString msg = MermaidRenderService::formatClampDetail(4.0, 1.77, QStringLiteral("tile memory"), 2408, 3347, 2.0);
    EXPECT_NE(msg.indexOf(QStringLiteral("Scale 4.00 clamped to 1.77")), -1);
    EXPECT_NE(msg.indexOf(QStringLiteral("tile memory")), -1);
    EXPECT_NE(msg.indexOf(QStringLiteral("render size 2408x3347")), -1);
    EXPECT_NE(msg.indexOf(QStringLiteral("dpr 2.00")), -1);
}
