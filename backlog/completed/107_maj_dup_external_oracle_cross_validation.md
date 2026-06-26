# [DUPLICATION][RESEARCH] Cross-validation of the duplicate detector against external oracles (NiCad + Bellon)

**Created:** 2026-06-11
**Started:** 2026-06-11
**Status:** wip — data collected, quantitative comparison blocked (see Progress)
**Module:** DUPLICATION / RESEARCH / EXPERIMENTS
**Priority:** major
**Complexity:** L
**Blocks:** a confident precision/recall claim for the duplication layers
**Blocked by:** —
**Related:** #053/#056/#052/#059/#054 (duplication layers), #106 (PMD known-answer fixtures)

## Goal

Run our duplicate detector on reference corpora from the clone-detection world and
compare against their labeled oracles, to get **numbers** (TP/FP, roughly
precision/recall), not just "works on our fixtures". We need an external,
not-made-by-us ground truth.

## Context

Two independent external sources with real C code:

1. **NiCad** (the clone detector itself) — ships with a real C system
   `monit-4.2` as an example:
   https://github.com/bumper-app/nicad/tree/main/examples
   Scenario: run NiCad (functions/blocks, Type-1/2/3) → its XML clone markup
   as the oracle → run our detector → compare. NiCad is installed via TXL
   (download: http://www.txl.ca/txl-nicaddownload.html).

2. **Bellon benchmark** — the academic gold standard: a manually labeled
   reference corpus of clones in C systems (cook, weltab, etc.), 4319 validated
   clone pairs. Recall of clone detectors has been measured against it for 20 years. Downloaded from
   Koschke's group (Bremen), links verified live 2026-06-11:
   - oracle + detector results (RCF, ~18 MB):
     https://www.softwareclones.de/download/bellon_benchmark.tar.gz
   - system sources (ISO, ~167 MB):
     https://www.softwareclones.de/download/bellon_sources.iso
   - format/scripts described at https://www.softwareclones.de/research-data.php
   Downside: the RCF format + its own "good/ok match" metric — we need an adapter from our
   JSON output of clone pairs into their comparison schema.

3. (boundary, not recall) **POJ-104 / IBM CodeNet (C++)** — Type-4 semantic clones.
   Our token layer by design must NOT catch them. Useful as a test of the FP boundary
   ("we don't flag this"), not of recall. Optional, at the end.

Context on our layers and FP classes — docs/duplication_architecture.md.

## What to do

1. **NiCad first (cheaper):** clone `monit-4.2`, install NiCad/TXL,
   capture its clone oracle; run our detector on the same sources;
   write the comparison in `experiments/` (our JSON ↔ NiCad XML, by overlap of
   `file:line` ranges). Record TP/FP and the classes of discrepancies.
2. **Bellon:** download the tarball+ISO, parse the RCF oracle, write an adapter from
   our output → their format, compute recall on cook/weltab by their metric
   (account for the "good" vs "ok" overlap threshold). All under `experiments/`, reproducible
   by a script.
3. Summarize a report: on which clone classes we're strong/weak, where the FP are, how
   selective normalization affects things. Tie the conclusions to #059 (precision).
4. (opt.) POJ-104 sanity: confirm that we do NOT flag Type-4 en masse.

## Definition of Done

- A reproducible run on NiCad/monit with TP/FP numbers and an analysis of the discrepancies.
- (Desirable) recall on Bellon cook/weltab via the adapter, or an explicitly
  documented blocker, if the RCF adapter turns out to be more expensive than the task budget.
- A report in `docs/research/` or `experiments/` with conclusions about the layers and a tie to #059.
- We do NOT commit the external corpora to the repo (large/licenses) — only download scripts
  + our results; the paths and download commands recorded in the task/script.

## Notes

- Licenses: we pull the NiCad examples and Bellon sources locally, we don't put them in our git.
- The Bellon metric is non-trivial (good/ok match, gaps) — budget time to study their
  comparison scripts, don't invent our own metric on top of their oracle.

## Progress (2026-06-11)

Full analysis: `experiments/clone_oracle_validation/FINDINGS.md`.

### Done
- Data downloaded (`experiments/clone_oracle_validation/`): NiCad repo + example
  `monit-4.2`; Bellon benchmark (RCF oracles cook/weltab/snns/postgresql) + sources ISO;
  key PMD files.
- **Our side of the comparison works**: `--duplication monit-4.2` → 21 meaningful pairs
  (EXACT/RENAMED/LITERAL/STRUCTURAL), platform sysdep twins are caught.
- Formats decoded: per-tool Bellon candidates — **text CPF**
  (`file s e file s e type`, parses trivially); the reference oracle — **binary RCF 2.1**.

### Blockers of the quantitative comparison
- **NiCad oracle blocked on TXL**: NiCad runs on top of TXL, which isn't on the system.
  We need to install TXL (free tarball txl.ca) + build NiCad. Estimate: 0.5–1 day of setup.
- **Bellon blocked on (a) sources and (b) an RCF reader**:
  - in `bellon_sources.iso` there are only results + 5 `.c` (dummy decls); the real sources
    cook/weltab of the needed 2002 version are missing, and the line numbers in CPF are the join key;
  - the validated oracle (`ok`/`good`) — in binary RCF, needs a Bauhaus reader or
    a re-published CSV oracle from later papers.

### §4 Anomaly (move to a separate under-investigation step, blocks #106)
An exact copy of a literal-rich function with a distinctive raw-string literal (df=2, rare)
is NOT reported in the corpus, although by the candidacy architecture it should be. Repro:
`experiments/clone_oracle_validation/pmd/ka_literals_funcs/`. An instrumented
run is needed (which floor cut it: minSharedRare / minDiversity / weighted / line).

### Priority recommendation
- Near-term ROI: **NiCad/monit** (one TXL install away from external ground truth).
- **Bellon — descope** to "format parsed, sources pending"; don't pursue actively
  until there are exact sources + an RCF reader.
- Before any known-answer suite, resolve §4.

## Update (2026-06-11, evening) — §4 resolved + methodology revised

- **§4 anomaly resolved**: a pure RENAMED wasn't reported due to the P0.6 joint-floor
  (line≥0.50 over raw strings) — a deliberate precision/recall tradeoff, NOT a bug.
  Proven by reading (similarity.cpp lineOverlap + phase8) and empirics
  (EXACT line=1 / light-rename 0.86 / heavy-rename <0.5 / monit-RENAMED 0.913).
  Locked in by `tests/duplication_renamed_recall_test.cpp`. The detector wasn't changed.
- **Validation methodology revised** (after reading the history of rule tuning):
  not recall-vs-oracle, but **disagreement-triage**. The detector rests on Kapser & Godfrey
  (#071): our guards deliberately suppress benign clones that NiCad/Bellon mark as
  true. Each discrepancy → (a) deliberate suppression / (b) documented boundary
  arch-spec §9 / (c) a real recall bug. Only (c) is actionable; so far there is no (c).
- Open design question (in #070/#059): whether to relax the P0.6 line-floor at lcs==1, to
  catch heavy Type-2. This is a precision/recall decision, not to be fixed in a hurry.

## Update (2026-06-11, late) — a real recall bug of class (c) found

Disagreement-triage gave the first actionable result: **phase9FunctionBoundaryAnchor
suppresses a copy-pasted function inserted right below the original** (gap ≤5 lines —
the typical case). The same clone with an 8-line gap is reported. The phase9 heuristic guards
against boundary-straddle windows, but our fragments are brace-anchored function bodies, straddle
is impossible → the heuristic only cuts TP. Fix candidate: remove phase9 / narrow it;
before that, a run of fp_corpus_r2 + monit before/after. See FINDINGS §7.

## FIX phase9/P0.4 — full chronology (2026-06-11, for the article and history)

### What it was
`phase9FunctionBoundaryAnchor` (P0.4) in `duplication_scanner.cpp` dropped same-file
pairs with a gap ≤5 lines between fragments. Intent: protection against "the sliding window
caught the tail of function A + the head of function B". Coverage: two TEST_CASE in
`duplication_fp_guards_test.cpp`.

### How it was found (the chain)
1. Cross-validation against external oracles: Bellon weltab.cpf is full of same-file pairs
   (`spol.c:234-250 ↔ 275-291` etc.) → the question "do we catch same-file at all?".
2. The frame was first "the oracle is right" → the user corrected: "read the history
   of tuning our rules, maybe we're more correct and the references have FP" →
   switch to disagreement-triage (Kapser & Godfrey, benign clones).
3. Same-file check: in monit gc.c the copy-paste triple `_gcperm/_gcuid/_gcgid`
   is reported (gaps ~7 lines) → "in general we catch it".
4. User: "the copy is pasted RIGHT below the original — that's the main case".
   Control experiment: an identical clone, 1-line gap → suppressed;
   the same clone, 8-line gap → reported. The hole confirmed.
5. Code analysis: the fragmenter is brace-anchored → a fragment cannot cross a function
   boundary → the target FP class of phase9 DOES NOT EXIST → the guard = pure recall tax.

### What it got caught on (why the bug lived)
Both P0.4 "tests" were dummies: the first — `REQUIRE(true)` in a loop, the second
constructed pairs but did NOT call the filter (it's in an anonymous namespace) and asserted
only `pairs.size()==1` before filtering. The guard had not a single real assert.
Lesson: "there's a TEST_CASE named after the guard" ≠ "the guard is tested".

### What was fixed (TDD order)
1. **Red test first**: a new TEST_CASE "Same-file copy-paste pasted directly
   below the original is reported" (3 functions: the copy right below + an unrelated filler,
   so idf doesn't degenerate). Run BEFORE the fix — honestly fails (reported=false).
2. **Fix**: removed `phase9FunctionBoundaryAnchor` entirely + the call in
   `applyCandidateFilters` (−44 lines). In its place — a NOTE comment with the reason
   (modeled on the removal of P0.7/P0.8 from 2026-06-03).
3. Both no-op P0.4 tests replaced with a real fixture (not "papered over" — the old ones
   checked nothing, see above).

### Verification (before/after on the same corpora)
- Test suite: 457 cases / 1551 assertions — all green; not a single existing
  test was adjusted for the fix.
- **monit-4.2: 21 pairs → 21 pairs** (byte-for-byte the same report) — zero new FP.
- **Self-scan src/: 1 → 2 pairs.** New: `token_normalizer.cpp:228-253 ↔ 258-278`
  (STRUCTURAL, w=0.695, line=0.526) = `tryConsumeString` ↔ `tryConsumeChar` —
  neighboring functions, 5-line gap, exactly the bug scenario. By the extractability test
  #071 — a real TP (an extractable shared helper "consume quoted with escapes").
  The fix paid off immediately on our own code.
- dogfood rules: `archcheck src include tests` → 0 violations; clang-format clean;
  lizard: my files clean (5 remaining warnings — pre-existing other TEST_CASE).
- Run artifacts: `experiments/clone_oracle_validation/results/{monit,self_src}_{before,after}_fix.txt`.

### Documentation
- `docs/duplication_architecture.md`: P0.4 struck from the guard list in §3.7,
  added a section "History of P0.4 (removed 2026-06-11)" next to the history of P0.7/P0.8.

### Open (NOT done, candidate for the next step)
- The TP found in token_normalizer (`tryConsumeString`/`tryConsumeChar`) — decide:
  extract a shared helper or accept (there's a real difference: string tracks
  `line`, char doesn't). A separate commit, if we decide.
- P0.6 line-floor vs heavy Type-2 renames — still an open
  design question (#070/#059), NOT part of this fix.

## A dividend finding of the fix and its resolution (2026-06-11)

**Finding:** the very first run after removing phase9 found a real TP in our own
lexer: `tryConsumeString ↔ tryConsumeChar` (token_normalizer.cpp:228↔258) —
neighboring functions, 5-line gap, exactly the bug scenario ("pasted right below,
changed a couple of lines"). The fix paid off on the very first run.

**Resolution (dedup):** both functions merged into one `tryConsumeQuoted(source, i, line,
delim, out)` — a shared scan "to the closing delimiter with escape skipping", the difference
was only in the quote character. The idea of decomposing into micro-primitives (step/skip/next)
was rejected: there's exactly one shared piece, three one-off micro-functions are premature
granulation; one parameterized helper was enough. Side improvement: the char branch
now also tracks `\n` (previously a malformed literal with a newline threw off line numbering).

**Verification:** suite 457/1551 green (lexer behavior unchanged);
self-scan src/ **2 → 1 pair** — the lexer pair disappeared from the detector report;
monit unchanged (21); dogfood 0 violations; lizard/clang-format clean.

**The remaining 1 self-scan pair** (`drift_bidirectional_coupling.cpp:48-49 ↔
duplication_scanner.cpp:168-169`, EXACT 2 lines — `toLowerCopy`) — existed before
all the edits; a candidate for a shared helper, by a separate decision (not in this fix).

## Second dividend finding: a dead `++line` inside literals (2026-06-11)

The user's question "what about line++?" on the merged `tryConsumeQuoted` exposed a bug deeper than the
dedup itself. A pin test for the new behavior (a newline inside a quoted run should advance
the numbering) failed with line=3 instead of 4 — the investigation showed:

- `consumeToken` took `line` **by value**; all literal consumers
  (`tryConsumeString`, `tryConsumeRawString`, the new `tryConsumeQuoted`) incremented
  a local copy, which was thrown away on return.
- => `++line` inside string/raw literals was **always dead** (since the port):
  a multiline raw-string already shifted the line numbers of all subsequent tokens —
  and `file:line` is a product contract.
- Why it wasn't caught: existing tests assert `.line` only for newlines
  **between** tokens (the `consumeTrivia` path, where `line` is by reference). A newline **inside**
  a literal wasn't in any test.

**Fix:** `consumeToken(..., int line, ...)` → `int &line` (one character).
**Pin:** a new TEST_CASE "newline inside a quoted run keeps later line numbers".
**Verification:** suite 458/1553 green (not a single test relied on the buggy
behavior); the monit report is identical; dogfood 0; lizard/format clean.

The chain for the article: dedup → reviewer's question about the semantics of `++line` → pin test →
test fails → digging → a senior bug found and fixed. Two bugs out in one day
of validation, both — via writing real fixtures instead of `REQUIRE(true)`.

## How it works

Validation = disagreement-triage against external corpora (NiCad monit, Bellon CPF):
each discrepancy is classified (benign suppression / documented boundary / a
recall bug) via the extractability test #071. Before/after runs are saved in
`experiments/clone_oracle_validation/results/` and compared by diff.

## Key decisions

- **Rejection of the "oracle = truth" frame** (user's correction): our guards deliberately
  suppress benign clones; only class (c) — a real recall bug — is actionable.
- **P0.4 removed, not relaxed**: its target FP class is impossible with a brace-anchored
  fragmenter — the guard was a pure recall tax.
- **P0.6 line-floor against heavy Type-2 left as is** — a conscious tradeoff, the decision
  to relax it (lcs==1 → skip) is moved to #070/#059.
- **The quantitative part (NiCad/TXL, Bellon) deferred** by the user's decision
  2026-06-11 → #132 (`backlog/new/132_maj_oracle_quantitative_validation.md`;
  un-mothballed from future 2026-06-19).

## Changed files

- `src/scan/duplication/duplication_scanner.cpp` — removed phase9/P0.4 (commit 1fbc9f4)
- `src/scan/duplication/token_normalizer.cpp` — tryConsumeQuoted + `int &line` (1fbc9f4)
- `tests/duplication_fp_guards_test.cpp` — no-op P0.4 tests → a real fixture (1fbc9f4)
- `tests/duplication_token_normalizer_test.cpp` — pin line-tracking (1fbc9f4)
- `docs/duplication_architecture.md` — history of P0.4 (1fbc9f4)

## Outcome
**Status:** completed (quantitative oracle validation descoped → future)
**Completed:** 2026-06-11
