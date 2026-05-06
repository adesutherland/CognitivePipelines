#include <gtest/gtest.h>

#include "CrexxControllerNode.h"
#include "CrexxRuntime.h"
#include "QuickJSRuntime.h"

#include <memory>

namespace {
void ensureCrexxRegistered()
{
    ScriptEngineRegistry::instance().registerEngine(QStringLiteral("crexx"), []() {
        return std::make_unique<CrexxRuntime>();
    });
}

void ensureQuickJsRegistered()
{
    ScriptEngineRegistry::instance().registerEngine(QStringLiteral("quickjs"), []() {
        return std::make_unique<QuickJSRuntime>();
    });
}

ExecutionToken tokenForPin(const QString& pinId, const QVariant& value)
{
    ExecutionToken token;
    token.triggeringPinId = pinId;
    token.data.insert(pinId, value);
    return token;
}
} // namespace

TEST(CrexxControllerNodeTest, StartRunsScriptAndEmitsCreatorRoute)
{
    ensureCrexxRegistered();
    CrexxControllerNode node;

    const TokenList out = node.execute(TokenList{
        tokenForPin(QString::fromLatin1(CrexxControllerNode::kInputStartId), QStringLiteral("draft brief"))
    });

    ASSERT_EQ(out.size(), 1u);
    const auto& token = out.front();
    EXPECT_TRUE(token.forceExecution);
    EXPECT_EQ(token.data.value(QString::fromLatin1(CrexxControllerNode::kOutputCreatorId)).toString(),
              QStringLiteral("draft brief"));
    EXPECT_EQ(token.data.value(QStringLiteral("_decision")).toString(), QStringLiteral("creator"));
}

TEST(CrexxControllerNodeTest, DescriptorUsesScriptVisiblePinNames)
{
    CrexxControllerNode node;

    const NodeDescriptor desc = node.getDescriptor();

    EXPECT_EQ(desc.inputPins.value(QString::fromLatin1(CrexxControllerNode::kInputStartId)).name,
              QString::fromLatin1(CrexxControllerNode::kInputStartId));
    EXPECT_EQ(desc.inputPins.value(QString::fromLatin1(CrexxControllerNode::kInputCreatorResultId)).name,
              QString::fromLatin1(CrexxControllerNode::kInputCreatorResultId));
    EXPECT_EQ(desc.inputPins.value(QString::fromLatin1(CrexxControllerNode::kInputValidatorResultId)).name,
              QString::fromLatin1(CrexxControllerNode::kInputValidatorResultId));
    EXPECT_EQ(desc.outputPins.value(QString::fromLatin1(CrexxControllerNode::kOutputCreatorId)).name,
              QString::fromLatin1(CrexxControllerNode::kOutputCreatorId));
    EXPECT_EQ(desc.outputPins.value(QString::fromLatin1(CrexxControllerNode::kOutputValidatorId)).name,
              QString::fromLatin1(CrexxControllerNode::kOutputValidatorId));
    EXPECT_EQ(desc.outputPins.value(QString::fromLatin1(CrexxControllerNode::kOutputDoneId)).name,
              QString::fromLatin1(CrexxControllerNode::kOutputDoneId));
}

TEST(CrexxControllerNodeTest, CreatorResultRoutesToValidator)
{
    ensureCrexxRegistered();
    CrexxControllerNode node;

    ASSERT_EQ(node.execute(TokenList{
        tokenForPin(QString::fromLatin1(CrexxControllerNode::kInputStartId), QStringLiteral("draft brief"))
    }).size(), 1u);

    const TokenList out = node.execute(TokenList{
        tokenForPin(QString::fromLatin1(CrexxControllerNode::kInputCreatorResultId), QStringLiteral("draft v1"))
    });

    ASSERT_EQ(out.size(), 1u);
    const auto& token = out.front();
    EXPECT_TRUE(token.forceExecution);
    const QString validatorPrompt = token.data.value(QString::fromLatin1(CrexxControllerNode::kOutputValidatorId)).toString();
    EXPECT_TRUE(validatorPrompt.contains(QStringLiteral("draft brief")));
    EXPECT_TRUE(validatorPrompt.contains(QStringLiteral("draft v1")));
    EXPECT_FALSE(validatorPrompt.contains(QStringLiteral("${")));
    EXPECT_EQ(token.data.value(QStringLiteral("_decision")).toString(), QStringLiteral("validator"));
}

