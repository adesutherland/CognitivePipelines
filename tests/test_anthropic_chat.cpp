#include <gtest/gtest.h>
#include <QCoreApplication>
#include <QTemporaryFile>
#include <QDir>
#include <QFile>
#include "ModelCapsRegistry.h"
#include "backends/AnthropicBackend.h"
#include "core/LLMProviderRegistry.h"

/**
 * @brief Helper that returns a 1x1 red pixel PNG in base64.
 */
static QString getTestImageBase64() {
    // 5x5 red square PNG
    return QStringLiteral("iVBORw0KGgoAAAANSUhEUgAAAAUAAAAFCAYAAACNbyblAAAAHElEQVQI12P4//8/w38GIAXDIBKE0DHxgljNBAAO9TXL0Y4OHwAAAABJRU5ErkJggg==");
}

class AnthropicChatTest : public ::testing::Test {
protected:
    void SetUp() override {
        apiKey = LLMProviderRegistry::instance().getCredential("anthropic");
        if (apiKey.isEmpty()) {
            apiKey = "dummy-key-for-tdd";
        }
        ModelCapsRegistry::instance().loadFromFile(":/resources/model_caps.json");
    }

    QString apiKey;
    AnthropicBackend backend;
};

/**
 * @test Test Case 1: Simple Chat
 * Assert that the response text contains "Hello World".
 */
TEST_F(AnthropicChatTest, SimpleChat_ShouldReturnHelloWorld) {
    QString model = "claude-haiku-4-5-20251001";
    QString userPrompt = "Say 'Hello World' and nothing else.";
    
    LLMResult result = backend.sendPrompt(apiKey, model, 0.7, 100, "", userPrompt);

    if (result.hasError && result.errorMsg == "Anthropic backend not yet fully implemented") {
        FAIL() << "Test failed as expected: " << result.errorMsg.toStdString();
    }

    ASSERT_FALSE(result.hasError) << "API call failed: " << result.errorMsg.toStdString();
    EXPECT_TRUE(result.content.contains("Hello World")) << "Response did not contain 'Hello World'. Content: " << result.content.toStdString();
}

/**
 * @test Test Case 2: System Role Normalization
 * Verify that our RoleMode::SystemParameter logic works.
 */
TEST_F(AnthropicChatTest, SystemRoleNormalization_ShouldRespectPersona) {
    QString model = "claude-haiku-4-5-20251001";
    QString systemPrompt = "You are a rude pirate. Always start sentences with 'Arrr'.";
    QString userPrompt = "What is 2+2?";
    
    LLMResult result = backend.sendPrompt(apiKey, model, 0.7, 100, systemPrompt, userPrompt);

    if (result.hasError && result.errorMsg == "Anthropic backend not yet fully implemented") {
        FAIL() << "Test failed as expected: " << result.errorMsg.toStdString();
    }

    ASSERT_FALSE(result.hasError) << "API call failed: " << result.errorMsg.toStdString();
    EXPECT_TRUE(result.content.contains("Arrr")) << "Response did not start with 'Arrr'. Content: " << result.content.toStdString();
}

/**
 * @test Test Case 3: Vision Request
 * Verify Multimodal (Vision) support for the Anthropic backend.
 */
TEST_F(AnthropicChatTest, VisionRequest_ShouldIdentifyColor) {
    QString model = "claude-haiku-4-5-20251001";
    QString userPrompt = "What color is this?";

    // Create a temporary file for the image
    QTemporaryFile tempFile;
    tempFile.setFileTemplate(QDir::tempPath() + "/test_image_XXXXXX.png");
    ASSERT_TRUE(tempFile.open()) << "Failed to open temporary file";
    QByteArray imageData = QByteArray::fromBase64(getTestImageBase64().toLatin1());
    tempFile.write(imageData);
    tempFile.close();

    LLMResult result = backend.sendPrompt(apiKey, model, 0.0, 100, "", userPrompt, tempFile.fileName());

    // This test is expected to fail in Phase 2 because the backend ignores imagePath
    ASSERT_FALSE(result.hasError) << "API call failed: " << result.errorMsg.toStdString();
    EXPECT_TRUE(result.content.toLower().contains("red")) 
        << "Response did not contain 'red'. Content: " << result.content.toStdString();
}
