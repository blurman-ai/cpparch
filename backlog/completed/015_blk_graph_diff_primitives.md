# [GRAPH] Graph diff primitives — new/removed edges, grown SCC

**Created:** 2026-05-26
**Started:** 2026-05-26
**Completed:** 2026-05-26
**Status:** done
**Module:** GRAPH
**Priority:** blocker
**Complexity:** S (< 1 day)
**Blocks:** #016 (graph_baseline_contract)
**Blocked by:** #014 (graph_algorithms), #012 (include_resolver)
**Related:** #008 (dependency_graph_foundation), #009 (ai_drift_regression_rules)

## Goal

Implement primitives for comparing two graphs: new edges, removed edges,
whether an SCC grew relative to the baseline.

## Context

See the #008 plan: "diff primitives for the first prototype: new edges, removed
edges, grown SCC". This is the foundation without which the first
prototype of `DRIFT.1` / `DRIFT.2` in #009 can't be built.

"Grown SCC" = an SCC of size ≥ 2 appeared where the baseline had none, or
the size of an existing SCC increased.

## Done

- **2026-05-26** — `include/archcheck/graph/diff.h`: `EdgeRef`, `GrownScc`, three free functions in `archcheck::graph`.
- **2026-05-26** — `src/graph/diff.cpp`: implementation of `added_edges`, `removed_edges`, `grown_sccs` via comparison by `path_of(...)`.
- **2026-05-26** — `tests/unit/graph/diff_test.cpp`: 11 cases — new edge, shortcut, new nodes, removed edge, vanished endpoint, no-op, brand-new cycle, grown cycle, unchanged cycle.
- **2026-05-26** — `src/CMakeLists.txt` and `tests/CMakeLists.txt`: new source files wired in.
- **2026-05-26** — Debug build green, `ctest` 98/98, `lizard --CCN 15 --length 30 --arguments 5` — no remarks.

## How it works

All three functions are pure: input is two `const DependencyGraph&`, output is a vector.
Comparison is **by the `path_of(...)` string**, because `NodeId` is an insertion-order
counter and changes from run to run.

- `added_edges` builds a `path → NodeId` index over the baseline, walks all edges of
  current; an edge is added if at least one of its endpoints is absent in the baseline
  by path, or if both paths exist but the edge doesn't (`baseline.has_edge(...)` == false).
- `removed_edges` symmetrically: index over current, traversal of baseline edges. Edges whose
  endpoints have vanished from current are **skipped** — they're redundant for the drift report
  (the node is already removed, a separate event signature).
- `grown_sccs` calls `compute_scc` on both graphs, builds for each non-trivial
  (`size >= 2`) SCC the set of paths, and matches a current-SCC against the baseline-SCC
  with which it has the maximum path overlap (singleton baseline SCCs are ignored —
  they're "not a cycle"). If the matched baseline-SCC is smaller than current (or not found
  at all), the current-SCC ends up in the result with the corresponding `baseline_size`.

The returned `EdgeRef`s always use NodeIds **from `current`** — this is needed for
the subsequent report on top of the fresh graph.

The resulting vectors are deterministically sorted (`added_edges`/`removed_edges` — by
`(from.value, to.value)`), `grown_sccs` follows the order in which `compute_scc` returned them,
which is itself already stable.

## What controls it

- No flags / environment variables.
- The API is entirely header-only in form — three free functions, two POD `struct`s.
- Access via `archcheck::core` (CMake target).

## What it relates to

- **Inside `graph/`:** relies on `DependencyGraph::path_of/has_edge/successors/node_count` and `compute_scc` from #014.
- **Further down the pipeline:** #016 (graph_baseline_contract) — baseline serialization; #009 (`DRIFT.1`/`DRIFT.2`) — turns the diff result into a violation report.
- **Doesn't touch:** `scan/`, `config/`, `report/`, libclang — this is pure graph algorithmics.

## Diagnostics

- If the linker can't find `added_edges/removed_edges/grown_sccs` — check that
  `graph/diff.cpp` is in the source files list of `archcheck_core` in `src/CMakeLists.txt`.
- If `grown_sccs` returns "extra" grown ones — check whether the
  baseline was handed the same graph version as current (e.g. for a unit test — compare
  `compute_scc(baseline)` and `compute_scc(current)`).
- If `removed_edges` returns empty where you expect an edge — check whether
  an endpoint path was renamed (the nodes exist, but under a different path → the diff treats them
  as "new + removed", the edge doesn't match).
- The test file `tests/unit/graph/diff_test.cpp` covers all the key scenarios —
  when changing the diff semantics, **update it first**.

## Key decisions

| Decision | Rationale |
|---------|-----------|
| SCC matching by membership, not by ID | NodeId is stable only within one run, baseline → current doesn't guarantee a match |
| Singleton SCCs in baseline don't count as "matched" | A singleton is not a cycle; otherwise a brand-new cycle `{a,b,c}` would match the singleton `{a}` and not produce `baseline_size=0` |
| `removed_edges` skips edges whose endpoint vanished from current | The drift reporter handles a "removed node" as a separate signal; here — only edges between still-alive nodes |
| `EdgeRef`s use NodeIds of the `current` graph | The diff is consumed by the report built on top of current; baseline NodeIds are useless outside the baseline |
| DependencyGraph API was **not extended** | An extra accessor isn't needed: NodeIds are dense `[0, n)`, iteration via `for i in [0, n)` + `path_of(NodeId{i})` is enough, no point leaking the private container |

## Changed files

| File | Change |
|------|--------|
| `include/archcheck/graph/diff.h` | new |
| `src/graph/diff.cpp` | new |
| `tests/unit/graph/diff_test.cpp` | new |
| `src/CMakeLists.txt` | added `graph/diff.cpp` to `archcheck_core` |
| `tests/CMakeLists.txt` | added `unit/graph/diff_test.cpp` to `archcheck_tests` |
