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
#pragma once

#include <string>

// A simple LLM API client using cpr to send a hardcoded chat completion request.
// This class sends a prompt to a standard chat completion endpoint and returns
// the first message content from the response, or an error description string on failure.
class LlmApiClient {
public:
    // Sends the given promptText to the LLM using the provided apiKey.
    // Returns the extracted assistant message content on success, or a human-readable
    // error message string if the HTTP call fails or the response is malformed.
    std::string sendPrompt(const std::string& apiKey, const std::string& promptText);

private:
    // Very small helper to extract the first choice.message.content from a JSON string
    // without introducing a full JSON dependency. This is a simplified extractor and
    // should be replaced with a robust JSON parser for production use.
    static std::string extractFirstMessageContent(const std::string& jsonBody);
};