TEST(CrexxControllerNodeTest, AcceptedValidatorEmitsDoneWithCreatorResult)
{
    ensureCrexxRegistered();
    CrexxControllerNode node;

    ASSERT_EQ(node.execute(TokenList{
        tokenForPin(QString::fromLatin1(CrexxControllerNode::kInputStartId), QStringLiteral("draft brief"))
    }).size(), 1u);
    ASSERT_EQ(node.execute(TokenList{
        tokenForPin(QString::fromLatin1(CrexxControllerNode::kInputCreatorResultId), QStringLiteral("draft v1"))
    }).size(), 1u);

    const TokenList out = node.execute(TokenList{
        tokenForPin(QString::fromLatin1(CrexxControllerNode::kInputValidatorResultId), QStringLiteral("PASS final answer"))
    });

    ASSERT_EQ(out.size(), 1u);
    const auto& token = out.front();
    EXPECT_FALSE(token.forceExecution);
    EXPECT_EQ(token.data.value(QString::fromLatin1(CrexxControllerNode::kOutputDoneId)).toString(),
              QStringLiteral("draft v1"));
    EXPECT_EQ(token.data.value(QStringLiteral("_decision")).toString(), QStringLiteral("done"));
}

TEST(CrexxControllerNodeTest, RetryUsesForceExecutionAndMaxIterationBrake)
{
    ensureCrexxRegistered();
    CrexxControllerNode node;
    node.setMaxIterations(2);

    ASSERT_EQ(node.execute(TokenList{
        tokenForPin(QString::fromLatin1(CrexxControllerNode::kInputStartId), QStringLiteral("seed"))
    }).size(), 1u);
    ASSERT_EQ(node.execute(TokenList{
        tokenForPin(QString::fromLatin1(CrexxControllerNode::kInputCreatorResultId), QStringLiteral("draft v1"))
    }).size(), 1u);

    const TokenList firstRetry = node.execute(TokenList{
        tokenForPin(QString::fromLatin1(CrexxControllerNode::kInputValidatorResultId), QStringLiteral("Needs detail"))
    });
    ASSERT_EQ(firstRetry.size(), 1u);
    EXPECT_TRUE(firstRetry.front().forceExecution);
    const QString retryPrompt = firstRetry.front()
                                    .data.value(QString::fromLatin1(CrexxControllerNode::kOutputCreatorId))
                                    .toString();
    EXPECT_TRUE(retryPrompt.contains(QStringLiteral("Needs detail")));
    EXPECT_TRUE(retryPrompt.contains(QStringLiteral("draft v1")));
    EXPECT_EQ(firstRetry.front().data.value(QStringLiteral("_iteration")).toInt(), 1);

    ASSERT_EQ(node.execute(TokenList{
        tokenForPin(QString::fromLatin1(CrexxControllerNode::kInputCreatorResultId), QStringLiteral("draft v2"))
    }).size(), 1u);

    const TokenList finalOut = node.execute(TokenList{
        tokenForPin(QString::fromLatin1(CrexxControllerNode::kInputValidatorResultId), QStringLiteral("Still weak"))
    });
    ASSERT_EQ(finalOut.size(), 1u);
    EXPECT_FALSE(finalOut.front().forceExecution);
    EXPECT_EQ(finalOut.front().data.value(QString::fromLatin1(CrexxControllerNode::kOutputDoneId)).toString(),
              QStringLiteral("draft v2"));
    EXPECT_EQ(finalOut.front().data.value(QStringLiteral("_decision")).toString(), QStringLiteral("done"));
}

