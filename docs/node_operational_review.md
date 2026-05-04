# Node Operational Review

Date reviewed: 2026-04-28

Scope: this review covers the nodes registered by `src/graph/NodeGraphModel.cpp` and their node/property-widget implementations under `src/nodes`. It is intended as a code-derived operating guide and completeness audit, not as marketing documentation.

## How To Read This

- The palette category is the category passed to `NodeDelegateModelRegistry::registerModel`.
- The descriptor category is the category returned by the node itself. A few older nodes disagree with the palette category; the palette category is what the user normally sees when adding nodes.
- Pin IDs are the runtime `DataPacket` keys. Several nodes also emit a generic `text` key for downstream convenience.
- Assessment is deliberately practical:
  - Complete: ready for normal use with only minor polish possible.
  - Mostly complete: operational, with clear improvement points.
  - Functional but rough: usable, but behavior or UI should be clarified before broad use.
  - Needs attention: a real gap exists in error handling, configuration, or runtime behavior.

## Registered Node Inventory

| # | Palette category | Node | Descriptor id | Primary source | Assessment |
|---|---|---|---|---|---|
| 1 | Text Utilities | Prompt Builder | `prompt-builder` | `src/nodes/text/prompt_builder` | Mostly complete |
| 1a | Text Utilities | Text Chunker | `text-chunker` | `src/nodes/text/text_chunker` | Complete |
| 2 | Input / Output | Text Input | `text-input` | `src/nodes/io/text_input` | Complete |
| 3 | Input / Output | Ingest Input | `ingest-input` | `src/nodes/io/ingest_input` | Mostly complete |
| 4 | Input / Output | Image | `image-node` | `src/nodes/io/image` | Mostly complete |
| 5 | Visualization | Mermaid Renderer | `mermaid-node` | `src/nodes/visualization/mermaid` | Mostly complete |
| 6 | Input / Output | PDF to Image | `pdf-to-image` | `src/nodes/io/pdf_to_image` | Functional but rough |
| 7 | Input / Output | Text Output | `text-output` | `src/nodes/io/text_output` | Complete |
| 8 | Input / Output | Vault Output | `vault-output` | `src/nodes/io/vault_output` | Mostly complete |
| 9 | External Tools | Process | `process-connector` | `src/nodes/external_tools/process` | Functional but rough |
| 10 | AI Services | Universal AI | `universal-llm` | `src/nodes/ai/universal_llm` | Mostly complete |
| 11 | AI Services | Image Generator | `image-gen` | `src/nodes/ai/image_generation` | Mostly complete |
| 12 | External Tools | Python Script | `python-script` | `src/nodes/external_tools/python` | Functional but rough |
| 13 | Persistence | Database | `database-connector` | `src/nodes/retrieval/database` | Functional but rough |
| 14 | Persistence | RAG Indexer | `rag_indexer` | `src/nodes/retrieval/rag_indexer` | Mostly complete |
| 15 | Retrieval | RAG Query | `rag-query` | `src/nodes/retrieval/rag_query` | Mostly complete |
| 16 | Input / Output | Human Input | `human-input` | `src/nodes/io/human_input` | Mostly complete |
| 17 | Control Flow | Conditional Router | `conditional-router` | `src/nodes/control_flow/conditional_router` | Mostly complete |
| 18 | Control Flow | Transform Scope | `transform-scope` | `src/nodes/control_flow/scope` | Complete first pass |
| 19 | Control Flow | Get Input | `scope-get-input` | `src/nodes/control_flow/scope` | Complete |
| 20 | Control Flow | Set Output | `scope-set-output` | `src/nodes/control_flow/scope` | Complete |
| 21 | Control Flow | Iterator Scope | `iterator-scope` | `src/nodes/control_flow/scope` | Complete first pass |
| 22 | Control Flow | Get Item | `iterator-get-item` | `src/nodes/control_flow/scope` | Complete |
| 23 | Control Flow | Set Item Result | `iterator-set-result` | `src/nodes/control_flow/scope` | Complete |
| 24 | Scripting | Universal Script | `universal-script` | `src/nodes/scripting/universal_script` | Mostly complete |

## 1. Prompt Builder

Processing status: reviewed.

Purpose: builds a text prompt from a user-defined template. Placeholders in braces, such as `{topic}`, become dynamic input pins and are replaced with incoming values at execution time.

Settings:

- Template text, persisted as `template`.
- Default template is `{input}`.

