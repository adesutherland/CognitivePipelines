### Universal Script Host: Static Registration Strategy

To ensure a robust and cross-platform plugin architecture without the complexities of dynamic library loading (especially on macOS), we will use a **Static Registration** pattern.

#### 1. Registry Mechanism
The `ScriptEngineRegistry` (defined in `IScriptHost.h`) acts as a singleton that holds a map of engine IDs to `ScriptEngineFactory` functions. This allows for late-binding of engine implementations while maintaining compile-time safety and avoiding global initialization order issues (Static Initialization Order Fiasco).

#### 2. Registration in `main.cpp`
We will register all available script engines at the very beginning of `main()`, before the `QApplication` starts and before the UI or execution engine are initialized.

```cpp
// main.cpp example integration
#include "IScriptHost.h"
#ifdef ENABLE_QUICKJS
#include "QuickJSEngine.h" // Concrete implementation
#endif

int main(int argc, char *argv[]) {
    // 1. Static Registration
#ifdef ENABLE_QUICKJS
    ScriptEngineRegistry::instance().registerEngine("quickjs", []() {
        return std::make_unique<QuickJSEngine>();
    });
#endif

    // Register other engines here (Python, Crexx, etc.)

    // 2. Standard Qt App Startup
    QApplication a(argc, argv);
    // ...
}
```

#### 3. Why this approach?
- **No dylib issues:** Avoids the "code signing" and "library path" headaches associated with `dlopen` or `QLibrary` on macOS.
- **Dependency Control:** We can use CMake options (e.g., `-DENABLE_QUICKJS=ON`) to include or exclude specific engines at compile time.
- **Decoupling:** The `UniversalScriptConnector` only needs to know about `IScriptEngine` and `ScriptEngineRegistry`. It doesn't need to know about `QuickJSEngine` or any other concrete implementation.
- **Extensibility:** Adding a new engine only requires implementing the `IScriptEngine` interface and adding one registration line in `main.cpp`.
