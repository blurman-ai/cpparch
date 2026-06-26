# [RESEARCH][SCAN] Re-download part 2: re-measuring CLONEFAIL + expanding the corpus ≥300

**Created:** 2026-06-01
**Started:** 2026-06-01
**Status:** wip
**Module:** RESEARCH / SCAN
**Priority:** major
**Related:** #060 (checker validation), #054 (dense corpus), #065 (generated-skip)

## Goal

Fix the mass CLONEFAIL in the measure phase and re-measure the failed candidates, in order to
find the TRUE number of AI-dense repos with ≥300 commits/year (currently undercounted ~3×)
and finish cloning the missing ones into `~/oss/_aidev_dense`.

## Context — the bug found (2026-06-01)

`measure_candidates.sh` was run with P=14–16 parallel blobless clones. GitHub started
refusing connections → **CLONEFAIL = 16399 of 24642 (66%!)**. Only 8247 were actually measured.
Of those, ≥300 commits/year = 794, AI-dense (conc≥5) = 81.

**Consequence:** large active repos clone longer → failed more often → it's precisely the
commit-dense repos that were systematically lost. The true number of ≥300 is estimated at ~2400
(794 / 0.33), AI-dense ≥300 — many times more than 81.

**Confirmation (with correction):** re-cloning 20 CLONEFAILs with P=1, timeout 120s → revives
**6 of 20** (str1ker etc.). So CLONEFAIL is a MIX of causes: part concurrency/rate-limit,
part timeout on large repos (120s too little for a blobless large history — e.g. FlexEngine),
part genuinely dead/private. The undercount factor is therefore <3× (TBD with a proper re-measure
P=2 + timeout 300 + retries). The key point — it's significant, ≥300 is undercounted.

## Execution plan

- [x] Fix `measure_candidates.sh`: default P=6→3, a retry wrapper around clone
      (RETRIES=3 = retry ≥2) with escalating pacing. The P=16 bug sources in
      `keep_downloading.sh` and `discover_finish4.sh` lowered to P=3 (re-measure) / P=2 (retry).
