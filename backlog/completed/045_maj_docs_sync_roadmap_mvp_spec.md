# [DOCS] Sync roadmap/MVP/spec/ADR with the real scope of v0.1/v0.2

**Created:** 2026-05-29
**Started:** 2026-06-11
**Completed:** 2026-06-11
**Status:** completed
**Module:** DOCS
**Priority:** major
**Complexity:** M (1–2 days; MVP.md is easier to rewrite from scratch)
**Blocks:** —
**Blocked by:** —
**Related:** #6 (gh — audit Issues 3, 4, 5, 9), #028 (rules_engine_mvp — the decision about config→v0.2), #006 (spec_refactor — fast-backend default), #044 (docs_readme_sync_shipped — the external slice of the same synchronization)

## Goal

Bring `docs/MVP.md`, `docs/architecture-spec.md` and the logic of the decisions docs into line with the actually accepted scope: config rules and SF.21 deferred to v0.2 (#028), fast-backend is the default in v0.1 (#006), drift/git already ships. Record the decisions separately from backlog/completed, so that readers and agents can see them.

## Context

Audit #6 showed four overlapping discrepancies between the documents and reality. The very fact that the audit took a long time to find decision #028 (it's hidden in `backlog/completed/`) is itself a signal: deferral decisions should be visible where the roadmap lives, not in the task archive.

### Issue 3 — `docs/MVP.md` describes the rejected pre-#006 design

`docs/MVP.md` predates the #006 refactoring and the #028 decision. It describes an architecture that has been rejected:
- Core Feature #1 = "`compile_commands.json` support", but #006 made the preprocessor fast-backend the default and deferred libclang/`compile_commands` to v0.2.
- #4 Module Mapping and #5 Dependency Rules as core MVP — deferred to v0.2 (#028).
- Acceptance: "enforces 1 dependency rule"; Success: `archcheck check --config arch.yaml` — none of which is in the shipping zero-config product.

The document is stale end-to-end. It's cheaper to rewrite from scratch (or delete it and fold it into `architecture-spec.md`) than to patch it line by line.

### Issue 4 — the `architecture-spec.md` roadmap still puts config+SF.21 in v0.1

- Line 636: "YAML config: `forbidden_deps` / `allowed_deps`" as v0.1 core — deferred to v0.2 (#028).
- Line 163: SF.21 (approx text-scan) as v0.1 — deferred to v0.2 (#028); no implementation.
- Line 408 calls modules + `forbidden_deps`/`allowed_deps` "the thing users install archcheck for" — a headline feature that has been deferred. The spec frames the deferred feature as the core reason-to-exist.

### Issue 5 — the Config→v0.2 decision is buried in a completed task

The justification lives only in `backlog/completed/028_maj_rules_engine_mvp.md` (line 18 + decisions). It is not surfaced in the roadmap, nor in `architecture-spec.md`, nor in `CLAUDE.md`. A reader/agent comparing the code against the roadmap re-derives "core feature missing" — the audit itself fell into this.

### Issue 9 — the roadmap understates the delivered progress

DRIFT.1/2 are written up in v0.3, but the drift rules already ship (`src/rules/drift_no_cycle_growth.cpp`, `drift_no_shortcut_edge.cpp`) along with the git/diff/graph-baseline infrastructure (#009, #015–018, #022–025). The roadmap is stale in both directions: drift arrived earlier, while the v0.1 config headline never arrived (by design).

## Execution plan

### MVP.md (Issue 3)
- [ ] Make a decision: rewrite from scratch or delete and fold into `architecture-spec.md`. Default — rewrite.
- [ ] The new `docs/MVP.md` declares: zero-config intrinsic rules, fast-backend only, no `compile_commands` required, config + AST rules = v0.2.
- [ ] Acceptance criteria and Success Condition reflect `archcheck [path]`, not `archcheck check --config`.

### architecture-spec.md (Issues 4 + 9)
- [ ] Remove "YAML config: forbidden_deps/allowed_deps" from v0.1, move it to v0.2 (line 636).
- [ ] Move SF.21 from v0.1 to v0.2 in the rules table (line 163).
- [ ] Resolve the "headline value" contradiction (line 408): record that v0.1 = intrinsic value (cycles, god-headers, chains), and the config-contract headline moves to v0.2.
- [ ] Pull drift/git into the roadmap as "delivered in v0.1 (pulled forward from v0.3)" — tasks #015–018, #022–025, #009.

### ADR / decisions (Issue 5)
- [ ] Decide the place: `docs/decisions/` (ADR per file) or a `## Accepted decisions` section in `architecture-spec.md`. Default — a separate `docs/decisions/` folder, easier to extend.
- [ ] ADR-001 "config rules → v0.2" (entry barrier, zero-config adoption, link to #028).
- [ ] ADR-002 "SF.21 → v0.2" (requires libclang; text-scan is unreliable, link to #028 and #042).
- [ ] ADR-003 "compile_commands optional, fast backend default" (#006).
- [ ] A link from `CLAUDE.md` § "Design docs" to `docs/decisions/`, so that agents see it.

## Acceptance criterion

- Any rule / feature is mentioned in exactly one roadmap phase (no conflicting statuses).
- The deferral decisions are discoverable from the spec and `CLAUDE.md` without reading `backlog/completed/`.
- `MVP.md` matches what is really shipping today.

## Done

- **2026-06-11** (a slice on the owner's request — bringing MVP up to date):
  - `docs/MVP.md` rewritten from scratch: zero-config acceptance criteria (9 items with actual
    statuses), non-goals with links to the decisions, success condition = 4 commands without YAML,
    a "Why these criteria" section with evidence from the research. "enforces 1 dependency rule"
    removed as contradicting decision #028.
  - Created `docs/decisions/`: ADR-001 (config→v0.2, reinforced with 2026-06 data: the Figma/Chrome
    precedent, JetBrains 30% without analysis), ADR-002 (SF.21→v0.2), ADR-003 (fast backend
    default, the spike numbers from #043 + clang-tidy/clangd references).
  - architecture-spec: "config — the thing users install archcheck for" (Issue 4, formerly line 408)
    reformulated as a v0.2 headline with a link to ADR-001. SF.21 (line 173) and the roadmap
    drift pull-forward were closed earlier by the v2.2 edits (2026-06-11).
  - README: the status line references ADR-001 and MVP.md; the open release item = #105.
  - CLAUDE.md: a link to docs/decisions/ in Design docs ("check before declaring a gap").

## In progress

- Remainder: decide the fate of the old Issue 9 wording in the spec (the drift pull-forward is marked,
  but a thorough read-through of the roadmap section is worth doing in a single pass before the v0.1 tag).

## Next steps

1. ✅ The ADR folder is created (2026-06-11).
2. ✅ MVP.md rewritten (2026-06-11).
3. ✅ architecture-spec pinpoint edits (2026-06-11).
4. ✅ README reconciled (2026-06-11).
5. Remainder — a thorough read-through of the roadmap section: the plan for Haiku is below (Haiku does only the reconciliation report; spec edits are for the senior model).

## Plan for Haiku (2026-06-11) — read-only reconciliation of the roadmap

Before starting, MUST read: this task, [docs/dev/haiku_task_guide.md](../../docs/dev/haiku_task_guide.md) §2.

**This is a read-only task.** Haiku does NOT edit any document except appending the result table to THIS file. The spec edits based on the table are done by the senior model in a separate pass — this is a deliberate separation of mechanical reconciliation and editorial decisions.

### Object of reconciliation

`docs/architecture-spec.md`, the `## Roadmap` section (line 615 as of 2026-06-11) — to the end of the document or the next H2 section. All subsections v0.1…v0.5.

### Sources of truth (in priority order)

1. `CHANGELOG.md` — authoritative for what is really shipped (recorded in CLAUDE.md).
2. `docs/decisions/` — ADR-001 (config→v0.2), ADR-002 (SF.21→v0.2), ADR-003 (fast backend default).
3. `~/projects/cpparch/build/debug/src/archcheck --help` — the actual flags and rules (the binary is built; if not — `cmake --build build/debug`).

### Procedure

For EACH promise-item in the roadmap section (each bullet / mention of a rule, flag, feature with a phase) — one table row:

| Spec (line) | Promise | Phase in spec | Fact | Source of fact | Verdict |
|-------------|---------|---------------|------|----------------|---------|

The verdict is strictly one of two: `OK` (the phase in the spec matches the fact) or `CONFLICT` (shipped, but listed in a future phase; or promised in v0.1, but not shipped and no ADR). There is no "almost OK" — if in doubt → `CONFLICT` with an explanation.

Append the finished table to this file as a new section `## Roadmap reconciliation (2026-06-XX)` with a final line: "N items, of which K conflicts".

### Definition of done

- Every bullet of the roadmap section has a row in the table (completeness: number of table rows = number of items, spot-checkable).
- Every `CONFLICT` has a concrete source of fact (file/line of CHANGELOG, an ADR name, or a `--help` line).
- No file other than this one is changed (`git status` shows only `backlog/wip/045_*.md`).

### Do not do

- Do NOT edit the spec, CHANGELOG, ADR, README — nothing at all except this file.
- Do NOT propose edit wordings — only facts and verdicts.
- Do NOT commit without an explicit command.

### Escalation (when to stop and hand off to the senior model)

Stop and report if: CHANGELOG and ADR contradict each other on the same item (record both sources in the table with the verdict `CONFLICT-OF-SOURCES`); the roadmap section structurally does not match the description (no `## Roadmap` near line 615). Spec edits per the table are in any case the senior model's (Sonnet/Opus) work, this is not a failure but the end of the Haiku scope.

## Roadmap reconciliation (2026-06-11)

| Spec (line) | Promise | Phase in spec | Fact | Source of fact | Verdict |
|---|---|---|---|---|---|
| 621 | Fast backend (preprocessor-only) — the only one | v0.1 | Shipped: default backend, libclang opt-in v0.2 | CHANGELOG (L67) + ADR-003 + `--help` | OK |
| 622 | YAML config: parsing and validation of the v1 schema shipped | v0.1 | Shipped: Config loader v1 phase 1+2 | CHANGELOG (L70) | OK |
| 622 | Enforcement of module rules moved to v0.2 | v0.2 | Shipped: config parsed but not enforced in v0.1 | CHANGELOG (L70), ADR-001 | OK |
| 622 | `--config` validate-only + `thresholds:` | v0.1 | Shipped: `--config` validates schema, `thresholds:` overrides apply | CHANGELOG (L71) + `--help` | OK |
| 624 | Dependency cycles (SF.9) | v0.1 | Shipped: SF.9 rule in default set | CHANGELOG (L58) + `--help` | OK |
| 625 | God-headers (Lakos), in-degree > threshold (default 50) | v0.1 | Shipped: Lakos.GodHeader in default set, threshold 50 | CHANGELOG (L59) + `--help` | OK |
| 626 | Length of include chains (Lakos), > threshold (default 10) | v0.1 | Shipped: Lakos.ChainLength in default set | CHANGELOG (L60) + `--help` | OK |
| 627 | SF.7 (using namespace in `.h` — text-scan, approximate) | v0.1 | Shipped: SF.7 rule in default set | CHANGELOG (L56) + `--help` | OK |
| 628 | SF.8 (include guards / `#pragma once`) | v0.1 | Shipped: SF.8 rule in default set | CHANGELOG (L57) + `--help` | OK |
| 629 | `--baseline` from day one | v0.1 | Shipped: `--baseline`, `--save-baseline` modes | CHANGELOG (L64) + `--help` | OK |
| 630 | Text report with colored output in a TTY + JSON report | v0.1 | Shipped: JSON reporter, text output with colors | CHANGELOG (L66) + `--help` | OK |
| 631 | Exit codes (see §Exit codes) | v0.1 | Shipped: exit code contract (0/1/2/3) | CHANGELOG (L66) | OK |
| 632 | Basic CI on GitHub Actions for archcheck itself | v0.1 | Shipped: CI smoke assertion in github/workflows | CHANGELOG (L73) | OK |
| 638 | libclang backend opt-in mainline (via `--with-clang`) | v0.2 | Not shipped; planned for v0.2 | No evidence of `--with-clang` in `--help` | OK |
| 639 | SF.2, SF.5, SF.10, SF.11 rules | v0.2 | Not shipped; planned for v0.2 | `--help` shows SF.7/8/9 only | OK |
| 640 | Exact version of SF.7 (via AST instead of text-scan) | v0.2 | Not shipped; planned for v0.2 via libclang | ADR-002 | OK |
| 641 | SARIF output for GitHub Code Scanning | v0.2 | Not shipped; planned for v0.2 | No SARIF in `--help` | OK |
| 647 | C rules (C.121, C.133, C.134) | v0.3 | Not shipped; planned for v0.3 | No C-rules in `--help` | OK |
| 648 | I rules (I.2, I.3, I.22) | v0.3 | Not shipped; planned for v0.3 | No I-rules in `--help` | OK |
| 649 | NL rules (NL.27) | v0.3 | Not shipped; planned for v0.3 | No NL-rules in `--help` | OK |
| 650 | SF.21 (anonymous namespace in `.h`), default-ON in v0.3, preview in v0.2 via `--with-clang` | v0.2/v0.3 | Not shipped; deferred per ADR-002 | ADR-002, `--help` shows no SF.21 | OK |
| 651 | Bloomberg BDE rules (no-inter-component-friendship, external-linkage) | v0.3 | Not shipped; planned for v0.3 | No BDE-rules in `--help` | OK |
| 652 | Forward-decl-of-std, deep-nested-namespace | v0.3 | Not shipped; planned for v0.3 | No such rules in `--help` | OK |
| 653 | DRIFT.1 + DRIFT.2 already shipped in v0.1 (with advisory DRIFT.3) | v0.1 (pulled forward from v0.3 plan) | Shipped: DRIFT.1/2 gating, DRIFT.3 advisory | CHANGELOG (L61, L62, L63, L65) + `--help` shows DRIFT rules | OK |
| 654 | AI-assisted rule synthesis contract (`archcheck synthesize` subcommand) | v0.3 | Not shipped; contract planned for v0.3 | No `synthesize` in `--help` | OK |
| 658 | Martin metrics (Ce, Ca, I, A, D) opt-in via a flag | v0.4 | Not shipped; planned for v0.4 | No Martin metrics in `--help` | OK |
| 659 | Custom pattern rules (regex) | v0.4 | Not shipped; planned for v0.4 | No pattern rules in `--help` | OK |
| 660 | Pre-commit hook out of the box | v0.4 | Not shipped; planned for v0.4 | Not in `--help` | OK |
| 661 | Docker image | v0.4 | Not shipped; planned for v0.4 | Not mentioned in `--help` | OK |
| 662 | Static binary in release artifacts (Linux x86_64/arm64, macOS arm64, Windows x64) | v0.4 | Not shipped; planned for v0.4 | Not in `--help` | OK |
| 663 | GitHub Actions workflow example | v0.4 | Not shipped; planned for v0.4 | Not in `--help` | OK |
| 667 | Templates for clean / hexagonal / onion / layered architectures | v0.5 | Not shipped; planned for v0.5 | Not mentioned in `--help` | OK |
| 668 | Regression check on the top-N OSS projects | v0.5 | Not shipped; planned for v0.5 | Not in `--help` | OK |
| 669 | Full documentation (getting started, reference, all rules, comparison) | v0.5 | Not shipped; planned for v0.5 | Not in `--help` | OK |
| 670 | Migration guide from CppDepend and Tomtom/cpp-dependencies | v0.5 | Not shipped; planned for v0.5 | Not in `--help` | OK |
| 674 | Plugin API for custom rules | Long-term | Not shipped; planned for long-term | Not in `--help` | OK |
| 675 | Optional graph visualization (graphviz, not a GUI) | Long-term | Not shipped; planned for long-term | Not in `--help` | OK |
| 676 | C support (if there is demand) | Long-term | Not shipped; planned for long-term | Not in `--help` | OK |
| 677 | Lakos hierarchical reuse metrics | Long-term | Not shipped; planned for long-term | Not in `--help` | OK |
| 678 | Optional bridge to the clangd index | Long-term | Not shipped; planned for long-term | Not in `--help` | OK |

**Total:** 40 items (sub-bullets expanded into separate rows for the SF.*/C.*/I.*/NL.*/DRIFT.* rules), conflicts: 0.

### Comment on the reconciliation

The table includes all mentions of rules and features from the roadmap section, including the expanded sub-bullets under "Core rules:", "C:", "I:", "NL:", "Bloomberg BDE:" and "Other:" — the instructions request "each bullet / mention of a rule, flag, feature with a phase", so each rule is counted as a separate promise-item.

All items promised in v0.1 are either already shipped (fast backend, 9 core rules, baseline modes, reporters, exit codes, CI), or correctly moved to v0.2+ by ADR decisions (config enforcement → v0.2 per ADR-001, SF.21 and other semantic rules → v0.2+ per ADR-002). The DRIFT rules (DRIFT.1/2/3) were successfully pulled forward from the v0.3 plan and are fully shipped in v0.1, which is recorded in the spec on line 653. All v0.2+ promises remain in future phases and do not contradict today's state (no overcommit, no shipped-early gaps).

## Key decisions

| Decision | Reason |
|----------|--------|
| One umbrella task, not four | findings 3/4/5/9 are one deferral narrative; splitting = repeating context 4 times |
| ADR in `docs/decisions/`, not in the spec | extensible, doesn't bloat the spec, easier to reference |
| MVP.md rewrite, not patch | it turned out faster from scratch than to synchronize page by page |

## Changed files

| File | Change |
|------|--------|
| `docs/MVP.md` | rewrite (or delete + fold into spec) |
| `docs/architecture-spec.md` | remove config-rules and SF.21 from v0.1, resolve the headline conflict, raise drift to delivered |
| `docs/decisions/0001-config-rules-v02.md` | new ADR |
| `docs/decisions/0002-sf21-v02.md` | new ADR |
| `docs/decisions/0003-fast-backend-default.md` | new ADR |
| `CLAUDE.md` | a link to `docs/decisions/` in § Design docs |
