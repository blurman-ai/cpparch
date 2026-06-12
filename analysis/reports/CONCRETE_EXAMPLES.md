# Concrete Code Quality Examples — Verified

**Source:** corpus repos under `/home/localadm/oss`, rescanned directly with
`archcheck` (HEAD full scan + `--diff`) on 2026-06-03. Every claim below is backed
by tool output, `git`, or a file read — not estimation.

> ⚠️ This replaces an earlier draft that was **fabricated**: its graph example
> mislabeled a normal `src→include` edge as "drift", and all of its duplication
> "cases" used invented file names and "probably similar structure" hand-waving
> with zero real pairs. None of that survived contact with the actual tool output.
> What's below is what the tool really reports.

---

## Part 1 — GRAPH: the metric measured churn, and the scan walked the wrong commits

### 1.1 What `graph_errors` actually counted

The corpus `graph_errors` column counts a commit as an "error" if **any** drift
signal fired (`corpus_orchestrator.py:236`):

```python
if any(int(record.get(k, 0)) > 0
       for k in ("added_edges", "removed_edges", "grown_cycles", "new_area_deps")):
    count += 1
```

Breakdown across all **44,645** scanned commits (295 repos):

| signal | commits fired | share of flagged |
|---|---|---|
| `added_edges`   | 6,315 | **98.0%** |
| `removed_edges` |   653 | 10.1% |
| `new_area_deps` |   495 | 7.7% |
| `grown_cycles`  | **0** | **0.0%** |

So 98% of "graph errors" are just **`added_edges > 0`** — a commit added an
`#include`. `src/main.cpp → include/bdd_node.h` (a `.cpp` including its own header)
counts as an "error". This is **development churn**, not an architectural problem.
That is exactly why it shows no correlation with AI% — churn isn't AI-specific.

### 1.2 `grown_cycles = 0` did NOT mean "no cycles"

Real include cycles exist and the detector finds them on a full HEAD scan:

**Krilliac_SparkEngine** (corpus repo):
```
[SF.9] cycle: RHIPipelineTypes.h → RHITypes.h → RHIPipelineTypes.h
```
Verified mutual include:
- `RHIPipelineTypes.h:14` → `#include "RHITypes.h"`  (comment: *"Split from RHITypes.h"*)
- `RHITypes.h:463`        → `#include "RHIPipelineTypes.h"`  (comment: *"Backwards compatibility"*)

Classic self-inflicted cycle: a header was split out, then a back-include added
"for backwards compatibility" → the two now include each other.

**UNIGINE SDK** (`gm/common/unigine/include/`, vendored 2.20.0.1):
```
[SF.9] cycle: UnigineBase.h    → UnigineLog.h        → UnigineBase.h
[SF.9] cycle: UnigineMathLib.h → UnigineMathLib2d.h  → UnigineMathLib.h
[Lakos.ChainLength] Unigine.h: include chain depth 33 exceeds threshold 10
```

### 1.3 Root cause: `--first-parent` skipped the commit that made the cycle

The SparkEngine cycle was introduced **inside the scan window** by:
```
8175be5d  2026-03-18  "Refactor engine: split 25+ oversized headers, fix ODR, consolidate shutdown paths"
```
Both edges of the cycle landed in that single refactor commit. Yet `grown_cycles`
stayed 0 for the repo. Why:

- The per-commit scan walks `git rev-list --first-parent HEAD`.
- `8175be5d` is a single-parent commit **off the first-parent spine** (it lived on
  a branch that was later merged) → first-parent traversal **never visited it** →
  it was never diffed.

Run the check directly on that diff and the tool is correct:
```
$ archcheck --diff 8175be5d~1..8175be5d  Krilliac_SparkEngine
added_edges:   54
grown_cycles:  1     ← the cycle IS detected
new_area_deps: 0
```

**Conclusion:** the `grown_cycles` detector works. The *methodology* was blind —
`--first-parent` skips the feature-branch commits where real work (and real
cycles) happen. On a merge/squash workflow, first-parent sees mostly merge
commits and misses the substance.

**Fix:** scan all non-merge commits (drop `--first-parent`, or walk every commit
with `--no-merges`), then re-aggregate `grown_cycles` as the real architectural
signal — separate from the `added_edges` churn that currently dominates.

---

## Part 2 — DUPLICATION: the top results are fork/vendor, not AI copy-paste

Rescanned the four headline repos. Counts below are real `grep` over the tool's
reported pairs.

### 2.1 CnC_Generals (2,538 pairs) → 96% is base-game vs expansion

README: *"source code for Command & Conquer Generals and its expansion pack
Zero Hour, released by EA under GPL v3."* The repo ships two parallel trees:
`Generals/` (base) and `GeneralsMD/` (Zero Hour). They share almost all code
**by design**.

- **2,433 / 2,538 (96%)** of pairs are `Generals/… ↔ GeneralsMD/…`.
- Spot check `SidesList.cpp:256-284` base vs expansion → **`diff` = 0 lines, identical.**
- This is upstream EA structure imported in a few commits, **not** AI-authored copy-paste.

