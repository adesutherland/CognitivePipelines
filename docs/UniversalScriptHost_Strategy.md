### Universal Script Host: Static Registration Strategy

The current scripting architecture uses a static registration pattern for script runtimes. This keeps runtime discovery simple and cross-platform while avoiding the deployment and code-signing complexity of dynamic plugin loading.

#### 1. Registry Mechanism

`ScriptEngineRegistry` in `include/IScriptHost.h` is a singleton that maps engine ids to `ScriptEngineFactory` callables. `UniversalScriptNode` only depends on the `IScriptEngine` interface and asks the registry to create an engine instance by id at execution time.

#### 2. Current Registration Point

The current code registers the bundled QuickJS runtime inside `NodeGraphModel::NodeGraphModel()`:

```cpp
ScriptEngineRegistry::instance().registerEngine(QStringLiteral("quickjs"), []() {
    return std::make_unique<QuickJSRuntime>();
});
```

This means the default engine is available as soon as the graph model is created, before any `UniversalScriptNode` is configured or executed.

Relevant classes:

- `src/graph/NodeGraphModel.*`
- `src/nodes/scripting/universal_script/UniversalScriptNode.*`
- `src/scripting/hosts/ExecutionScriptHost.*`
- `src/scripting/runtimes/QuickJSRuntime.*`

#### 3. Why this approach still fits

- **No dynamic-loading overhead:** avoids `dlopen`/`QLibrary` deployment issues, especially on macOS.
- **Clear runtime boundary:** `UniversalScriptNode` only knows about `IScriptEngine` and `ScriptEngineRegistry`.
- **Easy extension path:** adding a new engine still only requires an `IScriptEngine` implementation plus one registration call.
- **Deterministic startup:** runtime availability is tied to normal application initialization rather than filesystem plugin discovery.

#### 4. Follow-on Note

If script runtime registration later moves out of `NodeGraphModel` into a dedicated bootstrap step, the static registration pattern should stay the same. Only the initialization location would change.
