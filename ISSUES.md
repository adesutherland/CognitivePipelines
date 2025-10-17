# ISSUES (Non-Critical Findings and Housekeeping)

This file tracks minor issues discovered during the latest housekeeping pass. Items here are not blockers but should be triaged and addressed in future iterations.

## Resolved Since Last Pass
- No prior ISSUES.md was present; this is the initial list.

## New Findings
- LlmApiClient uses a naive, hand-rolled JSON serializer/parser:
  - Payload construction manually escapes a subset of characters.
  - Response parsing uses string scanning to extract choices[0].message.content.
  - Recommendation: adopt a proper JSON library (e.g., nlohmann/json or Qt JSON) for robustness and maintainability.
- Synchronous network calls on the UI thread:
  - MainWindow Run button and PromptDialog execute sendPrompt synchronously, which can freeze the UI.
  - Recommendation: move calls off the GUI thread (QThread/QThreadPool/QtConcurrent) and add a progress indicator.
- Hardcoded model and endpoint in LlmApiClient:
  - Model name (gpt-4o-mini) and base URL are hardcoded.
  - Recommendation: make them configurable via file or UI preferences.
- Error handling returns plain strings:
  - sendPrompt returns human-readable error text in-band.
  - Recommendation: return a structured result (status + payload + error details) to enable better UI behavior and testing.
- Configuration duplication:
  - Accounts.json lookup logic exists in both tests and PromptDialog; environment variable usage exists in tests/MainWindow.
  - Recommendation: centralize configuration/key-loading utilities to avoid drift.
- Tests are optional and not exercised in CI:
  - CI builds the app but does not run tests; lack of secret prevents live API test.
  - Recommendation: add a separate CI job that runs tests when OPENAI_API_KEY is provided as a secret, or add a mock/test double.
- Security/UX for API key display:
  - PromptDialog shows the API key in a read-only text field.
  - Recommendation: mask the value or provide a reveal/hide toggle.
- Documentation consistency:
  - README updated; ensure future changes to dependencies (e.g., Qt modules) are reflected in both README and CMake.
- vcpkg manifest minimal set:
  - vcpkg.json lists boost and cpr only; Qt is supplied by the Qt action in CI.
  - Recommendation: keep manifest aligned with local dev workflows or document the split clearly.
- Commented/unused helpers:
  - A small static helper (find_after) in llm_api_client.cpp is defined but currently unused.
  - Recommendation: remove unused helpers or use them consistently.
