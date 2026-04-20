# Thread-Safety Audit: Parallel Node Execution Feasibility

## 1. Overview
This audit assesses the feasibility of upgrading the `ExecutionEngine` to support parallel node execution. While the engine currently utilizes a `QThreadPool`, it relies on shared data structures that are not inherently thread-safe, posing significant risks for true concurrent operation.

## 2. Component Analysis

### 2.1 NodeGraphModel (and QtNodes base)
- **Shared Resources:** `NodeGraphModel` (and its parent `QtNodes::DataFlowGraphModel`) stores the topology in `std::unordered_set` collections (`_connections`, `_models`).
- **Protection:** None. There are no mutexes or locks guarding access to the graph structure.
- **Risk Assessment:** 
    - **Critical:** `ExecutionEngine` worker threads call `allConnectionIds()` and `delegateModel()` frequently. If the UI thread deletes a connection or node while a worker thread is iterating over these collections, it will result in iterator invalidation and a likely application crash.
    - **Data Integrity:** Reading graph state during a mutation from the UI thread leads to undefined behavior.

### 2.2 ExecutionEngine
- **Current State:** The engine is partially threaded. It uses `QtConcurrent::run` to execute nodes, but the logic for discovering next nodes (`handleTaskCompleted`) runs on the worker thread and accesses the unprotected `NodeGraphModel`.
- **Race Conditions:**
    - `m_dataLake` is well-protected by `QReadWriteLock`.
    - `m_activeTasks` and task scheduling are protected by `m_queueMutex`.
    - **Deadlock Risk:** `nodeOutputChanged` signals are emitted via `BlockingQueuedConnection`. If the main thread is blocked (e.g., waiting for the thread pool to finish), and a worker thread is waiting for the main thread to process this signal, a deadlock will occur.
- **Verdict:** True parallel execution is currently "accidentally" active but highly dangerous due to the lack of model synchronization.

### 2.3 UniversalScriptNode (QuickJS / Python)
- **Re-entrancy:** 
    - **QuickJS:** `QuickJSRuntime` creates a fresh `JSRuntime` and `JSContext` per instance. Since each `execute()` call uses a unique instance, it is thread-safe and re-entrant.
    - **Python:** Uses `QProcess` to launch external interpreters. Since `ExecutionEngine` provides a unique `_sys_node_output_dir` for each execution, there are no file-system collisions.
- **Verdict:** Script engines are ready for parallel execution.

## 3. Verdict: LOCKING STRATEGY REQUIRED
We **cannot** simply enable more threading or rely on the current implementation for parallel execution without a dedicated locking strategy. The primary blocker is the thread-unsafe nature of the `NodeGraphModel`.

## 4. Recommended Roadmap

### Phase 1: Foundation (High Priority)
1.  **Thread-Safe Graph Model:**
    *   Implement a `QReadWriteLock` in `NodeGraphModel`.
    *   Wrap all topological queries (`allNodeIds`, `allConnectionIds`, etc.) in read locks.
    *   Ensure UI-driven modifications (add/delete node/connection) use write locks.
2.  **Snapshot-Based Execution:**
    *   Modify `ExecutionEngine` to take a read-only snapshot of the graph topology at the start of `runPipeline()`.
    *   Alternatively, pass all necessary connectivity data for a node's downstream path into the `ExecutionTask` so worker threads don't need to query the model.

### Phase 2: Performance & Safety (Medium Priority)
3.  **Refactor Signaling:**
    *   Change `nodeOutputChanged` from `BlockingQueuedConnection` to `QueuedConnection`.
    *   Use the existing thread-safe `ExecutionEngine::nodeOutput()` accessor for UI updates to maintain consistency without blocking.
4.  **Serialization Refinement:**
    *   Maintain the `m_nodeInFlight` protection to prevent the same node instance from being executed concurrently (which would violate its internal state), but allow different nodes to saturate the `QThreadPool`.

### Phase 3: Robustness (Low Priority)
5.  **Re-entrancy Verification:**
    *   Audit 3rd-party libraries in `QuickJS` (like `quickjs-libc`) for global state that might be affected by multiple `JSRuntime` instances in the same process.
