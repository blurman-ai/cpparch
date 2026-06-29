# [RESEARCH][SCAN] New corpus run: verification of scanner changes

**Created:** 2026-06-19
**Started:** 2026-06-19
**Status:** wip (Group 1 done; Group 3 in progress 2026-06-29; Groups 2/4 pending; 5/6 dependencies #130/#119 landed → bookkeeping)

## Run 2026-06-29 — Group 3 (duplication precision on fp_corpus_r2.tsv) IN PROGRESS

Reconciliation after 10 days: source deps that have since landed in `completed/` —
**#127** (vendor-exclude), **#130** (perf, Group 5 code), **#119** (correlation, Group 6).
Groups 5/6 reduce to sign-off bookkeeping. Group 2 (#124) still wip (days-long).
Genuinely-open measurement work = Group 3 (#070/#072), the fastest to a real number.

**Key findings establishing the Group-3 method:**
- round2_verdicts JSON = **eyeball labels, NOT reproducible detector output** (no spans/scores;
  loose `where` text, `sha` often `top`). That's why the harness stayed a placeholder since 2026-06-02.
- Corpus sha split: **311 COMMIT rows / 29 TOP rows**. ALL 143 TP are COMMIT rows;
  the 29 TOP rows are all FP (snapshot `--duplication`). → measuring the COMMIT slice is what carries precision+recall.
- The per-commit `--diff` new-clone path (`new_clone_drift.cpp`, #123/#156) **reuses `duplication::scanForDuplication`
  with the P0/P1 guards** (whole-file guard off, joint floor + P1 classifiers on). So re-running
  `archcheck --diff --diff-mode=memory <sha>~1..<sha> <repo>` on the labeled commits exercises the #070 guards.
- Honest caveat: the round-2 labels were produced by the OLD `partial_duplication` spike
  (`--min-tokens 45 --threshold 0.85`); the current gate is **stricter** (parent-guard, added-line overlap,
  40 MiB byte-cap #149). So this measures **the current product gate's precision/recall on the labeled set** —
  more useful for #131 sign-off, but recall (TP-kept) must be reported alongside precision.
- 103/107 corpus repos present in `~/oss`. byte-cap skips large repos (monero fork) → must be
  reported as **skipped (not measured)**, distinct from suppressed; `skippedLargeTree` prints only in TEXT mode.
- Clone text line format:
  `<file>:<line>: DRIFT.NEW_CLONE — copy-paste introduced (<TYPE>): lines A-B — clone of <src>:C-D`.

Harness + raw data: `experiments/corpus_remeasure_131/group3_precision.py` (+ outputs).

**RESULT (full writeup: `experiments/corpus_remeasure_131/FINDINGS_group3.md`):**
current product new-clone gate on the labelled COMMIT slice (306 measured rows) —
**precision 72.2 %** (52/72; baseline spike 42.1 %), **FP-suppression 88.0 %**
(146/166), **recall 37.1 %** (52/140; 38.5 % excl. unscannable). Precision-first:
idiom FPs 95.6 % suppressed; cost = ~62 % of the spike's real copy-paste dropped.
Self-check caught + fixed a matcher bug (recall 23 %→37 %); the residual 88 misses are
genuine (55 zero-clone + 33 different-region). Minor scanner gap surfaced:
`project_files.cpp` extension match is case-sensitive → `.C`/`.cu` unscanned (5 rows) —
candidate fix for #127/#129. **Group 3 DONE.** (#070 precision measurement delivered;
#072 pair output already present.)

## Fix 2026-06-29 — P0.6b: recover Type-3 edited copies without idiom leaks

Root cause (traced in `docs/dev/duplication_decision_path.md`): the joint floor (P0.6)
gates on `line` = union-Jaccard over verbatim lines, which **deflates on an edited copy**
(insert/delete grows the union) — so renamed/edited copies die even though the absolute
shared-line run is long. The verbatim count (`inter`) was computed in `lineOverlap` and
**discarded**.

Fix (`similarity.*`, `duplication_scanner.*`): a pair below the line-ratio now also passes
P0.6 if it shares **≥ jointMinSharedLines (6) distinct verbatim lines** AND **≥ jointMinSharedRare
(2) rare project anchors** AND is diverse (not a low-diversity table). Two orthogonal
positive signals separate a real edited copy from the two FP families: short coincidences
(few shared lines) and framework idioms (no rare anchor — they share only common tokens).

Verified on the labelled corpus (`group3_compare.py`, before=baseline / after=fix):
- **FP-suppression 88.0% → 88.0% (0 new leaks)**; idioms (star/meteor, pqc, Qt-dialog scaffold)
  stay suppressed — confirmed by code.
- **20 new real clone detections** (finding-level), all sampled-verified as genuine copy-paste
  (fakelua int64→double for-codegen; LLL-TAO 1→2-byte header read; FlashCpp parser branches;
  ZoneManager/NameMangling/commands_cgroup). Labelled-pair recall 37.1% → 38.6%.
- Tests 605/605, dogfood clean, lizard clean.

**Honest limits:** (1) the rare-anchor at df≤4 is tuned conservative — it also suppressed some
genuinely-real copies whose shared tokens are moderately common (florinzgz pattern-draw, unit
renderers). A looser anchor cap recovers more at a precision cost — a calibration knob for the
product owner. (2) The original int64/uint64 motivating case is **not** recovered: it dies
upstream at **candidate generation** (`find_json_value_pos` not rare + edits break fingerprint
runs), a separate, harder lever independent of the joint floor.

## Re-run 2026-06-29 (evening) — high-recall posture + #158 A+C re-measured

Final detector `c8e2bec` = P0.6b + high-recall run-path (`b34d1d2`: ordered line-LCS,
`jointRunWeightedThreshold=0.60`, `jointMinSharedRare=0`) + #158 A+C (`5277cd2`: test-file
recognition, data-table real-drop). Same harness/labelled set. Full writeup:
`experiments/corpus_remeasure_131/FINDINGS_group3.md` (Re-run section).

| metric | P0.6b baseline | this run | Δ |
|---|---|---|---|
| Recall (TP_kept) | 37.1 % (52/140) | **45.7 % (64/140)** | **+8.6 pp (+12 TP)** |
| Precision | 72.2 % (52/72) | **68.8 % (64/93)** | −3.4 pp |
| FP suppression | 88.0 % | **82.5 %** | −5.5 pp |

The product-owner-chosen high-recall trade: **+12 real copy-paste catches for +9 FP fired**
(idiom/scaffold + 1 coincidental — all expected classes, no new pathology). Recall gain
hand-verified genuine (DataDog `refCountGuard` waitForRefCountToClear↔waitForAllRefCountsToClear,
a real Type-3 copy). **#158 A+C moved this labelled set by ~0 — correctly:** the labels carry
no `*Tests.cpp` rows (Part A's win is on chrxh_alien, 7→0) and the labelled data-table FPs are
not the LITERAL-low-diversity signature Part C drops (its win is on djeada team_color, 518→477).
Claiming a corpus precision gain from A+C would be false; the precision shift here is entirely
the high-recall run-path.

## Run 2026-06-19 — Group 1 (SF.*/graph golden) DONE

Release binary rebuilt at HEAD (#129/#127/#131). Default-scan run over the whole
growth corpus (**1689 repos**, 0 timeouts/errors, ~minutes). Raw data + analysis:
`experiments/corpus_remeasure_131/{run_corpus_sf.py,corpus_sf_131.tsv,FINDINGS.md}`.

Results (per-repo, not aggregate — #115):
- clean 332 (20%); SF.9 gate on 505 repos (real include cycles).
- **No regressions in authored detection.** rmm clean (total=2), foundationdb SF.9=5 —
  real cycles (not #109 artifacts), bpftrace almost clean.
- **#127 gap (actionable):** vendored-exclusion catches an in-repo `third_party/` SEGMENT,
  but NOT (a) whole-repo-vendored mirrors (musl/gnulib/android-kernel — SF.8 in the hundreds-thousands),
  (b) bundled kernel headers (bpftrace `src/stdlib/include/linux/`). → extend the
  bundled-kernel + whole-repo-vendored heuristic.
- **Corpus criteria (#122):** pure-vendored mirrors and ports/data trees
  (FreeBSD-Electron SF.8=9704) distort the aggregates — filter on input
  ([[project_corpus_criteria_gate]]).
- The curated golden repos from the table below (openvdb/mlpack/pcl/supercollider/newsboat/swig)
  ARE ABSENT FROM THE CORPUS — exact-delta checks require cloning; the growth corpus gave
  equivalent findings on the vendored repos that are present.

**Inputs from #132** (duplicate validation from the same session): test files are excluded from dup
(#129 confirmed); @generated Rcpp bindings (tulpa) — real duplicates, a candidate for
generated-exclusion if they carry a banner; corpus dup HEAD-sweep of 18 repos — 0 FP.
**Module:** RESEARCH / SCAN
**Priority:** major
**Blocked by:** #122 (growing the corpus to ~1000 + regen — for the full-corpus parts; some
golden checks can be run on the current repos right away)
**Blocks:** the final corpus sign-off (and hence the closing) of #128, #129, #125, #126,
#127; the republication of the numbers in `docs/research/agent_drift_within_repo.md`
**Related:** #119 (unified correlation — a parallel consumer of the same regen),
#124 (clone-gate validation — its own harness), #122 (the growth+regen mechanism)

## Role (read first)

This is a **single run task**. When we're about to do a new corpus measurement — here is
the master checklist: what we check, what MUST change, from which task the change comes.
It does NOT duplicate #122 (corpus growth+regen) and #119 (correlations) — it **depends** on them.

The source tasks carry the field `**Verification:** #131` and **are truly closed
only after this run** (the code may be ready and locally verified, but
corpus confirmation is here).

Why separately and why "later": these checks are deliberately NOT done "along the way" —
a release binary is needed, large repos, a window, **golden per-repo** (the aggregate lies,
[[feedback_verify_each_case_over_aggregate]]). The corpus is expensive → gather ALL
pending checks into one run, rather than poking at them one by one.

## When to run (preconditions)

- [x] #122: corpus grown (1188 repos) + measured — **done** (report). A full regen is
      not needed again; here — only a **targeted re-measure of vendored/generated-carrying
      repos** (Group 1), because the classification filter changed (#129/#127).
- [ ] All "code needed first" changes have landed (the "code" column in the table).
- [ ] #127 (vendor-exclude) is **already underway** (commits `ab21d5f`/`157c727` — external_libraries
      + generated headers dropped). Run the partial re-measure **after #127 is final**, not twice.

## What to check — master table

### Group 1 — SF.* / graph golden (changes to rules/classification)

| From | What changed | What to check (metric / set / method) | Expected delta | Code |
|------|--------------|----------------------------------------|----------------|------|
| **#128** | SF.8 strips comments before searching for the guard | openvdb/nanovdb on a real clone; truly guardless .h still fire | SF.8 FP **−30** (openvdb), TPs not lost | ✅ shipped `9f247a6` |
| **#129** | AuthoredScope: one read + shared vendored/generated/banner | golden per-repo: rmm, the SWIG repo, foundationdb, an Apache repo, openvdb | clone recall **↑** (Apache), graph **−generated**, complexity rmm fixed, foundationdb 0 (#109-safe) | ✅ shipped (step 7) |
| **#113** | Apache banner ≠ vendor (dominant-guard) | Apache repos in the corpus: how many files in the graph | graph metrics **↑** (previously lost ~90% of files) | ✅ shipped, not in the data |
| **#114** | `-test`/`testutil` filter | test directories don't leak into the graph | graph **−test nodes** | ✅ shipped, not in the data |
| **#126** | SF.9 gluing header+impl (full component model) | mlpack (16→~0), PCL `impl/` (206→~0); real inter-component cycles hold | SF.9 **↓** on template libs, TP held | ⚠️ only quick-fix `2690a34`; the full model not done |
| **#125** | extensionless headers (`<vector>`, GSL) | GSL graph 0→~10 nodes; Eigen vendor didn't fit (vendor-guard in the pair) | graph on stdlib style non-empty; vendor not added | ⚠️ not implemented |
| **#127** | vendor/generated multi-layer exclusion | supercollider (~2900→dozens), bpftrace SF.8 4→0, newsboat 2→0; **no over-exclude** of own code | violation count **↓** on bundled-deps | ⚠️ wip, not finished |

### Group 2 — clone-gate (product validation)

| From | What to check | Expected delta | Status |
|------|---------------|----------------|--------|
| **#123** | stages 1+2: a manual sample of 10 commits (5+/5−), a GitHub test repo | precision/recall ≥90% on the sample; the parent-guard doesn't kill "the commit touched an old duplicate" | wip, its own run |
| **#124** | Part B: fire-rate of `DRIFT.NEW_CLONE` over the whole corpus (1M+ commits) | product precision/recall for the gating decision | wip, long run (≈10 days @8w) |

### Group 3 — duplication precision on the labeled corpus (`fp_corpus_r2.tsv`)

| From | What to check | Expected delta | Status |
|------|---------------|----------------|--------|
| **#070** | precision after P0/P1 guards on r2.tsv (197 FP / 143 TP) | precision 42% → **~55–62%**, idiom-floor ~40 FP | measurement harness = placeholder, not measured |
| **#072** | the same after porting the detector + JSON output of pairs | precision **↑** on r2.tsv | JSON output of pairs not done |

### Group 4 — AI-repo research (needs a fresh binary)

| From | What to check | Expected delta | Status |
|------|---------------|----------------|--------|
| **#054** | graph+dup over the AI corpus on the binary after #113/#114/#128/#129 | graph metrics **↑** on Apache repos; dup stable (didn't depend on #113/#114) | numbers on the old binary |
| **#066** | re-measure CLONEFAIL: AI-dense repos with ≥300 commits (was 81 → ~200?) | **×2.5** vs the old re-measure | the background run may not have finished (snapshot 2026-06-02) |
| **#103** | copypaste per-commit vs the product `--diff new-clone` (#124) | Python-MD5 > token by fire-rate (explainable) | a recon of 22 repos; the full one not done |

### Group 5 — perf (#130 thread 1)

- [ ] Implement thread 1: `AuthoredScope::fromFiles((string_view,string_view))` +
      `read()` without a double copy of the content.
- [ ] Peak RSS before/after on the 2–3 largest repos (release build); scanner counters
      **bit-identical** (golden on fixtures + 50–100 commits).

### Group 6 — unified correlation (downstream)

- [ ] **#119**: the whole correlation matrix of per-commit signals — a separate task,
      consumes the same regen. Run after #122. **Don't duplicate the run** — give
      #119 a fresh dataset from this run.

## Run hygiene

git orphans (kill the GROUP, `start_new_session`+`killpg`), suspend baloo for the duration,
≤8 workers (the user is at the machine), `GIT_LFS_SKIP_SMUDGE=1`.
See [[project_archcheck_diff_git_orphans]], [[feedback_clone_oss_without_lfs]].

## Self-check

- Golden — **per-repo, not aggregate** (Simpson, #115): a summary number can match even with
  components that have diverged.
- "Identical" (thread 1) — a **diff of counters**, not by eye.
- A metric shift — by enumerating cases with an independent method
  ([[feedback_verify_each_case_over_aggregate]]).
- The already-done corpus runs #109 / #111 / #115 / #079 — the data is **STALE**
  after #113/#114/#128/#129; don't use their numbers as current.

## Changed files (plan)

| File | Change |
|------|--------|
| `include/archcheck/scan/authored_scope.h` | view overload of `fromFiles` (#130 thread 1) |
| `include/archcheck/scan/source_snapshot.h` | `read()` without a double copy (#130 thread 1) |
| `docs/research/agent_drift_within_repo.md` | new numbers after the shift (#129/#113/#114) |
| `experiments/…` | regen of the corpus jsonl on the current binary |
