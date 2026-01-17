//
// Cognitive Pipeline Application
//
// Universal LLM Node tests
//

#include <gtest/gtest.h>

#include <QApplication>
#include <QString>
#include <QJsonObject>
#include <QImage>
#include <QTemporaryFile>
#include <QDir>
#include <QFile>

#include "test_app.h"
#include "UniversalLLMNode.h"
#include "core/LLMProviderRegistry.h"
#include "backends/ILLMBackend.h"
#include <QtConcurrent>

// Minimal app helper in case QObject machinery needs it
static QApplication* ensureApp()
{
    return sharedTestApp();
}

static QString resolveApiKey(const QString& providerId)
{
    return LLMProviderRegistry::instance().getCredential(providerId);
}

// Helper function to create a dummy 10x10 red PNG image for testing
static QString createDummyImageFile()
{
    // Create a 10x10 red square
    QImage img(10, 10, QImage::Format_RGB32);
    img.fill(QColor(Qt::red));
    
    // Create a temporary file with .png extension
    QTemporaryFile* tempFile = new QTemporaryFile(QDir::tempPath() + "/test_image_XXXXXX.png");
    tempFile->setAutoRemove(false); // Keep file until manually deleted
    
    if (!tempFile->open()) {
        delete tempFile;
        return QString();
    }
    
    const QString filePath = tempFile->fileName();
    tempFile->close();
    
    // Save the image
    if (!img.save(filePath, "PNG")) {
        delete tempFile;
        return QString();
    }
    
    delete tempFile;
    return filePath;
}

TEST(UniversalLLMNodeTest, OpenAIIntegration)
{
    ensureApp();

    const QString apiKey = resolveApiKey(QStringLiteral("openai"));

    if (apiKey.isEmpty()) {
        GTEST_SKIP() << "No OpenAI API key provided. Set OPENAI_API_KEY environment variable or add to accounts.json.";
    }

    auto* node = new UniversalLLMNode();

    // Configure via loadState: provider "openai", model "gpt-5-mini"
    QJsonObject state;
    state.insert(QStringLiteral("provider"), QStringLiteral("openai"));
    state.insert(QStringLiteral("model"), QStringLiteral("gpt-5-mini"));
    state.insert(QStringLiteral("systemPrompt"), QStringLiteral("You are a concise assistant."));
    state.insert(QStringLiteral("temperature"), 1.0); // gpt-5-mini only supports default temperature of 1.0
    node->loadState(state);

    // Prepare input with prompt
    DataPacket inputs;
    inputs.insert(QStringLiteral("prompt"), QStringLiteral("What is the capital of France?"));

    // Execute via V3 token API
    ExecutionToken token;
    token.data = inputs;
    TokenList tokens;
    tokens.push_back(std::move(token));

    const TokenList outTokens = node->execute(tokens);
    ASSERT_FALSE(outTokens.empty());
    const DataPacket output = outTokens.front().data;

    // Check for error
    if (output.contains(QStringLiteral("__error"))) {
        const QString error = output.value(QStringLiteral("__error")).toString();
        if (isTemporaryError(error)) {
            GTEST_SKIP() << "Temporary LLM error: " << error.toStdString();
        }
        FAIL() << "LLM request failed with error: " << error.toStdString();
    }

    // Verify response
    const QString response = output.value(QStringLiteral("response")).toString();
    ASSERT_FALSE(response.isEmpty()) << "Response should not be empty";

    // Verify usage tokens (should be > 0 for successful request)
    const int totalTokens = output.value(QStringLiteral("_usage.total_tokens")).toInt();
    EXPECT_GT(totalTokens, 0) << "Total tokens should be greater than 0";

    // Basic sanity check: response should mention Paris
    const QString responseLower = response.toLower();
    EXPECT_NE(responseLower.indexOf(QStringLiteral("paris")), -1) 
        << "Response should mention 'Paris'. Response was: " << response.toStdString();

    delete node;
}

