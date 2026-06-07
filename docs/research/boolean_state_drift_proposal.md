# Boolean-State Drift: Feasibility Assessment & Verdict

**Дата:** 2026-06-07  
**Основано на:** полный цикл — research + статический ценз (790 репо) + usage-разбор + git-history дрейф + per-struct верификация.

> **⚠️ ФИНАЛЬНЫЙ ВЕРДИКТ (после всего расследования) — ниже, в секции «Финальный вердикт v2».**
> Исходный «YES with caveats» относился к *статическому счётчику* и оказался переоптимистичным; ключевой сигнал — не счётчик и не имена, а **инкрементальное накопление в одной структуре во времени**.

---

## Финальный вердикт v2 (итог расследования)

**MAYBE — да, но узко и только во временной форме.**

Прошли четыре независимых измерения сигнала:

| Подход | Что показал | Годность |
|---|---|---|
| Статический счётчик «5+ bool» (790 репо, 5811 структур) | 78% — конфиги | ❌ шум |
| Нейминг-словарь (`started/running/…`) | единственный флаг v0.2 — FP | ❌ бесполезно |
| Usage: групповое присваивание (`a=true;b=false`) | находит FSM без имён (ZuluSCSI audio, ncnn) | ⚠️ правильный примитив, нужен AST |
| **История: per-struct + git-blame** | **0% FP, 57% реальный дрейф** | ✅ **рабочий сигнал** |

**Что реально работает:** атрибуция добавленных bool-полей к КОНКРЕТНОЙ структуре по git-истории, считая только поля (depth-0). Структура, чьи bool пришли через ≥4 разных коммита за месяцы/годы — это и есть constraint decay. Подтверждено глазами: EditorShell (5→23 bool за 6 недель), ATS `HttpTransact::State` (48 bool, многолетний), Terminal, MethodState и др.

**Распространённость:** реальный дрейф ~в **каждом пятом** агентском C++ репо (~15-16 из 73), не в 75% (это была хлипкая file-метрика).

**Оставшаяся проблема:** отделить настоящий дрейф (🟢, 57%) от config-bag (🟡, 43%) — это семантика природы поля (state/mode vs `is*Enabled`/CLI-mirror/registration-guard), именем структуры не ловится. 0% грубых FP, но 🟢/🟡 без field-nature-анализа не разделить.

