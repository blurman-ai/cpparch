# [SCAN] lateral_drift_scan: a file-split must not give birth to a NEW event

**Created:** 2026-06-12
**Started:** 2026-06-12
**Completed:** 2026-06-12
**Status:** done
**Module:** SCAN
**Priority:** minor
**Difficulty:** small
**Blocks:** —
**Blocked by:** —
**Related:** #115 (full run, source of the FP — §8.3/§8.6 of the report), #117 (back-edge confirm — example of reading sources at a sha)

> Anchors in `experiments/ai_repo_run/lateral_drift_scan.py` taken on 2026-06-12.

## Goal

Remove the "file-split" FP class: moving types out of an existing header into a NEW header
+ re-include by consumers gives a false LATERAL.NEW (1 of 3 FPs in eyeball #115, §8.3
[lateral_module_drift_corpus_run.md](../../docs/research/lateral_module_drift_corpus_run.md)).

## Context

FP mechanics: a commit moves the contents of `B/old.h` into the newborn `B2/new.h`
(another module directory), consumers from module A switch their include. The edge
`A → B2` is formally new — but this is content relocation, not new connectivity.

The current rename heuristic (`IncrementalGraph`, lines 308-347) catches only a relocation
**that preserves the basename**: `looks_like_move()` (line 325) checks
`Path(to_f).name in self.target_basenames.get(from_f, ())` — i.e. "from_f already included
a target with the same name". When the header gets a NEW name during the move — the heuristic
is powerless (the same root cause as the scylladb FP `compress.hh → sstables/compressor.hh`
from #111 eyeball).

The scan already has ready building blocks:
- `touched_files(repo_dir, sha)` (line 164) — list of files in the commit
  (`git show --no-renames --name-only`);
- `confirm_backedge()` (line 181) — example of reading live sources at a revision
  (`git show <sha>:<path>`), and `_INCLUDE_RE` is there too (line 178).

## Algorithm (split-signature detector)

New function `detect_splits(repo_dir, sha, candidate_targets) -> dict[to_f, origin_f]`:

1. Candidate — an event whose target `to_f` **did not exist** at `sha~1`
   (check: `git cat-file -e <sha>~1:<to_f>`; rc != 0 → file is new).
2. Take `touched = touched_files(repo_dir, sha)`; select from them the **modified**
   (not new) headers `H_old` (by `_HEADER_EXTS`, line 73, and existence
   at `sha~1`).
3. For each pair (`H_old`, `to_f`):
   - `old_lines = set(non-empty lines of git show sha~1:H_old)`;
   - `new_lines = list(non-empty lines of git show sha:to_f)`;
   - ratio `|new_lines ∩ old_lines| / |new_lines| >= 0.5` → **split**:
     the content of the new header used to live half in the old one.
4. When a split is found: the module pair `(areaOf(consumer), module(to_f))` does NOT give birth to
   an event if the pair `(areaOf(consumer), module(H_old))` already existed in the graph —
   i.e. the event "inherits" the existence of the pair from the donor file. If the donor is in the same
   module as to_f — nothing changes (the pair is the same anyway).
5. Register in `IncrementalGraph` an alias `to_f ≈ H_old` (add
   `self.split_origin: dict[str, str]`), so that subsequent consumer commits
   also do not give birth to events on this pair.

Cost: 2 git calls per candidate + one per modified header of the commit —
executed ONLY for commits that already gave birth to a candidate event (hundreds on the corpus,
not thousands), same as confirm_backedge.

## Insertion points

- `scan_repo()` (line 390): events are collected during the replay — insert the split filter
  in the same place where the other candidate post-filters work
  (system-basename — line ~502, untouched-resolution — line ~506);
  do the split check AFTER the cheap filters (it is git-expensive).
- Threshold 0.5 — constant `SPLIT_CONTENT_RATIO = 0.50` near the others
  (lines 35-38), with a comment.

## Validation

- [ ] Unit on a synthetic repo (3 commits): (a) move half the lines of old.h →
      new.h in another directory + consumer switches → 0 events;
      (b) a genuinely new header with unique content → event present;
      (c) move into new.h WITHOUT consumers switching, the consumer arrives after
      5 commits → 0 events (the alias works across time).
- [ ] Re-run the full corpus (479 jsonl, same baseline dir):
      compare `lateral_drift_new.csv` before/after — diff of events.
      Expectation: the file-split FP from #115 §8.3 disappears; CYCLE events are not lost
      (split affects only the existence of the pair, not the back-edge).
- [ ] Every disappeared event by eye (there will be few): all are genuine splits.
- [ ] Update the numbers in `lateral_module_drift_corpus_run.md` §8.2, add a
      line about the change (following the #117 callout in §8.3).

## Done

- Constant `SPLIT_CONTENT_RATIO = 0.50` added (line 39).
- `IncrementalGraph.split_origin: dict[str, str]` added in `__init__`.
- `detect_splits(repo_dir, sha, candidate_targets)` implemented (lines 260-330):
  - lazy git cat-file -e on sha~1 to check the novelty of to_f
  - donor = modified (not new) headers from touched_files
  - overlap ≥ 50% → split
- split_cache (sha → dict) added before the main loop
- Filter in scan_repo() after the touched-guard: lazy `detect_splits` on the first candidate,
  `split_origin` alias for future commits, donor-pair check in mod_edges/mod_pair_first.

## In progress

- Waiting for Boxedwine64 jsonl to check the removed field (#120 validation).

## Next steps

1. Synthetic test (the 3 scenarios from the task) — optional.
2. The full re-run is already done (see below) together with #121.

## Key decisions

| Decision | Reason |
|---------|---------|
| Content signature (≥50% of lines from the donor), not a name heuristic | The name during a split is arbitrary — the basename heuristic fundamentally cannot catch it |
| Check only for candidate events | git calls are expensive; candidates number in the hundreds, commits in the tens of thousands |
| Pair inheritance from the donor + alias for the future | Consumers do not switch in a single commit — the alias suppresses the tail |

## Changed files

| File | Change |
|------|-----------|
| `experiments/ai_repo_run/lateral_drift_scan.py` | `detect_splits()` + `split_origin` in IncrementalGraph + filter in scan_repo |
| `experiments/ai_repo_run/lateral_drift_new.csv` | Regeneration |
| `docs/research/lateral_module_drift_corpus_run.md` | Number update + callout |

## How it works (summary)

**Principle.** `detect_splits(repo_dir, sha, candidate_targets)` recognizes "content relocation":
a new header `to_f` (did not exist at `sha~1`), ≥50% of whose non-empty lines used to live in
a modified donor header `H_old` of the same commit. This is a code relocation, not new connectivity —
a LATERAL.NEW event is not born if the pair `(consumer_module, donor_module)` was already in the graph.

**Alias for the future.** `IncrementalGraph.split_origin[to_f] = H_old`: consumers often
switch the include not in a single commit, so the pair inherits its existence from the donor in
subsequent commits too (the tail is suppressed across time).

**Cost.** git calls (`cat-file -e`, `show`) only for commits that already gave birth to a
candidate event, and only for header targets. The `split_cache[sha]` cache — one pass per commit.

## What controls it

`SPLIT_CONTENT_RATIO = 0.50` (share of shared lines) and `_HEADER_EXTS` in
`experiments/ai_repo_run/lateral_drift_scan.py`. The filter is inserted into `scan_repo()` after the
cheap post-filters (split is git-expensive).

## What it relates to

Part of the lateral-drift corpus scanner (Python twin of C++ DRIFT.4 #118). Batch neighbors:
#120 (removed lists — precise rename signal) and #121 (grace period). The source of the
FP class — eyeball #115 §8.3 (scylladb `compress.hh → sstables/compressor.hh`).

## Diagnostics

Re-run together with #121: corpus of 479 repos. Combined #119+#121 contribution: NEW −97,
SDP −21 (the separate contribution was not split out — both suppress via "pair immaturity"). Checking a
specific split: compare `git show sha~1:H_old` and `git show sha:to_f` for ≥50% shared lines.
