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
- Toolbar “Run” button that sends a predefined prompt, using the OPENAI_API_KEY environment variable
- Optional GoogleTest-based test target (run_tests) for API integration testing (disabled by default)

## Dependencies
Definitive dependency list derived from CMakeLists.txt and the CI workflow:
- Qt 6: Core, Gui, Widgets, Network, Concurrent (installed via install-qt-action in CI; via system package manager for local dev)
- QtNodes (paceholder/nodeeditor) fetched via CMake FetchContent
- Boost (headers only)
- cpr (HTTP/HTTPS client)
- OpenSSL (TLS, transitive for cpr)
- Zlib (transitive)
- GoogleTest (tests only, optional when ENABLE_TESTING=ON)

Installation methods by environment:
- CI (GitHub Actions):
  - Qt installed via jurplel/install-qt-action
  - C/C++ libraries resolved via vcpkg with NuGet-based binary caching; manifest is vcpkg.json
- macOS (local dev):
  - Homebrew for system packages (e.g., qt, cpr, googletest if testing)
- Linux (local dev):
  - apt for Qt and cpr, etc.
- Windows (local dev):
  - vcpkg with the CMake toolchain file

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
The app expects an API key provided either via environment or a local accounts.json.
- Environment variable: OPENAI_API_KEY
- Local file: accounts.json in the project root (ignored by Git)

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
- The Interactive Prompt dialog loads the API key from accounts.json
- The toolbar Run button reads the key from OPENAI_API_KEY

## CI/CD
- GitHub Actions builds on Ubuntu, macOS, and Windows
- Qt provided by jurplel/install-qt-action
- Dependencies resolved through vcpkg with NuGet-based binary caching to GitHub Packages
- Ninja generator on Unix; MSVC generator on Windows

## License
MIT License. See LICENSE for details.
