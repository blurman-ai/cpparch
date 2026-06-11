# [CLEANUP][QUALITY] Post-audit sweep: мёртвый код, дубли, lizard-долг

**Дата создания:** 2026-06-11
**Дата старта:** 2026-06-11
**Статус:** wip
**Модуль:** SCAN / GRAPH / CONFIG / GIT / REPORT / TESTS
**Приоритет:** minor
**Сложность:** medium
**Блокирует:** —
**Заблокирован:** —
**Related:** #070 (checker_fp_fix_proposals — P1.3), #073 (tech_debt_alignment_cleanup), #083/#084 (fp corpus eval)

## Цель

Закрыть подтверждённые grep'ом находки аудита кода 2026-06-11 (мёртвый код, дубли,
lizard-долг). Полный список с доказательствами — `docs/research/full_audit_and_research_2026_06.md`
§1.3–1.4. Каждый пункт — маленький самостоятельный коммит (≤50 строк, правило code_quality).

## 1. Мёртвый код (удалить или довести до потребителя)

- `diffTokens`/`DiffOp` + LCS-механика (~95 строк) в `clone_classifier.cpp:10-106` —
  вызываются только тестами; либо удалить вместе с тестами, либо перенести в test-only TU
  по образцу `fp_corpus_eval.cpp`.
- `reverseReachableFrom`, `hasPath` (`graph/algorithms.h:15-16`) — только тесты.
- Write-only/никогда не читаемые: `MetricThresholds::chainLengthLimit`
  (`regression_report.h:51`), `Pair::sharedRare` (`duplication_scanner.h:22`),
  `BaselineLoadError::line` (всегда 0), `Config::version` (читается только тестом).
- Мёртвые аксессоры: `ConfigError::file()/line()/column()`, `Worktree::valid()`,
  `DiskFileSource::root()`.
