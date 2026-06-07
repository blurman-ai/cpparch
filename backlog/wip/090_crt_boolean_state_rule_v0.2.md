# [RULES] Implement implicit_state_machine_growth Rule (v0.2)

**Дата создания:** 2026-06-07
**Дата старта:** 2026-06-07
**Статус:** wip
**Модуль:** RULES / IMPLEMENTATION
**Приоритет:** critical
**Сложность:** M
**Блокирует:** v0.2 release
**Заблокирован:** —
**Related:** #089 (boolean_state_drift — research phase complete; verdict: YES)

## Цель

Реализовать Rule 1 из research #089: `implicit_state_machine_growth`. Детектировать классы/структы с 5+ bool-полями, соответствующих state-pattern (started/running/paused/failed/etc.), как признак неявной state machine, нуждающейся в рефакторинге.

## Контекст

Research #089 доказала:
- Boolean-state growth — реальный drift-сигнал (28 candidates в 50-repo corpus sample, 16% prevalence)
- Вердикт: **YES**, можно реализовать с ~72% precision на regex-хеуристиках без semantic backend
- Authority: "Make Illegal States Unrepresentable" (Martin) + State Pattern (GoF)

Rule 1 параметры:
- Threshold: 5+ bool fields + 3+ state-pattern names (60% ratio)
- Implementation complexity: L (Low) — 100-150 lines
- FP mitigation: allowlist (*Options, *Config, Chord*, etc.)

## План выполнения

### 1. Integrate extractor into src/
- [ ] Port `experiments/boolean_state/extractor.py` logic в `src/rules/` (C++20 native)
- [ ] Создать `src/rules/boolean_state_detector.h` + `.cpp`
- [ ] Зарегистрировать в `rule_set.h`

### 2. Implement IRule class
- [ ] Inherit from IRule, implement `check(const DependencyGraph&, const Config&) -> vector<Violation>`
- [ ] Fields: min_bool_fields (configurable, default 5), state_pattern_ratio (default 0.6)
- [ ] Struct extraction (scan header files for `struct`/`class` with bool members)
- [ ] Naming heuristics (state-pattern vs config-pattern matching)

### 3. State-pattern dictionary
- [ ] State names: {started, running, paused, failed, completed, cancelled, ready, active, enabled, loaded, connected, initialized}
- [ ] Exclude patterns: {*Options, *Config, *Settings, *Flags, *Parameters, Chord*, Flat*}
- [ ] Config names: {enable_, use_, allow_, verbose, debug, trace}
- [ ] Make configurable via YAML `implicit_state_machine_growth:` block

### 4. Fixtures
- [ ] `fixtures/implicit_state_machine_growth/pass/valid_state_machine.h` — 5+ state-bools → flag (VIOLATION)
- [ ] `fixtures/implicit_state_machine_growth/fail_under_threshold.h` — 4 bools → OK
- [ ] `fixtures/implicit_state_machine_growth/fail_config_named.h` — 5 bools, all config-named → OK
- [ ] `fixtures/implicit_state_machine_growth/fail_excluded.h` — ChordCombo-style bitmask → OK (excluded)

### 5. Testing
- [ ] Unit tests (10 cases: pass/fail variants)
- [ ] Corpus validation: Run на 50-repo sample, verify precision ≥70%
- [ ] Dogfood: archcheck itself must pass (verified in #089)
- [ ] CI check with code_review

### 6. Documentation
- [ ] docs/rules/implicit_state_machine_growth.md
- [ ] Add to CHANGELOG.md (v0.2 section)
- [ ] CLI help text

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Create rule skeleton: `src/rules/boolean_state_detector.h`
2. Implement struct extraction & bool-field scanning
3. Add naming heuristics (state vs config patterns)
4. Write fixtures
5. Testing & validation

## Ключевые решения

| Решение | Причина |
|---------|---------|
| C++20 native (not Python wrapper) | Consistent with codebase style; avoid runtime Python dependency |
| Threshold: 5+ bools | From corpus study: balances recall (~80%) vs FP (~28%) |
| Exclude patterns allowlist | Handle known FP cases (ChordCombo, FlatCOptions, etc.) |
| State-pattern naming required | Not just boolean count; must match known state names |
| YAML configurable | Users can tune thresholds per project, add/remove patterns |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `src/rules/boolean_state_detector.h` | new (IRule subclass) |
| `src/rules/boolean_state_detector.cpp` | new (implementation) |
| `include/archcheck/rules/rule_set.h` | modify (register rule) |
| `fixtures/implicit_state_machine_growth/` | new (pass/ + fail_*/) |
| `docs/rules/implicit_state_machine_growth.md` | new (user docs) |
| `CHANGELOG.md` | update (v0.2 release notes) |

## Fixtures

- [ ] `fixtures/implicit_state_machine_growth/pass/state_machine.h` (5 bools, state-named)
- [ ] `fixtures/implicit_state_machine_growth/fail_under_threshold.h` (4 bools)
- [ ] `fixtures/implicit_state_machine_growth/fail_config_flags.h` (5 bools, config-named)
- [ ] `fixtures/implicit_state_machine_growth/fail_bitmask.h` (bitmask, excluded)
