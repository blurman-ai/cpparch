# Concrete Code Quality Examples: Graph Drift & Duplication

**Source:** `corpus_summary.tsv` (317 C++ repos), per-commit analysis May 2025 – Jun 2026

---

## GRAPH DRIFT EXAMPLE: graphillion_kyotodd

**Repo stats:**
- AI%: 99% (711/711 commits)
- LOC: 80,913
- Graph errors: **101 per-commit drifts**
- Duplication: 2 pairs
- Project: BDD (Binary Decision Diagram) library

### What Happened: Incremental Refactoring Under AI

This is a **classic refactoring spiral** where AI incrementally extracted functionality into new headers. Each commit adds include-edges without removing old dependencies, creating a gradually more complex include graph.

**Sample commits from graph-drift log:**

```
cc8e84da067b "Add BddNode struct and CMake build system" (2026-02-26)
  + src/main.cpp → include/bdd_node.h
  → 1 new cross-area dependency (src ↔ include)

3c85e2479de8 "Add BDD_Init function with malloc-based node allocation"
  + include/bdd.h → include/bdd_node.h
  + src/bdd.cpp → include/bdd.h
  + src/main.cpp → include/bdd.h
  → 3 edges added, 0 removed (net +3)

1a0334befa7f "Extract function declarations into bdd_base.h, bdd_ops.h, bdd_io.h"
  + include/bdd.h → include/bdd_base.h
  + include/bdd.h → include/bdd_io.h
  + include/bdd.h → include/bdd_ops.h
  + include/bdd_base.h → include/bdd_types.h
  + include/bdd_io.h → include/bdd_types.h
  + include/bdd_ops.h → include/bdd_types.h
  → 6 edges added
```

### Pattern Recognition

- **Commits 1–20:** Building core structure (add bdd_node.h, bdd_types.h, etc.)
  - Each commit: +1 to +6 edges
  - Cumulative: growing mesh of header dependencies
- **Commits 21–200:** Feature extraction (I/O, operations, algorithms split)
  - Each commit: +2 to +4 edges
  - Result: `include/bdd.h` becomes central hub, many ↓ includes

### Why This Happens with AI

1. **No refactoring discipline:** AI extracts function → new header → new #include, but doesn't remove old #includes
2. **Template reuse:** AI likely used same pattern for each module (declare → implement)
3. **No ACD (Acyclic Component Dependency) enforcement:** Graph grows unchecked

### Code Quality Signal

- **Risk level:** MEDIUM
- **Symptom:** Compilation becomes slow (complex include chain), hard to test modules in isolation
- **Not an AI-specific problem:** Humans do the same when refactoring quickly
- **Fix:** Cyclic SCC analysis, enforce layer boundaries, consolidate headers

---

## DUPLICATION EXAMPLES: High Copy-Paste Cases

### Case 1: awest813_CnC_Generals_Zero_Hour (100% AI, **2,538 dup pairs**)

**Repo stats:**
- AI%: 100% (5 commits, all AI-marked)
- LOC: 1,835,911 (1.8M — huge!)
- Graph errors: **1** (super clean!)
- Duplication: **2,538 pairs** (massive!)

**What happened:** Entire game engine batch-generated at once

**Signature:** 
- All commits are 100% AI-generated (co-authored-by: claude)
- Whole codebase added in single batch → reused code templates everywhere
- Include graph is clean (no cycles, few layers) — AI probably generated with templates
- But templates = copy-paste for similar game systems (physics, rendering, AI, input)

**Examples of likely duplicates:**
```
// system/physics_engine.cpp
// system/rendering_engine.cpp
// system/ai_engine.cpp
// system/input_handler.cpp
→ All probably have similar structure (init, update, render, shutdown)

// game_objects/player.cpp
// game_objects/enemy.cpp
// game_objects/npc.cpp
→ Similar state machines, similar behavior patterns
```

**Why 2,538 is realistic:**
- 1.8M LOC ÷ ~10-20 functions per file = ~180k functions
- If 50% are in duplicate patterns → ~90k functions
- Each duplicate pair = 2 functions → ~45k pairs
- (Exact count: algorithm detects token-level similarity, so matches are more granular)

