# ARCHITECTURE

## Component Breakdown
- MainWindow (src/mainwindow.h/.cpp)
  - Primary application window and menus/toolbars.
  - Hosts actions like "Interactive Prompt..." and a toolbar "Run" button.
  - Uses LlmApiClient to send a predefined prompt when "Run" is clicked (reads API key from OPENAI_API_KEY).
- PromptDialog (src/PromptDialog.h/.cpp)
  - Modal dialog that reads API key from accounts.json and lets the user send arbitrary prompts.
  - Displays full LLM response text.
  - Uses LlmApiClient for the network call.
- AboutDialog (src/about_dialog.h/.cpp)
  - Simple dialog that shows application name, version, git hash, build date, and Qt runtime.
- LlmApiClient (src/llm_api_client.h/.cpp)
  - Minimal HTTP client built on cpr to call an OpenAI-compatible Chat Completions endpoint.
  - Constructs JSON body manually and does a synchronous POST.
  - Performs basic status/error checks and extracts the first choice.message.content from the JSON response using a lightweight string search.
- IToolConnector (src/IToolConnector.h)
  - Abstract interface for future pipeline tools with metadata and async execution API.
- PythonScriptConnector (src/PythonScriptConnector.h/.cpp)
  - A concrete IToolConnector that runs a Python script as a separate process.
  - Provides configuration UI and async execution contract (future pipeline integration).

## Data Flow
Primary user interactions:
1) Toolbar Run button
- User clicks Run in MainWindow.
- MainWindow reads OPENAI_API_KEY from environment.
- MainWindow calls LlmApiClient::sendPrompt with a predefined prompt.
- Response string is shown via QMessageBox::information.

2) Interactive Prompt dialog
- User opens Tools -> "Interactive Prompt..." from MainWindow.
- PromptDialog loads API key from accounts.json (first entry in accounts array) and populates a read-only field.
- User enters a prompt and clicks Send.
- PromptDialog calls LlmApiClient::sendPrompt.
- Full response is placed into a read-only QTextEdit.

Notes:
- Networking is synchronous in both paths; UI thread is blocked during request. This is acceptable for initial validation but should be moved off the UI thread for production.

## Build System & CI/CD
- Build System: CMake (minimum 3.21) targeting C++17.
- Qt: Qt 6 with AUTOMOC/AUTOUIC/AUTORCC enabled.
- Dependencies:
  - Qt6::Core, Qt6::Gui, Qt6::Widgets
  - Boost::boost (headers)
  - cpr::cpr
  - ZLIB, OpenSSL (transitive/explicit finds)
- Testing (optional): GoogleTest discovered via find_package(GTest) when ENABLE_TESTING=ON. Tests registered with CTest.

CI/CD (GitHub Actions):
- Matrix builds on ubuntu-latest, macos-latest, windows-latest.
- Qt installed via jurplel/install-qt-action.
- Third-party C/C++ libraries installed and cached via vcpkg with NuGet-based binary caching to GitHub Packages.
- Ninja generator used on Unix; MSVC generator on Windows.
- Release build target: CognitivePipelines.

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
- The integration test and toolbar Run flow can also use OPENAI_API_KEY from the environment.