TEST(UniversalLLMNodeTest, GoogleIntegration)
{
    ensureApp();

    const QString apiKey = resolveApiKey(QStringLiteral("google"));

    if (apiKey.isEmpty()) {
        GTEST_SKIP() << "No Google API key provided. Set GOOGLE_API_KEY/GOOGLE_GENAI_API_KEY (or add accounts.json).";
    }

    auto* node = new UniversalLLMNode();

    // Configure via loadState: provider "google", model "gemini-2.5-flash-lite"
    QJsonObject state;
    state.insert(QStringLiteral("provider"), QStringLiteral("google"));
    state.insert(QStringLiteral("model"), QStringLiteral("gemini-2.5-flash-lite"));
    state.insert(QStringLiteral("systemPrompt"), QStringLiteral("You are a concise assistant."));
    node->loadState(state);

    // Prepare input with prompt
    DataPacket inputs;
    inputs.insert(QStringLiteral("prompt"), QStringLiteral("What is the capital of France?"));

    // Execute via V3 token API
    ExecutionToken token;
    token.data = inputs;
    TokenList tokens;
    tokens.push_back(std::move(token));

    const TokenList outTokens = node->execute(tokens);
    ASSERT_FALSE(outTokens.empty());
    const DataPacket output = outTokens.front().data;

    // Check for error
    if (output.contains(QStringLiteral("__error"))) {
        const QString error = output.value(QStringLiteral("__error")).toString();
        if (isTemporaryError(error)) {
            GTEST_SKIP() << "Temporary LLM error: " << error.toStdString();
        }
        
        // Also check responseRaw for model availability issues (Google specific)
        const QString responseRaw = output.value(QStringLiteral("response")).toString();
        if (responseRaw.contains(QStringLiteral("is not found for api version v1"), Qt::CaseInsensitive) ||
            responseRaw.contains(QStringLiteral("is not supported for generatecontent"), Qt::CaseInsensitive)) {
            GTEST_SKIP() << "Google Gemini model is not available for v1/generateContent in this environment. "
                         << "Full response: " << responseRaw.toStdString();
        }

        FAIL() << "LLM request failed with error: " << error.toStdString();
    }

    // Verify response
    const QString responseRaw = output.value(QStringLiteral("response")).toString();
    const QString response = responseRaw.toLower();
    
    // Check for model availability issues (environment-specific)
    if (response.contains(QStringLiteral("is not found for api version v1")) ||
        response.contains(QStringLiteral("is not supported for generatecontent"))) {
        GTEST_SKIP() << "Google Gemini model is not available for v1/generateContent in this environment. "
                     << "Full response: " << responseRaw.toStdString();
    }

    // Verify response is not empty
    ASSERT_FALSE(response.isEmpty()) << "Response should not be empty";

    // Verify usage tokens (should be > 0 for successful request)
    const int totalTokens = output.value(QStringLiteral("_usage.total_tokens")).toInt();
    EXPECT_GT(totalTokens, 0) << "Total tokens should be greater than 0";

    // Basic sanity check: response should mention Paris
    EXPECT_NE(response.indexOf(QStringLiteral("paris")), -1) 
        << "Response should mention 'Paris'. Response was: " << responseRaw.toStdString();

    delete node;
}

