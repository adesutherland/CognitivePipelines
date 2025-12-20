# Cognitive Pipelines

## Project Overview
Cognitive Pipelines is a Qt 6 desktop application for composing and running node‑based “cognitive” workflows. The UI provides a data‑flow canvas (via QtNodes) where each node represents a tool (e.g., a Large Language Model call or text utility). A Properties panel lets you configure the selected node, and an ExecutionEngine runs the graph and displays results in a Pipeline Output dock.

## Current Features
- Cross‑platform Qt 6 Widgets application
- Data‑flow canvas powered by QtNodes (paceholder/nodeeditor)
- Nodes/tools provided out of the box:
  - TextInputNode: emits user‑provided text
  - PromptBuilderNode: formats a template using upstream text
  - LLMConnector: calls an OpenAI‑compatible Chat Completions endpoint (via cpr)
- Properties panel for configuring the selected node (e.g., LLM prompt, model, temperature, tokens)
- Run action that executes the graph in topological order; work is performed asynchronously per node via QtConcurrent
- Pipeline Output dock and optional Debug Log dock
- About dialog (version, git hash, build date/time, Qt runtime)
- Optional GoogleTest‑based test target (unit_tests) for API integration testing (disabled by default)

## Dependencies (definitive)
From CMakeLists.txt and vcpkg manifest:
- Qt 6: Core, Gui, Widgets, Network, Concurrent
- QtNodes (fetched via CMake FetchContent)
- Boost (headers)
- cpr (HTTP/HTTPS client)
- OpenSSL (TLS, transitive for cpr)
- Zlib (transitive)
- GoogleTest (tests only, optional when ENABLE_TESTING=ON)

Installation methods by environment:
- CI (GitHub Actions):
  - Qt installed via jurplel/install-qt-action@v4
  - Third‑party libraries resolved via vcpkg with NuGet‑based binary caching to GitHub Packages
  - Ninja generator on Unix; Visual Studio generator on Windows
- macOS (local dev):
  - Homebrew: brew install qt cpr googletest (tests optional)
  - Or use vcpkg with this repo’s vcpkg.json manifest
- Linux (local dev):
  - Use your distro packages for Qt 6/cpr where available, or vcpkg
  - Ensure common build tools (pkg-config, ninja, etc.) are installed
- Windows (local dev):
  - vcpkg recommended; pass the toolchain file during CMake configure

## How to Build (local)
Prerequisites:
- C++17 compiler and CMake 3.21+
- Qt 6 and listed third‑party dependencies available via your environment

Generic configure/build:
- cmake -S . -B build
- cmake --build build --target CognitivePipelines -j 2

Using vcpkg (recommended on Windows or cross‑platform):
- cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
- cmake --build build --target CognitivePipelines -j 2

Enable tests (optional):
- cmake -S . -B build -DENABLE_TESTING=ON [ -DCMAKE_TOOLCHAIN_FILE=... ]
- cmake --build build --target unit_tests
- ctest --test-dir build -V

## Configuration
Runtime:
- Configure the LLMConnector’s API Key and Prompt via the Properties panel.

Tests and app credentials resolution:
- Order: 1) OPENAI_API_KEY environment variable; 2) accounts.json in the standard per-user app config directory.
- accounts.json.example is provided as a template; copy it to the canonical location below and set your key.

Canonical accounts.json location (see LLMConnector::defaultAccountsFilePath):
- macOS: ~/Library/Application Support/CognitivePipelines/accounts.json
- Linux: ~/.config/CognitivePipelines/accounts.json
- Windows: %APPDATA%/CognitivePipelines/accounts.json (e.g., C:\\Users\\<You>\\AppData\\Roaming\\CognitivePipelines\\accounts.json)

accounts.json structure:
```
{
  "accounts": [
    {
      "name": "default_openai",
      "api_key": "YOUR_API_KEY_HERE"
    }
  ]
}
```

Notes:
- The application and tests resolve credentials in this order: OPENAI_API_KEY, then accounts.json in the standard per-user app config directory. If neither is available, networked tests SKIP and the LLMConnector reports a clear error.

## CI/CD
- Matrix builds on Ubuntu, macOS, and Windows
- Qt via jurplel/install-qt-action; third‑party dependencies via vcpkg (with NuGet binary caching)
- Ninja on Unix; MSVC/Visual Studio on Windows
- Release build target: CognitivePipelines

## Third-Party Libraries
- MermaidJS (MIT License): https://github.com/mermaid-js/mermaid

## License
MIT License. See LICENSE for details.
