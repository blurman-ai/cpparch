# [SCAN] Resolver: false self-edge on a system-include suffix collision

**Created:** 2026-05-31
**Started:** 2026-05-31
**Completed:** 2026-05-31
**Status:** completed
**Module:** SCAN
**Priority:** major
**Difficulty:** S
**Blocks:** —
**Blocked by:** —
**Related:** #036 (ambiguous_mirror_dirs — same class, the ambiguous variant), #054 (ai_repo_duplication_run — where the bug surfaced), #057 (lakos_fanout_coupling_checks — the `[SELF]` detector is a candidate there)

## Goal

Remove the false dependency edge X→X that the resolver created when a system/library
`#include <name.h>` collided by suffix with a same-named project file.
Such an edge gave birth to a phantom 1-node cycle and made SF.9 lie.

## Context

The bug surfaced not from a unit test but from the research run #054: the cheap
graph detector `[SELF]` ([experiments/ai_repo_run/graph_probe.py](../../experiments/ai_repo_run/graph_probe.py))
flagged a self-include in 16 repos of the corpus. A manual `grep` check showed: almost
all of them are **not** `#include "self"` but system angle-bracket includes:

```bash
grep -n include _aidev_run/bitcoin/src/compat/cpuid.h   # 11: #include <cpuid.h>   (system!)
grep -n include _aidev_run/esphome/.../md5/md5.h        # 18: #include <md5.h>     (NOT its own)
grep -n include _aidev_run/FastLED/src/fl/stl/alloca.h  # 37: #include <alloca.h>  (system)
```

### Root cause (trace for `bitcoin/src/compat/cpuid.h` + `<cpuid.h>`)

1. `resolve_angle` → `find_exact("cpuid.h")` — miss (the index holds the full path
   `src/compat/cpuid.h`, not `cpuid.h`).
2. → `resolve_by_suffix` → `suffixIndex["cpuid.h"]` returns **the source itself**
   (its path ends with `cpuid.h`).
3. `candidates.size() == 1` → `make_project(... candidates.front())` → **edge X→X**.

The same class as #036, but #036 fixed the *ambiguous* case (2+ candidates, mirror filter);
here there is a **single** candidate, so the filter didn't fire.

### Scale (across the `_aidev_run/` corpus, 68 repos)

- **26 false self-edges**.
- **8 repos where the self-loop was the ONLY "cycle"** (sccs_cyclic entirely false):
  bitcoin, cvxpy, KataGo, llama.cpp, nntrainer, ScalableVectorSearch,
  scenario_simulator_v2, whisper.cpp → SF.9 reported a nonexistent cycle for them.

## Solution

A minimal, indisputable invariant: **a component does not depend on itself**. One guard in
`resolveInclude` (the single point after both resolve paths — catches both the
angle-suffix and the quote-dir-relative variants):

- a self-target (`files[target].path == sourceFile`) is downgraded to a not-found tag:
  External for `<...>`, Unresolved for `"..."` (exactly what a system header
  is);
- a legitimate edge to a *different* same-named file (path ≠ source) is not touched.

## Done

- [x] Guard in `resolveInclude` ([src/scan/include_resolver.cpp](../../src/scan/include_resolver.cpp)).
- [x] +3 unit tests ([tests/unit/scan/include_resolver_test.cpp](../../tests/unit/scan/include_resolver_test.cpp)):
      angle system-suffix → External; quote token-suffix → Unresolved; same
      basename in a DIFFERENT file → still Project (the guard does not overreach).
- [x] Clean build; **tests 260/260**; **lizard 0 warnings**.
- [x] Corpus recheck after the fix: **self-edges 0/68**; all 8 phantom
      cycles gone (cyclic=0); the real tangles intact (mc2 56, OptiScaler 13,
      FastLED 11, acts 46, esphome 1 — in place, i.e. the real 2-node cycle of
      esphome is not suppressed).
- [x] CHANGELOG.md — bullet in `### Fixed`.
- [x] GRAPH_PROBE_FINDINGS.md §3 — diagnosis/fix/check.

## How it works

`resolveInclude` first calls `resolve_quote`/`resolve_angle` as before,
then checks the result: if the verdict is `Project` and the target node is the
`sourceFile` itself, the edge is dropped (downgraded to External/Unresolved by the kind
of directive). The check is by full repo-relative path (`files[target].path ==
sourceFile`), so a same-named file in another directory stays a valid target.
The guard is single and sits at the common point, so it covers both resolve paths without
duplication.

## What controls it

Nothing — a zero-config invariant, no thresholds or flags. A self-edge is never
meaningful, neither as a dependency nor as a cycle.

## What it relates to

- **#036** — a related bug (suffix collision), but the ambiguous variant (2+
  candidates → mirror-dir filter). This fix is the single-candidate variant, which
  #036 did not cover.
- **#054** — the research run in which the `[SELF]` detector exposed the bug.
- **SF.9 / DRIFT.2** — graph consumers that were getting phantom cycles; after the
  fix they stop.

## Diagnostics

If a self-loop suspicion arises again:
```bash
build/debug/src/archcheck --save-graph-baseline /tmp/g.txt <repo>
python3 experiments/ai_repo_run/graph_probe.py /tmp/g.txt <repo> | grep -A3 '\[SELF\]'
```
Any `[SELF]` firing should now be a real `#include "self"` —
check `grep -n include <file>`: quotes + the same relative path = a genuine
defect; angle brackets = look whether a resolver regression came back.

## Changed files

| File | Change |
|------|-----------|
| `src/scan/include_resolver.cpp` | guard against the self-edge in `resolveInclude` |
| `tests/unit/scan/include_resolver_test.cpp` | +3 tests (angle-self / quote-self / different-file) |
| `CHANGELOG.md` | bullet in `### Fixed` |
| `experiments/ai_repo_run/GRAPH_PROBE_FINDINGS.md` | §3 diagnosis/fix/corpus check |