Pins:

- Dynamic inputs: one input for each `{variable}` placeholder found in the template. The legacy/default variable is `input`.
- Output `prompt`: the rendered prompt string.

Operational logic:

- Incoming tokens are merged with last-writer-wins semantics.
- Each placeholder is replaced by `inputs[variable].toString()`.
- The properties widget reparses placeholders with a debounce and requests dynamic pin updates.

Completeness assessment: mostly complete. It is useful and integrated with dynamic pins. Missing values are silently replaced with an empty string, and there is no escaping/default-value syntax, so future UX work should make missing inputs visible.

## 1a. Text Chunker

Processing status: reviewed after scope redesign.

Purpose: splits incoming text into a list of chunks for iterator or RAG-oriented workflows.

Settings:

- Chunk size, persisted as `chunk_size`.
- Chunk overlap, persisted as `chunk_overlap`.
- File type, persisted as `file_type`; supported values are plain, C/C-family, Python, Rexx, SQL, shell, Cobol, Markdown, and YAML/HCL.

Pins:

- Input `text`: source text to split.
- Output `chunks`: `QVariantList` of chunk strings.
- Output `text`: same list for convenience when connected to text-first downstream nodes.
- Output `count`: chunk count.
- Output `summary`: structured chunking settings and count.

Operational logic:

- Uses the existing `TextChunker::split` engine.
- Code-aware file types reuse the same separator/comment-glue behavior already tested for RAG chunking.
- Overlap is clamped below chunk size.

Completeness assessment: complete for the first palette node. It intentionally stays focused on text-to-list conversion; richer document/file chunking should remain with ingest/RAG nodes or a separate file chunker.

## 2. Text Input

Processing status: reviewed.

Purpose: source node for manually entered text.

Settings:

- Text body, persisted as `text`.

Pins:

- No inputs.
- Output `text`: the configured text.

Operational logic:

- Ignores incoming tokens.
- Emits one token with `text` set to the stored value.

Completeness assessment: complete. This is a simple, stable source node. Minor note: descriptor category is `Inputs`, while the palette registers it under `Input / Output`.

## 3. Ingest Input

Processing status: reviewed.

Purpose: source node for bringing files or clipboard content into a pipeline. This is the main general-purpose ingester for text, markdown, images, PDFs, and arbitrary file metadata.

Settings:

- Source path, persisted as `sourcePath`.
- MIME type, persisted as `mimeType`.
- Kind, persisted as `kind`.
- The widget supports file selection, drag/drop, clipboard paste, status labels, image preview, and text preview.

Pins:

- No inputs.
- Output `markdown`: full UTF-8 file content when kind is `markdown`.
- Output `text`: full UTF-8 file content when kind is `text`.
- Output `image`: source/cache file path when kind is `image`.
- Output `pdf`: source/cache file path when kind is `pdf`.
- Output `file_path`: absolute source/cache path for all ingested content.
- Output `mime_type`: detected MIME type.
- Output `kind`: one of `markdown`, `text`, `image`, `pdf`, or `file`.

Operational logic:

- Local files are classified by MIME/type and path.
- Clipboard ingest prefers file URLs, then clipboard images, then clipboard text.
- Clipboard images are saved as PNG files in a cache directory.
- Clipboard text is saved as markdown in the cache unless it resolves to an existing local file path.
- Execution emits metadata for all kinds, plus content/path on the kind-specific pin.
- A successful ingest marks the token as forced execution and asks the main window to run the scenario from this node.

Completeness assessment: mostly complete. The node already covers the practical ingest workflows. Gaps: arbitrary `file` kind emits only metadata, errors are not exposed on an output pin, there is no obvious clear/reset action, and the immediate-run path is tightly coupled to `MainWindow` and `NodeGraphModel`.

## 4. Image

Processing status: reviewed.

Purpose: image source or image viewer bridge. It can either output a configured image path or display and pass through an upstream image path.

Settings:

- Image path, persisted as `imagePath`.
- Properties widget provides file selection, preview, and a full-size popup.

Pins:

- Input `image`: optional incoming image path.
- Output `image`: resolved image path.

Operational logic:

- If incoming `image` is present, it becomes the active path.
- Otherwise the configured image path is used.
- The preview widget is updated on the GUI thread after execution.

Completeness assessment: mostly complete. It is useful as both source and viewer. It does not validate that the output path exists before emitting it, and using the same pin id for input and output is convenient but can make merged packet debugging less obvious.

