# [RULES] boolean_state_accumulation — drift-метрика (deferred v0.3+)

**Дата создания:** 2026-06-07
**Статус:** future
**Целевой релиз:** v0.3+ (когда будет спрос)
**Модуль:** RULES / DRIFT
**Приоритет:** minor
**Блокирует:** —
**Заблокирован:** — (НЕ #042: метрика идёт по git-истории, AST по коммитам нереально; строится на fast-бэкенде. #042 — лишь опц. буст гейта 4 на текущем срезе)
**Related:** #089 (research, вердикт MAYBE), #086/#087 (drift-семейство), #042 (опциональный буст, не зависимость)

> **Переосмыслено по итогам research #089.** Исходный план «статическое правило `implicit_state_machine_growth` (5+ bool + state-имена)» ОТМЕНЁН: эмпирика на 790 репо показала, что нейминг-детект бесполезен (единственный флаг — FP), а статический счётчик — 78% шум. Рабочий сигнал — только **per-struct накопление по git-истории**. См. дизайн: `docs/research/boolean_state_metric_design.md`.

## Цель

Реализовать (если будет запрос) drift-метрику `boolean_state_accumulation`: структура набрала bool-поля через ≥4 разных коммита, поля взаимозависимы → растущая неявная FSM. НЕ статический линтер; history-метрика рядом с #086/#087.

## Почему deferred

- **НЕ из-за #042.** Метрика по git-истории; AST по каждому коммиту нереально (×1350, старые коммиты не собираются). Гейты 1-3 = git blame + regex (fast-бэкенд). Гейт 4 (взаимозависимость) = дешёвый regex-прокси по групповому присваиванию на текущем срезе; #042 дал бы лишь маржинальный буст.
- Требует diff/историю (режим `--diff`, не single-shot scan).
- **YAGNI** — никто из пользователей не просил; для archcheck пограничный кандидат (риск нарушить «не линтер»). Это и есть реальный блокер — спрос, не техника.
- Прототип уже есть: `experiments/boolean_state/perstruct_drift.py` (0% грубых FP на верификации).

## План (когда/если делать) — из metric_design.md

- [ ] Гейт 1: depth-0 парсинг полей (без сигнатур/локальных).
- [ ] Гейт 2: per-struct атрибуция + git blame.
- [ ] Гейт 3: проверка полноты истории (shallow → lower-bound).
- [ ] Гейт 4: взаимозависимость — regex-прокси по групповому присваиванию на текущем срезе (config-bag/bloat vs implicit FSM); #042 — опц. буст потом.
- [ ] Пороги: nfields≥5, drift_commits≥4.

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

- **Rule implementation (C++20):** `src/rules/implicit_state_machine_growth.h/cpp` — struct extraction, bool-field counting, state-pattern matching
- **Threshold logic:** min_bool_fields=5, state_pattern_ratio=0.6 (60%) — tuned from corpus study (#089)
- **Heuristics:** state-pattern matching (started/running/paused/failed/active/ready/connected), config-pattern exclusion (enable_*/use_*/verbose/debug), exclude-list (*Options/*Config/Chord*/FlatC*)
- **Fixtures:** 4 test cases (pass + fail variants) → all correctly classified
- **Unit tests:** 5 test cases (Catch2) → all passed ✓
- **Integration:** registered in rule_set, added to CMakeLists, dogfood check pending
- **Documentation:** rule message with field names, ratios, suggestion to use State Pattern

## В работе

- (пусто)

## Следующие шаги

1. Dogfood: verify archcheck itself passes the rule (should pass, no 5+ bool structs found)
2. v0.3: implement Rules 2-3 (semantic backend #042 dependent)
3. Baseline integration: track boolean-field growth over time as drift metric
4. User feedback: gather ground truth from real projects

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
