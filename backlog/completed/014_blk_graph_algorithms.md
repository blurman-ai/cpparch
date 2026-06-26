# [GRAPH] Graph algorithms — SCC + reachability

**Created:** 2026-05-26
**Started:** 2026-05-26
**Completed:** 2026-05-26
**Status:** done
**Module:** GRAPH
**Priority:** blocker
**Difficulty:** M (1-2 days)
**Blocks:** #015 (diff_primitives)
**Blocked by:** #013 (graph_container)
**Related:** #008 (dependency_graph_foundation)

## Goal

Implement the primitive algorithms: SCC (Tarjan), forward reachable set, reverse
reachable set, path existence. Each one is a separate function, with no shared
"engine".

## Done

- **2026-05-26** — created `include/archcheck/graph/algorithms.h` (declarations of `compute_scc`, `reachable_from`, `reverse_reachable_from`, `has_path` in `archcheck::graph`).
- **2026-05-26** — implemented `src/graph/algorithms.cpp`: iterative Tarjan on an explicit frame stack, BFS traversals over successors / predecessors, `has_path` with early exit.
- **2026-05-26** — `archcheck_core` extended with `graph/algorithms.cpp` in `src/CMakeLists.txt`.
- **2026-05-26** — added `tests/unit/graph/algorithms_test.cpp` (14 cases: empty graph, DAG, single cycle, two components, determinism, forward / reverse symmetry, self-path, edge direction), wired into `tests/CMakeLists.txt`.
- **2026-05-26** — Debug build green, `ctest` 76/76.
- **2026-05-26** — `lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/` — clean.

## How it works

Four free functions in `archcheck::graph` — no shared "engine", each algorithm is self-contained.

**`reachable_from` / `reverse_reachable_from`** — a shared internal `bfs(g, start, forward)` traverses neighbors via `successors` or `predecessors` depending on the flag. The start node is included in the result. The `visited` hash set serves both as the duplicate filter and as the returned set.

**`has_path`** — a separate BFS that does not build the full reachability set: it checks the target right when expanding a neighbor and exits. The `from == to` case is handled up front (a trivial self-path is always true, so it doesn't depend on the presence of a self-loop in the graph).

**`compute_scc`** — iterative Tarjan on an explicit frame stack. State is held in the `TarjanState` struct:
- `index_of`, `lowlink_of` — arrays sized by `node_count()`, indexed by `NodeId.value` (we exploit the fact that ids form a dense range 0..n-1, see #013);
- `on_stack` — `vector<bool>` sized by `node_count()` for O(1) "node still in the current component" checks;
- `component_stack` — the node stack S from classic Tarjan;
- `call_stack` — a stack of **frames** `{node, succ_index}`, replacing C++ recursion;
- `next_index` — monotonic DFS-order counter;
- `result` — output components.

State machine (`run_tarjan_from`):
1. Push the root onto `call_stack`, `open_node` assigns it an index/lowlink, pushes it onto `component_stack`, marks `on_stack`.
2. Main loop: while `call_stack` is non-empty, look at the top frame.
3. `step_into_successor` walks `successors(frame.node)` starting from `frame.succ_index`:
   - if the neighbor is not yet visited — push a new frame for it and return `true` (the main loop will call `open_node` for the new frame and continue with it);
   - if the neighbor is `on_stack` — `lowlink[v] = min(lowlink[v], index[w])` (back-edge within the current SCC);
   - otherwise — the neighbor is in an already emitted SCC, ignored;
   - `succ_index` is incremented on each pass, so that on returning to a frame already processed edges are not re-examined.
4. When neighbors are exhausted, `close_node` checks `lowlink[v] == index[v]` — the SCC root — and `emit_component` pops nodes off `component_stack` up to and including v. Then the frame is popped, and the parent's `lowlink` is updated with `lowlink[v]` (this step replaces "after returning from recursion" in the textbook version).
5. The outer `compute_scc` loop runs `run_tarjan_from` for every root with `index_of[i] == kUnvisited`.

After collecting SCCs, `sort_components` sorts the nodes within each component and the components themselves by the `NodeId.value` of the minimum node — this gives deterministic output regardless of traversal order (Tarjan itself is order-dependent).

## Controlled by

- No flags / environment variables.
- Linked via `archcheck::core` (CMake target).
- SCC sorting is hardcoded to ascending `NodeId.value`; there is no separate "original Tarjan order" option — determinism is an invariant for CI.

## Related to

- Depends on `DependencyGraph` (#013): uses `node_count()`, `successors()`, `predecessors()`. NodeId is treated here as a dense index 0..n-1 — this is the container contract fixed in #013.
- Out-of-repo dependencies: only STL (`<algorithm>`, `<queue>`, `<unordered_set>`, `<vector>`, `<limits>`, `<cstdint>`).
- Hands off to #015 (diff primitives): added / removed edges and components will be computed on top of these SCC and reachability primitives.
- These same primitives will underpin cycle detection rules (Lakos) and the CCD / ACD / NCCD metrics (#006 / Martin metrics in v0.2+).

## Diagnostics

- If `compute_scc` returns an unstable order on the same input — that's a sorting bug, check `sort_components`: comparators must compare by `NodeId.value`, not by address.
- If a stack overflow is hit on large graphs — the algorithm degraded to recursion; make sure `run_tarjan_from` stays on the while-loop with the explicit `call_stack`, with no self-calls.
- If `reachable_from(g, x)` does not contain `x` — the start node insertion into `visited` before the main BFS loop was forgotten.
- If `has_path(g, a, a)` returned `false` for a node without a self-loop — the early `from == to` check at the start of the function was missed.
- `std::unordered_set<NodeId>::contains` — only GCC 9+ / C++20-lib. In tests we use `count(x) == 1`, because the environment includes GCC 8.3 (see the `stdc++fs` fallback in CMake).

## Key decisions

| Decision | Reason |
|----------|--------|
| Iterative Tarjan on an explicit frame stack | Recursion would crash with stack overflow on 100k+ files |
| `vector<uint32_t>` for `index_of` / `lowlink_of`, not `unordered_map` | NodeId is dense 0..n-1 (the #013 contract), an array is faster and more compact |
| `kUnvisited = UINT32_MAX` as the "not visited" marker | Doesn't require a parallel `visited` array; UINT32_MAX is explicitly impossible as a real DFS index |
| Helpers `open_node` / `close_node` / `step_into_successor` / `emit_component` | Splitting to stay under the lizard 30-line / CCN 15 thresholds; each function handles one step of the state machine |
| Shared `bfs(g, start, forward)` for forward / reverse | The logic is identical, branching only on the choice of `successors` vs `predecessors`; duplicated code would be AI slop |
| `has_path` — a separate function, not `reachable_from(g, from).count(to)` | Early exit saves traversal on large graphs with a nearby target |
| Deterministic SCC order via sorting by min `NodeId` | A CI tool must produce identical output on identical input; Tarjan itself is order-dependent |
| Free functions, not `DependencyGraph` methods | The #008 plan: "no unnecessary abstraction"; the container and the algorithms evolve independently |
| `r.count(x) == 1` in tests instead of `r.contains(x)` | `unordered_set::contains` is C++20, available from GCC 9; the environment includes GCC 8.3 |

## Changed files

| File | Change |
|------|--------|
| `include/archcheck/graph/algorithms.h` | new |
| `src/graph/algorithms.cpp` | new |
| `tests/unit/graph/algorithms_test.cpp` | new |
| `src/CMakeLists.txt` | added `graph/algorithms.cpp` |
| `tests/CMakeLists.txt` | wired in the new test source |