## 5. Mermaid Renderer

Processing status: reviewed.

Purpose: renders Mermaid diagram source into a PNG image.

Settings:

- Resolution scale, persisted as `scale`, range 0.1 to 4.0.
- Last rendered source, persisted as `lastCode` and shown read-only in the widget.

Pins:

- Input `code`: Mermaid source text.
- Output `image`: generated PNG path, or an error string when rendering fails.

Operational logic:

- Empty code returns `__error` and an error message on `image`.
- If `_sys_node_output_dir` is present, output is written as `diagram.png` and source as `source.mmd` in that directory.
- Otherwise it creates a non-auto-removed temporary PNG.
- Rendering is delegated to `MermaidRenderService`.
- Render warnings may be emitted as `__warning`; details may be emitted as `__detail`.

Completeness assessment: mostly complete. It has useful error signaling and persistent output support. Follow-up areas are dependency discovery/configuration for the render service and output filename collisions when a persistent directory is reused.

## 6. PDF To Image

Processing status: reviewed.

Purpose: converts a PDF into rendered PNG image output for vision/model pipelines.

Settings:

- PDF path, persisted as `pdf_path`.
- Split-pages option, persisted as `split_pages`.

Pins:

- Input `pdf_path`: optional incoming PDF file path.
- Output `image_path`: stitched PNG path, or the first page path when split-pages is enabled.
- Output `image_paths`: list of generated image paths.
- Output `page_count`: number of rendered pages.

Operational logic:

- Uses incoming `pdf_path` when provided, otherwise the configured PDF path.
- Uses `QPdfDocument` to render pages at scale 2.0.
- If `_sys_node_output_dir` is present, writes deterministic files there.
- Without `_sys_node_output_dir`, writes temporary files.
- In stitched mode, all pages are combined vertically into one image.
- On failure, emits `__error`, clears `image_path`, emits an empty `image_paths`, and sets `page_count` to 0.

Completeness assessment: mostly complete. The core conversion is useful and output variants are now explicit. Remaining follow-up: deterministic file names can be overwritten on repeated runs.

## 7. Text Output

Processing status: reviewed.

Purpose: terminal display sink for text-like pipeline output.

Settings:

- No user configuration.
- The widget is a read-only output display.

Pins:

- Input `text`: value to display.
- No declared outputs.

Operational logic:

- Merges incoming packets and reads `text`.
- Maps/lists are displayed as indented JSON in a fenced markdown block.
- Plain strings preserve newlines as markdown line breaks.
- UI updates are marshalled to the GUI thread.
- Runtime output is not persisted.

Completeness assessment: complete for display use. Design quirk: it returns an empty token after execution, so downstream triggering is possible but carries no useful data.

## 8. Vault Output

Processing status: reviewed.

Purpose: saves markdown into an Obsidian-style vault, using an LLM to choose a subfolder and filename from the vault context.

Settings:

- Vault root, persisted as `vault_root`.
- Provider, persisted as `provider_id`.
- Model, persisted as `model_id`.
- Routing prompt, persisted as `routing_prompt`.
- Temperature, persisted as `temperature`.
- Max tokens, persisted as `max_tokens`.
- Properties widget supports browsing the vault root, catalog provider/model selection, filtered-model display, and model testing.

Pins:

- Input `markdown`: markdown note content to save.
- Input `vault_root`: optional override for the configured vault root.
- Input `prompt`: optional override for the routing prompt.
- Output `saved_path`: absolute path of the saved note.
- Output `subfolder`: selected relative subfolder.
- Output `filename`: final markdown filename.
- Output `decision`: map containing the parsed routing decision plus provider/model context.

Operational logic:

- Requires markdown content, vault root, provider, and model.
- Creates the vault root if needed.
- Builds a vault summary from existing directories and markdown files.
- Sends a JSON-only routing request to the selected backend.
- Parses the first JSON object in the response.
- Sanitizes the subfolder and filename, blocks absolute/traversal paths, creates directories, and writes with `QSaveFile`.
- Generates a unique filename instead of overwriting existing notes.

Completeness assessment: mostly complete. The safety checks and atomic write path are good. Main gaps: no non-LLM fallback route, no overwrite/update mode, hard-coded vault summary limits, and reliance on the model returning parseable JSON.

## 9. Process

Processing status: reviewed.

Purpose: runs an external command and passes optional text to its standard input.

