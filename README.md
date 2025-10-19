# Cognitive Pipelines

## Project Overview
Cognitive Pipelines is a Qt 6 desktop application for composing and running node-based “cognitive pipelines.” Each node represents a tool, such as a Large Language Model (LLM) call or a future Python/utility step. The current iteration focuses on a minimal, working path from the UI through an LLM connector to a live OpenAI-compatible API and back to the UI.

## Current Features
- Qt 6 Widgets application with a dataflow canvas (QtNodes) and a main toolbar
- About dialog showing version, git commit hash, build date/time, and Qt runtime
- LLM integration via a lightweight C++ client (cpr) calling an OpenAI-compatible Chat Completions endpoint
- Interactive Prompt dialog that:
  - Loads an API key from accounts.json
  - Accepts user text input
  - Sends a request to the LLM and displays the response
- Properties panel for nodes; an LLM Connector node exposes editable Prompt and API Key fields
- Toolbar “Run” button triggers the ExecutionEngine which finds the LLM Connector node and executes it asynchronously
- Optional GoogleTest-based test target (run_tests) for API integration testing (disabled by default)

## Dependencies
Definitive dependency list derived from CMakeLists.txt:
- Qt 6: Core, Gui, Widgets, Network, Concurrent
- QtNodes (paceholder/nodeeditor) fetched via CMake FetchContent
- Boost (headers only)
- cpr (HTTP/HTTPS client)
- OpenSSL (TLS, transitive for cpr)
- Zlib (transitive)
- GoogleTest (tests only, optional when ENABLE_TESTING=ON)

Installation methods by environment:
- macOS (local dev):
  - Homebrew for system packages (e.g., qt, cpr, googletest if testing)
- Linux (local dev):
  - apt or your distro’s package manager for Qt and cpr, etc.
- Windows (local dev):
  - vcpkg with the CMake toolchain file (see vcpkg.json manifest in this repo)

## How to Build
Prerequisites:
- C++17 compiler and CMake 3.21+
- Qt 6 and third-party deps available via your platform’s package manager

Configure and build (replace <build_dir> as needed):
- cmake -S . -B <build_dir>
- cmake --build <build_dir> --target CognitivePipelines -j 2

Windows note: pass the vcpkg toolchain file at configure time if using vcpkg:
- -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\\scripts\\buildsystems\\vcpkg.cmake"

Enable tests (optional):
- cmake -S . -B <build_dir> -DENABLE_TESTING=ON
- Ensure GoogleTest is available (Homebrew or vcpkg)
- Build tests: cmake --build <build_dir> --target run_tests
- Run: ctest --test-dir <build_dir> -V

## Configuration
The app can use an API key from either a local accounts.json or environment (tests):
- Local file: accounts.json in the project root (should not be committed; use accounts.json.example as a template)
- Environment variable: OPENAI_API_KEY (primarily used by tests or external scripts)

Create accounts.json from the example:
- cp accounts.json.example accounts.json
- Edit accounts.json and set your key

Structure:
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

Behavior:
- Interactive Prompt dialog loads the API key from accounts.json and sends requests synchronously
- The toolbar Run button executes the LLM Connector node via the ExecutionEngine (async); configure its Prompt and API Key in the Properties panel
- The test suite looks for OPENAI_API_KEY or accounts.json and skips if none is available

## CI/CD
- GitHub Actions builds on Ubuntu, macOS, and Windows
- Qt provided by jurplel/install-qt-action
- Dependencies resolved through vcpkg with NuGet-based binary caching to GitHub Packages
- Ninja generator on Unix; MSVC generator on Windows

## License
MIT License. See LICENSE for details.
