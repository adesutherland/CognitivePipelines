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
#include "llm_api_client.h"

#include <cpr/cpr.h>
#include <sstream>

namespace {
// naive string search helper
static size_t find_after(const std::string& s, const std::string& needle, size_t pos = 0) {
    auto p = s.find(needle, pos);
    return p == std::string::npos ? std::string::npos : p + needle.size();
}
}

std::string LlmApiClient::sendPrompt(const std::string& apiKey, const std::string& promptText) {
    // Endpoint: OpenAI-compatible chat completions
    const std::string url = "https://api.openai.com/v1/chat/completions";

    // Construct minimal JSON payload as a string
    // Note: we escape embedded quotes in promptText very simply. For robust escaping, use a JSON library.
    std::string escapedPrompt;
    escapedPrompt.reserve(promptText.size() + 16);
    for (char c : promptText) {
        if (c == '"') escapedPrompt += "\\\"";
        else if (c == '\\') escapedPrompt += "\\\\";
        else if (c == '\n') escapedPrompt += "\\n";
        else if (c == '\r') escapedPrompt += "\\r";
        else if (c == '\t') escapedPrompt += "\\t";
        else escapedPrompt += c;
    }

    std::ostringstream oss;
    oss << "{\n"
        << "  \"model\": \"gpt-4o-mini\",\n"
        << "  \"messages\": [\n"
        << "    { \"role\": \"user\", \"content\": \"" << escapedPrompt << "\" }\n"
        << "  ]\n"
        << "}";
    const std::string payload = oss.str();

    // Headers
    cpr::Header headers{
        {"Authorization", std::string("Bearer ") + apiKey},
        {"Content-Type", "application/json"}
    };

    // Perform POST request
    auto response = cpr::Post(cpr::Url{url}, headers, cpr::Body{payload}, cpr::Timeout{60000});

    if (response.error) {
        std::ostringstream err;
        err << "Network error: " << response.error.message;
        return err.str();
    }

    if (response.status_code != 200) {
        std::ostringstream err;
        err << "HTTP " << response.status_code << ": "
            << (response.text.size() > 512 ? response.text.substr(0, 512) + "..." : response.text);
        return err.str();
    }

    // Extract first choice.message.content from JSON body
    const std::string content = extractFirstMessageContent(response.text);
    if (content.empty()) {
        return "Failed to parse response: message content not found.";
    }
    return content;
}

std::string LlmApiClient::extractFirstMessageContent(const std::string& jsonBody) {
    // This is a very simplified extractor tailored to typical OpenAI responses.
    // It is not a full JSON parser.
    // We search in order: "choices" -> first object -> "message" -> "content"

    // Find "choices"
    size_t pos = jsonBody.find("\"choices\"");
    if (pos == std::string::npos) return {};

    // Find first opening bracket [ after choices
    pos = jsonBody.find('[', pos);
    if (pos == std::string::npos) return {};

    // Find first opening brace { after [
    pos = jsonBody.find('{', pos);
    if (pos == std::string::npos) return {};

    // Find "message"
    pos = jsonBody.find("\"message\"", pos);
    if (pos == std::string::npos) return {};

    // Find "content"
    pos = jsonBody.find("\"content\"", pos);
    if (pos == std::string::npos) return {};

    // Find colon then opening quote
    pos = jsonBody.find(':', pos);
    if (pos == std::string::npos) return {};

    // Skip whitespace
    while (pos < jsonBody.size() && (jsonBody[pos] == ':' || jsonBody[pos] == ' ' || jsonBody[pos] == '\t' || jsonBody[pos] == '\n' || jsonBody[pos] == '\r')) {
        ++pos;
    }
    if (pos >= jsonBody.size() || jsonBody[pos] != '"') return {};

    // Capture until next unescaped quote
    ++pos; // move past opening quote
    std::string value;
    bool escape = false;
    for (; pos < jsonBody.size(); ++pos) {
        char c = jsonBody[pos];
        if (escape) {
            switch (c) {
                case 'n': value.push_back('\n'); break;
                case 'r': value.push_back('\r'); break;
                case 't': value.push_back('\t'); break;
                case '"': value.push_back('"'); break;
                case '\\': value.push_back('\\'); break;
                default: value.push_back(c); break;
            }
            escape = false;
        } else if (c == '\\') {
            escape = true;
        } else if (c == '"') {
            break; // end of string
        } else {
            value.push_back(c);
        }
    }

    return value;
}