Settings:

- Command line string, persisted as `command`.

Pins:

- Input `stdin`: optional stdin text.
- Output `stdout`: captured standard output.
- Output `stderr`: captured standard error or process startup/timeout errors.

Operational logic:

- Splits the configured command with `QProcess::splitCommand`.
- Starts the program directly without a shell wrapper.
- Writes stdin if present, closes the write channel, and waits up to 60 seconds.
- Kills the process on timeout.
- Emits `__error` and `_exit_code` for startup failures, timeouts, abnormal exits, and non-zero exits.

Completeness assessment: functional but rough. Direct execution is safer than shell execution, but there is no working-directory setting, environment override, or timeout control.

## 10. Universal AI

Processing status: reviewed.

Purpose: general chat/completion node for provider-backed LLM calls, including capability-aware attachment support.

Settings:

- Provider, persisted as `provider`.
- Model, persisted as `model`.
- System prompt, persisted as `systemPrompt`.
- User prompt, persisted as `userPrompt`.
- Temperature, persisted as `temperature`.
- Max tokens, persisted as `maxTokens`.
- Soft fallback enable flag, persisted as `enableFallback`.
- Fallback string, persisted as `fallbackString`.
- Properties widget uses the model catalog, supports recommended/available/investigate grouping, filtered-model display, provider/model testing, and capability-dependent controls.

Pins:

- Input `system`: optional system/developer prompt override.
- Input `prompt`: optional user prompt override.
- Input `attachment_in`: optional attachment path or list of paths. This pin is dynamically removed for models without vision support.
- Output `response`: model response or fallback/error text.

Operational logic:

- Merges inputs and treats pins as overrides for configured prompts.
- Parses attachments from a list, JSON array string, or single path.
- Validates prompt/model/backend/credentials and attachment readability.
- Resolves model capabilities and driver profile through the model caps registry.
- Preserves unknown model ids instead of silently changing the user's model choice.
- Calls the selected backend and emits `response`, hidden usage keys, `_raw_response`, and `logs`.
- Backend errors include provider, model, and message in logs; soft fallback can return the fallback string instead of failing hard.

Completeness assessment: mostly complete. The provider/model/capability work is the strongest node implementation. Gaps: OpenAI PDF attachment rejection is hard-coded, debug logging is still somewhat noisy, and fallback behavior can hide failures unless users inspect logs.

## 11. Image Generator

Processing status: reviewed.

Purpose: generates an image from a text prompt through an image-capable provider backend.

Settings:

- Provider, persisted as `provider`.
- Model, persisted as `model`.
- Size, persisted as `size`.
- Quality, persisted as `quality`.
- Style, persisted as `style`.

Pins:

- Input `prompt`: image generation prompt.
- Output `image_path`: path to the generated image, or an error string when generation fails.

Operational logic:

- Requires a non-empty prompt.
- Defaults provider/model from the image model catalog where possible.
- Uses `_sys_node_output_dir` when present as the backend output directory.
- Resolves provider backend and calls `generateImage`.
- Emits an absolute file path if the backend writes an existing file.
- Emits `__error` plus `_provider`, `_model`, and `_driver` where known on missing prompt, missing backend, or failed generation.

Completeness assessment: mostly complete. It is operational for OpenAI-style image generation and now uses the catalog model selector with filtered-model display and selection testing. Remaining follow-up: size/quality/style options are still provider-specific and should become capability-driven if other image providers are added.

## 12. Python Script

Processing status: reviewed.

Purpose: runs inline Python script content as an external process.

Settings:

- Python executable/command, persisted as `executable`, default `python3 -u`.
- Script content, persisted as `script`.

Pins:

- Input `stdin`: optional stdin text.
- Output `stdout`: captured standard output.
- Output `stderr`: captured standard error or process errors.

Operational logic:

- Writes script content to a unique `script_<uuid>.py` in `_sys_node_output_dir` when available, otherwise a temp directory.
- Splits the executable command with `QProcess::splitCommand`.
- Appends the script path as the final argument.
- Writes stdin, waits up to 60 seconds, and kills the process on timeout.
- Emits `__error` and `_exit_code` for invalid executables, script file creation failures, startup failures, timeouts, abnormal exits, and non-zero exits.

Completeness assessment: functional but rough. It works for simple scripts, but stale fields in the header (`pythonExecutable_`, `scriptPath_`, `timeoutMs_`) are unused, and there is no working directory/environment/timeout UI.

