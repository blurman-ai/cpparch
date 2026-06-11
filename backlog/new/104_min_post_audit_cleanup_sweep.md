# [CLEANUP][QUALITY] Post-audit sweep: мёртвый код, дубли, lizard-долг

**Дата создания:** 2026-06-11
**Дата старта:** —
**Статус:** new
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

## Acceptance

- `grep`-доказательства из аудита больше не воспроизводятся (символы удалены или обрели потребителя).
- Локальный lizard-гейт зелёный, CI-вариант выровнен с локальным.
- Все тесты зелёные; ни одного нового файла сверх необходимого.
