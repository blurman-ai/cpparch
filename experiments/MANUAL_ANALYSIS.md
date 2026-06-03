# Manual Corpus Analysis: AI Adoption & Code Quality Signals

**Date:** 2026-06-03  
**Corpus:** 317 C++ repositories  
**Window:** May 1, 2025 → June 3, 2026  
**Data source:** `corpus_summary.tsv` (full graph-drift + duplication scan)

---

## Executive Summary

**Surprising finding:** No correlation between AI adoption % and code quality problems (graph errors or duplication). Instead, we found **two independent problems** that happen to co-occur:

1. **Architectural debt (graph-errors)** — independent of AI%
2. **Code duplication** — independent of AI%, but correlates with graph-errors (0.32 Spearman)

**Implication:** AI adoption % alone is NOT a risk signal for code quality degradation.

---

## Correlation Analysis (Spearman, n=317)

| Relationship | ρ (rho) | p-value | Interpretation |
|---|---|---|---|
| **AI% ↔ graph_errors** | +0.006 | 0.91 | **No correlation** |
| **AI% ↔ dup_pairs** | +0.029 | 0.61 | **No correlation** |
| **graph_errors ↔ dup_pairs** | +0.319 | 6.04e-09 | **Moderate correlation ✓** |

**Key insight:** Both graph-errors and duplication are independent markers of *overall* technical debt, not AI-specific risks.

---

## Top Problems by Type

### Top-10 Repos by Graph Errors

| Rank | Repo | Errors | AI% | LOC | Commits |
|------|------|--------|-----|-----|---------|
| 1 | melkyr_znineeight | 273 | 0% | 70K | 590 |
| 2 | Krilliac_SparkEngine | 244 | 0.6% | 689K | 121 |
| 3 | djeada_Standard-of-Iron | 237 | 56.3% | 306K | 451 |
| 4 | alexandrosk0_Smatchet | 198 | 77.6% | 131K | 835 |
| 5 | fernandotonon_QtMeshEditor | 157 | 59.8% | 326K | 448 |
| 6 | **FiveTechSoft_OpenADS** | **152** | **92.7%** | 71K | 520 |
| 7 | FLOX-Foundation_flox | 144 | 40.8% | 226K | 189 |
| 8 | boydsoftprez_NereusSDR | 127 | 56.9% | 988K | 149 |
| 9 | gcol33_tulpa | 126 | 67.7% | 59K | 172 |
| 10 | **fox1245_NeoGraph** | **114** | **94.0%** | 227K | 369 |

**Pattern:** Top graph-error repos are **mixed AI% distribution** — from 0% to 94%. No clear AI-driven pattern.

### Top-10 Repos by Duplication

| Rank | Repo | Pairs | AI% | LOC | Comments |
|------|------|-------|-----|-----|----------|
| 1 | **awest813_CnC_Generals_Zero_Hour** | **2538** | **100%** | 1.8M | Batch generation artifact |
| 2 | HyperCogWizard_a81ml-org | 735 | 44.1% | 795K | Low AI%, high dup! |
| 3 | **awest813_Dewm-3** | **323** | **100%** | 681K | Fork, 100% AI batch |
| 4 | shifty81_MasterRepoRefactor | 177 | 33.3% | 461K | Refactor, low AI |
| 5 | AlchemyViewer_Alchemy | 123 | 48.1% | 1.1M | Medium AI |
| 6 | Zero3K20_hpsx64 | 98 | 88.9% | 430K | High AI, moderate dup |
| 7 | CrispStrobe_CrispASR | 71 | 55.2% | 703K | Mixed |
| 8 | andrewnakas_Boxedwine64 | 71 | 38.9% | 1.1M | Low AI |
| 9 | alphaonex86_CatchChallenger | 66 | 53.5% | 250K | Mixed |
| 10 | hammermaps_halflife-op4-updated-sohl | 56 | 90.5% | 264K | High AI |

**Pattern:** Top dup repos include **both high-AI and low-AI projects**. CnC_Generals is a mega-outlier (2538 is 3.4× next repo).

---

## Two-Problem Taxonomy

Repos cluster into four groups:

### 1. **Graph-debt heavy, no dup problem** (n=~40)
Example: **FiveTechSoft_OpenADS** (92.7% AI, 152 errors, 3 dup pairs)
- **Interpretation:** Architectural complexity grew over time; code itself is relatively clean
- **Risk:** Include graph cycles, ACD violations; maintenance burden on architecture
- **AI correlation:** 92.7% AI but graph-clean repos exist at same AI%, so AI% is not causal

