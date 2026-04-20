# ISSUES (Non-Critical Findings and Housekeeping)

This file tracks minor issues discovered during the latest housekeeping pass. Items here are not blockers but should be triaged and addressed in future iterations.

## Resolved Since Last Pass
- Source file header standardization: All `.h` and `.cpp` files use the canonical header from `src/app/main.cpp`.
- Documentation alignment: Repository docs now describe the reorganized `src/` layout and current node/backend architecture instead of the removed connector-era filenames.

## New Findings
- Graph cycle handling:
  - `ExecutionEngine` still assumes executable acyclic flow but does not present a dedicated user-facing cycle error before scheduling work.
  - Recommendation: add explicit validation and a clearer failure path when the graph contains cycles or unreachable control-flow states.
- Parallel execution still depends on mutable graph reads:
  - `ExecutionEngine` continues to query `NodeGraphModel` during task processing rather than executing from an immutable topology snapshot.
  - Recommendation: snapshot graph topology at run start, or add stronger synchronization around graph reads during execution.
- Blocking UI synchronization remains in the execution path:
  - `ExecutionEngine` uses `Qt::BlockingQueuedConnection` for some output notifications.
  - Recommendation: revisit these handoff points to reduce deadlock risk and improve responsiveness under long-running or highly parallel workloads.
- Code hygiene automation:
  - No clang-format or clang-tidy configuration is present.
  - Recommendation: add formatting/lint configs and a CI check or pre-commit hook.

## Notes for Next Pass
- Reassess dependency versions (QtNodes, cpr, Boost) and update docs if CMake changes.
- Review UI/UX around provider credential entry and storage in `CredentialsDialog`; consider additional masking or validation if needed.
