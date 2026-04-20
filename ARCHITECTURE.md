# ARCHITECTURE

## Component Breakdown

- `src/app/main.cpp`
  - Application entry point.
  - Sets app metadata, configures logging defaults, loads model capability metadata, and shows `MainWindow`.
- `src/app/MainWindow.h/.cpp`
  - Primary Qt Widgets shell for the application.
  - Owns the graph model/view, properties panel, stage output dock, debug log dock, run controls, file open/save flow, and credentials dialog entry point.
- `src/app/dialogs/`
  - `AboutDialog`, `CredentialsDialog`, and `UserInputDialog` provide supporting UI for application metadata, provider credentials, and blocking human-input requests.
- `src/graph/NodeGraphModel.h/.cpp`
  - Subclass of `QtNodes::DataFlowGraphModel`.
  - Registers the current node palette and categories, manages save/load integration with QtNodes, exposes entry points, and wires graph-level node signals.
  - Registers the bundled `QuickJSRuntime` with `ScriptEngineRegistry` during graph model construction so scripting nodes can resolve the default engine.
- `src/graph/ToolNodeDelegate.h/.cpp`
  - Adapter between `IToolNode` implementations and QtNodes `NodeDelegateModel`.
  - Maps `NodeDescriptor` pin metadata to QtNodes ports, owns node persistence for QtNodes save/load, and exposes the node configuration widget to the properties panel.
- `include/IToolNode.h`
  - Core execution interface implemented by all pipeline nodes.
  - Defines descriptor metadata, configuration widget creation, token-based execution, persistence hooks, and readiness rules.
- `include/CommonDataTypes.h`
  - Shared structural types such as `NodeDescriptor`, `PinDefinition`, and `DataPacket`.
- `include/ExecutionToken.h`
  - Event/token payload used by the execution engine to track source node, connection, triggering pin, and data payload.
- `src/execution/ExecutionEngine.h/.cpp`
  - Token-based scheduler for pipeline runs.
  - Owns the execution queue, thread pool, data lake, per-node serialization, run identity, and execution lifecycle signals used by the UI.
- `src/execution/ExecutionState*.h/.cpp`
  - Execution-state model and status enums used for node/connection highlighting in the UI.
- `src/ai/`
  - `backends/ILLMBackend.h` defines the provider abstraction for chat, embeddings, and image generation.
  - `registry/LLMProviderRegistry.*` registers and resolves OpenAI, Google, and Anthropic backends and loads credentials.
  - `capabilities/ModelCapsRegistry.*` and `ModelCaps.*` normalize model metadata and aliases loaded from resources.
- `src/retrieval/`
  - `documents/DocumentLoader.*` handles local document ingestion.
  - `chunking/` contains text and code chunkers used by RAG flows.
  - `storage/RagUtils.*` contains shared retrieval/database helpers.
- `src/scripting/`
  - `hosts/ExecutionScriptHost.*` bridges script execution to pipeline input/output and logging.
  - `bridges/ScriptDatabaseBridge.*` exposes database functionality to script runtimes.
  - `runtimes/QuickJSRuntime.*` is the bundled default scripting engine.
- `src/nodes/`
  - Concrete node implementations grouped by domain: `ai`, `control_flow`, `external_tools`, `io`, `retrieval`, `scripting`, `text`, and `visualization`.
  - Representative nodes include `UniversalLLMNode`, `ImageGenNode`, `RagIndexerNode`, `RagQueryNode`, `UniversalScriptNode`, `ProcessNode`, `PythonScriptNode`, `PromptBuilderNode`, and the control-flow nodes.
- `src/logging/`
  - Central logging helpers and categorized logging declarations used across the app.

## Execution Flow

1. Graph authoring
   - `NodeGraphModel` registers the available node types and categories with QtNodes.
   - Each QtNodes model is backed by a `ToolNodeDelegate`, which wraps an `IToolNode` implementation.
   - Selecting a node in `MainWindow` shows the node's configuration widget in the properties panel.

2. Pipeline start
   - `MainWindow` starts execution through `ExecutionEngine::runPipeline()`, optionally from selected entry points.
   - The engine resets run state, assigns a new run id, discovers source nodes when needed, and schedules initial tasks.

3. Node execution
   - Scheduled work runs through the engine's `QThreadPool`.
   - Each task calls the wrapped node's `execute(const TokenList&)` implementation and records outputs into the engine's thread-safe data lake.
   - Nodes can emit one or many output tokens, which enables fan-out and control-flow behavior without relying on direct reactive propagation from QtNodes.

4. UI updates
   - `ExecutionEngine` emits `nodeStatusChanged`, `connectionStatusChanged`, `nodeOutputChanged`, `nodeLog`, and `pipelineFinished`.
   - `MainWindow` uses these signals to refresh stage output, debug logging, and live execution highlighting.

## AI, Retrieval, and Scripting Layers

- AI execution is routed through `ILLMBackend` implementations rather than a single monolithic API client.
- `LLMProviderRegistry` registers the built-in OpenAI, Google, and Anthropic backends and resolves credentials per provider id.
- `UniversalLLMNode` uses the backend registry and `ModelCapsRegistry` to pick models, send prompts, and handle multimodal requests.
- `ImageGenNode` uses the same provider abstraction for image generation.
- Retrieval nodes build on `DocumentLoader`, chunkers, and `RagUtils` to populate and query a local RAG database.
- `UniversalScriptNode` requests a runtime from `ScriptEngineRegistry`, then executes the script through `ExecutionScriptHost`.
- `QuickJSRuntime` is the bundled default runtime; additional runtimes can be added by registering more `IScriptEngine` factories.

## Build System and Dependencies

- Build system: CMake 3.21+, C++17
- Main target: `CognitivePipelines`
- Test targets:
  - `unit_tests`
  - `integration_tests`
- Qt modules required by `CMakeLists.txt`:
  - `Core`, `Gui`, `Widgets`, `Network`, `Concurrent`, `Test`
  - `Sql`, `Pdf`, `WebChannel`, `Positioning`, `WebEngineWidgets`, `DBus`
- Other dependencies:
  - QtNodes via `FetchContent` (`paceholder/nodeeditor`, tag `3.0.12`)
  - bundled QuickJS under `third_party/quickjs`
  - Boost headers
  - `cpr`
  - OpenSSL
  - Zlib
  - GoogleTest

## Configuration and Credentials

- Provider credentials are resolved by provider id.
- Environment variables are checked first:
  - `OPENAI_API_KEY`
  - `GOOGLE_API_KEY`, `GOOGLE_GENAI_API_KEY`, `GOOGLE_AI_API_KEY`
  - `ANTHROPIC_API_KEY`
- Canonical `accounts.json` locations:
  - macOS: `~/Library/Application Support/CognitivePipelines/accounts.json`
  - Linux: `~/.config/CognitivePipelines/accounts.json`
  - Windows: `%APPDATA%/CognitivePipelines/accounts.json`
- For local development and tests, the code also checks fallback paths including:
  - the current working directory
  - the application directory
  - parent directories near the built binary
- Provider names expected in `accounts.json`:
  - `openai`
  - `google`
  - `anthropic`

Example:

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
    }
  ]
}
```
