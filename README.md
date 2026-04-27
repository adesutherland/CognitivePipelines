# Cognitive Pipelines

Cognitive Pipelines is a Qt 6 desktop application for composing and running node-based workflows. The project combines a Qt Widgets UI, a QtNodes canvas, asynchronous graph execution, local scripting, external process integration, retrieval tooling, and multi-provider LLM backends.

## Current Capabilities

- Qt 6 Widgets desktop application with a node canvas, properties panel, stage output, and debug log
- Save/load pipeline support from the main window
- Asynchronous graph execution via `ExecutionEngine` and `QtConcurrent`
- Built-in node categories:
  - `Input / Output`: text, ingest input, image, PDF-to-image, human input, text output, vault output
  - `Text Utilities`: prompt building
  - `Visualization`: Mermaid rendering
  - `External Tools`: process execution and Python script execution
  - `Scripting`: universal scripting via the bundled QuickJS runtime
  - `AI Services`: universal LLM node and image generation
  - `Persistence` / `Retrieval`: database, RAG indexing, and RAG querying
  - `Control Flow`: conditional router, loop, loop-until, retry loop
- LLM backends currently registered in code:
  - OpenAI
  - Google
  - Anthropic
  - Ollama
- Test targets:
  - `unit_tests` (GoogleTest)
  - `integration_tests` (Qt Test / CTest)

## Capture Workflows

- `Ingest Input` is a capture-first entry node for quick intake. It accepts file selection, file drop, and clipboard paste, classifies the payload, and immediately runs the downstream graph when used inside the main application window.
- The node exposes explicit typed outputs for `markdown`, `text`, `image`, and `pdf`, plus `file_path`, `mime_type`, and `kind` metadata so downstream routing can stay simple.
- `Vault Output` is a markdown writer for knowledge-vault workflows. It sends the incoming markdown, the current vault folder shape, and a routing prompt to the selected LLM backend, then writes the note as `.md` into the chosen subfolder.
- A typical capture pipeline is now `Ingest Input -> route by typed pin -> processing -> Vault Output`.

## Dependencies

Definitive dependencies come from [`CMakeLists.txt`](./CMakeLists.txt) and the source tree:

- CMake 3.21+
- C++17 compiler
- Qt 6 components:
  - `Core`
  - `Gui`
  - `Widgets`
  - `Network`
  - `Concurrent`
  - `Test`
  - `Sql`
  - `Pdf`
  - `WebChannel`
  - `Positioning`
  - `WebEngineWidgets`
  - `DBus`
- QtNodes via CMake `FetchContent` (`paceholder/nodeeditor`, tag `3.0.12`)
- Bundled QuickJS sources under `third_party/quickjs`
- Boost headers
- `cpr`
- OpenSSL
- Zlib
- GoogleTest when `ENABLE_TESTING=ON` (default)

## macOS First-Time Setup

For Apple Silicon development on macOS:

1. Install Xcode Command Line Tools if needed:

```bash
xcode-select --install
```

2. Install the local development dependencies:

```bash
brew install qt cpr boost googletest ninja pkgconf ripgrep
```

3. Optional but useful:

```bash
brew install python
```

Notes:

- The Python node defaults to `python3 -u`. `/usr/bin/python3` is usually enough on macOS, so Homebrew Python is optional unless you want a newer interpreter.
- Homebrew `qt` is usually sufficient for local development. If configure fails because a required Qt module is unavailable on your machine, install a matching Qt 6 release with at least `qtpdf`, `qtwebengine`, `qtwebchannel`, and `qtpositioning`, then point `CMAKE_PREFIX_PATH` at that Qt installation.

## Build

### Homebrew-based local build on Apple Silicon

Use `Debug` first. On macOS, `Release` enables additional signing and deployment logic that is not necessary for the initial development loop.

```bash
cmake -S . -B build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DDEPLOY_QT_ON_BUILD=OFF

cmake --build build --target CognitivePipelines -j 8
```

### Build with vcpkg

