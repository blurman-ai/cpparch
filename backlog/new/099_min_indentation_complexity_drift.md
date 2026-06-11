# [CHEAP-DRIFT][SCAN] Indentation-Complexity Drift

**Дата создания:** 2026-06-10
**Дата старта:** —
**Статус:** new
**Модуль:** SCAN / DIFF / REPORT
**Приоритет:** minor
**Сложность:** medium
**Блокирует:** —
**Заблокирован:** —
**Related:** #029 (metric_regression_detection), #030 (baseline_file_flag), #093 (flag_argument), #094 (param_count_and_accretion), #101 (local_complexity_drift)

## Цель

Дать грубый text-only proxy на "код стал глубже вложен / ветвистее", не притягивая AST или полноценный complexity analyzer.

Это именно drift-эвристика. Она не заменяет `lizard`, не спорит с cyclomatic complexity и не должна трактоваться как точная метрика сложности.

## Контекст

- Источник постановки: `docs/codex_archcheck_cheap_drift_tasks.md`, Task 7.
- В репозитории уже есть lizard в dogfood/static-analysis. Значит, новая эвристика должна отвечать на другой вопрос: "стало ли заметно хуже относительно base ref", а не "сколько у функции объективно CCN".
- Текущий baseline-механизм хранит violations, а не per-file text metrics. Поэтому реалистичный первый проход — comparison current vs git ref / diff, а не новый глобальный snapshot format в рамках этой задачи.
- Сигнал вероятностный и чувствителен к formatter/style changes. Значит, default gate запрещён.
- После появления #101 эту задачу нельзя реализовывать как отдельный параллельный продукт: либо она остаётся узким fallback/proxy внутри `local_complexity_drift`, либо закрывается как поглощённая более сильным function-aware сигналом.

## План выполнения

### Detection contract

- На уровне файла или function-like region считать:
  - max indentation;
  - average indentation;
  - variance;
  - число строк с глубокой индентацией.
- `CMP.1.indentation_complexity_drift` возникает, когда current заметно хуже base ref по одному или нескольким порогам.
- **Статус относительно #101 (после дизайн-ресёрча 2026-06-11):** отступы — прокси
  классических метрик с унаследованной корреляцией с объёмом (Hindle ICPC 2008;
  Gil & Lalouche EMSE 2017) и форматозависимостью (выравнивание, табы — репро в
  `docs/research/local_complexity_drift_scorer_review.md`). Поэтому: в score #101
  они НЕ входят; эта задача реализуется только как **file-level fallback** для
  файлов, где function-discovery #101 не сработал (макро-тяжёлые), и как
  диагностические поля отчёта. Если после #101 fallback окажется не нужен —
  закрыть как absorbed.

### Runtime shape

- Работать по non-empty, non-comment-only строкам.
- Tabs переводить в columns через фиксированный `tab_width`.
- Первый проход можно делать на file level; finer-grained function ranges допустимы только если это получается дёшево из уже существующего scanner logic.
- Сравнение вести через git ref / diff; отдельный saved metric baseline не обязателен.

### Noise control

- Документировать чувствительность к mass reformat.
- Желательно уметь быстро отключать файлы, где вся разница вызвана formatter-only commit.
- Не учитывать пустые строки и pure comment lines.

### Конкретный план в текущем коде

1. Не писать ещё один comment-stripper:
   использовать shared `text_scan.cpp`, который уже нужен #093/#095.
2. Добавить `include/archcheck/scan/indentation_metrics.h` + `src/scan/indentation_metrics.cpp` с чистой file-level функцией:
   вход — очищенный текст;
   выход — `IndentationMetrics { maxIndent, avgIndent, variance, deepLineCount }`.
3. Full snapshot можно считать по `collectNonVendoredSources()`, но практический приоритет — compare current vs base ref по changed files.
4. Для diff path не нужен hunk parser:
   достаточно `git::changedCppFiles()` из [src/git/git_state.cpp](~/projects/cpparch/src/git/git_state.cpp),
   затем old/new contents читать через `GitObjectFileSource` и `DiskFileSource`.
5. Baseline JSON v1 не расширять:
   текущий baseline contract хранит violations, а не текстовые метрики. Эта задача должна жить на ref comparison, а не на новом snapshot format.
6. Detector может возвращать file-level `Violation` с `line = 0`; искусственно выбирать "самую глубокую строку" не нужно.
7. Тесты:
   новый `tests/unit/scan/indentation_metrics_test.cpp`;
   1-2 compare сценария в [tests/integration/diff/git_diff_test.cpp](~/projects/cpparch/tests/integration/diff/git_diff_test.cpp), чтобы проверить работу через реальные refs.

## Fixtures & Tests

- `fixtures/indentation_complexity_drift/pass/shallow_baseline.cpp`
- `fixtures/indentation_complexity_drift/fail/deeper_current.cpp`
- `fixtures/indentation_complexity_drift/pass/comment_only_noise.cpp`
- `fixtures/indentation_complexity_drift/pass/tab_indentation.cpp`

Обязательные проверки:

- рост deep-indented lines детектится;
- comment-only шум не влияет;
- tabs считаются предсказуемо;
- result остаётся advisory/report only;
- formatter-only false-positive profile описан явно.

## Критерии готовности

- Без AST / libclang.
- Сравнение current vs base ref достаточно для v1.
- Метрика не претендует на точность CCN.
- По умолчанию не участвует в gate semantics.

## Не делать

- Подменять rule существующим `lizard`.
- Строить полноценный parser block structure.
- Считать это точной архитектурной метрикой.
- Включать default gate.

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Зафиксировать набор text metrics и thresholds для v1.
2. Реализовать line classifier и indent counter.
3. Подключить comparison against base ref.
4. Проверить реакцию на real-world reformat diff.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Drift relative to base ref, а не absolute complexity | Иначе check дублирует linter/metric tool |
| File-level v1 достаточно | Это самый дешёвый и объяснимый старт |
| Advisory/report only | Высокая чувствительность к formatting noise |
| Separate from graph metrics | Сигнал текстовый, не graph-derived |

## Планируемые файлы

| Область | Изменение |
|---------|-----------|
| `src/scan/` или `src/cheap_drift/` | Indentation metrics collector |
| `src/main.cpp` / diff comparison | Current vs base ref orchestration |
| `tests/unit/` | Метрики и шумоподавление |
| `tests/integration/` | Ref-comparison сценарии |
| `fixtures/indentation_complexity_drift/` | `pass/` и `fail/` фикстуры |
