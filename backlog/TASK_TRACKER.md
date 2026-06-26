# Backlog Task Tracker — MVP v1

_2026-06-23 (fifth pass); this file is the SSOT for v0.1 release-readiness. The core MVP
is already implemented: #103 delivered product precision, #123 shipped as advisory `--diff`
with parent-guard and durable local/Catch2 tests. What remains splits into board hygiene
(formally close/move #103) and public-readiness before announcement (#123 GitHub test repo,
#127/#131 vendored/generated applicability sign-off)._

## What we count as MVP v1

`archcheck` MVP v1 = **trusted dependency diff for a C++ PR**:

- zero-config first;
- one clear diff workflow for CI;
- deterministic `text/json` output;
- honest CLI/doc contracts;
- trusted graph signal without known gross false positives in the default mode.

This is **not** a duplication gate, not AI attribution, and not a broad semantic platform.

## How to read this file

- The priority in the task name (`blk/crt/maj/min`) stays as the task's local importance.
- The priorities below answer a different question: **what blocks shipping MVP v1 right now**.
- `P0` — MVP v1 blocker.
- `P1` — desirable in the current wave, but a first MVP is possible without it.
- `P2` — after the first MVP tag / `v0.1.x`.
- `OUT` — outside MVP v1; do not count as a release blocker.

## Already done

| Block | Tasks | What already exists |
|------|--------|--------------|
| Diff/core foundation | #018, #023, #024, #030 | `--diff`, fast-path without C/C++ changes, in-memory diff, baseline save/load |
| Reporting / CI | #025, #028, #055 | PR-comment integration, rules/report core, JSON hygiene |
| Trusted signal hardening | #034, #035, #038, #049, #068, #069 | SF.* fixes, vendor/vendored noise control |
| Config format contract | `v1_maj_config_format_minimal_contract`, #051 | schema + loader/validation, but **without runtime policy** |

Conclusion: **the MVP core already exists**. What remains is not "write a tool from scratch" but **bringing the shipped core to an honest product state**.

## P0 — MVP v1 blockers

| Block | Tasks | Why it is a blocker |
|------|--------|-------------------|
| ~~Contracts and alignment~~ | ~~#073~~, ~~#045~~ ✅ completed 2026-06-12 | contracts aligned, docs updated |
| ~~Product-grade diff workflow~~ | ~~#075~~ ✅ completed 2026-06-12 | advisory-first + stable JSON output shipped |
| Trust floor for SF.9 | ~~#032~~ ✅ implemented 04b523b | conditional edges tracked, SF.9 skips all-conditional cycles |
| ~~Copy-paste precision on the corpus~~ | ~~#103 wip → close~~ | ✅ product precision obtained: 70-findings triage, ≈86–91% precision; full 185-repo run not needed for MVP. Remainder = board move/cleanup, not a dev blocker |
| ~~new-clone in --diff~~ | ~~#123 wip~~ | ✅ shipped advisory: `DRIFT.NEW_CLONE`, parent-guard, local 10/10 control set, Catch2 E2E. Remainder = outward-facing GitHub test repo for public demonstration, not a core MVP blocker |

## P1 — do in the current wave if it doesn't break P0

| Block | Tasks | Comment |
|------|--------|-------------|
| Metric drift expansion | ~~#029~~ ✅ implemented c480e39 | chain length / god-headers / NCCD deltas in RegressionReport shipped; task file awaits closure |
| Cheap graph signals | #057 | a good next layer of value after the basic diff core |
| Small UX/doc cleanup | #046 | useful, but does not block the MVP |
| Security hardening | ~~#105~~ ✅ completed 2026-06-11 | S3–S6 closed (cb6e09d): symlink escape, 64 MiB limit, RFC 8259 jsonEscape, git hardening |

### P1 tail inside `#073`

- Cut `src/main.cpp` into a thinner application layer.
- Strengthen SF.8 to a real include-guard pattern.
- Update the outdated `AGENTS.md`.

These things are important, but **must not hold up MVP v1** if P0 is already closed.

## P2 — after the first MVP tag

| Block | Tasks / status | Comment |
|------|------------------|-------------|
| Runtime config policy | no dedicated task yet | this is already `v0.2`: `layers` / `forbidden` / `independence` as real enforcement |
| SARIF / richer integrations | `future/#005` | adoption-layer, not the first wedge |
| Selective semantic expansion | `future/#042`, `future/#050` | after the drift core stabilizes |
| Post-audit cleanup | ~~#104~~ ✅ completed; remainder — `new/#108` | section 1 closed (be56245); sections 2–4 (dups, lizard debt) moved to #108 |

## OUT — outside MVP v1

### Duplication / clone-detection branch

- `dropped/#053` (superseded #056), `wip/#054`, `wip/#056` (spike done, remainder = #072), `wip/#060` (remainder = #070), `wip/#061` (done-but-not-moved), `wip/#070`, `wip/#072`, `wip/#083` (blocked)
- `dropped/#062` (absorbed by design #072), `dropped/#064` (absorbed #065/#069/#070), `new/#088` (3/5 items already in code, remainder: №2.3 + re-run)
- `completed/#063`
- `future/#052`, `future/#071`

These are **not MVP v1 release blockers**. Keep separate from the MVP board.

### Cheap-drift / complexity wave (2026-06-10..11, from the cheap-guards research)

- Implemented (cb6e09d, 2026-06-11; in wip until moved to completed): ~~#096~~ (SATD
  delta), ~~#097~~ (test co-evolution), ~~#098~~ (god-file growth + `--history`),
  ~~#100~~ (defect attractor).
- Product candidates, advisory/delta-first: `new/#093` (flag-argument), `new/#094`
  (param accretion), `new/#095` (config-bag), `new/#099` (indentation proxy —
  fallback/absorbed by #101), `new/#101` (local complexity drift).
- Research: `wip/#102` (prototype of #101 on the corpus; verdict `revise`, see
  `docs/research/local_complexity_drift_scorer_review.md`), `wip/#103`
  (copy-paste per-commit — product precision obtained; board cleanup remains).

Not MVP v1 blockers; order of the wave remainder: #093 (shared text/signature scan) →
#094/#095/#099/#101; the git branch (#096→#097/#098/#100) is already closed, the
`git_exec`/`diff_query` infrastructure shipped and available for reuse.

### AI-repo corpus / methodology / discovery research

- `new/#074` (`#048` and `#067` — completed)
- `wip/#066`, `wip/#079`
- `future/#033`

This is a research / corpus-ops track, not a product MVP track.

### AI/config synthesis and other future scope

- `future/#010`
- `future/v1_maj_ai_config_iterative_loop`
- `future/v1_maj_ai_config_synthesis_eval_protocol`
- `future/v1_min_ai_config_synthesis_trial_spdlog`

## Active WIP not to confuse with the MVP

| Tasks | Status for the MVP |
|--------|----------------|
| ~~#041~~ | completed |
| #054, #066 | active research; do not count as the MVP v1 release tail |
| #056, #060, #061 | done / delegated (#072, #070) — candidates for closure, see backlog_review.md |

## Recommended order

1. ~~Close the `P0` slice from `#073`~~ ✅ completed 2026-06-12.
2. ~~Bring `#075` to a product-grade diff workflow~~ ✅ completed 2026-06-12.
3. ~~Core of `#123`~~ ✅ committed 344870f (advisory new-clone in `--diff`).
4. ~~Obtain product precision for `#103`~~ ✅ ≈86–91%, full 185-repo run canceled as non-decisive.
5. ~~Finish `#123` parent-guard + durable tests~~ ✅ local 10/10 + Catch2 E2E.
6. Before the public announcement: #123 GitHub test repo, #127/#131 applicability sign-off.
7. After the MVP tag: `#057`, then the cheap-drift wave. (`#088`/`#116` closed.)

## Condensed verdict (updated 2026-06-19, 5th pass)

**Core MVP-v1:** no code-level blockers remain. `#073`, `#075`, `#032`, `#045`,
`#105`, `#029`, `#103` product precision and `#123` product path are essentially closed.
Board hygiene is needed: move/close #103 and decide whether to keep #123 in wip for the
GitHub test repo or split that demo tail into a separate task.

**Before the public announcement:**
- `#127/#131` vendored/generated exclusion and corpus sign-off — the main applicability tail.
  Without it, archcheck on bundled-deps can still drown in vendor/generated noise
  (the supercollider class of cases is already sharply improved, but the bpftrace/newsboat tails are open).
- `#123` GitHub test repo — outward-facing validation/demo, not core behavior.
- First-run sanity on 3–5 known repos.

**Closed this session:** #128 (SF.8 banner), #130 (findFile), #108 (hardening SSOT).
**Board hygiene:** #129 core landed (wip, remainder = corpus golden #131 → near-close);
#122 grow+measure done (→ completed); #119 unblocked; #072 — one scope decision (dup-pairs JSON).
