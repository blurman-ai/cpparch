# [SCAN] SourceSnapshot perf nits: transient 2× memory in read(), O(n) findFile

**Created:** 2026-06-19
**Started:** 2026-06-19
**Completed:** 2026-06-19
**Status:** done (nit 2). nit 1 + measurement moved out to #131
**Module:** SCAN
**Priority:** minor
**Difficulty:** low
**Blocks:** —
**Blocked by:** —
**Related:** #129 (read-once snapshot — this code), b01707a (hoist)

## Goal

Remove two behaviorally neutral inefficiencies in `scan::SourceSnapshot`,
found in the adversarial architecture review of #129. **Both are about speed/memory, not
about correctness**; scanner results don't change. A separate task on purpose:
requires a release rebuild, to be done not "along the way" but with a measurement.

## Context (review findings #129, 2026-06-19)

1. **Transient 2× memory peak in `SourceSnapshot::read`.**
   `read()` copies the content of each file twice for the duration of classification —
   once into a vector for `AuthoredScope::fromFiles` (which needs the full set of
   `(path, content)`), a second time into `files()` (`SnapshotFile.content`). The peak is
   bounded and short-lived, but on the largest repos in the corpus under ≤8 parallel
   workers this is wasted headroom. Fix: build the `fromFiles` input from
   `string_view` over the already-read content (or classify on the fly while
   filling `files()`), so the content lives in a single place.

2. **`findFile` — O(n) linear search (O(n·m) for m queries).**
   `SourceSnapshot::findFile(path)` scans the vector. On the diff path, complexity
   does a lookup for each changed file. Not hot right now — the bulk gate (#117,
   `diff_max_added_lines`) limits m on non-bulk commits — but it's a trap for
   the future. Fix: `unordered_map<string_view→index>` alongside the vector.

### (adjacent, optional) empty-content in `authoredSources()`
Review nit: the old `collectNonVendoredSources` (clone) dropped empty files;
`authoredSources()` does not. Output-neutral (empty file → 0 tokens → 0 clones),
but the `fileCount`/`totalLoc` advisory counters get +0 records. If we touch this
file — add `&& !sf.content.empty()` in `authoredSources()` for parity. Not
mandatory.

## Execution plan

- [ ] Measure the current peak RSS on 2-3 large corpus repos (baseline)
- [ ] Nit 1: remove the double content copy in `read()` (string_view / on-the-fly)
- [x] Nit 2: index `findFile` via `unordered_map` — done 2026-06-19
- [ ] (opt.) empty-guard in `authoredSources()` — NOT done: changes the advisory counters
      (`fileCount`/`totalLoc`), and that requires the same golden comparison as nit 1
- [x] Re-verify output identity: 540/540 tests (including complexity-drift and
      new-clone-drift, which exercise `findFile`) + dogfood 0 — bit-identical at this level
- [ ] Rebuild release, measure peak RSS after — confirm the gain (for nit 1)

## Done (2026-06-19) — nit 2

`SourceSnapshot::findFile` was an O(n) linear search over the vector; complexity on
the diff path does a lookup for each changed file. Added an
`unordered_map<string_view→index>`, keys point into the `files_[i].path` buffers.
The snapshot was made **non-copyable** (a copy would reallocate the paths and leave
dangling views; move is safe — vector elements live on the heap and don't move).
All consumers take the snapshot by `const&` or via `optional<SourceSnapshot>`
(move), so banning copy didn't break anything. `findFile` behavior is identical
(the same first-match via `emplace`).

## Why nit 1 was deferred

A clean fix of the double copy requires `AuthoredScope::fromFiles` to accept
`(string_view, string_view)` — that's an overload in `authored_scope.h` (going beyond
"change only source_snapshot.h"), and the ticket itself requires an RSS measurement
before/after on a release build and large corpus repos (golden output comparison
mandatory). This is not an "along the way" edit — left for a separate pass with a measurement.

## Self-check

The main risk is accidentally changing BEHAVIOR while "optimizing" (file order,
handling of empties, classification). This is a refactor under the invariant "output doesn't change":
golden comparison mandatory, not by eye. "Got faster" without proof of output
identity is insufficient.

## Changed files

| File | Change | Commit |
|------|--------|--------|
| include/archcheck/scan/source_snapshot.h | index `findFile` via `unordered_map`; snapshot non-copyable | `29dab88` |

(nit 1 — read() without the double copy — NOT done, see #131.)

## Outcome

**Status:** completed for nit 2 (findFile O(n)→O(1), behaviorally identical).
nit 1 (double content copy in read) and its RSS measurement require a release build +
large corpus repos + golden — moved out to #131 (fresh golden measurement), item C.
