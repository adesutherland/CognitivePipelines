CognitivePipelines – Advanced Dev Notes (Junie)
Last updated: 2025-10-20 21:39 local

This document captures project-specific practices for building, configuring, and testing this repository in our CLion workspace, plus additional notes that help with day-to-day development and debugging. It intentionally avoids generic CMake/Qt basics.


1) Build and Configuration (project-specific)

- Build system: CMake ≥ 3.21, C++17. Qt’s AUTOMOC/AUTOUIC/AUTORCC enabled.
- Primary executable target: CognitivePipelines (Qt Widgets app).
- Dependencies resolved at configure time:
  - Qt6::Core, Qt6::Gui, Qt6::Widgets, Qt6::Network, Qt6::Concurrent, Qt6::Test
  - QtNodes::QtNodes (paceholder/nodeeditor via FetchContent, tag 3.0.12)
  - Boost::boost (headers only)
  - cpr::cpr (HTTP/HTTPS), OpenSSL, Zlib
- Build metadata injected as compile definitions in the app:
  - APP_VERSION = CMake project version
  - GIT_COMMIT_HASH = git rev-parse --short HEAD (falls back to "unknown")

CLion workspace specifics (do this):
- Do not create new build directories. Use the active CLion CMake profile and its existing generation dir.
- Prefer target-specific builds (faster iterations):
  - Example (Debug profile):
    cmake --build /Users/adrian/CLionProjects/CognitivePipelines/cmake-build-debug --target CognitivePipelines

Cross-platform configure/build patterns (outside CLion, for reference):
- vcpkg toolchain:
  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
  cmake --build build --target CognitivePipelines -j
- macOS (Homebrew):
  brew install qt cpr googletest
  cmake -S . -B build
  cmake --build build --target CognitivePipelines -j

Runtime configuration relevant to the app:
- LLMConnector API key and prompt are configured via the Properties panel in the GUI. The app does NOT auto-read accounts.json.


2) Testing – how it actually works here

Overview:
- Tests are optional and gated by ENABLE_TESTING (OFF by default). In the CLion Debug profile in this workspace, tests are already enabled and targets exist: run_tests (GoogleTest) and integration_tests (Qt Test based).
- GoogleTest binary: run_tests. Current suite exercises:
  - LlmApiClient integration (real HTTPS call to an OpenAI-compatible Chat Completions endpoint). This test SKIPs or PASSES depending on credentials/environment.
  - Node property widgets and execution (QtConcurrent off-UI thread) via TextInputNode/PromptBuilderNode and the ExecutionEngine.

How to build and run tests in this CLion workspace:
- Use the existing Debug profile and prefer a single command combining build + run:
  cmake --build /Users/adrian/CLionProjects/CognitivePipelines/cmake-build-debug --target run_tests && \
    /Users/adrian/CLionProjects/CognitivePipelines/cmake-build-debug/run_tests

Credentials for the LlmApiClient integration test (two options):
- Environment variable: OPENAI_API_KEY
- Root-level file: accounts.json (template in accounts.json.example)
  Minimal structure:
  {
    "accounts": [ { "name": "default_openai", "api_key": "YOUR_API_KEY_HERE" } ]
  }
Notes:
- The tests can consume accounts.json or the env var. The GUI does not.
- On missing/invalid credentials, the integration test is designed to SKIP, not fail.

Adding a new test (what to touch):
- Place a new .cpp under tests/ and add it to the run_tests sources block in CMakeLists.txt inside if(ENABLE_TESTING).
- Keep tests deterministic and fast. Avoid QApplication unless necessary; if needed, create a minimal QApplication only within the test (see tests/test_nodes.cpp).
- Prefer unit tests that isolate logic and avoid network access. If a network call is unavoidable, implement graceful SKIP behavior when credentials are absent.
- Keep run_tests link surface minimal (only list the sources needed by the tests; do not GLOB).

Demonstration: a simple unit test we validated locally (temporary file, then removed)
- We added tests/test_prompt_builder_state.cpp with this content:

  // tests/test_prompt_builder_state.cpp
  #include <gtest/gtest.h>
  #include <QJsonObject>
  #include "PromptBuilderNode.h"
  
  TEST(PromptBuilderNodeStateTest, SaveLoadRoundTrip) {
      PromptBuilderNode nodeA;
      nodeA.setTemplateText(QStringLiteral("Hello {input}, number {input}!"));
  
      const QJsonObject saved = nodeA.saveState();
      PromptBuilderNode nodeB;
      nodeB.loadState(saved);
  
      EXPECT_EQ(nodeB.templateText(), QStringLiteral("Hello {input}, number {input}!"));
  }

