# ARCHITECTURE

## Component Breakdown
- main.cpp
  - Entry point; creates QApplication and shows MainWindow.
- MainWindow (src/mainwindow.h/.cpp)
  - Primary application window: menus, toolbar, status bar, docks, and the data‑flow canvas view.
  - Hosts a Run action wired to ExecutionEngine; shows Pipeline Output and optional Debug Log docks.
  - Manages the Properties panel for configuring the selected node.
- AboutDialog (src/about_dialog.h/.cpp)
  - Shows application name, version, git hash, build date, and Qt runtime.
- LlmApiClient (src/llm_api_client.h/.cpp)
  - Lightweight HTTP client (cpr) that calls an OpenAI‑compatible Chat Completions endpoint.
  - Builds the JSON payload as a string and performs a POST; extracts the first choices[0].message.content via a simple string scan.
- QtNodes integration
  - NodeGraphModel (src/NodeGraphModel.h/.cpp): Subclass of QtNodes::DataFlowGraphModel; registers available node models.
  - ToolNodeDelegate (src/ToolNodeDelegate.h/.cpp): Adapter that maps an IToolConnector into a QtNodes NodeDelegateModel (ports, data types, execution triggering, and data propagation).
- Connectors (Tools)
  - IToolConnector (include/IToolConnector.h): Abstract interface for pipeline tools; defines node descriptor, configuration UI, and async Execute API.
  - LLMConnector (src/LLMConnector.h/.cpp): Single‑input (Prompt) to single‑output (Response) connector; performs work off the main thread via QtConcurrent using LlmApiClient.
  - LLMConnectorPropertiesWidget (src/LLMConnectorPropertiesWidget.h/.cpp): Properties editor for model, prompt, temperature, and tokens.
  - PromptBuilderNode (src/PromptBuilderNode.h/.cpp): Template‑based text transform; has a corresponding PromptBuilderPropertiesWidget.
  - TextInputNode (src/TextInputNode.h/.cpp): Emits user text; has a TextInputPropertiesWidget.
  - PythonScriptConnector (src/PythonScriptConnector.h/.cpp): Placeholder for a future external script tool.
- Shared Types
  - CommonDataTypes (include/CommonDataTypes.h): Pin, node descriptor, and data packet typedefs shared across connectors and delegates.
- Orchestration
  - ExecutionEngine (src/ExecutionEngine.h/.cpp): Builds an adjacency representation from the graph (QtNodes connections), computes a topological order (Kahn’s algorithm), triggers node execution, aggregates outputs, and emits pipelineFinished and nodeLog signals.

## Data Flow (High‑Level)
1) Graph authoring
- User places nodes on the canvas and connects outputs to inputs.
- Selecting a node shows its configuration UI in the Properties panel.

2) Execution
- User invokes the Run action from the toolbar.
- ExecutionEngine constructs a DAG from the current connections and computes a topological order.
- For each node in order, ToolNodeDelegate triggers the connector’s async Execute(inputs) via QtConcurrent and propagates outputs downstream.
- When the graph finishes, ExecutionEngine emits pipelineFinished, and MainWindow displays results in the Pipeline Output dock; Debug Log shows per‑node messages when enabled.

Notes:
- Work is moved off the UI thread for connector execution.
- Cycle handling is not explicitly reported; see ISSUES.md for improvements.

## Build System & CI/CD
- Build System: CMake (>= 3.21), C++17, AUTOMOC/AUTOUIC/AUTORCC enabled for Qt
- Dependencies (from CMakeLists.txt):
  - Qt6::Core, Qt6::Gui, Qt6::Widgets, Qt6::Network, Qt6::Concurrent
  - QtNodes::QtNodes (via FetchContent of paceholder/nodeeditor)
  - Boost::boost (headers)
  - cpr::cpr
  - ZLIB, OpenSSL (explicit/transitive finds)
- Tests (optional): GoogleTest discovered with find_package(GTest) when ENABLE_TESTING=ON; tests registered with CTest

CI/CD (GitHub Actions):
- Matrix on ubuntu-latest, macos-latest, windows-latest
- Qt via jurplel/install-qt-action; third‑party deps via vcpkg with NuGet binary caching to GitHub Packages
- Ninja generator on Unix; Visual Studio generator on Windows
- Release build target: CognitivePipelines

## Configuration Management
- Runtime: API key and prompt are set via the LLMConnector Properties panel.
- Credentials resolution (app and tests): 1) OPENAI_API_KEY environment variable; 2) accounts.json at the canonical per-user location (see LLMConnector::defaultAccountsFilePath).

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
- accounts.json.example is tracked by Git and serves as a template.