TEST(CrexxControllerNodeTest, CustomCrexxScriptCanDecideFromValidator)
{
    ensureCrexxRegistered();
    CrexxControllerNode node;
    node.setScriptCode(QStringLiteral(
        "phase = \"\"\n"
        "address pipeline \"GET phase INTO :phase\"\n"
        "if phase = \"from_validator\" then do\n"
        "    feedback = \"\"\n"
        "    address pipeline \"GET from_validator INTO :feedback\"\n"
        "    result = \"chosen: \" || feedback\n"
        "    address pipeline \"SET route done\"\n"
        "    address pipeline \"SET done :result\"\n"
        "    address pipeline \"SET status custom decision\"\n"
        "    return 0\n"
        "end\n"
        "if phase = \"from_creator\" then do\n"
        "    payload = \"\"\n"
        "    address pipeline \"GET from_creator INTO :payload\"\n"
        "    address pipeline \"SET route validator\"\n"
        "    address pipeline \"SET to_validator :payload\"\n"
        "    return 0\n"
        "end\n"
        "payload = \"\"\n"
        "address pipeline \"GET start INTO :payload\"\n"
        "address pipeline \"SET route creator\"\n"
        "address pipeline \"SET to_creator :payload\"\n"));

    ASSERT_EQ(node.execute(TokenList{
        tokenForPin(QString::fromLatin1(CrexxControllerNode::kInputStartId), QStringLiteral("seed"))
    }).size(), 1u);
    ASSERT_EQ(node.execute(TokenList{
        tokenForPin(QString::fromLatin1(CrexxControllerNode::kInputCreatorResultId), QStringLiteral("draft"))
    }).size(), 1u);

    const TokenList out = node.execute(TokenList{
        tokenForPin(QString::fromLatin1(CrexxControllerNode::kInputValidatorResultId), QStringLiteral("checker saw enough"))
    });

    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out.front().data.value(QString::fromLatin1(CrexxControllerNode::kOutputDoneId)).toString(),
              QStringLiteral("chosen: checker saw enough"));
    EXPECT_EQ(out.front().data.value(QStringLiteral("_status")).toString(), QStringLiteral("custom decision"));
}

TEST(CrexxControllerNodeTest, CustomJavaScriptScriptCanReadNamedPinsAndRoute)
{
    ensureQuickJsRegistered();
    CrexxControllerNode node;
    node.setEngineId(QStringLiteral("quickjs"));
    node.setScriptCode(QStringLiteral(
        "const phase = pipeline.input('phase');\n"
        "if (phase === 'start') {\n"
        "  pipeline.output('route', 'creator');\n"
        "  pipeline.output('to_creator', pipeline.input('start'));\n"
        "} else if (phase === 'from_creator') {\n"
        "  pipeline.output('route', 'validator');\n"
        "  pipeline.output('to_validator', pipeline.input('from_creator'));\n"
        "} else {\n"
        "  pipeline.output('route', 'done');\n"
        "  pipeline.output('done', 'js: ' + pipeline.input('from_validator'));\n"
        "}\n"));

    ASSERT_EQ(node.execute(TokenList{
        tokenForPin(QString::fromLatin1(CrexxControllerNode::kInputStartId), QStringLiteral("seed"))
    }).size(), 1u);
    ASSERT_EQ(node.execute(TokenList{
        tokenForPin(QString::fromLatin1(CrexxControllerNode::kInputCreatorResultId), QStringLiteral("draft"))
    }).size(), 1u);

    const TokenList out = node.execute(TokenList{
        tokenForPin(QString::fromLatin1(CrexxControllerNode::kInputValidatorResultId), QStringLiteral("validated"))
    });

    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out.front().data.value(QString::fromLatin1(CrexxControllerNode::kOutputDoneId)).toString(),
              QStringLiteral("js: validated"));
    EXPECT_EQ(out.front().data.value(QStringLiteral("_decision")).toString(), QStringLiteral("done"));
}
