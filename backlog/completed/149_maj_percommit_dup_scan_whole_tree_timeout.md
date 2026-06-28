# [PERF][SCAN] per-commit new-clone scan runs the WHOLE snapshot → timeout on large repos

**Created:** 2026-06-26
**Started:** 2026-06-26
**Status:** completed 2026-06-28 — byte-cap bulk-skip shipped; diff-scope rewrite split into #152
**Module:** SCAN / DUPLICATION / DIFF
**Priority:** major
**Related:** #123 (new-clone drift), #147 (lex-cap memory), #145 (trending run, where it was caught)

## Symptom (trending run #145)

28 of 56 trending repos — **timeout-blacklist** (`--timeout 120`/commit, guard blacklists after 2):
tensorflow ok=2, opencv ok=1, envoy ok=2, godot ok=1, mame ok=0, onnxruntime/openvino ok=1-2,
FreeCAD, Cataclysm-DDA… For the largest (most interesting) repos there's almost no data.

## Cause

For EVERY commit, `detectNewClones` (`new_clone_drift.cpp`) runs TWO whole-tree
duplication scans: `parentPairKeys(parentSnapshot)` = `scanForDuplication` over the WHOLE parent,
plus `scanForDuplication` over the WHOLE child. Clone detection is ~O(files × tokens) over the entire tree →
on a repo with thousands of files a single commit doesn't fit in 120s. #147 fixed memory, but not wall-clock.

## Fix direction

We need a diff scope: search for clones only in the FRAGMENTS that intersect the commit's changed lines, not
in the whole tree. Right now the `touchesAdded` filter is applied AFTER the full scan — too late. Options:
- index only changed files against a tree index (rather than tree×tree);
- or a separate budget/skip duplication on huge snapshots (like complexity bulk_skip #117).

## Profiled (2026-06-27) — confirmed the clone scan dominates

env-gated per-advisory timing (`ARCHCHECK_DEBUG_TIMING`, reverted) on `apache/arrow` HEAD
(1747 authored files, 21 MB): **newClones 4145 ms** of ~4.5 s total; complexity 11 ms,
flagArguments 338 ms, boolFields 10 ms. The new-clone scan is ~92% of the per-commit cost — the
#149 diagnosis holds. `bloomberg/blazingmq` exceeded 120 s on one commit.

## Shipped (2026-06-27) — byte-cap bulk-skip (option b)

`detectNewClones` now skips when the new tree's authored bytes exceed
`thresholds.diff_max_clone_scan_bytes` (default **40 MiB** ≈ 2× arrow), setting
`NewCloneDriftResult::skippedLargeTree`. Advisory-only, so the **gate (cycles/god-headers) still
runs** — a giant repo is now *measured* (gate result obtained) instead of timing out and getting the
whole repo blacklisted. Verified end-to-end: arrow with a 1 KB cap prints "new-clone drift
(advisory): skipped …", completes in 1.05 s (vs 5.30 s scanning), exit 0; at the 40 MiB default arrow
still scans. Config knob wired in YAML (`diff_max_clone_scan_bytes`). 574 tests green, dogfood 0,
lizard clean. Refactored `detectNewClones` into `authoredBytes` + `emitNewClones` to stay ≤30 lines.

This rescues **size-driven** timeouts (the 10k+-file giants: tensorflow/opencv/godot). It does **not**
rescue **pair-explosion** repos (moderate bytes, dense internal duplication → O(pairs) blowup, e.g.
blazingmq under 40 MiB) — those still time out at the default. The corpus harness can set a lower
`diff_max_clone_scan_bytes` to skip more aggressively for a re-run.

## Scope-rewrite ATTEMPTED + reverted (2026-06-27) — it is NOT behaviour-preserving

Tried option a as a contained change: `ScannerOptions::restrictToFiles` (changed-file set) +
a phase-3 skip of candidate pairs touching no changed file, passed to both the parent and new scans.
Index still spans the whole tree, only scoring is scoped. Profiling confirmed it would work for speed
(arrow 5.30 s → **2.58 s**; the unbounded scoring is what scales). 574 tests + 293 clone test-cases
stayed green, and the key negative case (touched-but-pre-existing clone stays silent) held.

**But the mandatory before/after validation killed it.** Ran old (unrestricted) vs scoped binaries on
30 real `xiaozhi-esp32` commits with new-clone findings, comparing actual `file:line:message`:
**17 / 30 differ — the scoped scan emits MORE clones** (e.g. one commit 2 → 20, another 8 → 40), a
superset of the unrestricted output. So restricting the candidate set is **not transparent**: it
interacts with the corpus-level FP filters / dedup / classify stages that were calibrated on the full
candidate set (root cause not yet isolated — index `sharedRare` is identical, so it is downstream in
`applyCandidateFilters`). The `#103` precision (86–91 %) was measured on the *unrestricted* behaviour;
shipping a different clone set without re-validating precision is unacceptable. **Reverted** —
the byte-cap (above) remains the shipped, behaviour-preserving #149 mitigation.

(Self-check note: the count-only comparison first reported "same=30" because the extractor read the
wrong JSON key (`ruleId` vs `rule`) → vacuously 0==0. The real divergence only showed once the
extractor was fixed and content was compared. Classic "the run produced numbers ≠ the conclusion is
right" — the comparison itself needed verifying.)

## Still open — the proper diff-scope rewrite → split into #152

The diff-scope rewrite (make the per-commit clone scan O(diff), not O(tree)) was attempted here and
reverted (it changes the emitted clone set — see above). It is now its own task:
**[#152](../new/152_maj_diff_scope_clone_scan_changed_files.md)** — isolate the set-dependent
`applyCandidateFilters` stage, decide semantics, re-validate #103 precision. That task also covers the
pair-explosion repos (blazingmq-type, under the 40 MiB cap), the corpus re-measure, and unblocking the
agentic-stratum bot-test re-run (#146).

This task (#149) is **done for its shipped scope**: the byte-cap mitigation. Consider it
closeable once #152 is filed (it is).

## How it works (completed summary)

`detectNewClones` in `src/diff/new_clone_drift.cpp` calls `authoredBytes()` on the new snapshot before
doing any token scan. If total authored bytes exceed `Config::thresholds.diff_max_clone_scan_bytes`
(default 40 MiB), it returns `NewCloneDriftResult{skippedLargeTree=true}` immediately — no scan, no
pair explosion. The advisory check is skipped; the gate checks (cycles, god-headers) still run.

## Key decisions

- **Option b (byte-cap) over option a (scope-rewrite)**: scope-rewrite is not behaviour-preserving (17/30 commits diverged); byte-cap is transparent — same output for repos under the cap, explicit skip for repos over it.
- **40 MiB default**: 2× apache/arrow (21 MB), covers all size-driven timeout cases in the trending corpus while leaving most repos below the cap.
- **Config knob exposed** (`diff_max_clone_scan_bytes` in YAML) so corpus harness can set it lower for re-runs.
- **Scope-rewrite deferred to #152** with proper semantics work (isolate `applyCandidateFilters`, re-validate #103 precision).

## Changed files

- `src/diff/new_clone_drift.cpp` — `authoredBytes()` helper + early-exit in `detectNewClones`
- `include/archcheck/config.h` — `diff_max_clone_scan_bytes` threshold field
- `src/config/config.cpp` — YAML key + default 40 MiB
- `tests/` — skippedLargeTree coverage
