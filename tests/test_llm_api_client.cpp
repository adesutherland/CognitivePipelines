//
// Cognitive Pipeline Application
//
// Copyright (c) 2025 Adrian Sutherland
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//

#include <gtest/gtest.h>
#include "llm_api_client.h"
#include "LLMConnector.h"

#include <cstdlib>
#include <string>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return s;
}

static bool fileExists(const std::string& path) {
    std::ifstream f(path);
    return f.good();
}

static std::string readFile(const std::string& path) {
    std::ifstream in(path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static std::string tryExtractApiKeyFromJson(const std::string& json) {
    // Very naive extraction: find "api_key" : "..."
    auto keyPos = json.find("\"api_key\"");
    if (keyPos == std::string::npos) return {};
    keyPos = json.find(':', keyPos);
    if (keyPos == std::string::npos) return {};
    // move to first quote
    while (keyPos < json.size() && json[keyPos] != '"') ++keyPos;
    if (keyPos >= json.size()) return {};
    ++keyPos; // after opening quote
    std::string val;
    bool esc = false;
    for (; keyPos < json.size(); ++keyPos) {
        char c = json[keyPos];
        if (esc) { val.push_back(c); esc = false; continue; }
        if (c == '\\') { esc = true; continue; }
        if (c == '"') break;
        val.push_back(c);
    }
    return val;
}

static std::string findApiKeyFromAccountsJson() {
    const std::vector<std::string> candidates = {
        "accounts.json",
        "../accounts.json",
        "../../accounts.json",
        "../../../accounts.json",
        "../../../../accounts.json"
    };
    for (const auto& p : candidates) {
        if (fileExists(p)) {
            auto json = readFile(p);
            auto k = tryExtractApiKeyFromJson(json);
            if (!k.empty()) return k;
        }
    }
    return {};
}

TEST(LlmApiClientIntegrationTest, ShouldReceiveValidResponseForSimplePrompt) {
    std::string apiKey;
    if (const char* keyEnv = std::getenv("OPENAI_API_KEY")) {
        apiKey = keyEnv;
    }
    if (apiKey.empty()) {
        // Use the same resolver as the application: single canonical accounts.json path
        LlmApiClient client;
        const QString key = client.getApiKey(QStringLiteral("openai"));
        apiKey = key.toStdString();
    }
    if (apiKey.empty()) {
        const QString canonicalPath = LLMConnector::defaultAccountsFilePath();
        qWarning() << "No API key available. The canonical accounts.json path checked would be:" << canonicalPath;
        GTEST_SKIP() << "No API key provided. Set OPENAI_API_KEY or add accounts.json at: " << canonicalPath.toStdString();
    }

    LlmApiClient client;
    const std::string prompt = "Briefly, what is the capital of France?";

    const std::string response = client.sendPrompt(apiKey, prompt);
    const std::string lower = toLower(response);

    // Acceptance criteria: not an error and mentions Paris (case-insensitive)
    // These substrings correspond to error messages from the implementation.
    EXPECT_EQ(lower.find("network error"), std::string::npos) << "Got network error: " << response;
    EXPECT_EQ(lower.find("http "), std::string::npos) << "Got HTTP error: " << response;
    EXPECT_EQ(lower.find("failed to parse response"), std::string::npos) << "Got parsing error: " << response;

    EXPECT_NE(lower.find("paris"), std::string::npos) << "Response did not contain expected keyword 'Paris'. Full response: " << response;
}
