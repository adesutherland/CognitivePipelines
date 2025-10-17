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