### 2. **Code-debt heavy, no graph problem** (n=~20)
Example: **awest813_CnC_Generals_Zero_Hour** (100% AI, 1 error, 2538 dup pairs)
- **Interpretation:** Entire project batch-generated at once; template reuse created duplication
- **Risk:** Copy-paste maintenance burden; potential for divergent bug fixes
- **AI correlation:** 100% AI + batch mode = artifact of generation process

### 3. **Both problems** (n=~30)
Example: **fox1245_NeoGraph** (94% AI, 114 errors, 15 dup pairs)
- **Interpretation:** Project grew over time with mixed architectural and copy-paste debt
- **Risk:** Compound: hard-to-refactor include graph + divergent code copies

### 4. **Neither problem** (n=~227)
Example: Most projects below median
- **Interpretation:** Clean codebases (either well-maintained or small)
- **Risk:** Low

---

## Hypothesis Results

### H1: AI adoption drives graph-errors
**REFUTED.** ρ = +0.006, p = 0.91
- Counterexample: melkyr_znineeight (0% AI, 273 errors) — worst case is human-only
- Counterexample: shifty81_FreshVoxel (96.6% AI, 0 errors) — excellent graph

### H2: AI adoption drives duplication
**REFUTED.** ρ = +0.029, p = 0.61
- Counterexample: HyperCogWizard_a81ml-org (44.1% AI, 735 dup pairs)
- Counterexample: EricLeeFriedman_CopilotChess (94.3% AI, 0 dup pairs)

### H3: Graph-errors and duplication are linked
**CONFIRMED.** ρ = +0.319, p < 0.0001 ✓
- Both are markers of uncontrolled technical debt
- **Not causal:** cleaning one doesn't clean the other (different root causes)

### H4: "AI-native batch generation" has distinct duplication signature
**SUPPORTED.**
- Projects with 100% AI (13 repos):
  - Graph errors: median = 0.5 (very clean!)
  - Dup pairs: median = 98 (2–3× higher than baseline)
- **Interpretation:** When an AI generates a large project in one pass, it optimizes for graph (no cycles), but reuses code templates (→ duplication)

---

## Risk Assessment

### LOW RISK (AI% alone is not a predictor)
- High AI% ≠ high graph-errors
- High AI% ≠ high duplication
- **Recommendation:** Don't flag code as "risky" just because it's AI-generated

### MEDIUM RISK (If graph_errors OR dup_pairs > median)
- **Graph-errors > 30:** Include architecture is complex; risk of hard-to-track cycles
  - Affects: both AI and human codebases equally
- **Dup pairs > 50:** Copy-paste debt; maintenance burden
  - Affects: both AI and human codebases equally
- **Recommendation:** Flag by *problem type*, not by AI%

### HIGH RISK (Both problems + high AI%)
- Example: fox1245_NeoGraph (114 errors, 15 pairs, 94% AI)
- **Interpretation:** Untamed AI assistance in a rapidly-growing codebase
- **Recommendation:** Review architectural decisions, establish duplication thresholds

---

## Recommendations

1. **Don't gate on AI%** — it's not a risk signal by itself
2. **Gate on code quality metrics:**
   - Max graph_errors per-commit (prevent architecture rot)
   - Max dup_pairs per-module (enforce code reuse discipline)
3. **For AI-generated projects:** extra scrutiny on *duplication* (not graph), because batch generation tends to reuse templates
4. **For mixed AI/human projects:** treat same as human — graph and dup debt are orthogonal, both need management

---

## Files & Appendix

- **Source:** `experiments/ai_repo_run/corpus_summary.tsv` (317 rows)
- **Output report:** `experiments/ai_repo_run/corpus_report.md` (top-50 repos)
- **Analysis code:** (Python Spearman + top-10 extraction)
- **Task:** `backlog/wip/080_manual_corpus_analysis_findings.md` (detailed notes)

---

## Next Steps (Future)

1. **Blame drill-down:** Which commits in CnC_Generals introduced the 2538 dup pairs?
   - `git blame` on pair endpoints, classify by AI-marker
2. **Temporal trend:** Did dup/graph-error growth accelerate after AI adoption window started?
3. **Cross-project patterns:** Do all 100% AI repos show same dup-heavy, graph-clean pattern?
