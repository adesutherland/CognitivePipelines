# Node Output Conventions

This document records the conventions used by built-in nodes. It is intended for node authors and for UI work that needs predictable node behavior.

## Categories

Palette registration owns the user-facing category. Descriptor categories should still be meaningful and normalized so tests, diagnostics, and future palette views do not drift.

Preferred descriptor category names:

- `AI Services`
- `Control Flow`
- `External Tools`
- `Input / Output`
- `Persistence`
- `Retrieval`
- `Scripting`
- `Text Utilities`
- `Visualization`

If a node is registered through a palette/delegate layer, that registration may group it differently for presentation, but the descriptor should not use one-off category spellings.

## Error Outputs

Operational failures should emit:

- `__error`: human-readable failure message.
- `status`: `FAIL` where the node already has a status concept or where a script host is setting failure state.
- Provider context where relevant: `_provider`, `_model`, and `_driver`.
- Useful hidden diagnostics where relevant, such as `_exit_code`, `_stderr`, or `_results_json`.

Nodes should also continue to use logging for diagnostics, but logs are not a substitute for `__error`. A downstream node or the UI should be able to detect failure by checking `__error`.

## Status Areas

Stateful nodes should expose a visible status line in their configuration panel. The status should describe the last important local action, for example:

- `Status: idle`
- `Status: indexing file 3 of 12: notes.md`
- `Status: indexed 48 chunks from 4 files`
- `Status: saved Daily Note.md`

Status labels are for operator feedback only. Machine-readable failure should still use `__error`.

## Structured Outputs

Within the pipeline, JSON-like data should be represented as `QVariantMap` and `QVariantList`, not as serialized JSON strings.

Use structured variants for:

- Search results.
- Metadata records.
- Lists of paths.
- Parsed API responses.
- Script outputs.

Use serialized JSON strings only at external boundaries:

- Writing a `.json` file.
- Sending an HTTP request body.
- Displaying compact debug text in a text-only field.
- Interoperating with a tool or provider that only accepts strings.

When compatibility is useful, emit both forms with the structured value as the primary pin and a hidden/debug string such as `_results_json` as the secondary output.

## Provider Context

Nodes that use model providers should emit provider context on success and failure:

- `_provider`: provider id, for example `openai`, `anthropic`, `google`, or `ollama`.
- `_model`: selected or resolved model id.
- `_driver`: driver/protocol selection when available.

This makes expired credentials, unsupported models, model alias drift, and provider-specific protocol issues visible in local and CI logs.