### 2.2 Dewm-3 (323 pairs) → 81% is base-game vs expansion

README: *"a fork of dhewm3, a Doom 3 GPL source port."* Trees `neo/game/` (base)
and `neo/d3xp/` (Resurrection of Evil expansion).

- **261 / 323 (81%)** of pairs are `neo/game/… ↔ neo/d3xp/…` (e.g. `Entity.cpp`,
  `Misc.cpp`, `Actor.cpp`, `Game_local.cpp`).
- id Software's base+expansion structure, **not** AI copy-paste.

### 2.3 HyperCogWizard (735 pairs) → 99.9% is vendored ggml ×3

The repo bundles `ggml/`, `llama.cpp/`, and `whisper.cpp/`. `llama.cpp` and
`whisper.cpp` each **vendor their own copy of ggml**, so `ggml-backend.cpp` etc.
exist three times.

- **734 / 735 (99.9%)** of pairs touch `ggml` / `llama.cpp` / `whisper.cpp`.
- Pure vendored-library duplication — the classic false-positive class that
  `vendor-exclude` (#071) is meant to drop but doesn't catch here.

### 2.4 The genuine authored copy-paste (real, smaller)

**FiveTechSoft_HarbourBuilder** — `gtk3_db_real.c:217-235` ↔ `hb_db_real.cpp:211-229`,
19 lines byte-identical (a C backend hand-copied into a C++ file):

```c
PHB_ITEM aRet = hb_itemArrayNew(0);
if( g_my.h ) {
   MYSQL * h = (MYSQL *) (HB_PTRUINT) hb_parnint(1);
   if( h && g_my.list_tables ) {
      MYSQL_RES * res = g_my.list_tables( h, NULL );
      if( res ) {
         MYSQL_ROW row;
         while( ( row = g_my.fetch_row( res ) ) ) {
            if( row[0] ) {
               PHB_ITEM s = hb_itemPutC( NULL, row[0] );
               hb_arrayAddForward( aRet, s );
               hb_itemRelease( s );
            }
         }
         g_my.free_result( res );
      }
   }
}
hb_itemReturnRelease( aRet );
```

But even here ~9 of the 23 pairs are **vendored Scintilla** copied into the
`bin/HbBuilder.app/` bundle — `resources/scintilla_src/…` ↔ `bin/…app/…/scintilla/…`.
The genuinely author-written copy-paste is the `gtk3_*` ↔ `hb_*` parallel backend
(a handful of pairs), not the headline count.

### 2.5 The low-dup repo is genuinely clean

**FiveTechSoft_OpenADS** (152 graph_errors, the "graph-heavy" repo): only **3**
duplication pairs, all real small structural near-dups in authored code
(`src/drivers/adi/adi_index.cpp:382-403 ↔ 409-429` self-similar, a test-file
near-dup). No fork tree, no vendored blob — so the count is genuinely 3.

---

## Part 3 — What this means

| Metric | What it actually measured | The fix |
|---|---|---|
| `graph_errors` | 98% include-churn; missed real cycles entirely | scan all commits (not `--first-parent`); report `grown_cycles` separately from churn |
| `dup_pairs` (top) | fork-within-repo (base+expansion) + vendored libs | exclude sibling fork trees and vendored dirs before counting |

The earlier headline — *"AI% does not correlate with graph_errors or dup_pairs"* —
still holds numerically, but the **reason** is deeper and less flattering than the
first draft claimed: both signals, as run, are dominated by **upstream/vendored
structure and dev churn**, not by authored code quality. Before any AI-vs-human
claim can be made, the metrics need:

1. **Graph:** re-run over all non-merge commits; treat `grown_cycles` (and
   chain-depth / cyclic-SCC on HEAD) as the real signal, drop raw `added_edges`.
2. **Duplication:** exclude vendored directories and recognize base/expansion
   sibling trees (`Generals/` vs `GeneralsMD/`, `neo/game/` vs `neo/d3xp/`) before
   counting, so the headline number reflects authored copy-paste only.

---

## Verification log (so nothing here is hand-waved)

- Graph signal breakdown: parsed all 295 `*_graph_drift.jsonl` → 0/44,645 `grown_cycles`.
- SparkEngine cycle: `grep` of the two `#include` lines + full-scan `[SF.9]` output.
- SparkEngine cycle date / off-spine: `git log -S`, `git rev-list --first-parent | grep`.
- Cycle caught on direct diff: `archcheck --diff 8175be5d~1..8175be5d`.
- UNIGINE cycles: full scan of `gm/common/unigine`.
- CnC/Dewm/HyperCog FP shares: `grep -c` over `/tmp/dup_*.txt` reported pairs.
- CnC identity: `diff` of `SidesList.cpp:256-284` across the two trees (0 lines).
- HarbourBuilder real pair: `sed` of both fragments (identical).