This project uses vcpkg for the non-Qt C++ dependencies in CI. A local macOS configure looks like:

```bash
cmake -S . -B build \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DVCPKG_TARGET_TRIPLET=arm64-osx \
  -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt \
  -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DDEPLOY_QT_ON_BUILD=OFF

cmake --build build --target CognitivePipelines -j 8
```

### Tests

`ENABLE_TESTING` defaults to `ON`.

Build the test targets:

```bash
cmake --build build --target unit_tests integration_tests -j 8
```

Run them with CTest:

```bash
ctest --test-dir build -V
```

Disable tests only if you need a lighter configure:

```bash
cmake -S . -B build -DENABLE_TESTING=OFF
```

## CLion Configuration

Recommended CLion setup for macOS Apple Silicon:

- Toolchain:
  - C compiler: `/usr/bin/clang`
  - C++ compiler: `/usr/bin/clang++`
  - CMake: bundled CLion CMake or `/opt/homebrew/bin/cmake`
  - Build tool / generator: Ninja (`/opt/homebrew/bin/ninja`)
  - Debugger: LLDB
- CMake profile:
  - Use a `Debug` profile first
  - Use an arm64 build directory such as `cmake-build-debug-arm64`, or reuse the active CLion generation directory already associated with the project
  - Recommended CMake options:

```text
-DCMAKE_OSX_ARCHITECTURES=arm64
-DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
-DDEPLOY_QT_ON_BUILD=OFF
```

If you are using vcpkg in CLion, add:

```text
-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
-DVCPKG_TARGET_TRIPLET=arm64-osx
```

## Credentials and Runtime Configuration

The application supports provider credentials through environment variables and `accounts.json`.

OpenAI authentication in this application is API-key based. ChatGPT or Codex account sign-in is not used to authenticate requests from the desktop app; configure `OPENAI_API_KEY` or an `accounts.json` entry instead.

Environment variables checked first:

- OpenAI: `OPENAI_API_KEY`
- Google: `GOOGLE_API_KEY`, `GOOGLE_GENAI_API_KEY`, `GOOGLE_AI_API_KEY`
- Anthropic: `ANTHROPIC_API_KEY`
- Ollama: `OLLAMA_BASE_URL` for the local server URL and optional `OLLAMA_API_KEY` for proxied/hosted endpoints
- CI/headless runs can set `CP_DISABLE_OLLAMA=1` to avoid registering the local Ollama backend when no daemon is available.

Canonical `accounts.json` location:

- macOS: `~/Library/Application Support/CognitivePipelines/accounts.json`
- Linux: `~/.config/CognitivePipelines/accounts.json`
- Windows: `%APPDATA%/CognitivePipelines/accounts.json`

The GUI exposes `Edit -> Edit Credentials...`, which writes credentials to the canonical location above.

When resolving credentials, the code also checks a few local-development fallback paths after the canonical location, including:

- the current working directory
- the application directory
- parent directories near the application binary

Example `accounts.json`:

```json
{
  "accounts": [
    {
      "name": "openai",
      "api_key": "YOUR_OPENAI_KEY"
    },
    {
      "name": "google",
      "api_key": "YOUR_GOOGLE_KEY"
    },
    {
      "name": "anthropic",
      "api_key": "YOUR_ANTHROPIC_KEY"
    },
    {
      "name": "ollama",
      "api_key": "OPTIONAL_OLLAMA_PROXY_KEY"
    }
  ]
}
```

Provider visibility, Ollama host/port, model alias regex rules, driver profiles, and capability filters can also be overridden with a local model catalog file. See [`docs/model_catalog_config.md`](docs/model_catalog_config.md).

Networked tests skip when the required credentials are not available.

## CI/CD

GitHub Actions currently builds on Ubuntu, macOS, and Windows.

- Qt is installed separately in CI
- vcpkg is used for the C++ package dependencies
- Ninja is used on Unix-like runners
- Visual Studio is used on Windows

## License

MIT License. See [`LICENSE`](./LICENSE) for details.