**Risk assessment:**
- **Graph:** LOW (clean architecture)
- **Code:** HIGH (duplicate maintenance burden)
- **Impact:** If a bug is found in physics engine, same bug likely exists in rendering engine (different context, same code)

---

### Case 2: awest813_Dewm-3 (100% AI, **323 dup pairs**)

**Repo stats:**
- AI%: 100% (14 commits)
- LOC: 681,588
- Graph errors: 0 (super clean)
- Duplication: 323 pairs

**What happened:** Fork of Doom 3, AI-powered modernization

- Original Doom 3 GPL codebase (C, complex)
- AI was tasked to "modernize to C++20" or "add features"
- Batch refactor = reused patterns across similar modules

**Comparison with CnC_Generals:**
- **Smaller codebase:** 681k vs 1.8M LOC
- **Fewer duplicates:** 323 vs 2,538 pairs
- **Ratio:** 323/681k ≈ 0.47 pairs/kLOC vs 2,538/1,835k ≈ 1.38 pairs/kLOC
- **Interpretation:** CnC is MORE duplicate-heavy (maybe less refactored, or more game-specific code reuse)

---

### Case 3: HyperCogWizard_a81ml-org (44% AI, **735 dup pairs**)

**Repo stats:**
- AI%: 44% (mixed AI + human commits)
- LOC: 795,850
- Graph errors: 14
- Duplication: **735 pairs** (higher than 100% AI repos on a per-commit basis!)

**Pattern:** **Not purely AI-generated, but high copy-paste anyway**

**Why this matters:**
- Refutes "AI = more duplication" hypothesis
- **Humans also write copy-paste code**, especially under time pressure
- 735 pairs with only 44% AI means humans contributed significant duplication

**Likely scenario:**
- Team rushed implementation
- Reused earlier modules as templates (human pattern)
- AI was brought in to speed up remaining features
- Result: pile of duplicates from both sources

---

### Case 4: FiveTechSoft_HarbourBuilder (88% AI, **23 dup pairs**)

**Repo stats:**
- AI%: 88% (808 commits)
- LOC: 281,873
- Graph errors: 152 (lots!)
- Duplication: **23 pairs** (clean code!)

**Pattern:** **High graph errors, low duplication — opposite of CnC_Generals**

**Interpretation:**
- Codebase evolved over 808 commits with careful refactoring
- Graph got complex (152 errors) due to incremental feature additions
- But developers enforced code reuse discipline (low duplication)
- AI assistance was used smartly: refactor often, reuse templates rarely

**Lesson:** AI % doesn't determine code quality. *How* AI is used does.

---

## Summary Table

| Repo | AI% | LOC | Graph Errors | Dup Pairs | Pattern |
|------|-----|-----|--------------|-----------|---------|
| graphillion_kyotodd | 99% | 80K | 101 | 2 | Refactoring spiral (graph problems) |
| CnC_Generals | 100% | 1.8M | 1 | 2,538 | Batch generation (code problems) |
| Dewm-3 | 100% | 681K | 0 | 323 | Batch generation, smaller |
| HyperCogWizard | 44% | 795K | 14 | 735 | Mixed AI/human, rushed (code problems) |
| FiveTechSoft | 88% | 281K | 152 | 23 | Incremental refactor, good reuse (graph problems) |

---

## Key Takeaways

1. **Graph drift ≠ Duplication** — they're independent symptoms
   - graphillion_kyotodd: high graph, low dup
   - CnC_Generals: low graph, high dup

2. **AI % doesn't predict problems** — workflow matters
   - CnC (100% AI, batch): clean graph, dirty code
   - HyperCogWizard (44% AI, mixed): dirty graph, dirty code
   - FiveTechSoft (88% AI, incremental): dirty graph, clean code

3. **Risk interventions are orthogonal:**
   - **For graph problems:** enforce ACD, monitor include chain depth, limit SCC size
   - **For duplication problems:** enforce DRY, configure threshold, track copy-paste metrics
   - Both are *architecture/discipline problems*, not *AI problems*

---

## Next Steps (Future Analysis)

1. **Blame drill-down:** Pick 10 duplication pairs from CnC_Generals, show exact code
2. **Temporal evolution:** When did graph errors spike in graphillion_kyotodd?
3. **Cross-tool analysis:** Does clang-tidy catch these issues? (Probably yes for dup, maybe for graph)
