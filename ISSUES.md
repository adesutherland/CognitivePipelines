# ISSUES (Non-Critical Findings and Housekeeping)

This file tracks minor issues discovered during the latest housekeeping pass. Items here are not blockers but should be triaged and addressed in future iterations.

## Resolved Since Last Pass
- Source file header standardization: All .h and .cpp files use the canonical header from src/main.cpp.
- Documentation alignment: Removed outdated references to an Interactive Prompt dialog; docs now reflect the current LLM Connector + Properties panel flow.

## New Findings
- Hand-rolled JSON in LlmApiClient:
  - Payload construction manually escapes a subset of characters.
  - Response parsing uses string scanning to extract choices[0].message.content.
  - Recommendation: adopt a JSON library (e.g., nlohmann/json or Qt JSON) for robustness and maintainability.
- Hardcoded model and endpoint in LlmApiClient:
  - Model name (gpt-4o-mini) and base URL are hardcoded.
  - Recommendation: make them configurable via file or UI preferences.
- Error handling returns plain strings:
  - sendPrompt returns human-readable error text in-band.
  - Recommendation: return a structured result (status + payload + error details) to enable better UI behavior and testing.
- Graph cycle handling:
  - ExecutionEngine builds a DAG and performs a topological ordering but does not explicitly detect/report cycles.
  - Recommendation: add cycle detection and a user-visible error if the graph isn’t acyclic.
- Tests are optional and not exercised in CI:
  - CI builds the app but does not run tests; lack of secret prevents live API test.
  - Recommendation: add a CI job that runs tests when OPENAI_API_KEY is provided as a secret, or introduce a mock/test double.
- Code hygiene automation:
  - No clang-format or clang-tidy configuration is present.
  - Recommendation: add formatting/lint configs and a CI check or pre-commit hook.
- Commented/unused helpers:
  - A small static helper (find_after) in llm_api_client.cpp is defined but currently unused.
  - Recommendation: remove or consistently use it.

## Notes for Next Pass
- Reassess dependency versions (QtNodes, cpr, Boost) and update docs if CMake changes.
- Review UI/UX around API key entry in the LLM Connector properties; consider secure storage or masking options if needed.