## 13. Database

Processing status: reviewed.

Purpose: executes SQL against a SQLite database and returns either a markdown table or rows-affected text.

Settings:

- Database file path, persisted as `databasePath`.
- SQL query text, persisted as `sqlQuery`.

Pins:

- Input `database`: optional database path override.
- Input `sql`: optional SQL override.
- Output `database`: database path used.
- Output `stdout`: markdown table for SELECT results, or row count for non-SELECT statements.
- Output `stderr`: database/SQL errors.

Operational logic:

- Uses input pins when present, otherwise configured values.
- Opens SQLite with a unique connection name.
- Splits SQL text on semicolons and executes all statements inside a transaction.
- Rolls back on statement failure.
- Formats the final SELECT query as a markdown table with escaped cell content.
- Emits `__error` when database path, SQL text, open, or statement execution fails.

Completeness assessment: functional but rough. It is a useful SQLite helper, but semicolon splitting is not SQL-aware, only the last query result is surfaced, and there is no parameter binding model.

## 14. RAG Indexer

Processing status: reviewed.

Purpose: scans a directory, chunks text files, generates embeddings, and persists a local SQLite RAG index.

Settings:

- Input directory, persisted as `directory_path`.
- Database path, persisted as `database_path`.
- Metadata JSON string, persisted as `index_metadata`.
- Provider, persisted as `provider_id`.
- Embedding model, persisted as `model_id`.
- Chunk size, persisted as `chunk_size`.
- Chunk overlap, persisted as `chunk_overlap`.
- File filter, persisted as `file_filter`.
- Chunking strategy, persisted as `chunking_strategy`.
- Clear database flag, persisted as `clear_database`.

Pins:

- Input `directory_path`: optional input directory override.
- Input `database_path`: optional SQLite index path override.
- Input `index_metadata`: optional metadata override.
- Output `database_path`: index path used.
- Output `count`: number of chunks inserted.

Operational logic:

- Chooses a default embedding provider/model from `ModelCatalogService`.
- Validates directory, database path, provider/model, credentials, and backend.
- Creates/updates a two-table schema: `source_files` and `fragments`, including `start_line` and `end_line` references on fragments.
- Optionally clears both tables and resets sqlite sequences before indexing.
- Applies semicolon-separated filename filters such as `*.cpp; *.h`.
- Scans recursively with `DocumentLoader`, reads text files, applies the configured chunking strategy, and chunks with `TextChunker`.
- Stores source file provider/model metadata and fragment embedding blobs.
- Emits throttled progress packets roughly every 10 seconds.
- Logs embedding failures with provider, model, file, chunk, and message.
- Emits `__error`, `_provider`, `_model`, `_driver`, `embedding_failures`, `database_insert_failures`, `skipped_files`, and `chunking_strategy`.

Completeness assessment: mostly complete. The indexing path is real, the selected chunking strategy is active, line references are persisted, and run statistics are surfaced. Remaining follow-up: make clear-database more guarded and add richer UI for inspecting partial failures.

## 15. RAG Query

Processing status: reviewed.

Purpose: performs semantic search against a RAG index and returns context text plus structured result metadata.

Settings:

- Database path, persisted as `database_path`.
- Default query text, persisted as `query_text`.
- Max results, persisted as `max_results`.
- Min relevance, persisted as `min_relevance`.

Pins:

- Input `query`: optional query text override.
- Input `database`: optional SQLite index path override.
- Output `context`: concatenated source-labelled text chunks.
- Output `results`: structured `QVariantList` of result maps with source, score, text, fragment id, file id, chunk index, and line references when available.

Operational logic:

- Uses input pins when present, otherwise configured database/query values.
- Reads the index provider/model with `RagUtils::getIndexConfig`.
- Resolves credentials/backend for that stored provider.
- Embeds the query using the same provider/model as the index.
- Searches with `RagUtils::findMostRelevantChunks`.
- Resolves source file paths for matched file ids.
- Formats source-labelled context and structured results.
- Emits `_results_json` as a compact compatibility/debug view.
- Logs embedding failures with provider/model/message.

Completeness assessment: mostly complete. The logic is good, model consistency is handled by the stored index config, failures emit `__error`, and results are structured for downstream nodes.

## 16. Human Input

Processing status: reviewed.

Purpose: pauses execution to ask the user for text input.

Settings:

