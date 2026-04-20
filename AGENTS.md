# AGENTS.md

This file is the source of truth for repository-specific agent guidance in `CognitivePipelines`. Keep it aligned with `README.md` when build, test, dependency, or credential behavior changes.

## Working Conventions

- Inspect the current code and build files before changing documentation or setup guidance.
- Prefer the active CLion CMake profile and its existing generation directory. Do not create ad hoc build directories unless the user asks for that explicitly.
- Prefer target-specific builds during iteration:
  - `cmake --build <build-dir> --target CognitivePipelines`
  - `cmake --build <build-dir> --target unit_tests`
- On macOS, start with a `Debug` profile. `Release` enables additional signing and deployment logic in `CMakeLists.txt`.
- When behavior changes, update the docs that users and agents will actually read:
  - `README.md`
  - `AGENTS.md`
  - inline build comments if they become misleading

## Build and Dependency Facts

- Build system: CMake 3.21+, C++17
- Main app target: `CognitivePipelines`
- Test targets:
  - `unit_tests` (GoogleTest)
  - `integration_tests` (Qt Test / CTest)
- Qt dependencies required by CMake:
  - `Core`, `Gui`, `Widgets`, `Network`, `Concurrent`, `Test`
  - `Sql`, `Pdf`, `WebChannel`, `Positioning`, `WebEngineWidgets`, `DBus`
- Additional dependencies:
  - QtNodes via `FetchContent` (`paceholder/nodeeditor`, tag `3.0.12`)
  - bundled QuickJS under `src/3rdparty/quickjs`
  - Boost headers
  - `cpr`
  - OpenSSL
  - Zlib
  - GoogleTest when `ENABLE_TESTING=ON`

For Apple Silicon macOS local development, the expected Homebrew toolchain is:

```bash
brew install qt cpr boost googletest ninja pkgconf ripgrep
```

Optional:

```bash
brew install python
```

The Python script node defaults to `python3 -u`, so `/usr/bin/python3` is usually enough for local runs.

## CLion Guidance

Recommended CLion toolchain on macOS Apple Silicon:

- C compiler: `/usr/bin/clang`
- C++ compiler: `/usr/bin/clang++`
- CMake: bundled CLion CMake or `/opt/homebrew/bin/cmake`
- Generator/build tool: Ninja via `/opt/homebrew/bin/ninja`
- Debugger: LLDB

Recommended CMake options for a local Debug profile:

```text
-DCMAKE_OSX_ARCHITECTURES=arm64
-DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
-DDEPLOY_QT_ON_BUILD=OFF
```

If using vcpkg in CLion, add:

```text
-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
-DVCPKG_TARGET_TRIPLET=arm64-osx
```

## Testing Guidance

- `ENABLE_TESTING` defaults to `ON`.
- Prefer building only the affected target while iterating.
- Use CTest from the existing build directory:

```bash
ctest --test-dir <build-dir> -V
```

- Networked tests are expected to skip when credentials are absent.
- When adding tests:
  - put new test files under `tests/`
  - list them explicitly in `CMakeLists.txt`
  - avoid introducing new broad globbing patterns
  - keep tests deterministic unless the purpose is explicitly integration coverage

## Credentials

Credentials are resolved by provider id.

Environment variables checked first:

- `OPENAI_API_KEY`
- `GOOGLE_API_KEY`, `GOOGLE_GENAI_API_KEY`, `GOOGLE_AI_API_KEY`
- `ANTHROPIC_API_KEY`

Canonical `accounts.json` location:

- macOS: `~/Library/Application Support/CognitivePipelines/accounts.json`
- Linux: `~/.config/CognitivePipelines/accounts.json`
- Windows: `%APPDATA%/CognitivePipelines/accounts.json`

The GUI exposes `Edit -> Edit Credentials...` and saves to the canonical location. For local runs and tests, the code also checks fallback paths such as the current working directory and locations near the built application.

Provider names expected in `accounts.json`:

- `openai`
- `google`
- `anthropic`

## Codebase Notes

- `NodeGraphModel` registers the current node set and category layout. Prefer reading it before updating capability docs.
- `ExecutionEngine` owns pipeline execution order and asynchronous fan-out behavior.
- `ToolNodeDelegate` is the adapter between QtNodes and `IToolConnector` implementations.
- QuickJS is the bundled default scripting runtime registered in `NodeGraphModel`.

## Documentation Ownership

- `README.md` is the main human-facing setup document.
- `AGENTS.md` is the main repo-specific agent instruction file.
- `.junie/guidelines.md` should remain a stub that points here, not a second source of truth.
