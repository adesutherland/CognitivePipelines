# Scope Nodes User Guide

Scope nodes replace the older connector-heavy loop model with child canvases that have clear boundaries.

## Node Set

- `Transform Scope`: parent node for one input and one output.
- `Get Input`: body-only source node inside a Transform Scope.
- `Set Output`: body-only terminal node inside a Transform Scope.
- `Iterator Scope`: parent node for a list of items and a list of results.
- `Get Item`: body-only source node inside an Iterator Scope.
- `Set Item Result`: body-only terminal node inside an Iterator Scope.
- `Text Chunker`: list-producing helper node that splits text into chunks for an Iterator Scope.

## Transform Scope

Use `Transform Scope` when one value needs to be processed, optionally validated, and returned.

Parent graph:

```text
Text Input -> Transform Scope -> Text Output
```

Open the Transform Scope body and build:

```text
Get Input -> normal processing nodes -> Set Output
```

Settings:

- `Mode`: `Run once` or `Retry until accepted`.
- `Max attempts`: retry safety limit.

Important pins:

- Parent input `input`: value passed into the body.
- Parent output `output`: final body result.
- Parent output `status`: `completed`, `accepted`, `exhausted`, or `error`.
- `Get Input.input`: current body input.
- `Get Input.previous_output`: previous attempt output, if retrying.
- `Set Output.output`: value returned to the parent.
- `Set Output.accepted`: false asks retry mode to run again.
- `Set Output.next_input`: input for the next attempt. If empty, the last output is reused.

## Iterator Scope

Use `Iterator Scope` when a list should be processed item by item and collected back into a list.

Parent graph:

```text
Text Chunker -> Iterator Scope -> Text Output
```

Open the Iterator Scope body and build:

```text
Get Item -> normal processing nodes -> Set Item Result
```

Settings:

- `Failure policy`: stop on first error, skip failed items, or include error rows in the result list.

Important pins:

- Parent input `items`: `QVariantList`, JSON array text, newline text, or scalar fallback.
- Parent output `results`: list of item results.
- Parent output `summary`: count, skipped count, error count, and execution history.
- `Get Item.item`: current item.
- `Get Item.index`: zero-based item index.
- `Get Item.count`: total item count.
- `Set Item Result.result`: result for this item.
- `Set Item Result.skip`: true omits this item from the result list.

## Worked Validation Example

Goal: ask an LLM for an answer, validate it, and retry with feedback until accepted.

Root graph:

```text
Text Input -> Transform Scope -> Text Output
```

Transform body:

```text
Get Input -> Universal AI -> Universal Script -> Set Output
```

Suggested flow:

- `Get Input.input` feeds the prompt or draft into `Universal AI`.
- `Universal Script` examines the LLM output and sets fields such as `accepted`, `message`, and `next_input`.
- Connect the final answer to `Set Output.output`.
- Connect the validation boolean to `Set Output.accepted`.
- Connect feedback or an improved prompt to `Set Output.next_input`.

The parent scope retries until `accepted` is truthy or `Max attempts` is reached.

## Nesting

Scopes can be nested. For example, an Iterator Scope can process chunks, and each item body can contain a Transform Scope that validates one chunk result before returning it.

Body-boundary nodes are intentionally palette-scoped:

- `Get Input` and `Set Output` appear only in Transform Scope bodies.
- `Get Item` and `Set Item Result` appear only in Iterator Scope bodies.
