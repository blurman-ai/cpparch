# [TEST][DRIFT] DRIFT.1 fixture: LibreSprite PR #581 shortcut edge

**Created:** 2026-05-29
**Started:** 2026-05-29
**Completed:** 2026-05-29
**Status:** completed
**Module:** TEST / DRIFT
**Priority:** major
**Complexity:** S (half a day)
**Blocks:** public DRIFT.1 demo (README / HN post / `docs/research/ai_drift_cases.md`)
**Blocked by:** —
**Related:** #009 (DRIFT.1/2 impl), #033 (AI-drift dataset), docs/milestones.md run 10 (2026-05-29)

## Goal

Set up a minimal reproducible fixture
`fixtures/drift_real_world/libresprite_pr581/` for the single real DRIFT.1
hit verified 2026-05-29: a new edge
`app/ui/toolbar.cpp -> app/pref/preferences.h` in LibreSprite PR #581
(before `60eed0f` → after `276fdbd`, commit `0aa57ad`, Co-Authored-By: Claude
Opus 4.5).

## Context

Verification (see milestones.md, run 10, section "Verification") closed
the main objection: the include was **absent** in `toolbar.cpp` at the before-SHA
of the LibreSprite fork and was **added** by AI commit 0aa57ad, not reverted.
Upstream `aseprite/aseprite` keeps this edge — framing for the demo: "the fork
lost a dependency through drift, the AI commit brought it back, archcheck recorded
the change in this repo's graph".

This is the first and so far only CONFIRMED hit in the DRIFT corpus (3 runs:
1 hit / 2 clean), so it is also the headline case for public material.

## Execution plan

Per #033: fixture = **minimal reproducible graph slice**, not a repo clone.

- [x] Create `fixtures/drift_real_world/libresprite_pr581/`
- [x] After-state as a set of source files in the fixture directory
      (`app/ui/toolbar.cpp` with includes `toolbar.h` + `preferences.h`,
      `app/ui/toolbar.h`, `app/pref/preferences.h` — all stubs `#pragma once`)
- [x] Before-state via `baseline.graph.yml` (convention from neighboring
      DRIFT fixtures: nodes/edges YAML, not a separate `before/` dir) — all three
      nodes, only the edge `toolbar.cpp -> toolbar.h`, without preferences
- [x] `README.md` in the fixture folder: SHA pair, link to the PR, link to
      milestones.md, skeptic framing (upstream Aseprite vs LibreSprite fork)
- [x] Integration test in `tests/integration/rules/drift_fixtures_test.cpp` —
      assert `v.size()==1`, `ruleId=="DRIFT.1"`, message contains
      `app/ui/toolbar.cpp -> app/pref/preferences.h`
- [x] Fixture compile-free — only include directives + `#pragma once`

## Acceptance criteria

`ctest` green, the fixture test reproduces exactly the hit recorded in
milestones.md run 10. The fixture folder is self-contained — it does not depend on
sandbox clones.

## Done

Fixture `fixtures/drift_real_world/libresprite_pr581/` built following the scheme of
existing DRIFT fixtures (`drift_shortcut_edge/fail_new_coupling/`):

- **After-state** — files in the directory: `app/ui/toolbar.cpp` (includes
  `app/ui/toolbar.h` + `app/pref/preferences.h`), `app/ui/toolbar.h`,
  `app/pref/preferences.h`. All headers are `#pragma once` stubs.
- **Before-state** — `baseline.graph.yml`: the same 3 nodes, the single edge
  `toolbar.cpp -> toolbar.h`. We deliberately do NOT add the edge to `preferences.h`.
- **Test** — `tests/integration/rules/drift_fixtures_test.cpp` adds
  `TEST_CASE "drift fixture: real_world/libresprite_pr581 — DRIFT.1 fires on
  toolbar -> preferences"`. Checks `v.size()==1`, ruleId DRIFT.1, and that the
  message text contains the expected edge.
- **README.md** — context + skeptic framing (upstream Aseprite keeps the edge,
  the LibreSprite fork lost it, the AI commit brought it back).

Run: `archcheck_tests "[drift][fixtures]"` → 5 test cases, 18 assertions,
all green (vs 4/15 before the addition). Lizard on `src/ include/ tests/` —
green.

Principle: for the DRIFT.1 fixture, `baseline.graph.yml` + after-state sources
are sufficient. No need to compile, no `compile_commands.json` needed,
no full repo clone needed. From a real hit on the 1253-node graph of
LibreSprite we get a reproducible 3-node slice that holds
exactly one skeptic-proof claim: "the AI commit added an edge between
two pre-existing modules, archcheck caught it".

Demo framing (for README/HN): edge `app/ui/toolbar.cpp ->
app/pref/preferences.h` is present in upstream `aseprite/aseprite`, but
LibreSprite at before-SHA `60eed0f` did not carry it. This removes the objection
"just re-convergence to upstream" — the graph of **this** repo really
changed, and the AI commit changed it. Full git verification (before-grep /
after-grep / introducing commit) — in `docs/research/ai_drift_cases.md`
section "Verification" and `docs/milestones.md` Run 10.
