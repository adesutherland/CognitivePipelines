#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDir>
#include <QStandardPaths>
#include <cstdlib>

#include "ModelCapsRegistry.h"
#include "core/LLMProviderRegistry.h"
#include "ModelCaps.h"

using namespace ModelCapsTypes;

class AnthropicFoundationTest : public ::testing::Test {
protected:
    QByteArray m_originalApiKey;

    void SetUp() override {
        m_originalApiKey = qgetenv("ANTHROPIC_API_KEY");
        // Ensure we have a clean environment for each test
        qunsetenv("ANTHROPIC_API_KEY");
        ModelCapsRegistry::instance().loadFromFile(":/resources/model_caps.json");
    }

    void TearDown() override {
        if (!m_originalApiKey.isNull()) {
            qputenv("ANTHROPIC_API_KEY", m_originalApiKey);
        } else {
            qunsetenv("ANTHROPIC_API_KEY");
        }
    }
};

/**
 * @test Verify that ModelCapsRegistry can resolve a Claude model with RoleMode::SystemParameter.
 * NOTE: This is expected to FAIL until resources/model_caps.json is updated.
 */
TEST_F(AnthropicFoundationTest, CapabilityResolution_ShouldReturnSystemParameter) {
    auto& registry = ModelCapsRegistry::instance();
    
    // We expect claude-4.5 and newer to use the new SystemParameter role mode
    QString modelId = "claude-sonnet-4-5-20250929";
    QString providerId = "anthropic";

    auto capsOpt = registry.resolve(modelId, providerId);
    ASSERT_TRUE(capsOpt.has_value()) << "Model should be resolvable by registry";

    const auto& caps = capsOpt.value();

    // EXPECTATION: This should fail because the JSON currently likely says "system" or is missing.
    // Phase 1 strategy defined that we will add RoleMode::SystemParameter.
    EXPECT_EQ(caps.roleMode, RoleMode::SystemParameter) 
        << "Anthropic models should use RoleMode::SystemParameter for normalization";

    // Most Claude models have vision
    EXPECT_TRUE(caps.capabilities.contains(Capability::Vision)) 
        << "Claude 3.5 Sonnet should have Vision capability";
}

/**
 * @test Verify that ModelCapsRegistry correctly loads custom headers from JSON.
 */
TEST_F(AnthropicFoundationTest, Headers_ShouldLoadFromConfig) {
    auto& registry = ModelCapsRegistry::instance();
    QString modelId = "claude-sonnet-4-5-20250929";
    QString providerId = "anthropic";

    auto capsOpt = registry.resolve(modelId, providerId);
    ASSERT_TRUE(capsOpt.has_value()) << "Model should be resolvable";

    EXPECT_EQ(capsOpt->customHeaders.value("anthropic-version"), "2023-06-01")
        << "Custom header 'anthropic-version' should be loaded from JSON";
}

/**
 * @test Verify that LLMProviderRegistry prioritizes Environment Variables over accounts.json.
 * NOTE: This is expected to FAIL until logic is implemented in LLMProviderRegistry.cpp.
 */
TEST_F(AnthropicFoundationTest, CredentialPriority_EnvVarShouldWin) {
    // 1. Setup Environment Variable
    QString envKey = "env-key-123";
    qputenv("ANTHROPIC_API_KEY", envKey.toUtf8());

    // 2. Setup temporary accounts.json
    // We need to know where LLMProviderRegistry looks for it. 
    // Usually it uses QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) or similar.
    // However, for testing, we might need to mock or ensure it's in the expected path.
    // Based on mainwindow.cpp, it looks in CognitivePipelines/accounts.json in GenericConfigLocation/GenericDataLocation.
    
#if defined(Q_OS_MAC)
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
#else
    const QString baseDir = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
#endif
    QString configDirPath = QDir(baseDir).filePath("CognitivePipelines");
    QDir().mkpath(configDirPath);
    QString filePath = QDir(configDirPath).filePath("accounts.json");

    // Backup existing file if it exists
    bool hadFile = QFile::exists(filePath);
    QString backupPath = filePath + ".bak";
    if (hadFile) {
        QFile::remove(backupPath);
        QFile::rename(filePath, backupPath);
    }

    // Create test accounts.json
    QJsonObject root;
    QJsonArray accounts;
    QJsonObject anthropicAccount;
    anthropicAccount["name"] = "anthropic";
    anthropicAccount["api_key"] = "file-key-456";
    accounts.append(anthropicAccount);
    root["accounts"] = accounts;

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson());
        file.close();
    }

    // 3. Call Registry
    QString resultKey = LLMProviderRegistry::instance().getCredential("anthropic");

    // 4. Cleanup
    QFile::remove(filePath);
    if (hadFile) {
        QFile::rename(backupPath, filePath);
    }

    // ASSERTION: Env should win.
    // This will likely return empty or the file key depending on current implementation.
    EXPECT_EQ(resultKey, envKey) << "Environment variable should have higher priority than accounts.json";
}