- `evaluateAgainstCorpus` — placeholder с вечным precision=1.0: реализовать матчинг
  (#083/#084) или удалить, оставив `loadCorpusGroundTruth`.
- `tests/duplication_vmecpp_test.cpp`, `tests/duplication_all_projects_test.cpp` —
  не включены в CMakeLists, не компилируются: включить (как manual-tag) или удалить.

## 2. Дубли (порог проекта «>5 строк» нарушен)

- ~50 строк fork/exec между `git_state.cpp:33-118` и `git_object_file_source.cpp:40-113`
  → общий хелпер в `git/`.
- 3 копии `toLower` (duplication_scanner, drift_bidirectional_coupling, +1) →
  единая в `file_classification.h` (заявленный single source of truth).
- JSON-сериализация violations продублирована (`violation_baseline.cpp:115-125` vs
  `json_reporter.cpp:17-26`), парсер baseline — хрупкий substring-разбор → переиспользовать.
- Vendor/test-фильтр продублирован (`graph_builder.cpp:55-74` vs `project_files.cpp:115-136`).
- `plainJaccard` — 25-строчная копия `weightedJaccard` (`similarity.cpp:11-65`).

## 3. Гигиена duplication_scanner.cpp

Опечатки в именах (`Loclal`, `filer`), две функции «phase8», мёртвые ветки,
неименованные магические пороги, комментарии-пересказы (forbidden pattern).

## 4. Lizard-долг (локальный гейт красный)

5 функций в `src/` за порогами CCN15/len30 (3 в `duplication_scanner.cpp`,
`main.cpp:117 applyBaselineAndReport`, +1) и 13 TEST_CASE — разрезать до зелёного
`lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/`.
Причина расхождения с CI уже выяснена (2026-06-11): CI и `/commit`-гейт меряют
`-T nloc=30` (NLOC, без пустых/комментариев) и только `src/ include/` — по нему чисто;
красный — локальный вариант `--length 30` (полная длина) из памятки + `tests/`.
Решить: либо ужесточить CI до `--length`, либо ослабить памятку до NLOC — и выровнять.

## Сделано (секция 1 только)

### Удалённые символы и их тесты

1. **diffTokens, DiffOp, buildLcsTable, backtrackLcs, collapseDelInsPairs** (~95 строк)
   - Удалены из `include/archcheck/scan/duplication/clone_classifier.h` (11-22)
   - Удалены из `src/scan/duplication/clone_classifier.cpp` (10-106)
   - Удалены TEST_CASE-ы "DiffOp: identical sequences...", "DiffOp: different sequences..." из `tests/duplication_clone_classifier_test.cpp` (53-82)

2. **reverseReachableFrom, hasPath** (~30 строк)
   - Удалены декларации из `include/archcheck/graph/algorithms.h` (15-16)
   - Удалены функции из `src/graph/algorithms.cpp` (167, 169-196)
   - Удалены 6 TEST_CASE-ов из `tests/unit/graph/algorithms_test.cpp` (115-170) + using declarations (13-16)

3. **MetricThresholds::chainLengthLimit** (1 строка)
   - Удалено поле из `include/archcheck/diff/regression_report.h:51`

4. **Pair::sharedRare** (2 строки)
   - Удалено поле из `include/archcheck/scan/duplication/duplication_scanner.h:22`
   - Удалено присваивание из `src/scan/duplication/duplication_scanner.cpp:42`

5. **BaselineLoadError::line** (1 строка)
   - Удалено поле из `include/archcheck/graph/baseline.h:25`
   - Исправлена инициализация в `src/graph/baseline.cpp:131` (убрано `, 0`)

6. **ConfigError аксессоры и приватные поля** (~5 строк)
   - Удалены методы `file()`, `line()`, `column()` из `include/archcheck/config/config_loader.h:18-20`
   - Удалены приватные поля `file_`, `line_`, `column_` из `include/archcheck/config/config_loader.h:23-25`
   - Исправлена инициализация в `src/config/config_loader.cpp:14-16`
   - Улучшение: исправлена cppcheck-ошибка, сделав параметр `file` const-ref (bonus fix)

7. **Worktree::valid()** (1 строка)
   - Удалено из `include/archcheck/git/git_state.h:47`

8. **DiskFileSource::root()** (1 строка)
   - Удалено из `include/archcheck/scan/disk_file_source.h:23`

9. **evaluateAgainstCorpus + CorpusMetrics** (~50 строк)
   - Удалена CorpusMetrics struct из `include/archcheck/scan/duplication/fp_corpus_eval.h:20-30`
   - Удалена evaluateAgainstCorpus декларация из .h:40
   - Удалена evaluateAgainstCorpus функция из `src/scan/duplication/fp_corpus_eval.cpp:44-74`
   - Удалены 5 TEST_CASE-ов из `tests/duplication_fp_corpus_eval_test.cpp` (21-71)

**Всего удалено: ~180 строк кода + 14 TEST_CASE-ов**

### Гейты

- **clang-format**: exit code 0 ✓
- **cppcheck**: exit code 0 ✓ (+ bonus: исправлена cppcheck-ошибка на ConfigError)
- **lizard**: exit code 1 (но это из других коммитов — run_history функция вне скопа этой задачи)
- **cmake build**: exit code 0 ✓ (351 строк было удалено)
- **ctest**: exit code 0 ✓ (401 тест прошёл)
- **archcheck self-check** (src/, include/, tests/): exit code 0 ✓

## В работе (не выполнено)

- **Секция 2** (дубли fork/exec, toLower, JSON-сериализация, vendor/test-фильтр, plainJaccard) — отложена на отдельную задачу
- **Секция 3** (гигиена duplication_scanner.cpp — опечатки, phase8 дубли) — отложена
- **Секция 4** (lizard-долг, рефакторинг длинных функций) — отложена; функция run_history (line 330, 31 NLOC) — это из других коммитов, вне скопа #104
- **tests/duplication_vmecpp_test.cpp, tests/duplication_all_projects_test.cpp** — оставлены сознательно (не входили в CMakeLists, не компилируются, research-артефакты per CLAUDE.md инструкции)

## Acceptance

- ✓ `grep`-доказательства удалённых символов больше не воспроизводятся (0 вхождений)
- ✓ Все тесты зелёные (401/401)
- ✓ Сборка проходит (cmake exit code 0)
- ✓ Самопроверка archcheck чиста (src/, include/, tests/ — 0 нарушений)
- ✓ Нет новых файлов сверх необходимого (только перемещение задачи в wip/)
