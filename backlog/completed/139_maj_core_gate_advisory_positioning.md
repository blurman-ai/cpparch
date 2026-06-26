# [DOCS][PRODUCT] Rebuild the positioning around the core gate and the advisory layer

**Created:** 2026-06-23
**Start date:** 2026-06-23
**Status:** completed
**Module:** DOCS / PRODUCT / CLI
**Priority:** major
**Complexity:** M
**Blocks:** a consistent external narrative about what archcheck is and what it is not
**Blocked by:** —
**Related:** #075 (mvp_v1_trusted_diff_workflow), #093 (flag_argument), #096 (satd_delta), #097 (test_co_evolution), #098 (god_file_growth), #100 (defect_attractor), #101 (local_complexity_drift), #123 (diff_new_clone_gate)

## Goal

Define and record an honest product frame: how to explain `archcheck` when the core gate stays architectural, but next to it there already lives a broad advisory layer over new code and history.

## Context

The old positioning was simple: "Lakos physical design + Core Guidelines SF.* checks in CI", not a linter, not a bug finder, not an IDE tool. The code is already more complex than this formula.

- In `--diff` today there is not only graph drift, but also SATD, test co-evolution,
  local complexity drift, new clone drift and flag-argument drift:
  [src/cli/diff_command.cpp#L165-L211](../../src/cli/diff_command.cpp#L165-L211).
- The CLI already has a separate advisory mode `--history`:
  [src/main.cpp#L36-L42](../../src/main.cpp#L36-L42).
- The changelog honestly shows this evolution, but README and the spec still mostly
  speak the language of "architecture only":
  [CHANGELOG.md#L31-L62](../../CHANGELOG.md#L31-L62),
  [README.md#L123-L131](../../README.md#L123-L131).

Until this is framed, the user naturally feels a contradiction:
the tool says "not a linter", yet signals about TODO/HACK, tests, boolean flags and copy-paste.

## Execution plan

- [ ] Break down the current signals into layers: core gate, structural advisory, hygiene advisory, history analytics.
- [ ] Decide what goes into the product headline and what lives as a secondary/advisory capability.
- [ ] Rewrite README/spec/CI copy so that the advisory layer does not look like scope creep or a random set of smell-checks.
- [ ] Explain explicitly why this is still not a clang-tidy replacement: diff-scoped, advisory-first, regression-oriented, with no claim to style/semantic linting.
- [ ] Review section names and terms: a stable term for the non-gating signals may be needed (`advisories`, `delta heuristics`, `PR hygiene layer`).
- [ ] Record in the narrative that the gate is NOT uniform across modes: check/drift gate only on cycles (SF.9 / DRIFT.1·2·4.CYCLE), while `--diff` also gates on new god-headers ([include/archcheck/diff/regression_report.h#L79](../../include/archcheck/diff/regression_report.h#L79)). The comment [src/cli/check_command.cpp#L95](../../src/cli/check_command.cpp#L95) currently lies ("gate = cycles" about diff) — the positioning must not repeat this false symmetry (in sync with #141).

## Done

- README and the architecture spec got an explicit layer model: core gate, structural advisories, PR hygiene advisories, history analytics.
- Explained why the advisory layer does not make archcheck a clang-tidy replacement: diff-scoped, regression-oriented, advisory-first.
- The gate differs by mode: check = `SF.9`, drift = `DRIFT.1/2/4.CYCLE`, diff = new/grown cycles + new god-headers.
- The CI doc describes advisory blocks vs the gating verdict.

## In progress

- (empty)

## Next steps

- None. New signal waves must explicitly choose a layer: gate or advisory.

## Key decisions

| Decision | Reason |
|---------|---------|
| Don't sweep the advisory layer under the rug | it is already shipped and visible to the user in `--diff`/`--history` |
| Keep the core gate separate from the advisory | it is exactly the narrow gate that preserves the trust floor and distinguishes archcheck from broad smell-lint |
| Argue the frame through the operating mode, not through a list of topics | SATD/clone/test signals are acceptable as long as they stay diff-scoped and advisory-only |

## Changed files

| File | Change |
|------|-----------|
| `README.md` | rewrite the headline / feature framing |
| `docs/architecture-spec.md` | separate the core product and the advisory layers |
| `docs/MVP.md` | if needed, refine the acceptance language around trusted narrow gates |
| `docs/ci_integration.md` | explain advisory blocks vs the gating verdict |
