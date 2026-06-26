# [RESEARCH][CORPUS] boolean_state per-struct drift — full-corpus run + report

**Created:** 2026-06-23
**Started:** 2026-06-25
**Completed:** 2026-06-25
**Status:** done (done by a different method — see OUTCOME)
**Module:** RESEARCH / experiments/boolean_state

## OUTCOME (2026-06-25) — done from native #090 events, NOT Python blame

**Deviation from the letter of the task (deliberate):** instead of running the leaky-Python `perstruct_drift.py`
(VEND-regex), we built per-struct history from **native #090 events** — re-parsed 10,735
bool commits (`archcheck --diff`, 37.8 min, 8 workers), aggregated by `(repo,struct)` with file-move
dedup. Reason: the native vendored/test/generated filter is stricter than Python-VEND (user: "why a
Python script and not our filter?"). Scripts: `experiments/boolean_state/perstruct_history_from_events.py`
+ `perstruct_history_aggregate.py`. Report: `docs/research/boolean_state_perstruct_drift_fullcorpus.md`
(NEW file; the #089 report was not touched).

**Result:** 8,280 (repo,struct) accumulated a bool; **499** across ≥4 commits (335 content + 163 config + 1 churn)
in **240 repos**. Eye-check top-15: ~11/15 clean TP (UI controller/view god-objects dominate),
the reference boolean-blindness (RtlirVariable `is_real/is_string/is_event`), the rest — demo/config/churn.
**Prevalence 20.2% ≈ 21% from #089** (cross-validation on a corpus ×16). The #089 anchors MethodState (MOLA) +
Channel (FluidNC) were reproduced; EditorShell/HttpTransact were outside the corpus-run window (a lens difference).

**Acceptance criteria:** report + CSV exist, eye-check top-15 with verdicts done, prevalence estimated
and consistent with #089, #089 artifacts untouched. ✓
**Priority:** major
**Blocks:** provides fresh live examples for #090 (the "do/don't" decision on the drift metric)
**Blocked by:** —
**Related:** #089 (research, verdict MAYBE — done), #090 (drift-metric implementation — future), #124 (per-commit corpus run)

## Goal

Run the existing prototype `experiments/boolean_state/perstruct_drift.py` over the **full current corpus (~1685 repos in `~/oss`)**, rather than the old 73-repo agentic sample from #089. The output — a **report** with a fresh top list of content structs that accumulated boolean fields across ≥4 distinct commits (constraint decay in structs), + a CSV for further analysis.

Why: a conversation surfaced that we have no live examples of "bools accumulating in structs" from a large corpus — only the #089 reference cases (EditorShell, HttpTransact::State, Terminal, MethodState, Channel…) on 73 repos. This task closes the gap: it gives a current picture of prevalence and a list of fresh candidates.

## What this is NOT

- NOT a rule implementation in `src/` (that's #090, deferred under YAGNI — demand not yet confirmed).
- NOT a change to the product binary. A pure Python run of the prototype over the corpus.
- NOT a static "many bools right now" (that's 78% noise per #089). The signal is **per-struct accumulation over git history**, which the prototype already does.

## Input data (everything is in place, nothing to clone)

- Prototype: `experiments/boolean_state/perstruct_drift.py` (validated in #089: per-struct attribution + depth-0 parser → 0% gross FP on the top-14 eye-check).
- Corpus: `~/oss/<repo>/` — 1685 repos.
- Repo list: first column of `experiments/per_commit/worklist_full.tsv` (1685 distinct), or simply all `*/.git` in `~/oss`.
- Thresholds (as in #089, don't change without reason): `MIN_FIELDS=4`, `MIN_COMMITS=4`.

## Parameters / thresholds

| Parameter | Value | Source |
|---|---|---|
| `MIN_FIELDS` | 4 | bool fields in a struct to blame it at all |
| `MIN_COMMITS` | 4 | fields arrived across ≥4 distinct commits = drift |
| `OSS` | `~/oss` | corpus |
| vendor filter | `VEND` regex in the script | lib/third_party/extern/vendor/imgui/sdl/examples |
| config-bag filter | `CFG_NAME` regex | `*Config/*Options/*Settings/*Params…` → broken out separately |

## Plan

### 1. Parameterize paths (do NOT overwrite #089 artifacts)
The prototype currently hardcodes `LIST = "/tmp/agentic_local.txt"` and **overwrites** `docs/research/boolean_state_perstruct_drift.md` (this is the validated #089 report — DO NOT touch). Therefore:
- [ ] Assemble the full-corpus repo list: `cut -f1 experiments/per_commit/worklist_full.tsv | sort -u | xargs -n1 basename > /tmp/corpus_full.txt` (the script expects repo names relative to `OSS`, not absolute paths).
- [ ] In a copy/variant of the script (`perstruct_drift_fullcorpus.py`) override:
  - `LIST = "/tmp/corpus_full.txt"`
  - output report → **new file** `docs/research/boolean_state_perstruct_drift_fullcorpus.md`
  - CSV → `experiments/boolean_state/perstruct_drift_fullcorpus.csv`
- [ ] (experiments/ and /tmp — gitignored/outside the repo; the new md in docs/research/ is a new artifact, the old one is left untouched.)

### 2. Run
- [ ] Run in the background (1685 repos × git blame over headers — tens of minutes to hours; the corpus `--diff` run is going in parallel, don't disturb it — it's a different process).
- [ ] Progress is written line by line (`[i/N] repo: per-struct drifts=…`).

### 3. Report
- [ ] Fresh `boolean_state_perstruct_drift_fullcorpus.md`: top-40 content structs (config-bag excluded), commit/field/day counters, links to files.
- [ ] Summary: how many struct drifts total / content / config-bag, in how many repos, prevalence estimate (share of repos with real drift).

### 4. Self-check (mandatory — see CLAUDE.md "Self-check of conclusions")
- [ ] Eyeball-verify the top-10–14 content structs: real drift vs config-bag vs FP (open the file + `git log -L` / blame the field pairs). Don't trust the aggregate — enumerate the cases independently.
- [ ] Cross-check against the #089 reference cases (EditorShell donner, HttpTransact::State trafficserver, microsoft/terminal Terminal, MOLA MethodState, FluidNC Channel): are they in the full corpus? are the numbers plausible?
- [ ] Record the class of residual FPs (generated code, bool in signatures not at depth-0, different structs with the same name) if they appear on the larger corpus.

## Acceptance criterion

- The report `docs/research/boolean_state_perstruct_drift_fullcorpus.md` exists, the top-40 is filled with real structs with clickable paths.
- The CSV is complete.
- Eye-check of top-10+ done, TP/config/FP verdict for each, prevalence estimated.
- The #089 artifacts (`boolean_state_perstruct_drift.md`, examples.md) are NOT touched.
- Conclusion: do the fresh data confirm the ~21% prevalence from #089, or does the large corpus shift the estimate.

## Pitfalls (from #089 experience and corpus runs)

- **Do not overwrite `docs/research/boolean_state_perstruct_drift.md`** — it holds the #089 result. New file only.
- **shallow clones → incomplete blame**: the date of the first field may be "cut off" at the clone boundary, the span understated. This is a lower bound, not a bug (as in #089).
- **config-bag ≠ FP**: `*Options/*Config` with 10+ bools are legitimate; the `CFG_NAME` filter breaks them out; but it doesn't catch everything (in #089, 6 of the top-14 were config-bags whose name slipped past the filter). This is semantics, not a parser bug — resolve by eye.
- **git blame over a large corpus is slow**: a 120s per-file timeout is already in the script; repos with giant headers may fall off — that's fine, it's logged.
- Don't spawn orphan git processes (see memory: corpus-run hygiene). The prototype's blame is synchronous, per-file — it doesn't spawn orphans, but if parallelized — kill the GROUP.

## Notes for the executor (Haiku-friendly)

This is a mechanical run of a ready, validated script + eye-check. Do NOT rewrite the parser/blame logic (it's verified in #089). Do NOT change the thresholds. Do NOT touch `src/`. The only code edits are three path constants (LIST / two output files). If the result looks "too smooth" or the numbers diverge sharply from #089 — that's a reason to re-verify by hand, not to close the task.
