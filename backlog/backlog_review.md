# Backlog review — 2026-06-26 (snapshot after the bool wave + doc-sync)

**Active:** 21 (11 `new/` + 10 `wip/`). No empty templates.
Stale by date (>30 days without edits) — 0 (the oldest: #074 from 2026-06-02).
`pending/` not touched.

> This run already applied board hygiene: #103 and #122 → `completed/` (goals achieved);
> #093 → `wip/` (ARG.1 shipped c0f37db, in release 0.1.0; remainder ARG.2); #093/#094/#095
> got `Related: #090`. The tables below reflect the state AFTER these moves.

## Quick wins

| File | Goal | Module |
|------|------|--------|
| `new/144_min_diff_unresolved_baseline_exit2.md` | unresolvable `--diff` ref → exit 2 instead of a silent empty tree (false gate); fixture + integration test | CLI |
| `new/143_maj_diff_shallow_baseline_fetch_ci.md` | remove `fetch-depth: 0` from the recommended `--diff` CI snippet; changes only the workflow + 2 docs | CI/DOCS |
| `wip/093_maj_flag_argument.md` (remainder) | ARG.2 (call sites: ≥2 `true`/`false` in one call) on top of the already-shipped ARG.1 | SCAN |

## Medium

| File | Goal | Module | Difficulty |
|------|------|--------|-----------|
| `new/057_maj_lakos_fanout_coupling_checks.md` | `Lakos.GodComponentFanOut` + report-only metrics (avg coupling, blast radius, SCC size) | GRAPH/RULES/REPORT | medium |
| `new/094_maj_param_count_and_accretion.md` | `ARG.3/ARG.4` long parameter list + accretion; reuse the scanner from #093 | SCAN | medium |
| `new/095_maj_config_bag_growth.md` | `CFG.1/CFG.2` config-bag growth (>15 fields / increment); new `type_body_scan` | SCAN | medium |
| `new/125_maj_scan_extensionless_headers.md` | scanner doesn't see extensionless headers (`<vector>`, `gsl/span`) → false "0 violations" | SCAN | medium |
| `new/126_maj_sf9_collapse_impl_into_component.md` | SF.9: collapse header+impl into one Lakos component before cycle detection (mlpack 259→16 FP) | RULES | medium |
| `wip/127_maj_vendor_generated_exclusion.md` | exclude vendored/generated; scope A+B done, remainder — generic nested-LICENSE, `.gitmodules`, fixtures | SCAN | medium |
| `wip/133_maj_first_run_noise_floor.md` (remainder) | core (advisory in check-mode, gate only SF.9) shipped 67a5a2c; remainder — narrow `--diff` mass-rename guard | RULES/CLI | medium |

## Hard / research

| File | Goal | Module |
|------|------|--------|
| `wip/054_maj_ai_repo_duplication_run.md` | corpus run of graph+dup across AI repos | RESEARCH |
| `wip/066_maj_airepo_remeasure_clonefail.md` | re-measure CLONEFAIL + corpus expansion ≥300 | RESEARCH/SCAN |
| `wip/070_maj_checker_fp_fix_proposals.md` | 6 P0 FP-guards done; remainder — measurement harness + precision on the corpus | SCAN |
| `wip/072_maj_port_056_duplication_into_archcheck.md` | port of #056 done (CLI `--duplication`, 17 tests); remainder — JSON output of pairs (debatable for v0.1) | SCAN |
| `wip/123_maj_diff_new_clone_gate.md` | core `DRIFT.NEW_CLONE` shipped + E2E; remainder — GitHub demo repo + severity decision | SCAN |
| `wip/124_maj_corpus_validate_new_clone_gate.md` | production precision/recall of the new-clone-gate; remainder — Part B summary | SCAN |
| `wip/129_maj_unify_source_scan_one_pass.md` | unified `AuthoredScope` (one-pass); core committed, remainder — step 7 (golden, tracked in #131) | SCAN |
| `wip/131_maj_fresh_corpus_remeasure.md` | master corpus-verifier of the whole scanner wave; Group 1 done, Groups 2–6 pending | RESEARCH |
| `new/074_maj_ai_repo_discovery_roi_alignment.md` | consolidate discovery tech debt (ROI filters, giant-skip, resumable) | RESEARCH |
| `new/077_maj_per_commit_graph_drift_export.md` | per-commit export of the include graph for manual verification of drift cases | RESEARCH |
| `new/078_maj_clone_cochange_harm_signal.md` | clone severity by inconsistent co-change in git history (layer on top of #054) | SCAN/RESEARCH |

## Duplicates / merges (don't merge — bundles)

| Files | Verdict |
|-------|---------|
| #093 / #094 / #095 vs bool wave #090/#135 | **Not duplicates, not subsumed** — bool arguments / parameter count / config fields ⟂ bool class fields. The only commonality is methodology. `Related: #090` set. Order: #093(remainder)→#094→#095. |
| #123 / #124 | bundle: #123 is the production entry (shipped), #124 is its corpus validation of precision/recall. |
| #127 / #129 | bundle: #129 is the unified one-pass, #127 is the accuracy of the vendored/generated predicate in that pass. Close together via #131. |
| #054 / #066 / #124 / #131 | converge on #131 as the umbrella verifier (`Verification: #131` in each). |
| #054 / #066 / #072 / #070 / #078 | duplication family: port → FP-guards → co-change severity → corpus infra. A chain, keep it. |

## Blocked (de facto unblocked)

| File | Blocked by | Reality |
|------|------------------|-----------|
| `wip/129` | #131 (step 7 golden) | a live block (Groups 2–6 of #131) |
| `wip/124` | #123 | core #123 shipped → not a real block |

## Summary

- **21 active**, all with real analysis; stale by date — 0.
- **Board hygiene applied:** #103, #122 → completed; #093 → wip (ARG.1 in release, remainder ARG.2).
- **The scanner wave's bottleneck** — corpus-verifier #131 (Groups 2–6), through which #054/#066/#124/#129 are closed.
- **Three quick wins first:** (1) #144 exit-2 on an unresolvable ref; (2) #143 shallow base-fetch; (3) #093 ARG.2 call-sites.
