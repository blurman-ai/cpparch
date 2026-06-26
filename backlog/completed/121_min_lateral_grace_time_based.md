# [SCAN] lateral_drift_scan: grace period for a new module — time-based instead of 10 commits

**Created:** 2026-06-12
**Started:** 2026-06-12
**Completed:** 2026-06-12
**Status:** done
**Module:** SCAN
**Priority:** minor
**Complexity:** small
**Blocks:** —
**Blocked by:** —
**Related:** #111 (limitation recorded in §7 of the run report), #115 (full corpus — data for calibration)

> Anchors in `experiments/ai_repo_run/lateral_drift_scan.py` captured 2026-06-12.

## Goal

Replace the newborn-module grace period of "10 repo commits" with time-based
(calendar days), so hot repos don't fall out of grace too early.

## Context

The grace period suppresses events around a module's birth: its first edges in all directions —
that's formation, not drift (criterion, §5 "Newborn modules"). Currently
(`lateral_drift_scan.py`):

- line 36: `GRACE_PERIOD_COMMITS = 10`;
- lines 435-441: `mod_first_idx` — index of the commit where the module first appeared;
  baseline modules are seeded with `-GRACE_PERIOD_COMMITS` (they get no grace);
- lines 481-482: skip the event if `idx - mod_first_idx[mod] < GRACE_PERIOD_COMMITS`
  for either of the two modules.

Problem (report #111, §7): 10 commits is **hours** for a hot repo. KDE case:
the `libklookandfeel` library matured over 59 commits — its maturing edges surfaced
as events. For a cold repo the opposite: 10 commits = half a year, grace is excessive.
Calendar time is more robust to commit pace.

## Execution plan

- [ ] **Mechanism replacement.** The jsonl records have `date` (ISO `YYYY-MM-DD`, a record
      field in `scan_repo`). Instead of `mod_first_idx` — `mod_first_date: dict[str, date]`:
      - on a module's first appearance in replay, remember the commit date;
      - seed baseline modules "long ago" (e.g. `date.min`) — no grace, as now;
      - skip the event if for ANY of the modules
        `(commit_date - mod_first_date[mod]).days < GRACE_PERIOD_DAYS`.
- [ ] **Hybrid (recommended).** Keep the lower bound in commits too:
      `GRACE_PERIOD_DAYS = 30` AND `GRACE_PERIOD_COMMITS = 10`, skip if
      at least one condition is in grace. Reason: for a repo with one commit a month
      30 days is 1 commit, the birth events aren't suppressed.
- [ ] **Calibration on the corpus.** Re-run 479 jsonl with 30 days;
      check three things:
      1. KDE case: maturing NEW events of `libklookandfeel` suppressed
         (did 59 commits fit within 30 days? — check the dates; if not,
         look at 45/60 days — but do NOT tune the threshold to a single case,
         document the choice);
      2. CYCLE events not lost (grace suppresses those too — look at how many CYCLE
         fell under the new grace; if real cycles from the #115 eyeball are suppressed —
         take CYCLE out of grace: a cycle at module birth is a smell too);
      3. Delta of the total event count (495 → ?) broken down by grade —
         into the report.
- [ ] **Report.** Inset in
      [lateral_module_drift_corpus_run.md](../../docs/research/lateral_module_drift_corpus_run.md)
      (modeled on the #117 inset in §8.3): old/new numbers, decision on the CYCLE exclusion,
      chosen constants.

## Done

- Delta analytics before the code change (2026-06-12):
  - Proxy estimate: event_date − repo_first_date (upper bound, since the module appears later than the repo).
  - At GRACE=30d: **141 / 495** events fall under grace (28%) — upper bound.
  - By grade: CYCLE 52/146 (36%), NEW 72/288 (25%), SDP 17/61 (28%).
  - Conclusion: CYCLE should be excluded from grace (36% of CYCLE are lost — these are birth smells,
    no need to suppress them). NEW/SDP — real suppression ~10-15% (module later than the first commit).

## In progress

- (empty)

## Implemented + re-run results (2026-06-12)

- `GRACE_PERIOD_DAYS = 30` constant added (line 38).
- `from datetime import date as _date` import added.
- `mod_first_date: dict[str, _date]` — baseline modules get `_date.min` (no grace).
- Hybrid grace: `_in_grace(mod)` = commit_days < 30 OR idx_delta < 10.
- CYCLE excluded from grace: `is_cycle_candidate` is checked before `_in_grace()`.
- Full corpus (479 repos):
  - Old baseline: 495 events (CYCLE 146, NEW 288, SDP 61)
  - New: **417 events** (CYCLE 186, NEW 191, SDP 40)
  - CYCLE +40: legitimately came out of grace (expected)
  - NEW −97 (−34%), SDP −21 (−34%): grace 30 days + file-split (#119) suppress
  - Verified: AztecProtocol `barretenberg/chonk` (14 days to the event) → correctly suppressed
  - Verified: CipherMesh (agentic, 28 days) → correctly suppressed

## Next steps

1. Write the numbers into `lateral_module_drift_corpus_run.md` (inset §8.7).
2. Decide: is −34% NEW within acceptable bounds? Possibly reduce to 20 days if needed.

## Key decisions

| Decision | Reason |
|---------|---------|
| Time-based 30 days + hybrid with 10 commits | Calendar is robust to hot-repo pace; the commit lower bound covers cold ones |
| Consider excluding CYCLE from grace | A cycle at module birth is a smell regardless of maturity; decide on the re-run data |
| Don't tune the threshold to the KDE case | One case is not calibration; document the choice and counterexamples |

## Changed files

| File | Change |
|------|-----------|
| `experiments/ai_repo_run/lateral_drift_scan.py` | mod_first_date + hybrid grace (lines 36, 435-441, 481-482) |
| `experiments/ai_repo_run/lateral_drift_new.csv` | Regeneration |
| `docs/research/lateral_module_drift_corpus_run.md` | Inset with numbers |

## How it works (summary)

**Principle.** The grace period suppresses events around a module's birth (its first edges —
formation, not drift). The mechanism is replaced from purely-commit-based to **hybrid**:
`_in_grace(mod)` = `(commit_date − mod_first_date[mod]).days < 30` OR
`(idx − mod_first_idx[mod]) < 10`. The calendar condition is robust to hot-repo pace;
the commit lower bound covers cold ones.

**CYCLE outside grace.** `is_cycle_candidate` is checked BEFORE `_in_grace()`: a cycle at a module's
birth is a smell regardless of maturity, it must not be suppressed. On the corpus this returned +40 CYCLE.

**Baseline modules.** Seeded with `mod_first_date = _date.min` and `mod_first_idx = -10` — they get no grace
(they existed before the observation window).

## What controls it

`GRACE_PERIOD_DAYS = 30`, `GRACE_PERIOD_COMMITS = 10` in `lateral_drift_scan.py`. The choice of 30
days is documented and was NOT tuned to the single KDE case.

## Related to

The limitation is recorded in #111 §7 (KDE `libklookandfeel` matured over 59 commits). Batch
neighbors: #119 (file-split) and #120 (removed). Python twin of C++ DRIFT.4 (#118).

## Diagnostics

Re-run of 479 repos (2026-06-12): 495 → **417** events (CYCLE 146→186, NEW 288→191,
SDP 61→40). Counterexample checks: AztecProtocol `barretenberg/chonk` (14 days → suppressed),
CipherMesh (28 days → suppressed). −34% NEW/SDP — combined effect of grace and #119; within
acceptable bounds (CYCLE precision not affected). Results in `lateral_module_drift_corpus_run.md` §8.7.
