# [RESEARCH/FIXTURES] AI-assisted C++ repos as architectural drift test cases

**Created:** 2026-05-28
**Started:** 2026-05-29
**Status:** in-progress
**Module:** RESEARCH/FIXTURES
**Difficulty:** M (research ~2 days, integration into fixtures — separate)
**Target release:** v0.2+
**Blocks:** —
**Blocked by:** ~~#009 (ai_drift_regression_rules)~~ ✅ DRIFT.1/DRIFT.2 ready
**Related:** #009 (ai_drift_regression_rules), #030 (baseline_file_flag)

## Goal

Collect and document a set of real C++ repositories with verified AI-assisted
commits as reference test cases for DRIFT.1/DRIFT.2 and validation of archcheck
under conditions of real architectural drift.

## Context

AI-assisted development leaves traces in git history: `Co-Authored-By: Claude`,
`Generated with Claude Code`, CLAUDE.md, AGENTS.md, bot PR authors. This opens the
possibility to find public repositories where specific changes were made by AI,
and run archcheck before/after — comparing the dependency graph.

Key hypothesis: an AI agent often makes a plausible shortcut across a module
boundary (adds a convenient helper in the wrong layer, pulls an include from the
"wrong" place) without creating obvious garbage, but accumulating structural drift.

The goal of the task is **not** to prove that AI is bad. The framing:
> Given an AI-assisted change set, did the dependency graph move further away
> from the project's existing architectural structure?

This turns real AI commits into an honest stress test for the DRIFT rules.

## Candidates (research 2026-05-28)

### Tier 1 — best first candidates

| Repo | Why it fits | What to check |
|------|-----------------|---------------|
| **LibreSprite/LibreSprite** | C++ GUI app, PR with a batch of Claude Code commits: macOS gestures, menu search, toolbar badges, Makefile, docs | UI does not reach into app/core; macOS-specific logic does not spread beyond the platform layer; fan-in of new helpers |
| **proximafusion/vmecpp** | C++/Python scientific, PR #359 explicitly marked Claude Code, touches C++ bindings, Python API, outputs, build | core numerical code does not depend on Python bindings; bindings do not pull test/demo deps; output validation does not spread into the solver core |
| **PrusaSlicer AI fork PR** | Large C++/CMake, PR with many Claude co-authored commits: geometry, G-code, UI dialogs, CI, docs | UI dialogs do not depend on low-level geometry impl; printer-specific logic does not spread across the common G-code path |

### Tier 2 — interesting, but with caveats

| Repo | Why it's interesting | Caveat |
|------|-----------------|----------|
| **BambuStudio PR #10794** | Large C++/CMake, PR has an explicit instruction to always add Co-Authored-By: Claude Opus 4 | One PR — need to check the systematicity of the AI workflow across the whole project |
| **PX4/PX4-Autopilot** | Huge safety-critical C/C++, has a CLAUDE.md with rules for Claude | Explicitly forbids AI attribution — can't be found by trailers. Good for testing "architectural rules are written down, enforcement is needed" |
| **nodos-dev/sys-device** | Fresh CI run with `Co-Authored-By: Claude Opus 4.6`, CMake changes | Niche, need to check size and structure |
| **OpenMS/contrib** | CMake/build scripts, release with co-authored Claude | More of a build/dependency repo than application architecture |

### Metrics for measuring drift

```
git diff <before-ai>..<after-ai> --name-only   # which files were touched
```

archcheck metrics for before/after comparison:
- new include edges between top-level modules
- new reverse dependencies (direction violations)
- fan-in growth of new "utility" headers
- platform-specific includes in generic layers
- test/build/demo deps in production code
- "one commit crosses too many architectural zones"

### Target demonstration form (for README)

```
Before AI PR:
  ui → app → core
  platform → she

After AI PR:
  core → ui        ❌  DRIFT.1: new shortcut edge
  app → macOS      ❌  DRIFT.1: platform leak into generic layer
  gcode → dialog   ❌  DRIFT.1: upward dependency
```

### GitHub search queries (for automation)

