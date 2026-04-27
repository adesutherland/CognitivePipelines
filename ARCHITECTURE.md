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
  - `registry/LLMProviderRegistry.*` registers and resolves OpenAI, Google, Anthropic, and optional Ollama backends and loads credentials.
  - `capabilities/ModelCapsRegistry.*` and `ModelCaps.*` load provider settings, virtual model aliases, regex capability rules, and driver profiles from the model catalog.
  - `catalog/ModelCatalogService.*` is the UI-facing model-selection layer. It combines registered backends, credentials, provider settings, dynamic provider model discovery, aliases, capability filtering, hidden-model diagnostics, and lightweight provider/model test calls.
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
- `LLMProviderRegistry` registers the built-in OpenAI, Google, Anthropic, and Ollama backends and resolves credentials per provider id. Ollama registration can be disabled with `CP_DISABLE_OLLAMA=1` for CI or headless environments without a local daemon.
- `ModelCapsRegistry` is the rule engine for model behavior. Rules map model-id regexes to capabilities, role modes, parameter constraints, and driver profiles. Broad provider-specific rules can set `requires_backend` so they only match when a provider has already been selected.
- `ModelCatalogService` is the selection facade used by properties widgets. It chooses usable providers, fetches dynamic model lists where available, groups models into recommended/available/hidden sections, exposes hidden filter reasons, filters by required capability, and runs small chat/embedding test calls for selected provider/model pairs.
- `UniversalLLMNode` uses the backend registry, `ModelCatalogService`, and `ModelCapsRegistry` to pick models, send prompts, adapt request parameters, and handle multimodal requests.
- `ImageGenNode` uses capability-filtered image model selection through the same catalog and provider abstraction.
- Retrieval nodes build on `DocumentLoader`, chunkers, and `RagUtils` to populate and query a local RAG database. RAG indexing and querying now use catalog-selected embedding providers/models; OpenAI and Ollama embedding drivers are implemented.
- `UniversalScriptNode` requests a runtime from `ScriptEngineRegistry`, then executes the script through `ExecutionScriptHost`.
- `QuickJSRuntime` is the bundled default runtime; additional runtimes can be added by registering more `IScriptEngine` factories.

## Model Catalog and Driver Mapping

- The shipped catalog lives at `resources/model_caps.json`.
- Application startup calls `ModelCapsRegistry::loadFromFileWithUserOverrides(":/resources/model_caps.json")`.
- User override files are merged by `id` from:
  - macOS: `~/Library/Application Support/CognitivePipelines/model_catalog.json`
  - Linux/Windows: the Qt generic config location under `CognitivePipelines/model_catalog.json`
  - current working directory: `model_catalog.json`
- Catalog arrays are merged by `id`; an override entry with `"disabled": true` removes a shipped `provider`, `driver_profile`, `virtual_model`, or `rule`.
- `providers` configure backend visibility, display names, local base URLs, whether credentials are required, optional API keys, and custom headers.
- `driver_profiles` name protocol families such as OpenAI chat completions, OpenAI assistants, OpenAI embeddings, OpenAI images, Anthropic messages, Google generate-content, Ollama chat, and Ollama embeddings.
- `rules` are regex mappings from concrete provider model IDs to:
  - capability flags such as `chat`, `vision`, `reasoning`, `longcontext`, `embedding`, `image`, and `pdf`
  - role mode and parameter constraints
  - a `driver` profile id
  - optional `requires_backend` gating for broad local/provider-specific patterns
- `virtual_models` provide curated aliases such as flagship, reasoning, coding, cost-optimized, and high-throughput choices. Aliases resolve to concrete provider model IDs before capability matching.
- Properties widgets currently expose the catalog through provider/model selectors, `Show filtered models`, and `Test Selection`. Full provider-management UI is the next natural layer: it should edit the same catalog concepts without requiring the user to hand-edit JSON.

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
  - `OLLAMA_BASE_URL`
  - `OLLAMA_API_KEY`
  - `CP_DISABLE_OLLAMA`
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
  - `ollama` (optional; only needed for proxied/hosted Ollama endpoints that require a key)

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
    },
    {
      "name": "ollama",
      "api_key": "OPTIONAL_OLLAMA_PROXY_KEY"
    }
  ]
}
```

Model/provider behavior that is not secret material belongs in `model_catalog.json`, not `accounts.json`. See `docs/model_catalog_config.md` for examples.

## Near-Term Architecture Work

- Add a Provider Management dialog backed by the same catalog structures:
  - enable/disable providers
  - edit Ollama/local gateway base URL, optional headers, and optional key use
  - inspect discovered models and filtered-out models with reasons
  - edit or add virtual aliases, regex rules, capabilities, and driver mappings
  - run provider/model test calls from the management UI
- Keep execution nodes consuming `ModelCatalogService` and `ModelCapsRegistry`; the management UI should write catalog overrides rather than adding node-specific configuration paths.