- Default prompt, persisted as `default_prompt`.
- Backward-compatible load key: `text`.

Pins:

- Input `prompt`: optional prompt override.
- Output `text`: user-entered response.

Operational logic:

- Effective prompt is incoming `prompt`, then configured default prompt, then `Please provide input:`.
- Finds `MainWindow` and invokes `requestUserInput` on the GUI thread.
- On cancel, emits `__error`.
- On success, emits `text`.

Completeness assessment: mostly complete. It is appropriate for GUI human-in-the-loop workflows. It depends on `MainWindow` and has no headless/test harness fallback.

## 17. Conditional Router

Processing status: reviewed.

Purpose: routes a payload to either a true or false branch based on a condition.

Settings:

- Default condition mode, persisted as `routerMode` and legacy `defaultCondition`.
- Modes are default false, default true, or wait for signal.

Pins:

- Input `in`: payload to route.
- Input `condition`: condition value.
- Output `true`: active branch when condition is truthy.
- Output `false`: active branch otherwise.

Operational logic:

- `isReady` requires input data. If the condition input appears connected, it also waits for condition data.
- In wait-for-signal mode, both data and condition are required.
- Truthy strings are `true`, `1`, `yes`, `pass`, and `ok`.
- Output carries both generic `text` and the active branch pin id.

Completeness assessment: mostly complete. It covers the common routing behavior and has tests. The readiness logic uses connection count as a proxy for condition-connected state, which is pragmatic but fragile if more inputs are added.

## 18. Transform Scope

Processing status: implemented after redesign.

Purpose: parent scope for one input value and one output value. It owns a transform body canvas and can optionally retry until the body marks the result accepted.

Settings:

- Mode, persisted as `mode`: `run_once` or `retry_until_accepted`.
- Max attempts, persisted as `max_attempts`.
- Body graph id, persisted as `body_id`.

Pins:

- Input `input`: value passed into the transform body.
- Input `context`: optional structured context map.
- Output `output`: final body output.
- Output `text`: same final output for text-first downstream nodes.
- Output `context`: context plus `_scope` metadata and history.
- Output `status`: `completed`, `accepted`, `completed_unaccepted`, `exhausted`, or `error`.
- Output `error`: human error message when execution fails.

Operational logic:

- The properties panel opens the nested transform body graph.
- The body is executed with an isolated data lake and a `ScopeFrame`.
- `Set Output.accepted=false` asks retry mode to run another body pass.
- `Set Output.next_input` supplies the next attempt input; otherwise the latest output is reused.
- Body failures emit `__error`, `message`, `error`, and `status=error`.

Completeness assessment: complete first pass. It removes the confusing feedback connectors from validation workflows. Future UX work should add body templates and execution highlighting inside child canvases.

User guide: see `docs/ScopeNodes_UserGuide.md`.

## 19. Get Input

Processing status: implemented after redesign.

Purpose: source boundary node for transform body graphs.

Settings: none.

Pins:

- Output `input`: current transform input.
- Output `text`: same current input for text-first nodes.
- Output `attempt`: zero-based attempt number.
- Output `previous_output`: prior attempt output when retrying.
- Output `context`: parent context.
- Output `history`: prior body-pass summaries.

Completeness assessment: complete. It is registered only in transform body graphs.

## 20. Set Output

Processing status: implemented after redesign.

Purpose: terminal boundary node for one transform body pass.

Settings: none.

Pins:

- Input `output`: candidate/final output.
- Input `accepted`: optional truthy value; defaults to true.
- Input `next_input`: optional input for the next retry attempt.
- Input `context`: context updates to merge into the parent scope.
- Input `message`: human status/debug message.
- Input `error`: failure message.

Operational logic:

- Emits an internal `_transform_output` map consumed by the parent `Transform Scope`.
- `error` also emits `__error` and `message`.
- The node has no output pins because it terminates a body pass.

Completeness assessment: complete. It intentionally replaces `Loop Decision` with transform-specific language.

## 21. Iterator Scope

Processing status: implemented after redesign.

Purpose: parent scope for processing a list of items with a child body canvas and collecting a result list.

Settings:

- Failure policy, persisted as `failure_policy`: `stop`, `skip`, or `include_error`.
- Body graph id, persisted as `body_id`.

Pins:

