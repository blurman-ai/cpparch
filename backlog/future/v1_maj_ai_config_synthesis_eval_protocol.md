# [AI][EVAL][V1] Evaluation protocol for AI config generation on Claude and Codex

**Creation date:** 2026-05-28
**Start date:** —
**Status:** new
**Module:** DOCS
**Priority:** major
**Difficulty:** M (methodology, criteria, report templates)
**Target release:** v1 phase 1 (post-MVP)
**Blocks:** a meaningful test of the hypothesis "AI lowers the barrier to entry into config mode"
**Blocked by:** future/v1_maj_agent_config_authoring_rules.md
**Related:** future/v1_min_ai_config_synthesis_trial_spdlog.md, future/010_maj_ai_rule_synthesis_contract.md, future/v1_maj_ai_config_iterative_loop.md (overfitting metrics: growth of exclude / removal of rules)

## Goal

A reproducible protocol comparing Claude and Codex on the task of creating `.archcheck.yml.draft`.
We measure the practical usefulness of the draft, not the "beauty of the YAML".

## A single input set for both models

1. The repo's file structure (`find src/ include/ -type f | head -200`)
2. The include graph in text form (archcheck --format json or analog)
3. README / ARCHITECTURE.md if present (the first 100 lines)
4. The system prompt from `docs/ai_config_authoring_rules.md` (the same for both)

Context budget: ≤ 8k input tokens. If the repo is larger — trim by the `head -200` rule.

## Metrics (compute for each run)

| Metric | How to compute |
|---------|-------------|
| **Structural correctness** | All `modules.*.paths` — actually existing directories (0/1 per module) |
| **Rule type quality** | Were `layers`/`independence` used instead of flat `forbidden`? (`layers_used`: yes/no) |
| **False boundaries** | The number of `forbidden`/`layers` rules that the existing code violates |
| **Edit distance** | The number of changed lines until the first usable config (count git diff) |
| **Confidence marking** | Are all `speculative` ones marked? (yes / partial / no) |
| **Stale entries** | The number of `# TODO`s the human removed as meaningless |

Pass threshold: `structural correctness` = 100%, `false boundaries` ≤ 2, `edit distance` ≤ 30%.

## Fair-comparison rules

- The same repository, the same git commit
- The same system prompt (from `docs/ai_config_authoring_rules.md`)
- The same input set of artifacts
- The run is saved as-is, without tuning the prompt for a specific model

## Human review sheet (fill out for each run)

```
Repo: ___________  Commit: ___________  Model: ___________  Date: ___________

Modules declared: ___   Modules correct: ___   False modules: ___
Rules declared: ___   Rule types used: [ ] layers  [ ] independence  [ ] forbidden only
False boundaries (violations in existing code): ___
Edit distance to usable (lines changed): ___
Confidence marking: [ ] full  [ ] partial  [ ] missing
Stale TODO entries removed: ___

Verdict: [ ] useful as-is  [ ] useful after minor edits  [ ] heavy rewrite needed  [ ] useless
```

## What counts as a "good" draft (criteria by references)

- Modules correspond to real directories (like Deptrac `paths:`)
- Hierarchy expressed via `layers`, not flat `forbidden` (like Import Linter `layers` type)
- No invented layers (the anti-pattern from the ArchUnit vs flat config tools study)
- Each non-obvious boundary marked as `inferred` or `speculative`

## Execution plan

- [ ] Accept `docs/ai_config_authoring_rules.md` as a prerequisite
- [ ] Lock down the single prompt contract in `docs/ai_config_eval_protocol.md`
- [ ] Lock down the human review sheet in `docs/ai_config_eval_template.md`
- [ ] Pick the first repository (candidate: spdlog) and run a pilot
- [ ] After the pilot: simplify or tighten the metrics based on the result

## Done

- (empty)

## Changed files

| File | Change |
|------|-----------|
| docs/ai_config_eval_protocol.md | evaluation methodology |
| docs/ai_config_eval_template.md | per-run report template |