TEST(UniversalLLMNodeTest, OpenAIVisionIntegration)
{
    ensureApp();

    const QString apiKey = resolveApiKey(QStringLiteral("openai"));

    if (apiKey.isEmpty()) {
        GTEST_SKIP() << "No OpenAI API key provided. Set OPENAI_API_KEY environment variable or add to accounts.json.";
    }

    // Create a dummy image file
    const QString imagePath = createDummyImageFile();
    ASSERT_FALSE(imagePath.isEmpty()) << "Failed to create dummy image file";

    auto* node = new UniversalLLMNode();

    // Configure via loadState: provider "openai", vision-capable model "gpt-5.1"
    QJsonObject state;
    state.insert(QStringLiteral("provider"), QStringLiteral("openai"));
    state.insert(QStringLiteral("model"), QStringLiteral("gpt-5.1"));
    state.insert(QStringLiteral("systemPrompt"), QStringLiteral("You are a helpful assistant that analyzes images."));
    state.insert(QStringLiteral("temperature"), 0.7);
    node->loadState(state);

    // Prepare input with prompt and attachment
    DataPacket inputs;
    inputs.insert(QStringLiteral("prompt"), QStringLiteral("What color is this image?"));
    inputs.insert(QString::fromLatin1(UniversalLLMNode::kInputAttachmentId), imagePath);

    // Execute via V3 token API
    ExecutionToken token;
    token.data = inputs;
    TokenList tokens;
    tokens.push_back(std::move(token));

    const TokenList outTokens = node->execute(tokens);
    ASSERT_FALSE(outTokens.empty());
    const DataPacket output = outTokens.front().data;

    // Clean up the temporary image file
    QFile::remove(imagePath);

    // Check for error
    if (output.contains(QStringLiteral("__error"))) {
        const QString error = output.value(QStringLiteral("__error")).toString();
        if (isTemporaryError(error)) {
            GTEST_SKIP() << "Temporary LLM error: " << error.toStdString();
        }
        FAIL() << "LLM request failed with error: " << error.toStdString();
    }

    // Verify response
    const QString response = output.value(QStringLiteral("response")).toString();
    ASSERT_FALSE(response.isEmpty()) << "Response should not be empty";

    // Verify usage tokens (should be > 0 for successful request)
    const int totalTokens = output.value(QStringLiteral("_usage.total_tokens")).toInt();
    EXPECT_GT(totalTokens, 0) << "Total tokens should be greater than 0";

    // Basic sanity check: response should mention red (the image is a red square)
    const QString responseLower = response.toLower();
    EXPECT_NE(responseLower.indexOf(QStringLiteral("red")), -1) 
        << "Response should mention 'red'. Response was: " << response.toStdString();

    delete node;
}

TEST(UniversalLLMNodeTest, OpenAIPdfRejection)
{
    ensureApp();

    auto* node = new UniversalLLMNode();

    // Configure via loadState: provider "openai", model "gpt-5.1"
    QJsonObject state;
    state.insert(QStringLiteral("provider"), QStringLiteral("openai"));
    state.insert(QStringLiteral("model"), QStringLiteral("gpt-5.1"));
    node->loadState(state);

    // Create a dummy PDF file (or just a path that looks like one)
    // The node will read it and then the backend should reject it.
    QTemporaryFile tempFile(QDir::tempPath() + "/test_XXXXXX.pdf");
    ASSERT_TRUE(tempFile.open());
    tempFile.write("%PDF-1.4 dummy");
    QString pdfPath = tempFile.fileName();
    tempFile.close();

    // Prepare input with prompt and PDF attachment
    DataPacket inputs;
    inputs.insert(QStringLiteral("prompt"), QStringLiteral("Summarize this."));
    inputs.insert(QString::fromLatin1(UniversalLLMNode::kInputAttachmentId), pdfPath);

    // Execute via V3 token API
    ExecutionToken token;
    token.data = inputs;
    TokenList tokens;
    tokens.push_back(std::move(token));

    const TokenList outTokens = node->execute(tokens);
    ASSERT_FALSE(outTokens.empty());
    const DataPacket output = outTokens.front().data;

    // Verify that an error was reported by the OpenAI backend
    ASSERT_TRUE(output.contains(QStringLiteral("__error")));
    EXPECT_TRUE(output.value(QStringLiteral("__error")).toString().contains(QStringLiteral("OpenAI backend does not support native PDF input.")));

    delete node;
}

