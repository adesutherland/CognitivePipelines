# Universal Script Node User Guide

The Universal Script node runs small scripts inside the pipeline. QuickJS is always available. CREXX is offered when the application can find a usable CREXX runtime at configure/run time.

## General Model

The node has one primary input pin named `input` and one primary output pin named `output`. It also publishes `status`, `logs`, and `__error` for operational feedback.

Incoming tokens are merged into a single input packet before the script runs. Later tokens with the same key overwrite earlier values for ordinary key lookups. The node also keeps the original incoming token list under the internal `_tokens` key so runtimes can support fan-in without relying on overwrite order.

Internal host fields start with `_`. They are for the runtime and should not appear as normal user inputs. CREXX filters system-only tokens out of `input[]`; JavaScript can still inspect internal fields explicitly if needed.

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

Select engine `crexx`. Blank scripts show an `Add Example` button; it inserts a complete `options levelb` starter with a `produce:` procedure. The node fills `input[]`, runs `produce`, then reads `output[]`, `log[]`, and `errors[]`.

```rexx
options levelb
import rxfnsb

produce: procedure = .int
  arg input = .string[], expose output = .string[], expose log = .string[], expose errors = .string[]

  do i = 1 to input.0
    output[i] = upper(input[i])
  end

  log[1] = "processed " || input.0 || " item(s)"
  return 0
```

For quick experiments, the runtime still accepts a plain `produce` body and wraps it with the same signature.

`input[]` contains primary user input values. If the node receives multiple incoming tokens, each token's `input` value becomes one array entry. If a single token already contains an array, that array is passed through as the input items. Tokens containing only internal fields such as `_sys_node_output_dir` are ignored.

Set `output[]` to return values. With fan-out enabled, each `output[]` entry becomes a downstream token. Set `log[]` for node logs. Set `errors[]` or return a non-zero value to fail the node.

```rexx
if input.0 = 0 then do
  errors[1] = "No input received"
  return 1
end

do i = 1 to input.0
  output[i] = upper(input[i])
end

log[1] = "done"
return 0
```

Advanced scripts may provide their own `produce:` procedure. A script that provides `main:` is treated as a full CREXX module and can use the `PIPELINE` ADDRESS environment directly:

```rexx
address pipeline "INPUT" expose cp_input[]
address pipeline "LOG :message" expose message
address pipeline "RETURN" expose cp_output[] cp_log[] cp_errors[]
```

Full modules use the `cp_` bridge stems because `input` and `output` are ADDRESS clause words in CREXX. The generated `produce` wrapper still presents the friendly `input[]`, `output[]`, `log[]`, and `errors[]` names to normal scripts.

### CREXX ADDRESS PIPELINE

The CREXX runtime currently exposes one ADDRESS environment named `PIPELINE`. The generated wrapper uses it for normal scripts, and full modules can use it directly. This is the complete command list today:

| Command | Exposed Variables | Purpose |
| --- | --- | --- |
| `INPUT` | `cp_input[]` | Fills `cp_input[]` with primary user input values. System-only tokens are ignored. |
| `RETURN` | `cp_output[] cp_log[] cp_errors[]` | Reads output values, log lines, and error lines back into the node. |
| `LOG text` | none | Appends `text` to the node logs. |
| `LOG :name` or `LOG ${name}` | `name` | Resolves an exposed CREXX variable and appends its value to the node logs. |
| `ERROR text` | none | Appends `text` to the error list. |
| `ERROR :name` or `ERROR ${name}` | `name` | Resolves an exposed CREXX variable and appends its value to the error list. |

`INPUT` and `RETURN` set the CREXX `rc` variable from the host callback. Unknown commands return `rc=99`. A non-empty error list, a non-zero `produce` return, or a failed ADDRESS command fails the node.

Example full module:

```rexx
options levelb
import rxfnsb

main: procedure = .int
  cp_input = .string[]
  cp_output = .string[]
  cp_log = .string[]
  cp_errors = .string[]

  address pipeline "INPUT" expose cp_input[]
  if rc <> 0 then return rc

  do i = 1 to cp_input.0
    cp_output[i] = upper(cp_input[i])
  end

  message = "Processed " || cp_input.0 || " item(s)"
  address pipeline "LOG :message" expose message

  address pipeline "RETURN" expose cp_output[] cp_log[] cp_errors[]
  if rc <> 0 then return rc
  return 0
```

`ADDRESSCALL` functions are not exposed yet. Use `INPUT`, `RETURN`, `LOG`, and `ERROR` for the current protocol.

## JavaScript

Select engine `quickjs`. JavaScript uses the `pipeline` host object.

Read inputs with `pipeline.getInput(name)`:

```javascript
const text = pipeline.getInput("input");
```

Write outputs with `pipeline.setOutput(name, value)`:

```javascript
pipeline.setOutput("output", text.toUpperCase());
pipeline.setOutput("status", "OK");
```

JavaScript strings, numbers, booleans, arrays, and plain objects are converted into Qt variants. Arrays become `QVariantList`; plain objects become `QVariantMap`.

Log with `console.log`, `print`, or `console.error`:

```javascript
console.log("Starting transform");
console.error("Recoverable warning");
```

`console.error` writes a log line prefixed with `ERROR:`. It does not fail the node by itself. Use `pipeline.setError(message)` for operational failure:

```javascript
const value = pipeline.getInput("required");
if (!value) {
  pipeline.setError("Missing required input: required");
  return;
}
```

Throwing an exception also fails the node and includes the QuickJS error and stack trace in `__error`.

Use `pipeline.getTempDir()` for scratch files:

```javascript
const path = pipeline.getTempDir() + "/script-output.txt";
pipeline.setOutput("path", path);
```

The QuickJS runtime exposes a small SQLite bridge:

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
