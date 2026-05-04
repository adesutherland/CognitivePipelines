# Scope Nodes Implementation Tracker

This document tracks the replacement of the confusing loop-canvas prototype with simpler parent/child scope nodes. It should be updated whenever a component is added, retired, or validated.

## Aim

The workflow model is split into two clear patterns:

- `Transform Scope`: one input enters a child canvas and one output leaves it. Optional retry mode lets the body validate and improve the result until accepted.
- `Iterator Scope`: a list enters a child canvas and a list of per-item results leaves it. The body sees one item at a time.

Boundary nodes are only visible inside the matching child canvas:

- `Get Input` and `Set Output` belong to a `Transform Scope` body.
- `Get Item` and `Set Item Result` belong to an `Iterator Scope` body.

Normal nodes can be placed inside either child canvas. Scopes can be nested, so an iterator body can contain a transform scope, and a transform body can contain an iterator scope.

## Component Status

| Component | Node id | Status | Notes | Test coverage |
| --- | --- | --- | --- | --- |
| Retire prototype `Loop Scope` | `loop-scope` | Implemented | Removed from palette/build in favor of explicit transform/iterator scopes. | Build + focused tests |
| Retire prototype `Loop Input` | `loop-input` | Implemented | Superseded by body-specific `Get Input` and `Get Item`. | Palette scoping test |
| Retire prototype `Loop Decision` | `loop-decision` | Implemented | Superseded by `Set Output` and `Set Item Result`. | Palette scoping test |
| Scope runtime helpers | n/a | Implemented | Shared frame/result parsing for parent scope nodes and subgraph execution. | Pending |
| Scope subgraph executor | n/a | Implemented | Executes child canvases synchronously from parent scope execution. | Pending |
| Scope body highlighting | n/a | Implemented | Child body execution emits graph-scoped node/connection status so subcanvas nodes turn green/error like root nodes. | `ScopeNodesTest.ScopeBodyExecutionEmitsNodeStatusForHighlighting` |
| Transform Scope | `transform-scope` | Implemented | Parent node for one input to one output, with optional retry-until-accepted. | Pending |
| Get Input | `scope-get-input` | Implemented | Transform-body source boundary node. | Pending |
| Set Output | `scope-set-output` | Implemented | Transform-body terminal boundary node. | Pending |
| Iterator Scope | `iterator-scope` | Implemented | Parent node for list input to list output. | Pending |
| Get Item | `iterator-get-item` | Implemented | Iterator-body source boundary node. | Pending |
| Set Item Result | `iterator-set-result` | Implemented | Iterator-body terminal boundary node. | Pending |
| Text Chunker | `text-chunker` | Implemented | Palette node backed by the existing chunking engine; produces list output for Iterator Scope input. | `ScopeNodesTest.TextChunkerNodeProducesListForIteratorInput` |
| Script Router | TBD | Planned | Future replacement/extension for ad hoc conditional branching with named outputs. | Pending |
| Conditional Router cleanup | `conditional-router` | Planned | Keep for simple true/false routing; document separately from loop/scopes. | Existing tests |

## Validation Plan

| Validation | Command or test | Status |
| --- | --- | --- |
| Unit tests for scope node boundary behavior | `ScopeNodesTest.GetInputEmitsTransformFrame`, `ScopeNodesTest.SetOutputDefaultsAccepted`, `ScopeNodesTest.GetItemEmitsIteratorFrame`, `ScopeNodesTest.SetItemResultDefaults` | Passed |
| Unit tests for parent runtime behavior | `ScopeNodesTest.TransformScopeRetriesUntilAccepted`, `ScopeNodesTest.IteratorScopeMapsList` | Passed |
| Unit test for list producer node | `ScopeNodesTest.TextChunkerNodeProducesListForIteratorInput` | Passed |
| Graph/subgraph execution tests | `ScopeNodesTest.TransformBodySubgraphExecutesPromptBuilder`, `ScopeNodesTest.IteratorBodySubgraphExecutesPromptBuilder` | Passed |
| Palette scoping tests | `ScopeNodesTest.BodyOnlyNodesArePaletteRegisteredInMatchingBodies` | Passed |
| Persistence tests | `ScopeNodesTest.SubgraphsPersistWithGraphKind`, `ScopeNodesTest.DeletingScopeRemovesOwnedBodyGraph` | Passed |
| Scope body highlighting test | `ScopeNodesTest.ScopeBodyExecutionEmitsNodeStatusForHighlighting` | Passed |
| Build | `cmake --build cmake-build-debug --target unit_tests CognitivePipelines integration_tests -j4` | Passed |
| Focused test run | `QT_QPA_PLATFORM=offscreen ctest --test-dir cmake-build-debug -R 'ScopeNodesTest' --output-on-failure` | Passed 12/12 |
| Chunker regression plus scope tests | `QT_QPA_PLATFORM=offscreen ctest --test-dir cmake-build-debug -R 'ScopeNodesTest|TextChunkerTest' --output-on-failure` | Passed 39/39 |
| Highlighting regression | `QT_QPA_PLATFORM=offscreen ctest --test-dir cmake-build-debug -R 'ScopeNodesTest|ExecutionControlTest' --output-on-failure` | Passed 14/14 |
| Diff hygiene | `git diff --check` | Pending |

## Worked Example Target

### Validate an LLM answer with a Transform Scope

Parent graph:

```text
Text Input -> Transform Scope -> Text Output
```

Transform body:

```text
Get Input -> Universal AI -> validation/script/prompt work -> Set Output
```

`Set Output.accepted` controls whether the parent scope stops. If accepted is false and the parent is in retry mode, `Set Output.next_input` becomes the next body input. If `next_input` is empty, the scope reuses the latest output.

### Process a list with an Iterator Scope

Parent graph:

```text
Text Chunker -> Iterator Scope -> Text Output
```

Iterator body:

```text
Get Item -> normal processing nodes -> Set Item Result
```

Each body run receives one item, index, count, context, and history. The parent returns a list of item results in input order.

## Open Follow-Ups

- Add a dedicated script-router node with user-defined output names.
- Decide whether a separate list-builder/list-join pair is needed beyond the existing text chunker and JSON/list handling.
- Revisit legacy loop source files after the new scope model has settled; they can remain unregistered while tests and docs move to the new nodes.