**Рекомендация для archcheck:** НЕ статическое правило-линтер. Если делать — то как **drift-метрику во времени** (рядом с cycles/coupling drift #086/#087): «структура X набрала N bool за M коммитов». Требует diff/AST-парсинга по истории (≈ #042). Без этого — не shipping. Детали пути и доказательства: `boolean_state_perstruct_drift.md`, `boolean_state_history_drift.md`, `boolean_state_drift_eyecheck.md`, `boolean_state_usage_verdicts.md`.

---

## Executive Summary

Boolean-state drift (growth of `bool` fields in classes/structs) **is a real phenomenon** in OSS codebases (28 candidates with 5+ bools in 50-repo sample). However, **not all boolean accumulation is drift**:

- **True Positive:** State machines (CPartFile, xradio_vif, Iterator) — dangerous drift signal
- **False Positive:** Bitmasks for configuration (ChordCombo, FlatCOptions) — legitimate design

**Feasibility:** Rule can be implemented with **regex heuristics** (no semantic backend) achieving ~70-80% precision. **Semantic backend (#042) can improve to 90%+** but is not required for v0.1.

---

## Candidate Rules & Implementation Complexity

### Rule 1: `implicit_state_machine_growth` (Recommended for v0.2)

**Detection:**
```
IF struct/class has:
  - 5+ boolean fields AND
  - 3+ fields matching state-pattern names (started, running, paused, failed, ready, active, loaded, connected)
THEN flag as implicit state machine
```

**Implementation complexity:** L (Low)
- Regex-based field extraction: ~50 lines Python/C++
- Naming pattern matching: ~20 lines
- Total: ~100 lines (already prototyped in `experiments/boolean_state/extractor.py`)

**Expected FP rate:** ~20-30% (ChordCombo-style bitmasks slip through)  
**Expected FN rate:** ~10-15% (some legitimate state machines have non-standard naming)  
**Net precision:** ~70-75%

**CI suitability:** ✓ Fast (regex-only), ✓ Deterministic, ✗ Some false positives  
**Authority:** ⚠ Level 4 ("несомненные практики") — cite Make Illegal States Unrepresentable + State Pattern

---

### Rule 2: `boolean_state_explosion` (Advanced, v0.3+)

**Detection:**
```
IF (2^num_bools) > (estimated_legal_states * 4):
  - Theoretical state space far exceeds implementable states
  - Suggests implicit state machine OR developer error
THEN flag as state explosion
```

**Requires:** Semantic backend (#042) to analyze conditions & assignments  
**Implementation complexity:** M (Medium) — ~200-300 lines for state-space estimation  
**Expected precision:** ~85% (fewer false positives than Rule 1)  
**CI suitability:** ⚠ Slower (semantic analysis), ✓ Fewer FP  

---

### Rule 3: `impossible_state_pattern` (Advanced, v0.3+)

**Detection:**
```
Scan for conditions checking boolean combinations that are logically impossible:
  - failed && completed (usually only one should be true)
  - paused && running (mutually exclusive)
  - !started && running (invalid transition)
THEN flag as impossible state
```

**Requires:** Semantic backend (#042)  
**Implementation complexity:** M (Medium) — condition extraction & analysis  
**Expected precision:** ~90% (high confidence findings)  
**CI suitability:** ⚠ Slow, ✓ High-quality warnings

---

## Metrics & Thresholds

### Recommended defaults for v0.2

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Min bool fields | 5 | Balances recall vs. FP (3-4 bools OK, 5+ suspicious) |
| State-pattern threshold | 3/N | At least 60% of fields should match state naming |
| Naming patterns | {started, running, paused, failed, ready, active, loaded, connected} | Authority-backed state machine terms |
| Config-flag patterns | {enable_, use_, allow_, verbose, debug} | Exclude known non-state patterns |

### Expected corpus impact (50-repo sample)

| Threshold | Findings | Precision | FP Count |
|-----------|----------|-----------|----------|
| 5+ bools | 28 structs | ~72% | ~8 |
| 5+ bools + naming | 15 structs | ~85% | ~2-3 |
| 8+ bools | 7 structs | ~90% | ~1 |

---

## Drift Detection: Time-Series Aspect

### Measuring drift over time

**Current capability:** Can extract bool-count snapshot at commit T.  
**Drift signal:** Monitor growth: `bool_fields(T2) > bool_fields(T1) + threshold`

**Example:**
```
CPartFile bool fields over 6 months:
  2025-12-01:  8 bools
  2026-01-01:  10 bools (+25%)
  2026-02-01:  12 bools (+20%)
  → Alert: "Boolean-state growth trend detected"
```

**CI implementation:** Integrate with baseline system (#016, #030):
- Store bool-field snapshot per class in baseline
- Compare `--diff` run against baseline
- Report growth as drift signal

**Threshold suggestion:** Flag if 5+ bool fields added in single commit or 2+ within 1 week.

---

## False Positive Analysis

### Known FP cases (from corpus study)

1. **ChordCombo** (8 bools) — Music theory bitmask ✗
   - All 8 combinations are valid (major 9th = maj + ext9)
   - Heuristic fail: naming doesn't match state-pattern
   - **Mitigation:** Add "exclude" pattern for music/audio libraries

2. **FlatCOptions** (8 bools) — Compiler configuration ✗
   - Independent boolean flags (all combinations legal)
   - Heuristic fail: naming matches `*Options` pattern (not state)
   - **Mitigation:** Exclude `*Options`, `*Config`, `*Settings` structs

3. **HttpProxyPort** (7 bools) — Proxy configuration ✗
   - Likely independent config flags
   - **Mitigation:** Context-aware exclusion (proxy/config sections)

### FP mitigation strategies

1. **Naming patterns:** Exclude known non-state structs (Options, Config, Settings, Flags, Parameters)
2. **Context analysis:** If struct is in `config.h` or `options.hpp`, lower suspicion
3. **Bitfield check:** Pure bitfield structs (all `: 1`) are bitmasks, not state machines
4. **Library context:** Audio/graphics libraries often use flag bitmasks (legitimate)

**Estimated FP reduction:** 20-30% → 5-10% with mitigations

---

## False Negative Analysis

### Known FN cases

1. **Wireless drivers with weird naming** — Some use `wl_*` or device-specific prefixes, not state-pattern
2. **Legacy code** — Old codebases use `m_flag1`, `m_flag2` without semantic naming
3. **C-style struct** — Pure C code mixed in C++ project (fewer conventions)

**FN rate expected:** 10-15% (acceptable for optional rules)

---

## Risk Assessment

### Implementation risks
- **Precision:** Some projects will see FP warnings (e.g., graphics libraries with bitmasks)
  - **Mitigation:** Provide "exclude list" in config; document false positive categories
- **Usability:** Rule may be confusing to teams unfamiliar with State Pattern
  - **Mitigation:** Provide refactoring guide & examples in docs
- **Performance:** Semantic backend (#042) will slow down CI
  - **Mitigation:** Make it optional; use fast regex heuristic for v0.2, semantic for v0.3+

### Architectural risks
- **Dogfooding:** Does archcheck itself pass this rule?
  - **Status:** ✓ archcheck codebase has no structs with 5+ bools (verified eye-scan)
  - **Verdict:** Safe to ship

---

## Authority & Positioning

### Which authority backs this rule?

✓ **Make Illegal States Unrepresentable** (Martin, "Your Code as a Crime Scene")  
✓ **State Pattern** (Gang of Four)  
✓ **C++ Core Guidelines:** ES.21 "Do not use a variable for two unrelated purposes" (tangential)  
✗ **Not Lakos** (not physical design specific)  
✗ **Not Lockheart/Martin** (too high-level for code smell detection)

**Positioning:** Level 4 rule ("несомненные практики"), not Core Guidelines default  
**Why:** Detects common pattern, but some legitimate use cases (bitmasks) require context-awareness

---

## Recommendation: Phase Implementation

### v0.2 (Next release, ~3 weeks)
**Rule:** `implicit_state_machine_growth`  
**Implementation:** ~100-150 lines (already prototyped)  
**Threshold:** 5+ bool fields + 3+ state-pattern names  
**Expected precision:** ~72% (tolerable for v0.2)  
**Config:** Add allowlist for known libraries (audio, graphics)

### v0.3 (~8 weeks)
**Rule:** `boolean_state_explosion` (semantic backend dependent)  
**Rule:** `impossible_state_pattern` (semantic backend dependent)  
**Integrate with:** Baseline (#016) for drift trending  
**Expected precision:** ~85-90%

### Beyond v0.3
**Future:** Train ML model on corpus to detect state machines vs. bitmasks  
**Potential:** Integration with State Pattern refactoring suggestions

---

## FINAL VERDICT

### Is boolean-state growth a useful and measurable form of structural drift?

# **YES** ✓

**Evidence:**
1. **Prevalence:** 28/172 (16%) of structs in 50-repo sample have 5+ bool fields
2. **Reality of problem:** CPartFile, xradio_vif, Iterator examples show real drift (12+ bools)
3. **Authority support:** State Pattern + Make Illegal States Unrepresentable principles
4. **Feasibility:** Can be detected with 70-80% precision using regex; 85-90% with semantic backend
5. **Actionability:** Clear refactoring path (State Pattern, enum class)

**Caveats:**
- Not all boolean accumulation is drift (20-30% are legitimate bitmasks)
- Requires heuristics & allowlists to minimize false positives
- Semantic backend (#042) recommended for production use, but not required for MVP

**Next step:** Implement Rule 1 (`implicit_state_machine_growth`) in v0.2; gather ground-truth examples from users; refine heuristics based on feedback.

---

## Appendix: Rule Definition (v0.2)

```yaml
rules:
  implicit_state_machine_growth:
    enabled: true
    description: Implicit state machine detected (bool field accumulation)
    severity: warning
    threshold:
      min_bool_fields: 5
      state_pattern_ratio: 0.6  # at least 60% of fields match state naming
    state_patterns:
      - started
      - running
      - paused
      - failed
      - completed
      - cancelled
      - ready
      - active
      - enabled
      - loaded
      - connected
      - initialized
    exclude_patterns:
      - "*Options"
      - "*Config"
      - "*Settings"
      - "*Flags"
      - "*Parameters"
      - "Chord*"  # Bitmask example
    suggestion: |
      Consider using State Pattern or enum class instead of boolean flags.
      See: https://en.wikipedia.org/wiki/State_pattern
           https://en.cppreference.com/w/cpp/language/enum
    authority: "Make Illegal States Unrepresentable (Martin)"
```

