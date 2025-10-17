# Cognitive Pipelines

## Project Overview
Cognitive Pipelines is a cross-platform Qt desktop application for building and running “cognitive pipelines” — sequences of tools that can include Large Language Model (LLM) calls and other processors. The current focus is delivering a minimal, working end-to-end path from the UI to a live LLM API and back to the UI.

## Current Features
- Interactive UI built with Qt 6 (Widgets)
- About dialog showing version, git hash, and build info
- LLM integration via a simple C++ client (cpr) that calls an OpenAI-compatible Chat Completions endpoint
- Integration test using GoogleTest that verifies a live roundtrip to the LLM API
- Interactive Prompt dialog that:
  - Loads API key from accounts.json
  - Lets you type a prompt
  - Sends it to the LLM and displays the response
- Toolbar “Run” button that sends a predefined prompt using the OPENAI_API_KEY environment variable

## Dependencies
The project uses the following third-party libraries:
- Qt 6 (Core, Gui, Widgets)
- Boost (headers)
- cpr (HTTP/HTTPS requests)
- OpenSSL (TLS, transitively used by cpr)
- Zlib (transitive)
- GoogleTest (tests only)

Installation methods by environment:
- CI (GitHub Actions):
  - Qt installed via jurplel/install-qt-action
  - C/C++ libraries installed via vcpkg with binary caching to GitHub Packages (NuGet)
- macOS (local dev):
  - Homebrew for system packages (e.g., qt, googletest if testing)
- Linux (local dev):
  - apt packages for Qt and cpr, etc.
- Windows (local dev):
  - vcpkg with the CMake toolchain file

## How to Build
Prerequisites:
- A C++17 compiler and CMake 3.21+
- The dependencies above available via your platform’s package manager

Example commands:
- macOS (Homebrew):
  - brew install qt cpr googletest
- Ubuntu (apt):
  - sudo apt-get update && sudo apt-get install -y build-essential cmake qt6-base-dev libcpr-dev libssl-dev zlib1g-dev
- Windows (vcpkg):
  - vcpkg install boost:x64-windows cpr:x64-windows

Configure and build (replace <build_dir> if you use a custom build folder):
- cmake -S . -B <build_dir>
- cmake --build <build_dir> --target CognitivePipelines -j 2

Note for Windows: pass the vcpkg toolchain file at configure time:
- -DCMAKE_TOOLCHAIN_FILE="%VCPKG_ROOT%\\scripts\\buildsystems\\vcpkg.cmake"

## Testing
Testing is optional and gated by a CMake option.
- Enable tests at configure time: -DENABLE_TESTING=ON
- Install GoogleTest via your environment (Homebrew or vcpkg toolchain)
- Provide your OpenAI API key via environment:
  - export OPENAI_API_KEY="your_actual_api_key_here"
- Build and run tests:
  - cmake --build <build_dir> --target run_tests
  - ctest --test-dir <build_dir> -V

## Configuration
API keys are managed via a local accounts.json file that is ignored by Git.
- Copy the template and fill in your key:
  - cp accounts.json.example accounts.json
  - Edit accounts.json and replace YOUR_API_KEY_HERE
- Expected structure:
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
- The Interactive Prompt dialog reads the key from accounts.json. The toolbar Run button uses the OPENAI_API_KEY environment variable for convenience.

## CI/CD
- GitHub Actions builds on Windows, macOS, and Linux
- Qt installed via jurplel/install-qt-action; C++ dependencies via vcpkg with NuGet-based binary caching
- Release build uses Ninja (Unix) or MSVC (Windows) generators in CI

## License
This project is licensed under the MIT License. See the LICENSE file for details.
