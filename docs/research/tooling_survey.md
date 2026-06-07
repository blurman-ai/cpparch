# Tooling Survey: Boolean-State Drift Detection

**Дата:** 2026-06-07  
**Методология:** review existing tools for boolean-state & state-machine detection capabilities

## Surveyed Tools

### 1. SonarQube
**Status:** ✓ Installed at scale  
**Boolean-related rules:**
- `boolean:S1764` — Redundant boolean operations
- `boolean:S1134` — "TODO" and "FIXME" markers (not related)
- `cognitive:S3776` — Cognitive complexity (includes boolean conditions)

**Findings:** Detects **boolean complexity** but NOT:
- ✗ State-machine patterns
- ✗ Boolean-state growth (no historical metrics)
- ✗ Impossible-state combinations
- ✗ 5+ boolean fields accumulation

**Verdict:** Gap exists

### 2. Cppcheck
**Status:** ✓ C++ specialization  
**Boolean-related checks:**
- `unusedFunction`, `unusedVariable` (not boolean-specific)
- `invalidPointerCast` (type-related)

**Findings:**
- ✗ No boolean-field accumulation detector
- ✗ No state-pattern inference
- ✗ No implicit state machine detection

**Verdict:** Gap exists

### 3. Clang-Tidy (LLVM)
**Status:** ✓ C++ AST-based  
**Relevant checks:**
- `cppcoreguidelines-pro-type-member-init` (initialization)
- `readability-avoid-const-params-in-decls` (code quality)
- `bugprone-bool-pointer-implicit-conversion` (type safety)

**Findings:**
- ✓ Can detect some boolean patterns via AST
- ✗ No boolean-accumulation metric
- ✗ No drift detection (no historical comparison)
- ✗ No state-space explosion detection

**Verdict:** Gap exists

### 4. CppDepend (Commercial)
**Status:** ⚠ Commercial tool  
**Capabilities:**
- Dependency graphs
- Metrics: complexity, coupling, cohesion
- Custom rule engine

**Findings:**
- ✓ Can express custom rules via API
- ✓ Metrics include NDepend-style coupling/cohesion
- ✗ No built-in boolean-state detection
- ? Could be configured for custom analysis (requires implementation)

**Verdict:** Potential for custom rules, but not out-of-the-box

### 5. Designite (Academic)
**Status:** ⚠ Academic tool (slow uptake in industry)  
**Focus:** Design smell detection  
**Relevant:** "God Class" detection (large class with many responsibilities)

**Findings:**
- ✓ Detects classes with excessive boolean fields (tangentially)
- ✗ No state-machine-specific detection
- ✗ No drift metrics

**Verdict:** Partial coverage via "God Class" heuristics

### 6. PMD (Java-focused, limited C++)
**Status:** ✗ Java/JavaScript focus  
**C++ support:** Minimal  

**Verdict:** Not applicable

### 7. FindBugs / SpotBugs
**Status:** ✗ Java-focused  

**Verdict:** Not applicable

---

## Academic & Open-Source Research

### Boolean-State Smell Detectors (Literature)
Searched: "boolean field accumulation", "implicit state machine", "state explosion"

**Findings:** Few dedicated tools
- **Program verification:** SMT solvers (Z3, CVC4) can check state reachability theoretically
- **Model checking:** SPIN, UPPAAL — detect state-space issues but require formal models
- **Practical C++ tools:** None found that detect boolean-state drift in real codebases

**Conclusion:** Boolean-state-growth detection is **novel** in the practical C++ ecosystem.

---

## Gap Analysis

### What Exists
- ✓ Boolean complexity metrics (SonarQube, Clang-Tidy)
- ✓ Generic "large class" detection (CppDepend, Designite)
- ✓ Type-safety checks (Clang-Tidy)

### What's Missing
- ✗ **Boolean-field accumulation over time** (historical metrics)
- ✗ **Implicit state machine detection** (pattern recognition)
- ✗ **State-space explosion metrics** (2^N vs. possible states)
- ✗ **Impossible-state pattern detection** (e.g., `failed && completed`)
- ✗ **CI-friendly state-drift reporting**

---

## Proposed archcheck Rule

### Rule: `implicit_state_machine_growth` (candidate)

**Input:** Struct/class definition with N boolean fields  
**Heuristics:**
1. N ≥ 5 boolean fields
2. At least 3 fields match state-pattern naming (started/running/paused/failed/etc.)
3. Optional: Semantic analysis of conditions (requires libclang #042)

**Output:** Warning with:
- Struct name & file
- Number of bool fields
- Estimated state-space (2^N)
- Suggestion: "Consider State Pattern or enum class"

**CI-suitability:**
- ✓ Fast: regex-based heuristic (no semantic backend required)
- ⚠ Some false positives (ChordCombo-style bitmasks)
- ⚠ Moderate false negatives (config-heavy structs missed)
- ✓ Suitable for Level 4 ("несомненные практики") — not Core Guidelines authority, but defensible

---

## Alternative Approaches

### Option A: Conservative (High Precision, Low Recall)
- Threshold: 8+ bool fields
- Require: State-pattern naming (3+ of: started/running/paused/failed/ready/active/connected)
- Result: Fewer findings, high confidence

### Option B: Aggressive (High Recall, Some FP)
- Threshold: 5+ bool fields
- Require: Any naming OR condition interdependencies (requires #042)
- Result: More findings, need filtering

### Option C: Two-Phase (Recommended for v0.2)
- Phase 1: Scan (5+ bools, state-pattern naming) → flag for review
- Phase 2: Semantic analysis (#042) → check for impossible states & transitions
- Result: Conservative first, refined later

---

## Recommendation

**For archcheck v0.2:**
1. Implement Phase 1 (regex + heuristics) — no libclang dependency
2. Document as Level 4 rule (not Core Guidelines authority)
3. Collect ground-truth examples from corpus (this research)
4. Later: Phase 2 (semantic backend integration)

**Expected outcome:** Useful signal for drift detection, with clear limitations (may flag valid bitmasks, miss some state machines).

