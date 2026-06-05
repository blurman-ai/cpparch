# [SCAN][DUPLICATION][BUILD] Вынести `fp_corpus_eval` из shipped `archcheck_core`

**Дата создания:** 2026-06-05
**Дата старта:** —
**Статус:** new
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

- [ ] `archcheck_core` (shipped-lib) больше не содержит placeholder corpus-eval кода.
- [ ] Тест `duplication_fp_corpus_eval_test` продолжает собираться и проходить (или
      перемещён вместе с кодом).
- [ ] Build зелёный; coverage-пороги держатся.
