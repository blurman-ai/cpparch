# [DOCS] Refactoring the architecture spec v2.0 → v2.1

**Created:** 2026-05-26
**Started:** 2026-05-26
**Completed:** 2026-05-26
**Status:** done
**Module:** DOCS
**Priority:** major
**Difficulty:** medium
**Blocks:** #004 (project_skeleton — defines what we build in v0.1), #002 (github_actions_ci — defines what we run in CI)
**Blocked by:** —
**Related:** —

## Goal

Reposition [docs/architecture-spec.md](../../docs/architecture-spec.md) so that the product headline becomes **"module boundaries + cycles in CI for C++"**, while SF.* hygiene, Lakos metrics, and Martin metrics move into supporting roles. In parallel — sync the roadmap with its own risks (baseline) and make the fast backend the foundation of v0.1.

## Context

We received detailed external criticism of the spec. The main points (still relevant today):

1. **The spec mixes three different products**: an archunit-style guardrail, source hygiene, and architectural analytics. These are three jobs-to-be-done with three different buyers (team lead, developer, architect) and different expectations from the first run.
2. **The real value prop** = module boundaries + cycles + baseline + good CI output. The headline. Everything else is supporting.
3. **The fast backend (preprocessor-only)** should be the foundation of v0.1, not a deferred spike. This is a **product** decision (barrier to entry), not a technical one — which the spec itself acknowledges on line 308, but in the roadmap (lines 402–406) v0.1 still starts via libclang.
4. **Baseline should be day-one.** In the risks section (line 457) this is stated directly, but in the roadmap (line 418) baseline is in v0.2. This contradiction is a bug in the document.
5. **SF.4/SF.5/SF.10 are weak rules** (product-wise). SF.4 has almost no value. SF.5 breaks on generated code. SF.10 is semantically expensive, with low confidence.
6. **Martin metrics are premature in v0.3.** They are not what drives early adoption. Move them to v0.4+ or make them "on user request."
7. **`--suggest-config` — a cut candidate.** Auto-inferring module structure is either trivial (by directories) or magic that nobody will trust.
8. **"These rules are not up for discussion"** on line 146 reads as hostile. Rephrase — keep the meaning (attribution as a defense against pushback), soften the rhetoric.
9. **The constraint decay LLM paper** is the strongest positioning lever (AI guardrail), and the spec underuses it. Raise it into the first paragraph.

For what I agree with and what I dispute — see the final position in the conversation log of the 2026-05-26 session.

## Execution plan

- [x] **1. Headline refactor of README.md and the spec's `## TL;DR`.** New cycle: `module boundaries + cycles in CI` → `baseline for legacy` → `Lakos and CCG as cited sources, not a brand`. The constraint-decay paper goes into the first paragraph.
- [x] **2. Roadmap edit `## Roadmap` (lines 398–449).** Specifically:
   - v0.1: fast backend (preprocessor-only), `forbidden_deps`/`allowed_deps`, cycles (SF.9), god-headers, include-chain length, **baseline day-one**, simple text report, JSON report, exit codes.
   - v0.2: libclang backend, remaining SF (SF.2, SF.5, SF.10, SF.11, SF.21), precise version of SF.7.
   - v0.3: rules from the C/I/NL sections of CCG, rules from the BDE wiki (see [docs/research/rules/](../../docs/research/rules/)).
   - v0.4: Martin metrics (only if users ask), templates for clean/hexagonal/onion.
   - v0.5: distribution polish, docs, regression checks.
   - **Cut**: `--suggest-config`.