TEST(UniversalLLMNodeTest, GoogleVisionIntegration)
{
    ensureApp();

    const QString apiKey = resolveApiKey(QStringLiteral("google"));

    if (apiKey.isEmpty()) {
        GTEST_SKIP() << "No Google API key provided. Set GOOGLE_API_KEY/GOOGLE_GENAI_API_KEY (or add accounts.json).";
    }

    // Create a dummy image file
    const QString imagePath = createDummyImageFile();
    ASSERT_FALSE(imagePath.isEmpty()) << "Failed to create dummy image file";

    auto* node = new UniversalLLMNode();

    // Configure via loadState: provider "google", vision-capable model "gemini-2.5-flash"
    QJsonObject state;
    state.insert(QStringLiteral("provider"), QStringLiteral("google"));
    state.insert(QStringLiteral("model"), QStringLiteral("gemini-2.5-flash"));
    state.insert(QStringLiteral("systemPrompt"), QStringLiteral("You are a helpful assistant that analyzes images."));
    state.insert(QStringLiteral("temperature"), 0.7);
    node->loadState(state);

    // Prepare input with prompt and attachment
    DataPacket inputs;
    inputs.insert(QStringLiteral("prompt"), QStringLiteral("What color is this image?"));
    inputs.insert(QString::fromLatin1(UniversalLLMNode::kInputAttachmentId), imagePath);

    // Execute via V3 token API
    ExecutionToken token;
    token.data = inputs;
    TokenList tokens;
    tokens.push_back(std::move(token));

    const TokenList outTokens = node->execute(tokens);
    ASSERT_FALSE(outTokens.empty());
    const DataPacket output = outTokens.front().data;

    // Clean up the temporary image file
    QFile::remove(imagePath);

    // Check for error
    if (output.contains(QStringLiteral("__error"))) {
        const QString error = output.value(QStringLiteral("__error")).toString();
        if (isTemporaryError(error)) {
            GTEST_SKIP() << "Temporary LLM error: " << error.toStdString();
        }

        // Also check responseRaw for model availability issues (Google specific)
        const QString responseRaw = output.value(QStringLiteral("response")).toString();
        if (responseRaw.contains(QStringLiteral("is not found for api version"), Qt::CaseInsensitive) ||
            responseRaw.contains(QStringLiteral("is not supported"), Qt::CaseInsensitive)) {
            GTEST_SKIP() << "Google Gemini model is not available in this environment. "
                         << "Full response: " << responseRaw.toStdString();
        }

        FAIL() << "LLM request failed with error: " << error.toStdString();
    }

    // Verify response
    const QString responseRaw = output.value(QStringLiteral("response")).toString();
    const QString response = responseRaw.toLower();
    
    // Check for model availability issues (environment-specific)
    if (response.contains(QStringLiteral("is not found for api version")) ||
        response.contains(QStringLiteral("is not supported"))) {
        GTEST_SKIP() << "Google Gemini model is not available in this environment. "
                     << "Full response: " << responseRaw.toStdString();
    }

    // Verify response is not empty
    ASSERT_FALSE(response.isEmpty()) << "Response should not be empty";

    // Verify usage tokens (should be > 0 for successful request)
    const int totalTokens = output.value(QStringLiteral("_usage.total_tokens")).toInt();
    EXPECT_GT(totalTokens, 0) << "Total tokens should be greater than 0";

    // Basic sanity check: response should mention red (the image is a red square)
    EXPECT_NE(response.indexOf(QStringLiteral("red")), -1) 
        << "Response should mention 'red'. Response was: " << responseRaw.toStdString();

    delete node;
}

TEST(UniversalLLMNodeTest, MissingImageFileError)
{
    ensureApp();

    // For this test, we don't need real credentials since we're testing error handling
    // The backend will fail on file read before making any API call
    // However, we still need to configure valid provider/model to reach the file check
    
    auto* node = new UniversalLLMNode();

    // Configure via loadState: use OpenAI as provider (could be any provider)
    QJsonObject state;
    state.insert(QStringLiteral("provider"), QStringLiteral("openai"));
    state.insert(QStringLiteral("model"), QStringLiteral("gpt-5.1"));
    state.insert(QStringLiteral("systemPrompt"), QStringLiteral("You are a helpful assistant."));
    state.insert(QStringLiteral("temperature"), 0.7);
    node->loadState(state);

    // Prepare input with a non-existent attachment file path
    const QString nonExistentPath = QStringLiteral("this_file_does_not_exist_12345.png");
    DataPacket inputs;
    inputs.insert(QStringLiteral("prompt"), QStringLiteral("What color is this image?"));
    inputs.insert(QString::fromLatin1(UniversalLLMNode::kInputAttachmentId), nonExistentPath);

    // Execute via V3 token API
    ExecutionToken token;
    token.data = inputs;
    TokenList tokens;
    tokens.push_back(std::move(token));

    const TokenList outTokens = node->execute(tokens);
    ASSERT_FALSE(outTokens.empty());
    const DataPacket output = outTokens.front().data;

    // Verify that an error was reported
    ASSERT_TRUE(output.contains(QStringLiteral("__error"))) 
        << "Output should contain __error key when image file is missing";

    const QString error = output.value(QStringLiteral("__error")).toString();
    ASSERT_FALSE(error.isEmpty()) << "Error message should not be empty";

    // Verify the error message mentions the file issue
    const QString errorLower = error.toLower();
    EXPECT_TRUE(errorLower.contains(QStringLiteral("failed")) || 
                errorLower.contains(QStringLiteral("error")) ||
                errorLower.contains(QStringLiteral("read")) ||
                errorLower.contains(QStringLiteral("open")))
        << "Error message should indicate file read/open failure. Error was: " << error.toStdString();

    // The response field should also contain error information
    const QString response = output.value(QStringLiteral("response")).toString();
    EXPECT_FALSE(response.isEmpty()) << "Response should contain error information";

    delete node;
}