```bash
gh search commits '"Generated with Claude Code" "C++"' --limit 100
gh search commits '"Co-Authored-By: Claude" "CMakeLists"' --limit 100
gh search prs '"Generated with Claude Code" "CMakeLists.txt"' --limit 100
gh search code 'filename:CLAUDE.md "C++"' --limit 100
gh search code 'filename:AGENTS.md "C++"' --limit 100
```

For each candidate:
```bash
git log --all --grep='Claude\|Generated with\|Co-Authored-By' \
    --regexp-ignore-case --stat
```

## Execution plan

- [ ] For each Tier 1 repo: clone, find AI commits, fix the commit range
- [ ] Run archcheck include-scan before/after AI commits (once DRIFT.1 is ready)
- [ ] Document the violations found in `docs/research/ai_drift_cases.md`
- [ ] Pick 2-3 scenarios for integration fixtures in `fixtures/drift_*/`
- [ ] Update README with a demonstration on real data
- [ ] Extend the search with automated GitHub queries for new candidates

## Done

- Initial candidate research (2026-05-28): 8 repos evaluated, Tier 1/Tier 2 identified
- **2026-05-29: first DRIFT run on real repos.** Results in [docs/research/ai_drift_cases.md](../../docs/research/ai_drift_cases.md):
  - **LibreSprite PR #581** (60eed0f → 276fdbd): **1 DRIFT.1 hit** — `app/ui/toolbar.cpp -> app/pref/preferences.h` (commit `0aa57ad` "toolbar shortcut badges"). A textbook shortcut edge, the agent dragged an include from the preferences layer into a UI widget that had not previously talked to it.
  - **vmecpp PR #360** (df63271 → 5eabd51, asymmetric infra): 0 DRIFT — large refactor without drift.
  - **vmecpp PR #340** (b44fb7f → a7797dc, consolidate constants): 0 DRIFT — refactoring done correctly.
- Confirmed: DRIFT rules do not produce false-positive noise on normal AI code; they fire exactly on the findings they were designed for.
- **2026-05-29 (second session): corpus expanded to 33 PRs across 10 repositories.**
  Full working log: [docs/research/ai_drift_runlog.md](../../docs/research/ai_drift_runlog.md).
  Run 15 in milestones.md. Result: **12 DRIFT.1 hits across 7 PRs** (21% of PRs
  with drift). New repos with hits: BambuStudio #10794 (2), IrredenEngine #727 (2),
  Kartend #27 (5), Skyrim #2326 (1), Skyrim #2207 (1). 4 archetypes confirmed:
  UI→widgets, UI-config→core, generic→features, system→component, data→ui-config.
- **Found 2 bugs in archcheck during the run:** #047 (BOM, closed) and #048
  (dirty checkout methodology, new). Helper `scripts/drift_run.sh` written.

## In progress

- (empty)

## Next steps

1. ~~Wait for DRIFT.1/DRIFT.2 implementation in #009~~ ✅
2. ~~Start with LibreSprite — the smallest of Tier 1, clear architecture~~ ✅ (PR #581 hit)
3. ~~Fix the specific commit SHAs for before/after in this file~~ ✅ (in docs/research/ai_drift_cases.md)
4. Turn the LibreSprite hit into a minimal fixture `fixtures/drift_real_world/libresprite_pr581/`
5. Run the PrusaSlicer AI fork PR — the third Tier 1 case
6. Add 2-3 Tier 2 cases (BambuStudio #10794, nodos-dev/sys-device)
7. When the corpus reaches ≥ 5 PRs — update README with a demonstration on real data

## Key decisions

| Decision | Reason |
|---------|---------|
| Neutral framing ("stress test", not "AI is bad") | Scientific honesty; the task is tool validation, not accusation |
| Tier 1 — projects with explicit AI attribution in commits | Clear before/after by SHA without ambiguity |
| PX4 in Tier 2, not Tier 1 | Forbids attribution — AI commits are not identifiable by git |
| Tier 1 as fixtures first, Tier 2 as open-ended research | Fixtures are needed for CI; Tier 2 is better for paper/blog |

## Changed files

| File | Change |
|------|-----------|
| docs/research/ai_drift_cases.md | future documentation of findings |
| fixtures/drift_*/real_world/ | future integration fixtures from real repos |