- We temporarily listed it in CMake under add_executable(run_tests ...). Then we built and executed:
  cmake --build /Users/adrian/CLionProjects/CognitivePipelines/cmake-build-debug --target run_tests && \
    /Users/adrian/CLionProjects/CognitivePipelines/cmake-build-debug/run_tests

Observed output (condensed):
- With the temporary test present locally, we observed 5 tests passing. Example:

  [==========] Running 5 tests from 5 test suites.
  [ RUN      ] LlmApiClientIntegrationTest.ShouldReceiveValidResponseForSimplePrompt
  [       OK ] ... (~1.3 s)
  [ RUN      ] TextInputNodeTest.EmitsConfiguredTextViaExecute
  [       OK ] ...
  [ RUN      ] PromptBuilderNodeTest.FormatsTemplateWithInput
  [       OK ] ...
  [ RUN      ] ExecutionEngineTest.LinearTwoNodes_DataFlowsAndOrderIsCorrect
  [       OK ] ...
  [ RUN      ] PromptBuilderNodeStateTest.SaveLoadRoundTrip
  [       OK ] ...
  [  PASSED  ] 5 tests.

- After validation, we removed tests/test_prompt_builder_state.cpp and reverted the CMake change to keep the committed test surface minimal. This keeps the repository state lean while preserving this guideline as a working example you can reintroduce when needed.

Troubleshooting notes for tests:
- If HTTPS integration tests fail:
  - Verify OPENAI_API_KEY env or accounts.json.
  - Ensure the system trusts CA bundles (OpenSSL vs macOS Keychain). Homebrew OpenSSL paths may need to be discoverable by cpr.
  - Inspect run_tests output for messages prefixed by "Network error:" or "HTTP <code>:" which come from llm_api_client.
- If UI-related tests hang/crash:
  - Ensure a QApplication is created (see tests/test_nodes.cpp). Heavy work should stay off the UI thread; UI updates via signals.


3) Additional development information

- Data-flow orchestration: ExecutionEngine uses Kahn’s algorithm for topological ordering. Cycles are currently not surfaced to the user; see ISSUES.md for roadmap.
- Threading model: Per-node work runs via QtConcurrent; be mindful of QObject affinity. Emit signals from worker logic; let UI layer update on the main thread.
- Node integration: ToolNodeDelegate adapts IToolConnector implementations to the QtNodes graph. Ensure port ids/types align with include/CommonDataTypes.h when adding connectors.
- HTTP client (cpr): Default timeout is 60 s for LLM requests; consider backoff/cancellation in future connectors.
- Response parsing: LlmApiClient intentionally uses minimal string scanning for JSON extraction. If swapping to a real JSON library, update tests accordingly (preserve error message strings where practical).
- Build reproducibility: QtNodes is fetched via a pinned tag (3.0.12). Be cautious when upgrading; upstream API changes may ripple into ToolNodeDelegate and model code.
- Tests structure: ENABLE_TESTING gates all testing. The run_tests target lists specific sources to keep link times low. Prefer adding focused test .cpp files and explicitly listing them.
- Coding style: Modern C++17, RAII, nullptr, snake_case for files, PascalCase for Qt types. No repo-level clang-format/clang-tidy yet; keep style consistent.
- Version/About dialog: Verify APP_VERSION and GIT_COMMIT_HASH appear correctly after changes to CMake versioning.
- Secrets: Don’t commit real API keys. accounts.json.example exists as a template. The GUI relies on user-provided keys via Properties, not test credentials.


Appendix: Quick reference commands (CLion Debug profile)
- Build app target:
  cmake --build /Users/adrian/CLionProjects/CognitivePipelines/cmake-build-debug --target CognitivePipelines
- Build+run tests:
  cmake --build /Users/adrian/CLionProjects/CognitivePipelines/cmake-build-debug --target run_tests && \
    /Users/adrian/CLionProjects/CognitivePipelines/cmake-build-debug/run_tests
