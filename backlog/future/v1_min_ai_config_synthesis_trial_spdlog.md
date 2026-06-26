# [AI][EVAL][V1] Pilot run of AI config generation on `spdlog`

**Created:** 2026-05-28
**Started:** —
**Status:** new
**Module:** DOCS
**Priority:** minor
**Complexity:** S (one practical experiment)
**Target release:** v1 phase 1 (post-MVP)
**Blocks:** —
**Blocked by:** future/v1_maj_agent_config_authoring_rules.md, future/v1_maj_ai_config_synthesis_eval_protocol.md
**Related:** future/v1_maj_ai_config_synthesis_eval_protocol.md

## Goal

Check on one repo: can an agent produce a useful `.archcheck.yml.draft` that a human
doesn't rewrite from scratch.

## Repository: spdlog

Repo: https://github.com/gabime/spdlog

Reasons for the choice: C++, well-known, manageable, non-trivial structure.

Expected module structure for the draft (observed, verify on the current commit):

```
include/spdlog/          → core (main headers)
include/spdlog/sinks/    → sinks (output backends)
include/spdlog/details/  → details (low-level, not for public include)
include/spdlog/fmt/      → fmt (embedded fmt library)
```

Expected rules (inferred, without running):
- `details` — the lowest level, sinks and core may include it
- `sinks` and `core` — should not depend on each other (independence?)
- `fmt` — an isolated embedded lib, no one should include it directly except details

These are hypotheses — the agent must confirm or refute them against the include graph.

## Input artifacts for the run

1. `find include/ -type f -name "*.h" | sort` — the file structure
2. The include graph (run archcheck or the python include scanner)
3. `head -100 README.md`
4. The system prompt from `docs/ai_config_authoring_rules.md`

## What to record from the result

- The exact `git rev-parse HEAD` of spdlog the run was performed on
- Claude's `.draft` — `docs/research/ai_eval/spdlog_claude_draft.yml`
- Codex's `.draft` — `docs/research/ai_eval/spdlog_codex_draft.yml`
- Filled-in human review sheet — `docs/research/ai_eval/spdlog_review.md`
- The final usable config after edits — `docs/research/ai_eval/spdlog_final.yml`

## Conclusion at the end (fill in afterwards)

```
Verdict Claude: ___
Verdict Codex: ___
Edit distance Claude: ___  Codex: ___
Main mistakes: ___
Hypothesis: synthesis useful / not useful / useful with heavy review
```

## Execution plan

- [ ] Record the exact spdlog commit
- [ ] Gather the input artifacts
- [ ] Run Claude per the protocol, save the `.draft`
- [ ] Run Codex per the same protocol, save the `.draft`
- [ ] Fill in the review sheet using the metrics from eval_protocol
- [ ] Write the conclusion: synthesis useful / not useful / useful with heavy review

## Done

- (empty)

## Changed files

| File | Change |
|------|--------|
| docs/research/ai_eval/spdlog_claude_draft.yml | Claude run artifact |
| docs/research/ai_eval/spdlog_codex_draft.yml | Codex run artifact |
| docs/research/ai_eval/spdlog_review.md | comparison and conclusions |
| docs/research/ai_eval/spdlog_final.yml | final config after edits |
