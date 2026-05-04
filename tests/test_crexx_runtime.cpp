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

TEST(CrexxRuntimeTest, ProduceBodyWritesOutputAndLog)
{
    CrexxRuntime runtime;
    CrexxMockScriptHost host;
    host.inputs[QStringLiteral("input")] = QStringLiteral("alpha");

    const QString script =
        QStringLiteral("output[1] = input[1]\n"
                       "output[2] = \"beta\"\n"
                       "log[1] = \"ran\"\n");

    const bool success = runtime.execute(script, &host);

    ASSERT_TRUE(success) << (host.errors.empty() ? "" : host.errors.front().toStdString());
    ASSERT_FALSE(host.logs.empty());
    EXPECT_EQ(host.logs.front(), QStringLiteral("ran"));

    const QVariant output = host.outputs[QStringLiteral("output")];
    ASSERT_EQ(output.typeId(), QMetaType::QVariantList);
    const QVariantList values = output.toList();
    ASSERT_EQ(values.size(), 2);
    EXPECT_EQ(values.at(0).toString(), QStringLiteral("alpha"));
    EXPECT_EQ(values.at(1).toString(), QStringLiteral("beta"));
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

TEST(CrexxRuntimeTest, MultipleTokenInputsBecomeInputArray)
{
    CrexxRuntime runtime;
    CrexxMockScriptHost host;

    QVariantList tokens;
    tokens << QVariantMap{{QStringLiteral("input"), QStringLiteral("one")}};
    tokens << QVariantMap{{QStringLiteral("input"), QStringLiteral("two")}};
    host.inputs[QStringLiteral("_tokens")] = tokens;

    const QString script =
        QStringLiteral("output[1] = input[1]\n"
                       "output[2] = input[2]\n");

    const bool success = runtime.execute(script, &host);

    ASSERT_TRUE(success) << (host.errors.empty() ? "" : host.errors.front().toStdString());
    const QVariantList values = host.outputs[QStringLiteral("output")].toList();
    ASSERT_EQ(values.size(), 2);
    EXPECT_EQ(values.at(0).toString(), QStringLiteral("one"));
    EXPECT_EQ(values.at(1).toString(), QStringLiteral("two"));
}

TEST(CrexxRuntimeTest, SystemOnlyTokensAreNotInputItems)
{
    CrexxRuntime runtime;
    CrexxMockScriptHost host;

    QVariantList tokens;
    tokens << QVariantMap{{QStringLiteral("input"), QStringLiteral("visible")}};
    tokens << QVariantMap{{QStringLiteral("_sys_node_output_dir"), QStringLiteral("/tmp/internal")}};
    host.inputs[QStringLiteral("_tokens")] = tokens;

    const QString script =
        QStringLiteral("output[1] = input.0\n"
                       "output[2] = input[1]\n");

    const bool success = runtime.execute(script, &host);

    ASSERT_TRUE(success) << (host.errors.empty() ? "" : host.errors.front().toStdString());
    const QVariantList values = host.outputs[QStringLiteral("output")].toList();
    ASSERT_EQ(values.size(), 2);
    EXPECT_EQ(values.at(0).toString(), QStringLiteral("1"));
    EXPECT_EQ(values.at(1).toString(), QStringLiteral("visible"));
}

TEST(CrexxRuntimeTest, ErrorsArrayFailsExecution)
{
    CrexxRuntime runtime;
    CrexxMockScriptHost host;

    const bool success = runtime.execute(QStringLiteral("errors[1] = \"nope\"\n"), &host);

    EXPECT_FALSE(success);
    ASSERT_FALSE(host.errors.empty());
    EXPECT_EQ(host.errors.front(), QStringLiteral("nope"));
}
