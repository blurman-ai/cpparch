# [RESEARCH][SCAN] New corpus run: verification of scanner changes

**Created:** 2026-06-19
**Started:** 2026-06-19
**Status:** wip (Group 1 done; Groups 2–6 pending)

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
