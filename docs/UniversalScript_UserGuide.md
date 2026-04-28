# Universal Script Node User Guide

The Universal Script node runs small scripts inside the pipeline. The bundled runtime is QuickJS, and the node is designed so future runtimes can share the same host API.

## Inputs and Outputs

Incoming tokens are merged into a single input packet before the script runs. Later tokens with the same key overwrite earlier values.

Use `pipeline.getInput(name)` to read a value:

```javascript
const text = pipeline.getInput("in");
```

Use `pipeline.setOutput(name, value)` to emit a value:

```javascript
pipeline.setOutput("out", text.toUpperCase());
pipeline.setOutput("status", "OK");
```

JavaScript strings, numbers, booleans, arrays, and plain objects are converted into Qt variants. Arrays become `QVariantList`; plain objects become `QVariantMap`.

## Failure Handling

Use `pipeline.setError(message)` when the script detects an operational failure:

```javascript
const value = pipeline.getInput("required");
if (!value) {
  pipeline.setError("Missing required input: required");
  return;
}
```

This sets the standard `__error` output and marks `status` as `FAIL`. Throwing an exception also fails the node and includes the QuickJS error and stack trace in `__error`.

## Logging

Use `console.log`, `print`, or `console.error` for messages shown in the node logs:

```javascript
console.log("Starting transform");
console.error("Recoverable warning");
```

`console.error` writes a log line prefixed with `ERROR:`. It does not fail the node by itself; call `pipeline.setError(...)` for that.

## Fan-out

When fan-out is enabled and the script writes an array to `out`, the node emits one token per array item. Other output fields, including `logs` and `status`, are copied to each token.

```javascript
pipeline.setOutput("out", ["alpha", "beta", "gamma"]);
```

For structured fan-out, emit objects:

```javascript
pipeline.setOutput("out", [
  { id: 1, text: "first" },
  { id: 2, text: "second" }
]);
```

## Temporary Files

Use `pipeline.getTempDir()` for scratch files:

```javascript
const path = pipeline.getTempDir() + "/script-output.txt";
pipeline.setOutput("path", path);
```

The script host provides the path only. File access depends on the runtime modules available in the selected engine.

## SQLite

The bundled QuickJS runtime exposes a small SQLite bridge:

```javascript
const dbPath = pipeline.getTempDir() + "/example.db";
if (!sqlite.connect(dbPath)) {
  pipeline.setError("Could not open database");
  return;
}

sqlite.exec("CREATE TABLE IF NOT EXISTS items (name TEXT, score INTEGER)");
sqlite.exec("INSERT INTO items (name, score) VALUES (?, ?)", ["alpha", 42]);

const rows = sqlite.exec("SELECT name, score FROM items WHERE score > ?", [10]);
pipeline.setOutput("rows", rows);
```

`sqlite.exec` returns an array of objects for result sets. Use parameter arrays for values rather than building SQL strings.

## Structured Data Guidance

Inside a pipeline, prefer real arrays and objects over JSON strings:

```javascript
pipeline.setOutput("metadata", {
  source: "notes.md",
  line_start: 12,
  line_end: 18
});
```

Serialize to JSON only when crossing an external boundary, such as writing a file, making an HTTP request, or preparing display text for a system that only accepts strings.

## Runtime Boundary

The current engine id is `quickjs`. Future CREXX integration should preserve the host API above so script examples can move between runtimes where possible. CREXX-specific facilities should be added as runtime-specific helpers while keeping `pipeline.getInput`, `pipeline.setOutput`, `pipeline.setError`, logging, and status behavior consistent.