- [~] Re-measure CLONEFAIL with P=2–3. **First a sample of 2000** (user's decision) to
      estimate the undercount factor without a 45h full run → `sample_estimate.sh` (durable,
      detached), writes `sample2000_measured.tsv` + a funnel in `sample_estimate.log`.
      Sanity 12 repos: 4/12 revived (~33%), all <300 (skip). The full re-measure — after the sample.
- [x] Size-cap on clone into `_aidev_dense`: 1.5GB → **500 MB** (user's decision,
      cuts 10 heavy repos out of the 81 dense, ~9.4GB). Added an API size precheck in
      `clone_expand.sh` (don't download the knowingly large). The `_aidev_dense` disk ceiling = 45 GB.
- [ ] Rebuild the ≥300 pool and the clonelist (conc≥5) from the updated data.
- [ ] Finish cloning the new AI-dense ≥300 into `_aidev_dense` (per-repo cap 500 MB, disk 45 GB).
- [ ] harvest2 (a new axis: `sort=stars`, the pushed window going back). Do NOT include the C pass.
- [ ] Update EXPANSION_REPORT + METHODOLOGY with an honest funnel (corrected for the bug).

### Measuring the dense corpus (2026-06-01, agent via `gh api`)
81 AI-dense ≥300 (conc≥5), all alive, all C++, none archived. ~16.5 GB total.
Heavy >500 MB — 10 (HyperXTalk 2.6GB, arduino-esp32 2.1GB, UE5-MCP 1.4GB, Fincept 769,
Exasim 767, piper-plus 728, NAAb 716, LogViewer 617, gse_fork 583, wiRedPanda 565) =
~9.4 GB / 56% of the budget. Without them 71 repos ≈ 7.5 GB. Top by commits/year: deltahdl (9911),
kiln (4444), PlasmaZones (3757), komai (3419), NereusSDR (2788).

## Estimation result (a sample of 410 random CLONEFAIL, 2026-06-01)

The bug is confirmed, undercount **~2.5–3×**:

| metric | was | +from re-measure (extrap.) | true |
|---|---:|---:|---:|
| revival of CLONEFAIL | — | **89%** clone | bug = nearly all rate-limit |
| ≥300 commits/year | 794 | +~1670 | **~2460** (×3.1) |
| AI-dense conc≥5 ≥300 | 81 | +~120 | **~200** (×2.5) |

The sample was merged into MEAS → CLONEFAIL 16399 → **16028**. Dense found in the re-measure: Banana (conc 81),
QCView-Player (13), esphome/esphome (6639 commits/year, conc 9).

## Reboot-safety (2026-06-02): resuming after a reboot into Windows

All state was moved out of `/tmp` (cleared on reboot) into the persistent
`~/oss/_aidev_state/` (on ext4, survives a reboot into Windows).
The scripts are made resumable (they skip what's done), with autostart via `@reboot`.

| Process | Resumable mechanism | File |
|---|---|---|
| CLONEFAIL re-measure | OUT `_aidev_state/clonefail_remeasured.tsv`, skip by repo, merge+flag `remeasure.done` | `remeasure_resumable.sh` |
| Dense download | waits for `remeasure.done`, `clone_expand` idempotent, flag `finish.done` | `finish_resumable.sh` |
| round2 verification | skip of repos with a ready `verify_results2/<repo>.json`, exclude from state | `verify_findings_round2.js` (resume filter) |
| Autostart @reboot | `/etc/cron.d/airepo_resume` → master, idempotent (flags+pgrep) | `resume_all.sh`, `round2_headless.sh` |

On the next boot into Linux, cron itself will bring up `resume_all.sh` → re-measure+finisher
(shell) + round2 (headless claude) will continue from the stop point. Manual launch:
`bash experiments/ai_repo_run/resume_all.sh`.

## "Don't download extra" strategy (2026-06-02)

The root of the inefficiency: clone pulled the whole history graph, while we measure only the
post-May part. + uncaught giants (chromium, o3de) got stuck on timeout retries.

| Template | Implementation |
|---|---|
| **shallow-since=2025-05-01** (the main one) | `measure_candidates.sh`: pull only the post-May history → cut by several times; a general safeguard against any giants |
| name filter of mirrors | `remeasure_resumable.sh` GIANT regex: chromium/llvm/aosp/webkit/gecko/o3de/unreal/tensorflow/src-leak → TOOBIG-skip without cloning |
| API fallback on shallow failure | 1 call `commits?since` distinguishes "no fresh" (skip) vs CLONEFAIL |

Boundary: conc can't be known without measuring → we still go through 16k, but cheaply.

## In progress (durable, resumable — snapshot 2026-06-02 ~14:4x)

- **Re-measure of 16028** (resumable, writes to state): OUT **7616**, new dense **62**, TOOBIG-skip 10.
  Processes cleaned up by hand (zombie `xargs` got stuck on o3de; kill from sandbox doesn't reach them —
  the user struck from the terminal). On resume one clean instance with shallow-since will come up.
- **round2 verification**: **66/135** repos.
- **Dense finisher** waits for `remeasure.done`. Corpus **29 GB / 367 dirs**. Scratch junk ~3.7GB (being cleaned).

## Key decisions

| Decision | Reason |
|----------|--------|
| P=2–3 instead of 14–16 | P=16 → GitHub rate-limit → 66% CLONEFAIL |
| retry + pacing | a single clone is unreliable under load |
| size-cap 500 MB (was 1.5GB) | 10/81 heavy = 56% of the disk budget; breadth matters more |
| API precheck `.size` before cloning | don't download knowingly large repos (and then delete) |
| sort the clonelist by commits/year | maximum author signal; `commits/MB` — a secondary density metric |
| `language:c` removed | that's pure C, not C++ — junk for the research |

## Changed files

| File | Change |
|------|--------|
| `measure_candidates.sh` | default P 6→3, retry (RETRIES=3) + pacing, **shallow-since=2025-05-01**, API fallback |
| `remeasure_resumable.sh` | + GIANT name filter of mirrors (chromium/o3de/llvm/…) → TOOBIG-skip |
| `clone_expand.sh` | cap 600→500, API size precheck before cloning |
| `keep_downloading.sh` | measure P=16→3, clone cap 1500→500 |
| `discover_finish4.sh` | measure P=16→3 (retry P=2), clone cap 1500→500 |
| `remeasure_clonefail.sh` (new) | re-measure CLONEFAIL P=2 + merge/dedup into MEAS |
| `sample_estimate.sh` (new) | undercount estimate on a sample of 2000 |
| `remeasure_resumable.sh` (new) | resumable re-measure (state, skip-done, flag) |
| `finish_resumable.sh` (new) | resumable dense download by the `remeasure.done` flag |
| `resume_all.sh` + `round2_headless.sh` (new) | the @reboot resume master + headless round2 |
| `verify_findings_round2.js` (new) | round2: copy-paste 3× (exclude round1) + cycle-intro archaeology, resume filter |
| `round2_resume_prompt.txt` (new) | the round2 headless-resume prompt |
| `new300_measured.tsv` | append of the re-measured (via resumable merge) |
| `harvest2.sh` | a new axis (cpp-only) — planned |

State (not in git, persistent): `~/oss/_aidev_state/`. Cron: `/etc/cron.d/airepo_resume`.

## Phase 2026-06-26 — fold the visible AI cohort (377) in as an agentic STRATUM

Trigger: the trending experiment (top-20 C++ by star-velocity) → along the way confirmed bug #066
on a fresh sample: **6 of 6** trending repos marked by the old method as CLONEFAIL/TOOBIG
(openvino, OrcaSlicer, PCSX2, librealsense, wazuh, tensorflow) **revived** with the proper
flags (`--filter=blob:none --shallow-since=2025-05-01 --no-checkout` + `GIT_LFS_SKIP_SMUDGE=1`,
P≤5, timeout 900). CLONEFAIL ≠ junk — a method bug (as established above).

Correction of a false conclusion made along the way: "0 AI repos in CLONEFAIL" is a **tautology**, not an argument
(CLONEFAIL repos were not measured for AI at all, the clone failed before the measurement). The real loss is
precisely commit-dense/AI-dense, as stated in §Context.

State: `_aidev_dense` / `_aidev_state` cleared (0 dirs) — we clone anew.

Source when: `ai_guaranteed_cpp.tsv` (461 AI-measured) ∩ NOT in the drift dataset
`results_full.boolrule.jsonl` = **377** repos. Of the 460 AI cohort, only 83 are in the corpus.

- [~] Clone of 377 visible AI repos (`experiments/trending_run/clone.sh` method: blob:none +
      shallow-since + no-checkout + LFS-skip, P=4, timeout 300, retries) → `~/oss/<owner_name>`.
- [ ] Full per-commit drift scan (`run_worklist.py`, window from 2025-05-01, no merges,
      no cap) → `experiments/per_commit/results_ai_stratum.jsonl` with the field `cohort="ai_stratum"`.
- [ ] Add to the corpus AS A LABELED STRATUM, don't silently merge into `results_full`.

**Sampling discipline (mandatory, otherwise the work does harm):** this is an **oversampled agentic stratum**,
NOT a representative sample. It goes into population estimates (drift shares, agentic-vs-human by
population) ONLY with weighting or exclusion; for within-stratum / case-control
comparisons — as is. Cherry-picking by AI% is forbidden (selection-on-the-variable); we take the WHOLE
visible stratum, not the top. The source of this discipline: the analysis in the 2026-06-26 session.

## Note
Split out of the 2026-06-01 session: the current focus is the ANALYSIS of the existing corpus
(`CORPUS_CHECK_REPORT.md`, 112 ≥300-repos), the download moved here (part 2).
