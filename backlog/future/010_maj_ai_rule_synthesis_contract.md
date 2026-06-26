# [RULES] AI-assisted rule synthesis contract

**Created:** 2026-05-26
**Started:** —
**Status:** new
**Module:** RULES
**Priority:** major
**Complexity:** M (design 1-2 days, implementation — a separate task)
**Target release:** v0.3+ (not v0.1)
**Blocks:** —
**Blocked by:** #008 (dependency_graph_foundation — synthesis may rely on the same graph contract)
**Related:** #009 (drift_regression_rules — synthesis ↔ drift pair), future/v1_maj_ai_config_iterative_loop.md (iterative mode on top of this single-shot contract)

## Goal

Pin down the **contract** for the "AI-assisted rule synthesis" process — without implementation, but concretely enough that a future implementation is unambiguous. So that the spec reader, the contributor and the user share the same picture of what archcheck does, what it doesn't, and where the boundaries are.

## Context

The term **AI-assisted rule synthesis** appeared in spec v2.1 — a process where an AI agent reads a repository, builds an architectural hypothesis and turns it into checkable rules. Conceptually paired with DRIFT rules: synthesis derives the contract, DRIFT watches that it doesn't erode.

The spec records:
- Operational shape: a separate shell flow (`archcheck synthesize`), not magic inside an ordinary run.
- The archcheck core does **not** call an LLM itself; synthesis is either local heuristics or a separate wrapper agent (Claude Code / Cursor / a homegrown script) reading the same repository and emitting YAML.
- Output — `.archcheck.yml.draft`, which the user explicitly confirms.
- Risk set: false hypothesis, privacy, volatility (see §"Key risks" item 6).

This task formalizes it into a full-fledged contract with invocation examples, the output schema, modes and integration points. Without it, any implementation would re-litigate the basic questions.

## Execution plan

- [ ] **CLI interface.** Pin down the subcommand:
   - `archcheck synthesize` — the main entry point.
   - Which flags (`--out`, `--from-git`, `--mode=heuristic|wrapper-prompt`, `--accept-baseline`).
   - Exit codes.
- [ ] **Two modes.**
   - **Heuristic mode** (default, always available): local inference rules without an LLM. Sources — path structure, common header detection, facade pattern by actual includes.
   - **Wrapper-prompt mode**: archcheck emits a structured prompt that the user feeds to any agent (Claude Code, Cursor, a local Llama). The agent's output is returned via `archcheck synthesize --apply-wrapper-output <file>`. This preserves privacy — the user decides which agent to trust.
- [ ] **Output schema.** What exactly lives in `.archcheck.yml.draft`:
   - A YAML-valid config with the same keys as the ordinary one (`modules`, `rules`, `defaults`, `thresholds`).
   - **Each synthesized rule is marked with a comment `# synthesized: <source heuristic> [confidence: low|med|high]`.**
   - A `version: 1` field + `synthesized: true` in the header.
   - A hash of the source repository (e.g. git HEAD), so it's visible what it was synthesized from.
- [ ] **User confirmation flow.** How the user accepts the synthesized config:
   - Default: synthesized rules go in `report-only` mode. archcheck reports "here's what synthesis thinks, it fails nothing".
   - On an explicit `archcheck synthesize --confirm` or a manual rename `.draft → .archcheck.yml` the rules become real.
   - In the `report-only` report — a diff with what's already in `.archcheck.yml` (if any).
- [ ] **Heuristic catalog (minimal for v0.3).** Exactly which heuristics are implemented in heuristic mode:
   - Path-as-module: top-level directories under `src/` become modules.
   - Facade detection: if `src/A/*.cpp` accesses `src/B/` 90%+ through a single header — that's an inferred facade.
   - Layered hint: if `src/domain/` exists — assume `domain forbidden_deps: [infrastructure, ui]` if those exist too.
   - A confidence rating for each heuristic.
- [ ] **Wrapper-prompt format.** What archcheck gives the agent:
   - A compact YAML description of the current structure (modules with paths, existing fan-in/fan-out, top hubs).
   - Candidate lists of modules and presumed boundaries.
   - An output template (the same YAML format, to fill in).
- [ ] **Privacy guarantees.** Explicitly in the documentation:
   - archcheck heuristic mode is fully offline.
   - wrapper-prompt mode — the user chooses who to hand the prompt to.
   - No telemetry in the synthesize command.
- [ ] **Volatility mitigation.** How synthesize handles repeated runs:
   - Doesn't overwrite an existing `.archcheck.yml.draft`, emits a diff.
   - Heuristic mode is fully deterministic (same repo → same output).
   - Wrapper-prompt mode is non-deterministic, but this is explicitly marked in the output (`source: wrapper-agent <agent-id>` + date).
- [ ] **Reference scenarios.** Small test projects in `fixtures/synthesize/`:
   - `clean_layered/` — three directories, obvious hierarchy → synthesis derives the corresponding `forbidden_deps`.
   - `mixed_old_new/` — partly legacy, partially refactored → synthesis marks unclear modules as `unknown`.
   - `monolith/` — no obvious structure → synthesis returns an empty draft with a warning.

## Done

- (empty)

## In progress

- (empty)

## Next steps

1. Pin down the CLI interface and output schema (this is a blocking decision for the subsequent steps).
2. Decide whether we implement heuristic mode and wrapper-prompt mode both at once, or heuristic first.
3. After closing #010 — open a separate implementation task (for v0.3).

## Key decisions

| Decision | Rationale |
|---------|-----------|
| archcheck doesn't call an LLM itself | Privacy + core determinism. LLM logic is outside, via wrapper-prompt. |
| Synthesis is a separate shell flow, not part of an ordinary run | So that `archcheck` in CI stays fast and offline. |
| Output is always `.archcheck.yml.draft`, not straight into `.archcheck.yml` | The user must explicitly confirm, no auto-apply magic. |
| Each synthesized rule is marked with a comment carrying source and confidence | When in half a year the rule starts getting in the way — it'll be visible why it appeared. |
| Heuristic mode = deterministic | So that CI results are reproducible. |
| Wrapper-prompt mode = optional | Not every user is ready to give their code to an LLM; heuristic must cover the basic scenarios. |

## Changed files

| File | Change |
|------|--------|
| `docs/architecture-spec.md` | the "AI-assisted rule synthesis" section will be expanded with the contract |
| `docs/dev/synthesis_contract.md` (new) | a detailed CLI + output schema contract (if it comes out long) |
| `src/cli/synthesize.cpp` (future) | implementation of the subcommand (a separate task) |
| `fixtures/synthesize/*` (future) | reference scenarios (a separate task) |

## Fixtures (contract level, no code)

- [ ] A sample `.archcheck.yml.draft` for a clean_layered project (what the output looks like)
- [ ] A sample wrapper-prompt output schema (what the agent should return)
- [ ] A sample diff output on a repeated synthesize (with an already-existing draft)
