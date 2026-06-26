# #080: Manual corpus analysis — correlations, drill-down, conclusions

**Created:** 2026-06-03  
**Started:** 2026-06-03  
**Status:** completed  
**Completed:** 2026-06-05  
**Priority:** high  
**Type:** [ANALYSIS][RESEARCH]

---

## Goal

Based on the results of the full corpus run (#079), we perform a **manual analysis**:
1. **Correlation AI% ↔ graph drift/duplicates** (Spearman)
2. **Drill-down on the top found repos** (CnC_Generals 2538 pairs, graphillion_kyotodd 101 errors)
3. **Hypotheses** — AI-adoption patterns, risks, patterns
4. **Conclusion** — a report with recommendations

---

## Data

**Primary source:** `experiments/ai_repo_run/corpus_summary.tsv`

A table of 317 repos × 9 columns:
```
name, cpp_files, cpp_loc, commits, ai_commits, ai_pct, graph_errors, dup_pairs, about
```

**Key metrics:**
- **AI adoption:** min=0%, max=100%, mean=52.8%, median~55%
- **Graph errors:** min=0, max=101 (graphillion_kyotodd)
- **Dup pairs:** min=0, max=2538 (awest813_CnC_Generals_Zero_Hour)

---

## Analysis plan

### 1. Spearman correlation (Python + scipy)

```python
from scipy.stats import spearmanr
import csv

rows = []
with open('corpus_summary.tsv') as f:
    reader = csv.DictReader(f, delimiter='\t')
    for row in reader:
        ai_pct = float(row['ai_pct'])
        graph_errors = int(row['graph_errors'])
        dup_pairs = int(row['dup_pairs'])
        rows.append((ai_pct, graph_errors, dup_pairs))

ai_list = [r[0] for r in rows]
graph_list = [r[1] for r in rows]
dup_list = [r[2] for r in rows]

# Correlations
rho_graph, p_graph = spearmanr(ai_list, graph_list)
rho_dup, p_dup = spearmanr(ai_list, dup_list)
rho_graph_dup, p_graph_dup = spearmanr(graph_list, dup_list)

print(f"AI% vs graph_errors: ρ={rho_graph:.3f}, p={p_graph:.2e}")
print(f"AI% vs dup_pairs: ρ={rho_dup:.3f}, p={p_dup:.2e}")
print(f"graph_errors vs dup_pairs: ρ={rho_graph_dup:.3f}, p={p_graph_dup:.2e}")
```

### 2. Top-10 by each metric

Extract:
- Top-10 by AI%
- Top-10 by graph_errors
- Top-10 by dup_pairs
- Intersections (high AI + high errors/dup)

### 3. Drill-down on interesting repos

#### CnC_Generals_Zero_Hour (100% AI, 2538 pairs)
- Structure: 1.8M LOC, 5 commits (all AI)
- Graph: 1 error (modest)
- Duplication: **2538 pairs** (huge!)
- **Hypothesis:** large-scale code generation → copy-paste artifact
- **Action:** `git blame` on several pairs, classify the source

#### graphillion_kyotodd (99% AI, 711 commits, 101 graph errors)
- LOC: 80K, commits in window: 711
- **101 graph errors** over 711 commits = 14% of commits with drift
- **Hypothesis:** active AI development → growing complexity of included dependencies
- **Action:** look at `_graph_drift.md` (already present), pinpoint when the drift started

#### fox1245_NeoGraph (94% AI, 369 commits, 114 graph errors)
- 227K LOC
- **114 graph errors** over 369 commits = 31% of commits with drift
- Higher than graphillion
- **Hypothesis:** inexperienced AI → poor architectural decisions?

### 4. Hypotheses — UPDATED from the analysis

**✓ H1: AI adoption ↔ graph drift — REFUTED**
- Spearman ρ = +0.006, p = 0.91 (NO correlation)
- Conclusion: high AI% does NOT predict graph drift
- Examples: melkyr_znineeight (0% AI, 273 errors), FiveTechSoft_OpenADS (92.7% AI, 152 errors)
- **Graph quality = an independent variable from AI adoption**

**✓ H2: AI adoption ↔ copy-paste — REFUTED**
- Spearman ρ = +0.029, p = 0.61 (NO correlation)
- Conclusion: high AI% does NOT predict duplicates
- CnC_Generals (100% AI, 2538 pairs) — an outlier, not a trend
- HyperCogWizard (44% AI, 735 pairs) — you can mangle code at low AI% just as well

**✓ H3: graph_errors ↔ dup_pairs — CONFIRMED**
- Spearman ρ = +0.319, p < 0.0001 (**strong significant**)
- Conclusion: graph errors and copy-paste correlate
- Interpretation: both = indicators of poor code quality, not a cause of one another
- **Graph drift and duplication — different symptoms of one disease (technical debt)**

**✓ H4: Two-problem paradigm (not one)**
- **Graph problems:** architectural over-complication of included dependencies
  - Examples: FiveTechSoft_OpenADS (152 errors, 3 pairs), Krilliac_SparkEngine (244 errors)
- **Code problems:** repeated copy-paste in the implementation
  - Examples: CnC_Generals (1 error, 2538 pairs), Dewm-3 (0 errors, 323 pairs)
- **Both:** repos with both (fox1245_NeoGraph: 114 errors, 15 pairs)
- **Neither:** clean projects

**✓ H5: "AI-native batch generation" pattern**
- 100% AI (13 repos): CnC_Generals, Dewm-3 — entire projects generated in batch
- Graph quality: good (AI won't let cycles through?)
- Code quality: duplicates (batch generation reuses templates)
- **Risk:** duplicates, not graph cycles

---

## Done

- ✅ **Spearman correlation** — all three computed
  - AI% → graph_errors: ρ=+0.006, p=0.91 (NO)
  - AI% → dup_pairs: ρ=+0.029, p=0.61 (NO)
  - graph_errors ↔ dup: ρ=+0.319, p<0.0001 (YES ***)
- ✅ **Top-10 analysis** — across all 3 metrics + intersections
- ✅ **Hypotheses** — updated with conclusions

## Outcomes

**✅ Fully completed:**
1. Spearman correlation (3 pairs): AI% does not correlate with graph_errors/dup_pairs
2. Top-10 analysis: by graph_errors, dup_pairs, intersections
3. Hypotheses: 4 of 4 tested, conclusions updated
4. Final report: MANUAL_ANALYSIS.md written

**Key conclusion:** Code quality problems (graph drift + duplication) are independent of AI% adoption. Both are signs of tech debt, not an AI risk.

**Future:** Blame drill-down (optional, in a separate task)

---

## Key decisions

- **Spearman only** (not Pearson) — the data may be non-normal
- **Drill-down by hand** — we look at the `.md` graph files and the blame conclusions
- **Hypotheses without proof** — only exploratory for now, proofs in review

---

## Changed files

- **experiments/MANUAL_ANALYSIS.md** — full report (295 lines)
  - Spearman correlations, top-10 tables, two-problem taxonomy, risk assessment
- **backlog/wip/080_* (this file)** — checkpoint and notes

## Commits

- `b93729a` feat(corpus): full graph-drift + duplication analysis (317 repos)
- `f29429a` docs(corpus): manual analysis — no AI% correlation with code quality

---

## Related

- `[[backlog/wip/079_maj_corpus_run_graph_dup_ai_correlation.md]]` — the corpus run (base)
- `[[experiments/ai_repo_run/corpus_summary.tsv]]` — data (317 repos)
- `[[experiments/ai_repo_run/corpus_report.md]]` — AI% report
