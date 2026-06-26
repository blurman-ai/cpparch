# [AI][CONFIG][V1] Iterative config synthesis loop: archcheck runs as feedback

**Created:** 2026-05-30
**Started:** —
**Status:** new
**Module:** DOCS, CONFIG, AI
**Priority:** major
**Complexity:** M (loop contract + termination + anti-overfit guardrail)
**Target release:** v1 phase 2 (after the first agent-authoring pass)
**Blocks:** a practical "agent writes the config" without manual cleanup to a clean state
**Blocked by:** future/v1_maj_agent_config_authoring_rules.md, future/v1_maj_config_format_minimal_contract.md
**Related:** future/010_maj_ai_rule_synthesis_contract.md (single-shot contract), future/v1_maj_ai_config_synthesis_eval_protocol.md (metrics), #053 / #056 (duplicates — signal source)

## Context

Three adjacent tasks cover *a single pass*:

- **#010** — the translation contract: graph features as input → YAML with citations as output. Explicitly single-shot, "an LLM translator, not an oracle".
- **agent_config_authoring_rules** — static rules for filling in the `.draft` (allowed/forbidden, confidence levels, noise files).
- **eval_protocol** — precision/recall of the `.draft` against golden, one-shot generation.

What's nowhere: the **convergence loop**. In practice a config isn't written in one pass —
the agent (or human) runs archcheck, looks at the firings, distinguishes real
violations from false ones, edits the config/exclude and runs again, until the output becomes
clean/meaningful. This task fixes the contract for such a loop.

## Goal

Describe the iterative config synthesis process, where **archcheck's own output is
the feedback signal**, and pin down:
- the termination condition (what counts as "converged"),
- the "real violation vs false positive" fork,
- a guardrail against overfitting the config to silence firings.

## Loop (draft contract)

1. **Seed.** The agent generates an initial `.archcheck.yml.draft` from a clean repo
   (per the #010 contract + the authoring-task rules): modules, layers/independence,
   exclude for hard noise classes.
2. **Run.** Run archcheck on the repo (by the agent or human) → a list of violations
   + graph metrics + duplicates (#053/#056).
3. **Triage.** For each firing — a classification:
   - **real** → this is real architectural debt; do NOT touch the config, it goes into
     `--baseline` or into a TODO for a human (fixed in code, not in the config).
   - **false-positive** → the rule is too broad / caught vendor-generated code /
     a wrong threshold → a config edit is justified.
   - **noise** → a noise file not caught by the seed exclude → extend `exclude:`.
4. **Refine.** A minimal config edit addressing only false-positive/noise.
   Each edit — with a source comment and rationale (why this is false, not debt).
5. **Re-run.** Repeat step 2.
6. **Stop.** Termination by the termination condition (below).

## Termination condition

Open question — which invariant to treat as convergence. Candidates:
- No firings classified as **false-positive/noise** (real violations are
  allowed — they go into the baseline, not a reason to edit the config).
- Δ between iterations = 0 (the config has stabilized, new runs change nothing).
- A hard iteration limit (anti-runaway) + a report if it didn't converge.

## Anti-overfit guardrail (the key risk of this task)

The main danger of the loop, which is NOT present in single-shot #010: the agent "achieves cleanliness"
not by finding the architecture, but by **silencing firings** — a swelling
`exclude:`, removed rules, raised thresholds, just to make the output go quiet.

- A config that forbids nothing / excludes everything is a **failure**, not a success.
  Clean output with an empty rule set is a red flag.
- Every silencing edit must carry a source: *why this is a false
  positive*, not "let's remove it so it doesn't bother us".
- `exclude:` grows only with hard noise classes (vendor/generated/build from the
  authoring task) — not "a folder with lots of violations".
- Removing/weakening a rule between iterations is logged separately (rule diff),
  so a human sees that the agent specifically weakened, rather than found.
- Real violations are NOT silenced by a config edit — only the baseline. Shifting
  real debt into `exclude` is forbidden.

## Connection to eval (#protocol)

The metric "converged in N iterations" and "how much did the exclude swell / how many rules were removed
by the end" are candidates for eval_protocol as overfitting indicators. A loop yielding
clean output at the cost of an empty config should be caught by eval as low recall.

## Open questions

- Who turns the loop: does the agent run archcheck itself (needs a tool-call / CLI access) or
  does a human run it and hand the output back? The MVP variant — human-in-the-loop.
- The format for feeding violations back to the agent (the same JSON slice as the input to #010?).
- Termination: which of the candidate invariants we take as the primary one.
- Where the triage classification (real/fp/noise) lives — it's not in archcheck's output,
  it's the agent's decision; whether it needs to be serialized for audit.
- Granularity of edits: is the whole config rewritten each iteration or a delta.

## Plan (research-only, no code)

- [ ] Describe the loop in docs/research/ (or extend ai_assisted_rule_synthesis.md with an "iterative loop" section)
- [ ] Pin down the triage contract: real / false-positive / noise + what to do with each
- [ ] Formulate the termination invariant (pick from the candidates)
- [ ] Write the anti-overfit guardrail as an explicit checklist (rule diff, exclude growth, empty config = failure)
- [ ] Decide: human-in-the-loop vs an agent with CLI access for the MVP
- [ ] Link to eval_protocol (overfitting metrics) and #010 (feedback format = input format)
- [ ] A prototype prompt for the iterative mode (outside the repo, in the research doc)

## Done

- (empty)

## Changed files

| File | Change |
|------|--------|
| docs/research/ai_assisted_rule_synthesis.md (or a new doc) | a section on the iterative loop + anti-overfit guardrail |
| future/010_maj_ai_rule_synthesis_contract.md | cross-ref: feedback format = single-shot input format |
| future/v1_maj_ai_config_synthesis_eval_protocol.md | cross-ref: overfitting metrics (exclude growth / rule removal) |
