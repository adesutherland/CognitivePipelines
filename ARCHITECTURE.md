# ARCHITECTURE

## Component Breakdown
- MainWindow (src/mainwindow.h/.cpp)
  - Primary application window that sets up menus, toolbar, status bar, and the dataflow canvas view.
  - Hosts actions such as "Interactive Prompt..." and a toolbar "Run" button.
  - Wires the "Run" button to ExecutionEngine, which executes the currently present LLM Connector node.
- PromptDialog (src/PromptDialog.h/.cpp)
  - Modal dialog that reads the API key from accounts.json and lets the user send arbitrary prompts.
  - Displays the full LLM response text.
  - Uses LlmApiClient for the network call.
- AboutDialog (src/about_dialog.h/.cpp)
  - Shows application name, version, git hash, build date, and Qt runtime.
- LlmApiClient (src/llm_api_client.h/.cpp)
  - Minimal HTTP client built on cpr to call an OpenAI-compatible Chat Completions endpoint.
  - Constructs JSON body manually and performs a synchronous POST.
  - Performs basic status/error checks and extracts the first choices[0].message.content from the response via a lightweight string search.
- QtNodes integration
  - NodeGraphModel (src/NodeGraphModel.h/.cpp): Subclass of QtNodes::DataFlowGraphModel, registers available node models.
  - ToolNodeDelegate (src/ToolNodeDelegate.h/.cpp): Adapter that maps an IToolConnector into a QtNodes NodeDelegateModel (ports, data types, execution triggering, and data propagation).
- Connectors (Tools)
  - IToolConnector (include/IToolConnector.h): Abstract interface for pipeline tools; defines node descriptor, configuration UI, and async Execute API.
  - LLMConnector (src/LLMConnector.h/.cpp): Concrete connector implementing a single-input (Prompt) to single-output (Response) node that calls LlmApiClient off the main thread via QtConcurrent.
  - PythonScriptConnector (src/PythonScriptConnector.h/.cpp): Placeholder connector for running external Python scripts (stubbed for now).
- Shared Types
  - CommonDataTypes (include/CommonDataTypes.h): Pin, node descriptor, and data packet typedefs shared across connectors and delegates.

## Data Flow
Primary user interactions:
1) Toolbar Run button
- User clicks Run in MainWindow
- ExecutionEngine searches the graph for an LLM Connector node (via NodeGraphModel)
- The connector executes asynchronously (QtConcurrent) using its configured Prompt and API Key
- When finished, the response text is shown in a message box

2) Interactive Prompt dialog
- User opens Tools -> "Interactive Prompt..." from MainWindow
- PromptDialog locates and loads accounts.json and populates a read-only API key field
- User enters a prompt and clicks Send
- PromptDialog calls LlmApiClient::sendPrompt and shows the response in a QTextEdit

3) Dataflow canvas (foundations in place)
- NodeGraphModel registers ToolNodeDelegate-wrapped connectors
- ToolNodeDelegate maps high-level PinDefinitions to QtNodes ports
- When inputs arrive, ToolNodeDelegate triggers connector->Execute asynchronously (QtConcurrent) and emits dataUpdated for downstream nodes when finished

Notes:
- The LLMConnector executes work off the UI thread for graph-driven execution.
- The Interactive Prompt dialog currently performs a synchronous network call and can block the UI; consider moving it off the GUI thread and adding a busy indicator.

## Build System & CI/CD
- Build System: CMake (minimum 3.21), C++17 standard, AUTOMOC/AUTOUIC/AUTORCC enabled for Qt
- Dependencies (from CMakeLists.txt):
  - Qt6::Core, Qt6::Gui, Qt6::Widgets, Qt6::Network, Qt6::Concurrent
  - QtNodes::QtNodes (via FetchContent of paceholder/nodeeditor)
  - Boost::boost (headers)
  - cpr::cpr
  - ZLIB, OpenSSL (explicit/transitive finds)
- Tests (optional): GoogleTest discovered with find_package(GTest) when ENABLE_TESTING=ON; tests registered with CTest

CI/CD (GitHub Actions):
- Matrix on ubuntu-latest, macos-latest, windows-latest
- Qt installed via jurplel/install-qt-action
- vcpkg used for dependency resolution with NuGet-based binary caching to GitHub Packages
- Ninja generator on Unix; Visual Studio generator on Windows
- Release build target: CognitivePipelines

## Configuration Management
- accounts.json (ignored by Git) provides one or more API key entries using the structure:
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
- PromptDialog loads the first account’s api_key for interactive prompts.
- The test suite can use OPENAI_API_KEY from the environment; the runtime graph execution uses the API key set in the LLM Connector’s Properties panel.
