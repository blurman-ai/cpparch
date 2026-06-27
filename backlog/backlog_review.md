# Backlog review — 2026-06-27

**Active:** 27 (16 `new/` + 11 `wip/`). No empty templates.
Stale by date (>30 days without edits) — 0 (oldest: #054 ~27 days, #066 ~25 days).
`pending/` not touched.

---

## Misplaced — wip tasks sitting in `new/`

These 5 files have `Status: wip` but live in `backlog/new/`. Move to `backlog/wip/` on next `/checkpoint` or `/fix-issue`.

| File | State | Note |
|------|-------|------|
| `new/133_maj_first_run_noise_floor.md` | wip | Advisory gate done+verified; only `--diff` mass-rename guard remains |
| `new/145_maj_trending_cpp_cohort_drift.md` | wip | Clone list assembled, 20 cloned; per-commit scan pending |
| `new/146_maj_bot_review_lets_drift_through.md` | wip | Inverted corpus result done; ai-377 stratum fold-in pending |
| `new/147_maj_duplication_oom_large_embedded_file.md` | wip | Core fix done (lex-cap + generated-data exclude); residual = streaming read refactor |
| `new/150_maj_translate_repo_to_english.md` | wip | All P0–P3 checkboxes ticked — **candidate for completed/** |

**#150 specifically:** every checkbox is `[x]`, exclusions documented. Likely done — verify with `grep -rlP '\p{Cyrillic}'` and move to `completed/`.

---

## Stale

None. Everything touched within 30 days (today's i18n batch reset all timestamps).

---

## Quick wins

| File | Goal | Module | Complexity |
|------|------|--------|-----------|
| `new/143_maj_diff_shallow_baseline_fetch_ci.md` | Remove `fetch-depth: 0` from `--diff` CI snippet; add depth=1 base refetch | CI / DOCS | S — YAML + docs only, no C++ |
| `new/144_min_diff_unresolved_baseline_exit2.md` | Unresolvable `--diff` ref → exit 2 (not silent empty tree + phantom gate) | CLI / git | S — one guard in `diff_command.cpp`, integration test |
| `wip/129_maj_unify_source_scan_one_pass.md` | Core (steps 1–6) committed; only step 7 remains | SCAN / INFRA | S (step 7 only) |
| `wip/093_maj_flag_argument.md` | ARG.2 call-site detection (≥2 `true`/`false` literals in one call); ARG.1 shipped in v0.1.0 | SCAN / RULES | S–M |

**#143 + #144 are the most obvious pair:** both S complexity, no C++/graph risk, unblock CI docs quality before public release.

---

## Medium tasks

| File | Goal | Module | Difficulty |
|------|------|--------|-----------|
| `new/057_maj_lakos_fanout_coupling_checks.md` | Fan-out god-component rule + avg coupling + max blast radius in report | GRAPH / RULES | M (A+B+C sequential, calibration needed) |
| `new/125_maj_scan_extensionless_headers.md` | Scan extension-less headers (GSL, stdlib-style) | SCAN | M (performance measurement required first) |
| `new/126_maj_sf9_collapse_impl_into_component.md` | Collapse `foo.hpp` + `foo_impl.hpp` + `impl/foo.hpp` into one Lakos component before cycle detection | RULES / GRAPH | M (design decision: component model vs band-aid) |
| `new/133_maj_first_run_noise_floor.md` | `--diff` mass-rename guard (narrow remainder; advisory gate shipped) | CLI / RULES | S–M |
| `new/151_maj_p02_wholefile_suppress_kills_extracted_module_tp.md` | Fix P0.2 whole-file suppress: two-sided coverage check so extract-into-module TPs are not silenced | SCAN / DUPLICATION | M |
| `new/094_maj_param_count_and_accretion.md` | Parameter count + accretion drift signal | SCAN / DIFF | M (needs signature scanner from #093) |
| `new/095_maj_config_bag_growth.md` | Config-bag field count + accretion | SCAN / DIFF | M (parallel to #094, separate scanner) |

**#126 blocks demo quality on template libs (mlpack, PCL). Priority = go-public.**
**#151 is a concrete bug with repro fixture — no design ambiguity, just implementation.**

---

## Hard / blocked

| File | Blocker / Reason |
|------|----------------|
| `wip/054_maj_ai_repo_duplication_run.md` | Long-running corpus work; depends on stable scanner (#072, #070) |
| `wip/066_maj_airepo_remeasure_clonefail.md` | Discovery pipeline fix (#074 is the cleanup task); linked to #054 |
| `wip/070_maj_checker_fp_fix_proposals.md` | Big FP-triage umbrella; feeds #072 and #123 |
| `wip/072_maj_port_056_duplication_into_archcheck.md` | Depends on #070 FP guards being stable |
| `wip/123_maj_diff_new_clone_gate.md` | Depends on #072 port + #070 precision |
| `wip/124_maj_corpus_validate_new_clone_gate.md` | Depends on #123 gate being implemented |
| `wip/127_maj_vendor_generated_exclusion.md` | Part of #131 validation umbrella; in progress |
| `wip/131_maj_fresh_corpus_remeasure.md` | Master checklist; Groups 2–6 pending; depends on #126/#127/#129 |
| `wip/149_maj_percommit_dup_scan_whole_tree_timeout.md` | Byte-cap shipped; diff-scope rewrite reverted (not behaviour-preserving) — design open |
| `new/145_maj_trending_cpp_cohort_drift.md` | Ongoing corpus scan |
| `new/146_maj_bot_review_lets_drift_through.md` | Depends on ai-377 stratum scan |
| `new/078_maj_clone_cochange_harm_signal.md` | L difficulty; needs group clustering (#056) + history walk infra |

---

## Without analysis / needs research

| File | What's missing |
|------|----------------|
| `new/074_maj_ai_repo_discovery_roi_alignment.md` | Pipeline fix clearly scoped; no analysis gap — ready to execute |
| `new/077_maj_per_commit_graph_drift_export.md` | Script design is described; no blocker — just not started |

Both are research-infra tasks, not product tasks. Low urgency given the corpus work is ongoing.

---

## Duplicates / related

| Files | Relationship | Proposal |
|-------|-------------|---------|
| #054 + #066 | Sequential phases of AI-repo corpus campaign (initial run → remeasure CLONEFAILs) | Not duplicates; keep, add `Related:` cross-link if missing |
| #074 + #066 | #074 = fix the discovery pipeline; #066 = rerun using the fixed pipeline | Keep order: #074 first |
| #094 + #095 | Parallel cheap-drift signals (param count vs config-bag); share a token-scan substrate | Note `Blocked by: #093 shared scanner` in #095 if #093 ARG.2 not done first |
| #126 + #133 | #126 (impl-component collapse) reduces #133's SF.9 noise floor | Mark #133 `Blocked by: #126` for the SF.9 portion |
| #127 + #131 | #127 = vendor exclusion implementation; #131 = corpus validation umbrella that gates on #127 | Already linked; #127 must close before #131 Group 2+ |
| #072 + #123 + #070 | Port (#072) → gate (#123) → corpus-validate (#124); #070 feeds FP precision to all | Chain is correct; do not merge |
| #147 + #127 | Both touch generated-file exclusion; #147 added `hasMinifiedContent`, #127 covers vendor paths | Note in #127 that the content-heuristic from #147 is already implemented |

---

## Summary

- **Total active:** 27 (16 `new/`, 11 `wip/`). None empty.
- **Stale:** 0.
- **Misplaced (wip in new/):** 5 — move on next checkpoint. **#150 likely done → verify + move to completed/.**
- **Quick wins:** 4 — **#143 + #144** are the easiest (S, docs+YAML, no C++ risk); #129 step 7; #093 ARG.2.
- **Go-public blockers:** #126 (SF.9 noise on template libs), #143/#144 (CI docs), #150 (i18n done?).
- **Blocked / corpus work:** 12 tasks in the duplication/corpus pipeline chain (#054→#066→#070→#072→#123→#124→#131).
- **Without analysis:** 0 (all tasks have real content).
