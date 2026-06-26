# [FIXTURES] Graph integration fixtures

**Date created:** 2026-05-26
**Date started:** 2026-05-26
**Date completed:** 2026-05-26
**Status:** done
**Module:** FIXTURES
**Priority:** blocker
**Complexity:** S (< 1 day)
**Blocks:** #008 (dependency_graph_foundation) — closes the fixtures branch
**Blocked by:** #012 (include_resolver), #015 (graph_diff_primitives)
**Related:** #008 (dependency_graph_foundation)

## Goal

Assemble a minimal set of integration fixtures that verify graph construction
from real code and the diff primitives.

## Done

- **2026-05-26** — 7 fixtures in `fixtures/graph/`:
  - `minimal_dag/` — 3 files, linear chain a→b→c.
  - `single_scc/` — 3 files, cycle a→b→c→a.
  - `new_edge/baseline/` + `new_edge/current/` — diff "edge a→c appeared".
  - `shortcut_edge/baseline/` + `shortcut_edge/current/` — diff "shortcut a→d added on top of a→b→c→d".
  - `cycle_growth/baseline/` + `cycle_growth/current/` — diff "SCC grows from 2 to 3" (free c joins the 2-cycle via `b→c`, `c→a`).
  - `unresolved_include/` — a single `.cpp` with `#include "missing.h"`.
  - `ambiguous_include/` — `main.cpp` with `#include <util.h>`, two implementations in `sub_a/util.h` and `sub_b/util.h`.
- **2026-05-26** — `tests/integration/graph/end_to_end_test.cpp` with 7 TEST_CASE entries, one per fixture. The `build_graph(root)` helper runs `discover_files → scan_includes → resolve_includes → DependencyGraph::add_edge` and returns the graph + external/unresolved/ambiguous counters.
- **2026-05-26** — `tests/CMakeLists.txt` wires in the new integration source. Full count 115/115, lizard green.

## How it works

The pipeline of each fixture:

```
fixtures/graph/<scenario>/ → discover_files
                          → scan_includes per file
                          → resolve_includes (project / external / unresolved / ambiguous)
                          → DependencyGraph: add_node + add_edge (project only)
                          → compute_scc / added_edges / grown_sccs
```

Diff scenarios (`new_edge`, `shortcut_edge`, `cycle_growth`) have two
`baseline/` and `current/` subdirectories side by side. The test builds a graph
for each and passes the pair to `added_edges` or `grown_sccs`. No git magic is
required.

`ambiguous_include` is set up through the angle-include `<util.h>`: for an angle
include the resolver goes straight to exact + suffix-search, skipping
dir-relative. The two candidates `sub_a/util.h` and `sub_b/util.h` produce the
same suffix `util.h` → ambiguous.

`unresolved_include` — a single-file case with `#include "missing.h"`: dir-rel
does not find it, exact does not find it, suffix does not find it → Unresolved.

## What controls it

- `ARCHCHECK_FIXTURES_DIR` — compile definition from `tests/CMakeLists.txt`, the path to the `fixtures/` root.
- There are no other flags.

## What it relates to

- Uses all pipeline layers: `scan/` (scanner + resolver + project_files), `graph/` (container + algorithms + diff).
- Does not depend on the baseline format (#016) — the diff is made between two graphs built directly from the file systems. When `archcheck --diff` (#018) with `git_state` arrives, these same fixtures can be reused via a git repo.
- Does not use scanner fixtures (#008h) — those are single-file scenarios for the scanner itself.

## Diagnostics

- If a new fixture is not found — check that the path in `graph_fixture("…")` matches the directory name on disk.
- If a diff is unexpectedly empty — the `baseline/` and `current/` fixtures may diverge in file structure, not only in content. The resolver compares paths, so renaming a file = "node removed + node added", not "node changed".
- If SCC-related tests fail — `compute_scc` is stabilized (deterministic), but fixtures with equivalent cycles can yield differently shaped SCC vectors. The check is by `size`, not by composition.

## Key decisions

| Decision | Reason |
|---------|---------|
| Fixtures are real `.h`/`.cpp` files, not in-memory strings | This is exactly what tests zero-config discovery — if discovery misses something, the tests will notice |
| `baseline/` + `current/` subdirectories for diff | A simple way to show "before" and "after" without git machinery. #018 will later be able to reuse it via `git_state` |
| Angle-include `<util.h>` for the ambiguous case | A quote include searches dir-relative first, which would complicate the fixture; an angle include goes straight to exact + suffix |
| The `build_graph(root)` helper is assembled in the test TU, not in src/ | YAGNI — for now this unifying "end-to-end builder" is needed only by the tests; when #018 / CLI arrives, we will extract it into src/ |
| One TEST_CASE per fixture | Readable, easy to run separately via `ctest -R`, the error message points directly at the specific scenario |

## Changed files

| File | Change |
|------|-----------|
| `fixtures/graph/minimal_dag/{a,b,c}.h` | new |
| `fixtures/graph/single_scc/{a,b,c}.h` | new |
| `fixtures/graph/new_edge/{baseline,current}/{a,b,c}.h` | new |
| `fixtures/graph/shortcut_edge/{baseline,current}/{a,b,c,d}.h` | new |
| `fixtures/graph/cycle_growth/{baseline,current}/{a,b,c}.h` | new |
| `fixtures/graph/unresolved_include/main.cpp` | new |
| `fixtures/graph/ambiguous_include/main.cpp` + `sub_{a,b}/util.h` | new |
| `tests/integration/graph/end_to_end_test.cpp` | new (7 cases + helper) |
| `tests/CMakeLists.txt` | integration test source wired in |
