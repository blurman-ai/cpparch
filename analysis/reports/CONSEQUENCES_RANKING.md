# Consequences-first ranking — demo specimens for archcheck

Reframe (verified on the corpus, no new repos): the causal angle **"AI → drift" is dead even
at the most honest, commit-level slice**, so we stop trying to prove it and instead rank by the
*outcome* (structural debt), then attach development-culture context.

## 1. Commit-level attribution — why the causal thesis is dropped

`commit_attribution.py` joins per-commit `*_graph_drift.jsonl` (`sha`) against the actual commit
body (`git show -s --format=%B`), within-repo:

| event | AI | human | observed AI | expected AI (null) | ratio |
|---|---|---|---|---|---|
| grown-cycle births | 17 | 55 | 17 | 25.9 | **0.66×** |
| cross-area dep events | 194 | 661 | 194 | 427.9 | **0.45×** |

`expected = Σ_repo (repo_ai_rate × repo_drift_events)`. AI commits are **under-represented** in
drift, not over. Partly a size confound — among edge-adding commits, median size is equal (3)
but the **human mean is 2.3× higher** (21.5 vs 9.4 added_edges): the rare giant structural
commits (imports, mass-refactors) where cycles are born are disproportionately human.

**Honest claim we can stand behind:** *"No evidence AI commits disproportionately introduce
architectural drift."* The demo asset is not causation but specimens — AI-developed unguarded
C++ projects carrying real structural debt the tool surfaces.

## 2. Ranking — `ranking_consequences.py` → `ranking_consequences.tsv`

Per repo, all local/free: debt `= 5×grown_cycles + cross_area` (cycles weighted as the rarest
real signal; churn `graph_errors` deliberately excluded), plus context — AI%, effective human
authors + top-author dominance (`git log --pretty=%an`, bots tagged), and guard files on disk
(CODEOWNERS, CI workflows, agent-config: CLAUDE.md/.cursorrules/AGENTS.md/copilot-instructions).

**Demo cell** = `debt>0 ∧ AI≥50% ∧ no CODEOWNERS ∧ ≤4 human authors` → **88 repos**.

Caveat learned: CODEOWNERS-as-file ≠ enforced review — `makr-code_ThemisDB` (corpus #1 drift)
*has* CODEOWNERS yet drifts hardest. The guard-proxy is presence-only; real review-gating needs
the PR-review API (expensive, shortlist-only).

## 3. HEAD static-debt enrichment — `enrich_head.py` (top-25 of the cell)

`archcheck --save-graph-baseline` + reused `graph_probe` parse, only on flagged repos.

| repo | AI% | win-cyc | fan-in (god-header) | chain | HEAD mutual | profile |
|---|---|---|---|---|---|---|
| Zero3K20_hpsx64 | 88.9 | 3 | 52 `types.h` | 11 | 5 | PSX emulator, solo |
| HendrikGC02_Astroray | 74.1 | 3 | 66 `raytracer.h` | 9 | 2 | raytracer, agent-cfg |
| santzit_go-guitar2 | 72.5 | 3 | 32 `base.hpp` | 10 | 2 | Godot game, solo, agent-cfg |
| shifty81_FreshVoxel | 96.6 | 0 | 80 `Logger.h` | 5 | 0 | textbook god-header, solo |
| deltahdl_deltahdl | 77.3 | 0 | 65 `ast.h` | 9 | 0 | HDL, solo |
| b-macker_NAAb | 90.8 | 0 | 74 `interpreter.h` | 7 | 0 | interpreter, solo |
| shifty81_MasterRepo | 79.8 | 2 | 37 `Log.h` | 9 | 0 | solo |

God-header pattern is the recurring textbook finding: `Logger.h` (fan-in 80), `Log.h`,
`types.h`, `config.h` — "everyone includes the logger".

## 4. Scripts (experiments/ai_repo_run/)

- `commit_attribution.py` — within-repo per-event AI attribution + size-confound check.
- `ranking_consequences.py` — full ranking → `ranking_consequences.tsv`, demo-cell print.
- `enrich_head.py` — HEAD god-header/chain/mutual scan over the shortlist.

## 5. Next

- Bug A/B (#081) are on the critical path: clean-SPDX solo projects (the best targets) scan 0
  files until fixed — the dup half of any demo needs them.
- Phase-2 enrichment for *new* repos: agent-config code search (`path:CLAUDE.md …
  language:C++ fork:false`) → metadata → tree (vendor/amalgam exclude) → clone (authors,
  velocity changepoint before/after first AI commit).
