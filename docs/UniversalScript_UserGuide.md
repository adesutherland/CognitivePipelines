# Universal Script Node User Guide

The Universal Script node runs small scripts inside the pipeline. QuickJS is always available. CREXX is offered when the application can find a usable CREXX runtime at configure/run time.

## General Model

The node starts with one input pin named `input` and output pins named `output` and `status`. You can edit the input and output pin lists in the node properties; scripts read and write those names directly.

Incoming tokens are merged into a single input packet before the script runs. Later tokens with the same key overwrite earlier values for ordinary key lookups. The node also keeps the original incoming token list under the internal `_tokens` key so runtimes can support fan-in without relying on overwrite order.

Internal host fields start with `_`. They are for the runtime and should not appear as normal user inputs.

New script nodes start with an engine-specific template. If the script is still empty or still one of the managed templates, changing the engine swaps in the matching template. Once you edit the script, engine changes preserve your code.

Syntax highlighting can be toggled in the node properties. Highlighter commands are configured globally from `Settings -> Syntax Highlighting Options...`. When DSLSH is available, the settings dialog shows one row per file type. Leave a row blank to use the built-in emergency rules. Enter a DSLSH parser command to use a language parser instead. If the command cannot be found, the editor silently falls back to the built-in rules.

The current file types are:

| File Type | Default Behavior | External Command Example |
| --- | --- | --- |
| JavaScript `.js` | Built-in JavaScript emergency rules | blank |
| CREXX `.rexx` | Built-in CREXX emergency rules | `~/.local/bin/rxc` |

For CREXX, a bare `rxc` command is treated as the CREXX DSLSH parser and the editor adds `--syntaxhighlight` automatically. You can also enter the full command yourself, for example `~/.local/bin/rxc --syntaxhighlight`.

Use the `Check` button in the settings dialog to confirm that a configured highlighter can be found and can parse a small sample. Diagnostics returned by parser-backed highlighters are underlined in the script editor; hover over the underlined text to see the message when the parser provides one.

Set `output` to return a single value, or an array/list to return multiple values. When fan-out is enabled and `output` is an array/list, the node emits one downstream token per item. Other output fields, including `logs` and `status`, are copied to each emitted token.

Failures should set `__error` and `status=FAIL`. Runtimes provide friendlier helpers for this, described below.

Use the script temp directory for scratch files. The host provides the path; file access depends on the selected runtime.

Inside the pipeline, prefer native arrays and objects where the runtime supports them. Serialize to JSON only when crossing an external boundary, such as writing a file, calling a process, or preparing display text for a system that only accepts strings.

## CREXX

Select engine `crexx`. Blank scripts show an `Add Example` button. CREXX scripts use the `PIPELINE` ADDRESS environment to read and write named pins.

```rexx
value = ""
address pipeline "GET input INTO :value"

if value = "" then do
  address pipeline "ERROR No input received"
  return 1
end

result = upper(value)
address pipeline "SET output :result"
address pipeline "LOG Processed input pin"
return 0
```

For quick experiments, the runtime accepts a plain script body and wraps it in a small `produce:` procedure. Advanced scripts may provide their own `produce:` procedure or a full `main:` module. The pin contract remains the same.

Compose normal Rexx strings before writing them to pins:

```rexx
question = ""
draft = ""
address pipeline "GET question INTO :question"
address pipeline "GET draft INTO :draft"
prompt = "Question is " || question || "0a0a"x || "Answer was " || draft
address pipeline "SET prompt :prompt"
```

### CREXX ADDRESS PIPELINE

The CREXX runtime exposes one ADDRESS environment named `PIPELINE`. The command shape follows the SQLite ADDRESS demo style: command words are literal, and `:name` is a host-variable anchor.

| Command | Purpose |
| --- | --- |
| `GET pin INTO :target` | Reads a named input pin into the Rexx variable `target`. |
| `SET pin text` | Writes literal `text` to a named output pin. |
| `SET pin :value` | Writes the Rexx variable `value` to a named output pin. |
| `LOG text` / `LOG :value` | Appends a log line. |
| `ERROR text` / `ERROR :value` | Fails the node with an error message. |

```rexx
produce: procedure = .int
  topic = ""
  address pipeline "GET topic INTO :topic"
  answer = "topic: " || topic
  address pipeline "SET summary :answer"
  return 0
```

Unknown commands return `rc=99`. A non-zero script return, an `ERROR` command, or a failed ADDRESS command fails the node.

## JavaScript

Select engine `quickjs`. JavaScript uses the `pipeline` host object.

Read inputs with `pipeline.input(name)`:

```javascript
const text = pipeline.input("input");
```

Write outputs with `pipeline.output(name, value)`:

```javascript
pipeline.output("output", text.toUpperCase());
pipeline.output("status", "OK");
```

JavaScript strings, numbers, booleans, arrays, and plain objects are converted into Qt variants. Arrays become `QVariantList`; plain objects become `QVariantMap`.

Log with `console.log`, `print`, or `console.error`:

```javascript
console.log("Starting transform");
console.error("Recoverable warning");
```

`console.error` writes a log line prefixed with `ERROR:`. It does not fail the node by itself. Use `pipeline.error(message)` for operational failure:

```javascript
const value = pipeline.input("required");
if (!value) {
  pipeline.error("Missing required input: required");
  return;
}
```

Throwing an exception also fails the node and includes the QuickJS error and stack trace in `__error`.

Use `pipeline.tempDir()` for scratch files:

```javascript
const path = pipeline.tempDir() + "/script-output.txt";
pipeline.output("path", path);
```

The QuickJS runtime exposes a small SQLite bridge:

```javascript
const dbPath = pipeline.tempDir() + "/example.db";
if (!sqlite.connect(dbPath)) {
  pipeline.error("Could not open database");
  return;
}

sqlite.exec("CREATE TABLE IF NOT EXISTS items (name TEXT, score INTEGER)");
sqlite.exec("INSERT INTO items (name, score) VALUES (?, ?)", ["alpha", 42]);

const rows = sqlite.exec("SELECT name, score FROM items WHERE score > ?", [10]);
pipeline.output("rows", rows);
```

`sqlite.exec` returns an array of objects for result sets. Use parameter arrays for values rather than building SQL strings.
