# [GRAPH] Graph-baseline contract — format and I/O

**Created:** 2026-05-26
**Started:** 2026-05-26
**Completed:** 2026-05-26
**Status:** done
**Module:** GRAPH
**Priority:** blocker
**Complexity:** M (1-2 days)
**Blocks:** #008 (dependency_graph_foundation) — closes the graph branch
**Blocked by:** #015 (graph_diff_primitives)
**Related:** #008 (dependency_graph_foundation), #009 (ai_drift_regression_rules)

## Goal

Pin down a minimal, stable `graph-baseline` format:
`format_version + normalized nodes + normalized edges`, plus save/load.

## Context

See §5 mini-design in #008: "`graph-baseline` stores only nodes + resolved
project edges". This is the contract for CI: the baseline lives in the repo, is compared with
the current graph, and any drift violations show up in the diff.

The format is deterministic (sorted nodes/edges), human-readable
(YAML via `ryml`, already a dependency of the whole core), with an explicit version.

## Done

- **2026-05-26** — format specification `docs/baseline_format.md` (~1 page): YAML schema, determinism rules, version policy, the list of `BaselineLoadError::Kind`.
- **2026-05-26** — public API `include/archcheck/graph/baseline.h`: `save_baseline`, `load_baseline`, structured `BaselineLoadError`.
- **2026-05-26** — implementation `src/graph/baseline.cpp`: manual YAML emit (determinism without the ryml emitter) + parsing via `ryml::parse_in_arena` with an error-callback → exception → `BaselineLoadError`.
- **2026-05-26** — Catch2 tests `tests/unit/graph/baseline_test.cpp` (10 cases): round-trip idempotency, determinism across different insertion orders, failures for each error `Kind`, topology recovery.
- **2026-05-26** — `src/CMakeLists.txt` extended with `graph/baseline.cpp`, `tests/CMakeLists.txt` — with the new test.
- **2026-05-26** — Debug build green, ctest 98/98, lizard `--CCN 15 --length 30 --arguments 5` clean.

## How it works

**Format.** A single YAML document:

```yaml
format_version: "1"
nodes:
   - "include/a.h"
   - "src/b.cpp"
edges:
   - [1, 0]
```

`nodes` — lexicographically sorted normalized paths of the graph nodes.
`edges` — pairs `[from_idx, to_idx]`, indices in the post-sort numbering of
`nodes`, and the edges themselves are sorted by `(from, to)`. This gives byte-for-byte
equality of serializations of two logically identical graphs regardless of
insertion order.

**Save** (`save_baseline`):
1. Collect the node paths from the graph, sort → `sorted_paths`.
2. Build the remap `old_idx → new_idx`.
3. Walk all `successors(NodeId{i})` for all `i`, rewrite the indices, sort the pairs.
4. Emit YAML by hand: control over formatting is more stable than running the ryml emitter for three lines.

**Load** (`load_baseline`):
1. Read the entire stream.
2. `parse_tree`: install a ryml error-callback that throws `RymlParseException`, parse into the arena, restore the default callbacks.
3. Check `format_version` (only `"1"`).
4. Parse `nodes` and `edges` via separate helpers; each error → `BaselineLoadError`.
5. Assemble a `DependencyGraph` via the public `add_node` / `add_edge`.

Errors are represented as `BaselineLoadError{kind, message, line}` and returned in `std::optional` — no exceptions are propagated outward.

## Controlled by

- API only, no flags / environment variables.
- Linked via `archcheck::core` (CMake target); `ryml::ryml` is pulled in
  through the core's PUBLIC dependency.

## Related to

- The `graph/` subsystem — the third file after `dependency_graph.cpp` and `algorithms.cpp`.
- Uses only the public `DependencyGraph` API (`node_count`, `path_of`, `successors`, `add_node`, `add_edge`) — extending the container for this task was not needed.
- Prepares the ground for #015 (graph-diff primitives) and DRIFT.1 / DRIFT.2 (#009).

## Diagnostics

- **A test with malformed YAML hangs without exiting.** This means the default ryml error-callback beat ours — it calls `abort()` / loops forever, because the error handler "is not allowed to return" (see quickstart §sample_error_handler). Check that `set_callbacks(rcb)` is called BEFORE `parse_in_arena`, and that the callback itself is `[[noreturn]]` and throws an exception caught inside `load_baseline`.
- **Round-trip is not idempotent.** Most likely the `edges` are not sorted after the remap, or `sorted_node_paths` lost stability because `path_of` now returns a `string_view` into storage that may have reallocated — in the current code `paths_` is stable for the lifetime of the graph, but on modification keep the string_views in mind.
- **`UnknownVersion` fires in unexpected places.** The version is a **string** `"1"`, not a number. `format_version: 1` (without quotes) also works on read (ryml gives `val()` "1"), but always write it quoted.

## Key decisions

| Decision | Reason |
|----------|--------|
| Edges as `[from_idx, to_idx]`, not pairs of full paths | More compact, and baseline diffs are more stable under line permutations |
| `format_version` as a string (`"1"`), not a number | YAML numbers have their own surprises; a string is simpler |
| One YAML document, not two files | Simplicity for the user; v0.1 doesn't need sharding |
| Save formats YAML by hand, doesn't run the ryml emitter | The ryml emitter gives few guarantees about order/indentation; by hand is cheaper and more precise |
| ryml error callback throws an exception caught in `load_baseline` | ryml contract: the callback is not allowed to return; the alternatives — `setjmp/longjmp` or `abort` — are incompatible with C++ resource management |
| `DependencyGraph` API was not extended | `node_count + path_of + successors` is already enough for a full iteration; we don't breed accessors for a single task (YAGNI) |

## Changed files

| File | Change |
|------|--------|
| `docs/baseline_format.md` | new (mini-spec of the format) |
| `include/archcheck/graph/baseline.h` | new (public API) |
| `src/graph/baseline.cpp` | new (ryml-based implementation) |
| `tests/unit/graph/baseline_test.cpp` | new (10 Catch2 cases) |
| `src/CMakeLists.txt` | linked `graph/baseline.cpp` |
| `tests/CMakeLists.txt` | linked `unit/graph/baseline_test.cpp` |
