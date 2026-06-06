# [SCAN][DUPLICATION][BUILD] Вынести `fp_corpus_eval` из shipped `archcheck_core`

**Дата создания:** 2026-06-05
**Дата старта:** 2026-06-05
**Дата завершения:** 2026-06-05
**Статус:** completed
**Модуль:** SCAN/DUPLICATION + BUILD
**Приоритет:** minor
**Сложность:** S
**Блокирует:** —
**Заблокирован:** —
**Related:** #082 (alignment umbrella — родитель, Slice 5a/5b), #083 (precision — единственный реальный потребитель честной корпус-метрики)

## Цель

Убрать research/QA-харнесс оценки precision из основной shipped-библиотеки
`archcheck_core`, чтобы product-код не нёс non-product placeholder.

## Контекст (откуда задача)

[src/scan/duplication/fp_corpus_eval.cpp](../../src/scan/duplication/fp_corpus_eval.cpp)
скомпилён в `archcheck_core` ([src/CMakeLists.txt](../../src/CMakeLists.txt) строка ~13),
но:

- используется **только тестами** ([tests/duplication_fp_corpus_eval_test.cpp](../../tests/duplication_fp_corpus_eval_test.cpp)),
  не на runtime-пути `--duplication`;
- `evaluateAgainstCorpus` — **placeholder** («assume all pairs are TP»), реальной
  метрики не считает (тестируется только degenerate-кейс).

В #082 (Slice 5b) это помечено как «internal QA-tooling вне user-facing surface» и
сознательно оставлено — relocation это структурная build-правка, отдельная от
alignment контрактов.

## Что нужно (выбрать одно)

1. **Переместить** в test-only target (убрать из `archcheck_core`, линковать только в
   `archcheck_tests`); либо отдельная `research/`-директория вне основного билда.
2. **Реализовать** `evaluateAgainstCorpus` по-настоящему, если корпус-метрика нужна
   (тогда это часть #083).
3. **Явно изолировать** как research-namespace/комментарий с пометкой «not product».

Рекомендация: вариант 1 (test-only target) — минимально и честно.

## Acceptance criteria

- [x] `archcheck_core` (shipped-lib) больше не содержит placeholder corpus-eval кода.
- [x] Тест `duplication_fp_corpus_eval_test` продолжает собираться и проходить.
- [x] Build зелёный; coverage-пороги держатся.

## Как сделано (2026-06-05)

Выбран вариант 1 (test-only target), файлы оставлены на месте:

- `src/CMakeLists.txt` — `scan/duplication/fp_corpus_eval.cpp` убран из списка
  источников `archcheck_core`, на его месте — комментарий-пойнтер.
- `tests/CMakeLists.txt` — `${CMAKE_SOURCE_DIR}/src/scan/duplication/fp_corpus_eval.cpp`
  добавлен в источники `archcheck_tests` (компилируется прямо в тест-бинарь).
- `include/archcheck/scan/duplication/fp_corpus_eval.h` — шапка помечена как
  research/QA test-only harness (не часть shipped `archcheck_core`).

**Проверка:** `ar t libarchcheck_core.a` — нет `fp_corpus_eval.o`; `nm` — нет
corpus-символов в shipped-lib. Тест `duplication_fp_corpus_eval_test` собирается и
проходит. Полный gate: clang-format/cppcheck/lizard чисто, build, 344/344, smoke,
coverage 91.4/95.8/57.2 — PASS.

**Заметка:** placeholder `evaluateAgainstCorpus` остался placeholder'ом — реальную
корпус-метрику считать здесь не требовалось; если понадобится, это часть #083.
