# [GRAPH] Graph container — NodeId + dependency_graph

**Created:** 2026-05-26
**Started:** 2026-05-26
**Completed:** 2026-05-26
**Status:** done
**Module:** GRAPH
**Priority:** blocker
**Complexity:** S (< 1 day)
**Blocks:** #014 (graph_algorithms)
**Blocked by:** —
**Related:** #008 (dependency_graph_foundation)

## Goal

Implement the minimal canonical file-level graph container: `NodeId`,
path normalization, add node/edge, adjacency, reverse adjacency, edge
existence.

## Done

- **2026-05-26** — created `include/archcheck/graph/node_id.h` (`struct NodeId`, `operator==/!=`, `std::hash` specialization).
- **2026-05-26** — created `include/archcheck/graph/dependency_graph.h` (public API: `add_node`, `add_edge`, `has_edge`, `successors`, `predecessors`, `node_count`, `path_of`).
- **2026-05-26** — implemented `src/graph/dependency_graph.cpp` (POSIX + `./`-prefix normalization, idempotent `add_node` / `add_edge`, synchronous forward + reverse adjacency).
- **2026-05-26** — `archcheck_core` extended with `graph/dependency_graph.cpp` in `src/CMakeLists.txt`.
- **2026-05-26** — added `tests/unit/graph/dependency_graph_test.cpp` (12 cases), wired into `tests/CMakeLists.txt`.
- **2026-05-26** — Debug build green, `ctest` 51/51 (39 prior + 12 new graph cases).
- **2026-05-26** — `lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/` — clean.

## How it works

The container stores files as `NodeId` — a wrapper around `std::uint32_t`, handed out monotonically: `id.value == index` in `paths_`. This gives `O(1)` `path_of` and a compact hash.

`add_node` normalizes the path before placing it into `path_to_id_`:
- `\` → `/` (POSIX separator);
- leading `./` are stripped, multiple levels supported (`./././foo` → `foo`).

If the normalized path is already in `path_to_id_` — the previous `NodeId` is returned (idempotency).

`add_edge(from, to)` writes to both tables synchronously — `forward_[from]` and `reverse_[to]` — but only if the edge doesn't yet exist (linear check via `std::find`; for a file-level graph the fan-out is small). This symmetry is an invariant: every edge lives in both halves, which makes `predecessors` a cheap operation without rebuilding.

`successors` / `predecessors` return `const std::vector<NodeId>&`, and an empty static vector when a node is absent (so the caller can iterate without checks).

## What controls it

- No flags / environment variables at this stage.
- Wired in via `archcheck::core` (CMake target).
- Normalization behavior is fixed in `normalize_path` inside `dependency_graph.cpp` (POSIX separator + strip of leading `./`).

## What it relates to

- The `graph/` subsystem — the first file in `src/graph/` and in `include/archcheck/graph/`.
- Dependencies: STL only (`<unordered_map>`, `<vector>`, `<string>`, `<algorithm>`).
- `archcheck_core` (STATIC) now contains `scan/include_scanner.cpp` and `graph/dependency_graph.cpp`.
- Hands the baton to #014 (algorithms: SCC, reachability, levelization) — this container stores them but doesn't implement them.

## Diagnostics

- If the linker can't find `DependencyGraph::*` — check that `src/graph/dependency_graph.cpp` is in the source list of `archcheck_core` in `src/CMakeLists.txt`.
- If a test can't find the header — check that `${CMAKE_SOURCE_DIR}/include` stays `PUBLIC` on `archcheck_core` (tests link against `archcheck::core` and inherit the include path).
- If `add_node` returns different `NodeId`s for logically identical paths — the path wasn't brought to canonical form before the call; the normalization in `dependency_graph.cpp` is deliberately conservative (POSIX separator and leading `./` only), not a full `lexically_normal()`.

## Key decisions

| Decision | Reason |
|---------|---------|
| Reverse adjacency stored upfront | Lakos Ca / reverse-reachable is cheaper to obtain upfront than to rebuild |
| `NodeId` — a `uint32_t` wrapper | Cheap hash, less memory than storing strings everywhere |
| Return `const std::vector<NodeId>&`, not `std::span` | Simple API, no C++20 dependency on the caller; empty static vector for absent nodes |
| Idempotency of `add_edge` via linear `find` | File-level fan-out is small; an `unordered_set` per node is unnecessary memory overhead |
| `normalize_path` — minimal (POSIX + `./`), not `std::filesystem::lexically_normal` | YAGNI: the task is to eliminate trivial duplicates, not to build a normalizer; `..` and absolute paths will be handled by the resolver (#012) |
| One file — one class (no `IGraph`) | docs/code_quality.md — no abstractions without a request; container and algorithms (#014) are separate classes by plan |

## Changed files

| File | Change |
|------|-----------|
| `include/archcheck/graph/node_id.h` | new |
| `include/archcheck/graph/dependency_graph.h` | new |
| `src/graph/dependency_graph.cpp` | new |
| `src/CMakeLists.txt` | added `graph/dependency_graph.cpp` |
| `tests/unit/graph/dependency_graph_test.cpp` | new |
| `tests/CMakeLists.txt` | wired in the new test source |