class MockErrorBackend : public ILLMBackend {
public:
    QString id() const override { return QStringLiteral("fallback_test_provider"); }
    QString name() const override { return QStringLiteral("Mock Error Backend"); }
    QStringList availableModels() const override { return {QStringLiteral("model1")}; }
    QStringList availableEmbeddingModels() const override { return {}; }
    QFuture<QStringList> fetchModelList() override {
        return QtConcurrent::run([](){ return QStringList{QStringLiteral("model1")}; });
    }
    LLMResult sendPrompt(const QString&, const QString&, double, int, const QString&, const QString&, const LLMMessage& = {}) override {
        LLMResult res;
        res.hasError = true;
        res.errorMsg = QStringLiteral("Simulated API Error");
        return res;
    }
    EmbeddingResult getEmbedding(const QString&, const QString&, const QString&) override { return {}; }
    QFuture<QString> generateImage(const QString&, const QString&, const QString&, const QString&, const QString&, const QString&) override { 
        return QFuture<QString>(); 
    }
};

TEST(UniversalLLMNodeTest, FallbackMechanism) {
    // Register mock backend
    auto mockBackend = std::make_shared<MockErrorBackend>();
    LLMProviderRegistry::instance().registerBackend(mockBackend);
    LLMProviderRegistry::instance().setAnthropicKey(QStringLiteral("dummy_key"));

    UniversalLLMNode node;
    // We need to use "anthropic" as provider id to bypass getCredential check easily
    class AnthropicMockErrorBackend : public MockErrorBackend {
    public:
        QString id() const override { return QStringLiteral("anthropic"); }
    };
    auto anthropicMock = std::make_shared<AnthropicMockErrorBackend>();
    LLMProviderRegistry::instance().registerBackend(anthropicMock);

    node.onProviderChanged(QStringLiteral("anthropic"));
    node.onModelChanged(QStringLiteral("model1"));
    node.setEnableFallback(true);
    node.setFallbackString(QStringLiteral("FALLBACK_VALUE"));

    TokenList inputs;
    ExecutionToken token;
    token.data.insert(QStringLiteral("prompt"), QStringLiteral("Hello"));
    inputs.push_back(token);

    TokenList outputs = node.execute(inputs);
    ASSERT_EQ(outputs.size(), 1);
    
    QVariant response = outputs.front().data.value(QStringLiteral("response"));
    EXPECT_EQ(response.toString(), QStringLiteral("FALLBACK_VALUE"));
    
    // Check that __error is NOT present
    EXPECT_FALSE(outputs.front().data.contains(QStringLiteral("__error")));
    
    // Test with fallback DISABLED
    node.setEnableFallback(false);
    outputs = node.execute(inputs);
    ASSERT_EQ(outputs.size(), 1);
    EXPECT_TRUE(outputs.front().data.contains(QStringLiteral("__error")));
    EXPECT_EQ(outputs.front().data.value(QStringLiteral("__error")).toString(), QStringLiteral("Simulated API Error"));

    // Clear test key to avoid poisoning subsequent tests
    LLMProviderRegistry::instance().setAnthropicKey(QString());
}

TEST(UniversalLLMNodeTest, FallbackSaveLoadPersistence) {
    UniversalLLMNode node;
    node.setEnableFallback(true);
    node.setFallbackString(QStringLiteral("CUSTOM_FAIL"));
    
    QJsonObject state = node.saveState();
    
    UniversalLLMNode node2;
    node2.loadState(state);
    
    EXPECT_TRUE(node2.getEnableFallback());
    EXPECT_EQ(node2.getFallbackString(), QStringLiteral("CUSTOM_FAIL"));
}
