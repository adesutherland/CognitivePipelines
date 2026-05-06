#include <gtest/gtest.h>

#include "CrexxRuntime.h"
#include "UniversalScriptTemplates.h"

#include <QDir>

#include <map>
#include <vector>

class CrexxMockScriptHost : public IScriptHost {
public:
    void log(const QString& message) override
    {
        logs.push_back(message);
    }

    QVariant getInput(const QString& key) override
    {
        const auto it = inputs.find(key);
        return it == inputs.end() ? QVariant() : it->second;
    }

    void setOutput(const QString& key, const QVariant& value) override
    {
        outputs[key] = value;
    }

    void setError(const QString& message) override
    {
        errors.push_back(message);
    }

    QString getTempDir() const override
    {
        return QDir::tempPath();
    }

    std::vector<QString> logs;
    std::vector<QString> errors;
    std::map<QString, QVariant> inputs;
    std::map<QString, QVariant> outputs;
};

TEST(CrexxRuntimeTest, Identity)
{
    CrexxRuntime runtime;
    EXPECT_EQ(runtime.getEngineId(), QStringLiteral("crexx"));
}

TEST(CrexxRuntimeTest, BodyReadsAndWritesNamedPins)
{
    CrexxRuntime runtime;
    CrexxMockScriptHost host;
    host.inputs[QStringLiteral("input")] = QStringLiteral("alpha");

    const QString script =
        QStringLiteral("value = \"\"\n"
                       "address pipeline \"GET input INTO :value\"\n"
                       "result = value || \" beta\"\n"
                       "address pipeline \"SET output :result\"\n"
                       "address pipeline \"LOG ran\"\n");

    const bool success = runtime.execute(script, &host);

    ASSERT_TRUE(success) << (host.errors.empty() ? "" : host.errors.front().toStdString());
    ASSERT_FALSE(host.logs.empty());
    EXPECT_EQ(host.logs.front(), QStringLiteral("ran"));
    EXPECT_EQ(host.outputs[QStringLiteral("output")].toString(), QStringLiteral("alpha beta"));
}

TEST(CrexxRuntimeTest, StarterProcedureTemplateRuns)
{
    CrexxRuntime runtime;
    CrexxMockScriptHost host;
    host.inputs[QStringLiteral("input")] = QStringLiteral("alpha");

    const bool success = runtime.execute(UniversalScriptTemplates::crexx(), &host);

    ASSERT_TRUE(success) << (host.errors.empty() ? "" : host.errors.front().toStdString());
    EXPECT_EQ(host.outputs[QStringLiteral("output")].toString(), QStringLiteral("ALPHA"));
    ASSERT_FALSE(host.logs.empty());
    EXPECT_TRUE(host.logs.front().contains(QStringLiteral("Processed")));
}

TEST(CrexxRuntimeTest, ProduceProcedureUsesNamedPins)
{
    CrexxRuntime runtime;
    CrexxMockScriptHost host;
    host.inputs[QStringLiteral("topic")] = QStringLiteral("corn laws");

    const QString script =
        QStringLiteral("produce: procedure = .int\n"
                       "  topic = \"\"\n"
                       "  address pipeline \"GET topic INTO :topic\"\n"
                       "  answer = \"topic: \" || topic\n"
                       "  address pipeline \"SET summary :answer\"\n"
                       "  return 0\n");

    const bool success = runtime.execute(script, &host);

    ASSERT_TRUE(success) << (host.errors.empty() ? "" : host.errors.front().toStdString());
    EXPECT_EQ(host.outputs[QStringLiteral("summary")].toString(), QStringLiteral("topic: corn laws"));
}

TEST(CrexxRuntimeTest, NamedPinGetSerializesStructuredValues)
{
    CrexxRuntime runtime;
    CrexxMockScriptHost host;
    host.inputs[QStringLiteral("items")] = QVariantList{QStringLiteral("one"), QStringLiteral("two")};

    const QString script =
        QStringLiteral("json = \"\"\n"
                       "address pipeline \"GET items INTO :json\"\n"
                       "address pipeline \"SET json :json\"\n");

    const bool success = runtime.execute(script, &host);

    ASSERT_TRUE(success) << (host.errors.empty() ? "" : host.errors.front().toStdString());
    EXPECT_EQ(host.outputs[QStringLiteral("json")].toString(), QStringLiteral("[\"one\",\"two\"]"));
}

TEST(CrexxRuntimeTest, AddressErrorFailsExecution)
{
    CrexxRuntime runtime;
    CrexxMockScriptHost host;

    const bool success = runtime.execute(QStringLiteral("address pipeline \"ERROR nope\"\n"), &host);

    EXPECT_FALSE(success);
    ASSERT_FALSE(host.errors.empty());
    EXPECT_EQ(host.errors.front(), QStringLiteral("nope"));
}

TEST(CrexxRuntimeTest, AddressEnvironmentCanGetAndSetNamedPins)
{
    CrexxRuntime runtime;
    CrexxMockScriptHost host;
    host.inputs[QStringLiteral("prompt")] = QStringLiteral("corn laws");

    const QString script =
        QStringLiteral("prompt = \"\"\n"
                       "address pipeline \"GET prompt INTO :prompt\"\n"
                       "answer = \"topic: \" || prompt\n"
                       "address pipeline \"SET result :answer\"\n");

    const bool success = runtime.execute(script, &host);

    ASSERT_TRUE(success) << (host.errors.empty() ? "" : host.errors.front().toStdString());
    EXPECT_EQ(host.outputs[QStringLiteral("result")].toString(), QStringLiteral("topic: corn laws"));
}

TEST(CrexxRuntimeTest, AddressPayloadUsesComposedHostVariable)
{
    CrexxRuntime runtime;
    CrexxMockScriptHost host;
    host.inputs[QStringLiteral("question")] = QStringLiteral("Explain TLS");
    host.inputs[QStringLiteral("answer")] = QStringLiteral("TLS protects traffic");

    const QString script =
        QStringLiteral("brief = \"\"\n"
                       "draft = \"\"\n"
                       "address pipeline \"GET question INTO :brief\"\n"
                       "address pipeline \"GET answer INTO :draft\"\n"
                       "prompt = \"Question is \" || brief || \". Answer was \" || draft\n"
                       "address pipeline \"SET prompt :prompt\"\n");

    const bool success = runtime.execute(script, &host);

    ASSERT_TRUE(success) << (host.errors.empty() ? "" : host.errors.front().toStdString());
    EXPECT_EQ(host.outputs[QStringLiteral("prompt")].toString(),
              QStringLiteral("Question is Explain TLS. Answer was TLS protects traffic"));
}
