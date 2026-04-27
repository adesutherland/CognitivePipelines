#include <gtest/gtest.h>

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <QTextStream>

#include <QtConcurrent>

#include "test_app.h"
#include "NodeGraphModel.h"
#include "ToolNodeDelegate.h"
#include "VaultOutputNode.h"
#include "ai/backends/ILLMBackend.h"
#include "ai/registry/LLMProviderRegistry.h"

using namespace QtNodes;

namespace {

QApplication* ensureApp()
{
    return sharedTestApp();
}

bool writeTextFile(const QString& path, const QString& content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream << content;
    file.close();
    return true;
}

class ScopedCurrentDir {
public:
    explicit ScopedCurrentDir(const QString& path)
        : m_oldPath(QDir::currentPath())
    {
        QDir::setCurrent(path);
    }

    ~ScopedCurrentDir()
    {
        QDir::setCurrent(m_oldPath);
    }

private:
    QString m_oldPath;
};

class MockVaultBackend : public ILLMBackend {
public:
    QString id() const override { return QStringLiteral("mockvault"); }
    QString name() const override { return QStringLiteral("Mock Vault"); }
    QStringList availableModels() const override { return {QStringLiteral("router-1")}; }
    QStringList availableEmbeddingModels() const override { return {}; }

    QFuture<QStringList> fetchModelList() override
    {
        return QtConcurrent::run([]() {
            return QStringList{QStringLiteral("router-1")};
        });
    }

    LLMResult sendPrompt(const QString&,
                         const QString&,
                         double,
                         int,
                         const QString&,
                         const QString&,
                         const LLMMessage& = {}) override
    {
        LLMResult result;
        result.content = QStringLiteral(
            "```json\n"
            "{\n"
            "  \"subfolder\": \"projects/research\",\n"
            "  \"filename\": \"captured-note\",\n"
            "  \"reason\": \"Matches the existing research notes folder.\"\n"
            "}\n"
            "```");
        return result;
    }

    EmbeddingResult getEmbedding(const QString&, const QString&, const QString&) override
    {
        return {};
    }

    QFuture<QString> generateImage(const QString&,
                                   const QString&,
                                   const QString&,
                                   const QString&,
                                   const QString&,
                                   const QString&) override
    {
        return QtConcurrent::run([]() { return QString(); });
    }
};

} // namespace

TEST(VaultOutputNodeTest, Registration)
{
    ensureApp();

    NodeGraphModel model;
    const NodeId nodeId = model.addNode(QStringLiteral("vault-output"));
    ASSERT_NE(nodeId, InvalidNodeId);

    auto* delegate = model.delegateModel<ToolNodeDelegate>(nodeId);
    ASSERT_NE(delegate, nullptr);
    ASSERT_NE(dynamic_cast<VaultOutputNode*>(delegate->node().get()), nullptr);
}

TEST(VaultOutputNodeTest, WritesMarkdownToModelSelectedLocation)
{
    ensureApp();

    LLMProviderRegistry::instance().registerBackend(std::make_shared<MockVaultBackend>());

    QTemporaryDir tempDir;
    ASSERT_TRUE(tempDir.isValid());
    ScopedCurrentDir scopedDir(tempDir.path());

    const QString vaultRoot = tempDir.filePath(QStringLiteral("vault"));
    ASSERT_TRUE(QDir().mkpath(QDir(vaultRoot).filePath(QStringLiteral("projects/research"))));
    ASSERT_TRUE(writeTextFile(QDir(vaultRoot).filePath(QStringLiteral("projects/research/existing.md")),
                              QStringLiteral("# Existing Research Note\n\nPrior note.")));
    ASSERT_TRUE(writeTextFile(tempDir.filePath(QStringLiteral("accounts.json")),
                              QStringLiteral("{\"accounts\":[{\"name\":\"mockvault\",\"api_key\":\"test-key\"}]}")));

    VaultOutputNode node;
    QJsonObject state;
    state.insert(QStringLiteral("vault_root"), vaultRoot);
    state.insert(QStringLiteral("provider_id"), QStringLiteral("mockvault"));
    state.insert(QStringLiteral("model_id"), QStringLiteral("router-1"));
    node.loadState(state);

    const QString markdown = QStringLiteral("# Fresh Capture\n\nA new note captured from ingestion.");
    DataPacket input;
    input.insert(QStringLiteral("markdown"), markdown);

    ExecutionToken token;
    token.data = input;
    TokenList tokens;
    tokens.push_back(std::move(token));

    const TokenList outTokens = node.execute(tokens);
    ASSERT_FALSE(outTokens.empty());

    const DataPacket& output = outTokens.front().data;
    ASSERT_FALSE(output.contains(QStringLiteral("__error")))
        << output.value(QStringLiteral("__error")).toString().toStdString();

    const QString savedPath = output.value(QStringLiteral("saved_path")).toString();
    ASSERT_FALSE(savedPath.isEmpty());
    EXPECT_TRUE(QFileInfo::exists(savedPath));
    EXPECT_EQ(output.value(QStringLiteral("subfolder")).toString(), QStringLiteral("projects/research"));
    EXPECT_EQ(output.value(QStringLiteral("filename")).toString(), QStringLiteral("captured-note.md"));

    QFile savedFile(savedPath);
    ASSERT_TRUE(savedFile.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString savedContent = QString::fromUtf8(savedFile.readAll());
    EXPECT_EQ(savedContent, markdown);

    const QVariantMap decision = output.value(QStringLiteral("decision")).toMap();
    EXPECT_EQ(decision.value(QStringLiteral("provider")).toString(), QStringLiteral("mockvault"));
    EXPECT_EQ(decision.value(QStringLiteral("model")).toString(), QStringLiteral("router-1"));
}