- [x] **3. Section `## Default analysis` (lines 138–215).** Level 1 (SF) — keep, but rephrase "not up for discussion" → "attribution: the source is official, you can disable it, but the default is justified." SF.4 — remove from defaults (low value). SF.5/SF.10 — mark as requiring libclang and v0.2+. Level 3 (Martin) — move to options behind a flag.
- [x] **4. Section `## Config-driven analysis` (lines 217–292).** The headline config example — `forbidden_deps` + `no_cycles`, without SF flags. SF.* appear as `defaults: core_guidelines: true`, below the line about baseline.
- [x] **5. Section `### Open architectural question: the two-backend scheme` (lines 306–317).** Close the question: the fast backend is the default, libclang is opt-in for semantic rules. Rewrite as an accepted decision, not as "deferred."
- [x] **6. `## Key risks` (lines 453–467).** Remove the risk about baseline (it's now solved in v0.1). Remove the risk about fast/slow backend (solved). Keep: name, libclang performance (for v0.2+), compile_commands availability (but less acute with fast backend default), usability of the default rules, C++ templates.
- [x] **7. Recheck `## What it does (and doesn't do)` (lines 110–134).** Keep the "doesn't do" list. Rephrase "does" to match the new headline.
- [x] **8. Change the document version to v2.1, add a short summary of changes from v2.0 to the header.**

## Done

- **2026-05-26** — Step 5: the "Open architectural question" section rewritten as an accepted decision. Fast backend (preprocessor-only) — default in v0.1, libclang — opt-in via `--with-clang` / fully in v0.2. The v0.1 rule list narrowed to the include-only set, semantic SF.2/5/10/11 explicitly deferred to v0.2. The adjacent "Parsing" bullet in the tech stack section brought into line. Commit `fcfef01`.
- **2026-05-26** — Steps 1 + 7: rewrote `## TL;DR` and `### Does` in the spec, synced the README. New headline: "module boundaries and dependency cycles in CI", baseline day-one, AI-guardrail (constraint decay paper) raised to the third paragraph. Lakos / Core Guidelines / Martin demoted to "in addition" — attribution, not a brand. README: added a line about AI constraint decay in Why, Key Features renumbered (baseline and no-setup moved up), the "What it does" list reflects the fast backend as default. Commit `21173fc`.
- **2026-05-26** — Step 2: roadmap fully rewritten. v0.1 is now the cover story "module boundaries + cycles in CI without compile_commands", with an explicit rule set (SF.9 / god-headers / chain-length / SF.7/8/21) and baseline day-one. v0.2 — libclang backend and semantic SF (SF.2/5/10/11). v0.3 — rules from C/I/NL CCG + BDE (link to docs/research/rules/). v0.4 — Martin metrics optional + distribution. v0.5 — templates + regression check. A separate "What we don't do" section with `--suggest-config` cut forever. Commit `89737d7`.
- **2026-05-26** — Steps 3 + 4: reworked the "Default analysis" section — the phrase "not up for discussion" replaced with "attribution, but everything can be disabled"; added a "Phase" column to the SF-rules table (v0.1 / v0.2); SF.4 explicitly removed from defaults with justification; Martin metrics marked optional for v0.4. The "Config-driven analysis" section reoriented — comments in the YAML example emphasize that `modules` + `forbidden_deps` is the core, defaults is a secondary section; added a "Minimally useful config" as an entry point. Commands: removed `--suggest-config`, added `--with-clang`, an explicit `archcheck` with no arguments as the first run, a v0.2 note on SARIF.

## In progress

- (empty)

## Next steps

1. Read `docs/architecture-spec.md` in full once more with a marker for "what falls under the refactor."
2. Read `docs/research/rules/` — there are candidates for the rules of the next waves, which need to be distributed across v0.2/v0.3/v0.4.
3. Make diff-style edits (don't rewrite wholesale — pointwise by section).
4. Afterwards — check README.md and [docs/research/README.md](../../docs/research/README.md) for consistency with the new spec.

## Key decisions

| Decision | Reason |
|----------|--------|
| Don't rewrite the spec wholesale, edit diff-style | v2.0 is a large document; rewriting it entirely is expensive and risks losing important nuances; pointwise edits by section are safer |
| Fast backend = default in v0.1 | Product decision (barrier to entry), not technical |
| Baseline = day-one | Otherwise the "easy adoption in legacy" promise is empty |
| Martin metrics → v0.4 (optional) | They don't drive early adoption, they add semantic debt in v0.3 |
| `--suggest-config` → cut | Expensive heuristic with dubious value |
| Constraint decay paper → headline | The freshest and strongest niche position; in the spec it's buried deep |

## Changed files

| File | Change | Commit |
|------|--------|--------|
| `docs/architecture-spec.md` | step 5: "two-backend scheme" → accepted decision; "Parsing" bullet updated | `fcfef01` |
| `docs/architecture-spec.md` | step 5b: removed reference to the process doc from the stability contract | `f19c130` |
| `docs/architecture-spec.md` | steps 1+7: TL;DR and "What it does" rewritten for the new headline | `21173fc` |
| `README.md` | steps 1+7: synced — Why with constraint decay, Key Features renumbered | `21173fc` |
| `docs/architecture-spec.md` | step 2: roadmap fully reworked for fast-backend-first | `89737d7` |
| `docs/architecture-spec.md` | steps 3+4: default analysis + config-driven analysis reoriented | `1f6d587` |
| `docs/architecture-spec.md` | steps 6+8: risks cleaned up, version → v2.1 with summary | (current) |

## How it works

After #006 the v2.1 spec is structured as follows:

1. **Header (`# archcheck …`)** — a short summary of changes from v2.0 (a list with links to task numbers).
2. **`## TL;DR`** — four paragraphs: (1) the core = module boundaries + cycles + baseline in CI; (2) the gap in the ecosystem; (3) AI-guardrail / constraint decay; (4) "in addition" — Lakos / Core Guidelines / Martin as cited sources, not a brand.
3. **`## Problem` / `## Demand evidence` / `## Why C++ specifically` / `## Target audience`** — unchanged, marketing wrapper.
4. **`## What it does (and doesn't do)`** — the "does" list reflects the fast backend default, baseline day-one, links to §"Two-backend scheme" and §"Default analysis".
5. **`## Default analysis`** — three levels (SF / Lakos / Martin) + Level 4 (undisputed practices). Each SF rule has a "Phase" column (v0.1 / v0.2). Martin — optional, v0.4. Rule management via CLI / config / inline comment.
6. **`## Config-driven analysis`** — a YAML example with comments per block (modules → rules → defaults → thresholds) + a "minimally useful config" for legacy. Commands without `--suggest-config`.
7. **`## Stability contract`** — a breaking vs non-breaking table by interface (#007).
8. **`## Tool architecture`** — the two-backend scheme as an accepted decision (fast = default, libclang = opt-in / v0.2).
9. **`## Roadmap`** — v0.1 cover story (module boundaries + cycles in CI without compile_commands) → v0.2 (libclang + semantic SF) → v0.3 (C/I/NL + BDE) → v0.4 (Martin + distribution) → v0.5 (templates + community). A separate "What we don't do" section with `--suggest-config`.
10. **`## Key risks`** — 5 items, updated for the new state; the licensing risk removed (solved).

## What controls it

- **Document version** — in the header and the footer. On the next rework: bump to 2.2, add a summary section at the start.
- **Link to CHANGELOG.md** — the spec describes the design, CHANGELOG records changes to release artifacts. Don't duplicate.
- **Link to roadmap facts** — the "Phase" table in the SF rules must match what's written in `## Roadmap`. On the next version bump: cross-check.

## What it relates to

- **#003** — the product name is fixed, the spec is cleaned of cpparch / cpparch-alternatives.
- **#007** — the "Stability contract" section appeared as part of #007, mentioned in the v2.1 spec header.
- **#001, #002, #004, #005** — now unblocked (or become more concrete):
  - **#004 (project_skeleton)** — the CMake skeleton must match the v0.1 roadmap: fast backend single binary, no mandatory libclang dependency in v0.1.
  - **#002 (github_actions_ci)** — CI runs archcheck on itself by the v0.1 rules; new steps will be added as v0.2/v0.3 arrive.
  - **#005 (sarif_reporter_spec)** — SARIF is now explicitly v0.2+, not a blocker for v0.1.
  - **#001 (dogfood_static_analyzers)** — can be started immediately, doesn't depend on the spec refactor.
- **`docs/research/rules/`** — this is a long list of rule candidates; the roadmap (v0.2 / v0.3 / v0.4) now explicitly references this directory but doesn't duplicate its contents.

## Diagnostics

How to tell the spec is in the right state:

```bash
# Version 2.1:
head -2 docs/architecture-spec.md

# TL;DR starts with module boundaries, not with Lakos / CCG:
sed -n '/^## TL;DR/,/^---/p' docs/architecture-spec.md | head -5

# SF.4 not in defaults (only in a comment):
grep -n "SF.4" docs/architecture-spec.md

# Two-backend scheme — an accepted decision:
grep -n "^### " docs/architecture-spec.md | grep -i "backend"

# --suggest-config cut:
grep -n "suggest-config" docs/architecture-spec.md
# should return only the "What we don't do" section

# Licensing risk removed:
grep -ni "License" docs/architecture-spec.md
# should return zero or only LICENSE-related references

# Stability contract in place:
grep -n "^## Stability contract" docs/architecture-spec.md
```