- Input `items`: list input. Accepts `QVariantList`, JSON array text, newline text, or a scalar fallback.
- Input `context`: optional structured context map.
- Output `results`: result list in input order, excluding skipped items.
- Output `text`: same result list for text-first downstream nodes.
- Output `summary`: structured count, skipped, error, and history metadata.
- Output `errors`: list of item error maps.
- Output `context`: context plus `_scope` metadata.
- Output `status`: `completed` or `error`.

Operational logic:

- Runs the iterator body once per item.
- Each pass receives item, index, count, context, and history.
- `Set Item Result.skip=true` omits that item from `results`.
- Failure policy decides whether errors stop execution, skip items, or become error rows.

Completeness assessment: complete first pass. It gives the application a simpler, list-oriented replacement for connector-based loop workflows.

## 22. Get Item

Processing status: implemented after redesign.

Purpose: source boundary node for iterator body graphs.

Settings: none.

Pins:

- Output `item`: current item.
- Output `text`: same item for text-first nodes.
- Output `index`: zero-based item index.
- Output `count`: total item count.
- Output `context`: parent context.
- Output `history`: prior item summaries.

Completeness assessment: complete. It is registered only in iterator body graphs.

## 23. Set Item Result

Processing status: implemented after redesign.

Purpose: terminal boundary node for one iterator body pass.

Settings: none.

Pins:

- Input `result`: result for the current item.
- Input `skip`: truthy value that omits the item from the parent result list.
- Input `context`: context updates to merge into the parent scope.
- Input `message`: human status/debug message.
- Input `error`: failure message.

Operational logic:

- Emits an internal `_item_result` map consumed by the parent `Iterator Scope`.
- `error` also emits `__error` and `message`.
- The node has no output pins because it terminates an item body pass.

Completeness assessment: complete.

## 24. Universal Script

Processing status: reviewed.

Purpose: runs code through a registered in-process script engine. At present, `NodeGraphModel` registers the `quickjs` engine.

Settings:

- Script engine id, persisted as `engineId`, default `quickjs`.
- Script code, persisted as `scriptCode`.
- Fan-out flag, persisted as `enableFanOut`.

Pins:

- Input `input`: generic input value available to the script host.
- Output `output`: script output value.
- Output `status`: status string, default `OK` or `FAIL`.

Operational logic:

- Merges all incoming token data into the script host input map.
- Creates the selected script engine from `ScriptEngineRegistry`.
- Runs the script with `ExecutionScriptHost`.
- If the script does not set `status`, inserts `OK` or `FAIL`.
- If fan-out is enabled and `out` is a list, returns one token per item.
- Adds a summary of fan-out items to `logs`.
- Missing engines and script/runtime failures emit `__error`.
- QuickJS exposes `pipeline.getInput`, `pipeline.setOutput`, `pipeline.setError`, `pipeline.getTempDir`, `console`, `print`, and the SQLite helper.

Completeness assessment: mostly complete. The QuickJS host is tested and supports data exchange, logging, explicit errors, arrays/objects, and SQLite integration. The host API is now documented in `docs/UniversalScript_UserGuide.md`.

## Cross-Cutting Observations

- Palette registration owns the user-facing category. `Transform Scope` and `Iterator Scope` are the active repeat/validation palette entries, while body-boundary nodes are registered only in their matching child graph kind.
- Error signaling is now more consistent across non-loop nodes: operational failures should emit `__error`, with provider/model/driver context where relevant.
- Generic `text` is useful, but it can blur branch-specific semantics. Control-flow and scope nodes should document when downstream nodes should connect to the named pin versus read `text`.
- Structured outputs should use `QVariantMap`/`QVariantList` inside the pipeline. Serialized JSON should be reserved for external boundaries or hidden debug compatibility fields.
- Provider/model selection is catalog-driven in `Universal AI`, `Vault Output`, `RAG Indexer`, and `Image Generator`.
- External process nodes need a shared policy for working directory, environment, timeout, exit status, and error pins.
- The new `Ingest Input` and `Vault Output` nodes are real operational nodes, not stubs. They now have visible status feedback; broader user-facing docs are still useful.

## Suggested Follow-Up Backlog

1. Add shared working-directory, environment, and timeout configuration for external process/script nodes.
2. Make RAG clear-database behavior more guarded, for example requiring a confirmation or separate maintenance action.
3. Make image generation size/quality/style controls capability-driven per provider/model.
4. Add broader user-facing docs for the non-script operational nodes.
5. Add scope-body templates and richer execution/status visualization inside nested scope graphs.
