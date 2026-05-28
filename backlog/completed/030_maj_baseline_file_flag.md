# [CLI][REPORT] Флаг --baseline <file> для check-режима

**Дата создания:** 2026-05-28
**Дата старта:** 2026-05-28
**Дата завершения:** 2026-05-28
**Статус:** completed
**Модуль:** CLI / REPORT
**Приоритет:** major
**Сложность:** S (несколько часов)
**Блокирует:** —
**Заблокирован:** —
**Related:** #016 (graph_baseline_contract — другой смысл, граф), #029 (metric_regression_detection — метрики в diff-режиме)

## Цель

Добавить `--baseline <file>` и `--save-baseline <file>` в check-режим, чтобы legacy-проекты
могли принять archcheck в CI без rewriting: первый запуск фиксирует нарушения, последующие
репортят только новые.

## Контекст

Сейчас `archcheck [path]` выдаёт все нарушения сразу. На legacy-проекте (LLVM, Qt, и т.п.)
это может быть 200+ violations — overwhelming, команда не знает с чего начать и откладывает
инструмент в сторону. Это классический барьер adoption для архитектурных чекеров.

Референсы:
- **ArchUnit** `FreezingArchRule` — сохраняет known violations в файл, CI падает только на новых.
- **clang-tidy** `--config` с `Checks: -<rule>` — аналогичная идея, другой UX.
- **ESLint** `--no-error-on-unmatched-pattern` + `.eslintignore`.

`--diff main..HEAD` частично решает задачу (показывает только новые рёбра), но требует
git-контекста и не работает для `archcheck [path]` без git-репозитория.

Пример workflow для legacy-проекта:
```bash
# Один раз: зафиксировать текущее состояние
archcheck --save-baseline .archcheck.baseline /path/to/project

# В CI (и локально): только новые нарушения
archcheck --baseline .archcheck.baseline /path/to/project
# exit 0 пока команда не добавляет новых нарушений
# exit 1 если появилось что-то новое относительно baseline
```

## Формат baseline-файла

Простой JSON, схема v1:
```json
{
  "version": 1,
  "violations": [
    {"rule": "SF.9", "file": "src/foo.h", "line": 0, "message": "cycle: a → b → a"},
    {"rule": "SF.7", "file": "include/bar.h", "line": 12, "message": "using namespace std"}
  ]
}
```

Идентификатор нарушения для matching: `(rule, file, line)`. `message` — информационно.
Два нарушения считаются одинаковыми если совпадают все три поля.

## План выполнения

### Структура данных

- [ ] Добавить `ViolationBaseline` в `include/archcheck/report/violation_baseline.h`:
  ```cpp
  struct ViolationBaseline {
     std::vector<Violation> known;
  };
  ViolationBaseline loadBaseline(const std::filesystem::path&);  // throws on parse error
  void saveBaseline(const ViolationBaseline&, const std::filesystem::path&);
  ViolationList filterNew(const ViolationList& all, const ViolationBaseline& baseline);
  ```

### CLI

- [ ] Добавить в `print_help()` строки:
  ```
  archcheck --save-baseline <file> [path]   (save current violations as baseline)
  archcheck --baseline <file> [path]        (report only new violations vs baseline)
  ```
- [ ] Добавить в `dispatch()` обработку `--save-baseline` и `--baseline`.
- [ ] `run_check` принять `std::optional<std::filesystem::path> baselinePath`:
  - если `--save-baseline`: запустить check, сохранить все нарушения в файл, exit 0.
  - если `--baseline`: загрузить baseline, запустить check, отфильтровать, репортить только новые.
  - Ошибка загрузки baseline → exit 2 + сообщение в stderr.

### Отчёт

- [ ] В текстовом отчёте при `--baseline` добавить строку-суммари:
  ```
  suppressed: 42 known violations (run without --baseline to see all)
  ```
  — если есть подавленные нарушения.

### Unit-тесты (`tests/unit/report/violation_baseline_test.cpp`)

- [ ] `saveBaseline + loadBaseline` — round-trip идемпотентен
- [ ] `filterNew` — нарушение из baseline не попадает в вывод
- [ ] `filterNew` — нарушение с другим `line` считается новым
- [ ] `filterNew` — нарушение с другим `rule` считается новым
- [ ] `loadBaseline` на несуществующем файле → exception с понятным message
- [ ] `loadBaseline` на невалидном JSON → exception

### Интеграционный тест (`tests/integration/cli/` или в `main_test.cpp`)

- [ ] `--save-baseline` создаёт файл, `--baseline` возвращает exit 0 на том же наборе
- [ ] `--baseline`: добавленное нарушение → exit 1, новое нарушение в выводе
- [ ] `--save-baseline` на чистом коде → пустой массив violations в JSON

## Сделано

- `include/archcheck/report/violation_baseline.h` — `ViolationBaseline`, `BaselineError`, `saveBaseline`, `loadBaseline`, `filterNew`
- `src/report/violation_baseline.cpp` — реализация: JSON save/load с ручным escape/unescape, `filterNew` через `set<tuple<rule,file,line>>`
- `src/main.cpp` — `--save-baseline <file>` и `--baseline <file>` в CLI; вынесены `trySaveBaseline`, `tryLoadAndFilter`, `applyBaselineAndReport`, `dispatch_baseline`, `dispatch_format` (попутно сделал dispatch чище)
- `tests/unit/report/violation_baseline_test.cpp` — 9 тестов: round-trip, special chars, filterNew варианты, missing file, invalid content
- 191/191 тестов зелёные, lizard чист

## Как работает

**Формат файла.** Тот же JSON что выдаёт `--format json`, но без `summary`:
```json
{ "version": 1, "violations": [
    {"rule": "SF.7", "file": "include/a.h", "line": 12, "message": "using namespace std"},
    {"rule": "SF.9", "file": "src/b.h", "line": 0, "message": "cycle: b → c → b"}
] }
```

**save_baseline.** Запускает полный check, сериализует все нарушения, всегда exit 0.

**load + filter.** Загружает файл, строит `set<(rule, file, line)>`, фильтрует текущие нарушения — в вывод попадают только те, которых нет в set. Подавленные нарушения показываются строкой `suppressed: N known violation(s)`.

**Matching key** — тройка `(ruleId, file, line)`. Message не участвует в сравнении — стабильнее при рефакторе сообщений.

**Chем управляется.** CLI флаги `--save-baseline <file>` и `--baseline <file>`. Формат файла — JSON v1 (рядом с `--format json` output, но без summary-секции).

**С чем связана.** `archcheck::report` — рядом с `text_reporter` и `json_reporter`. `ViolationBaseline` не знает про граф (`graph/baseline.cpp` — другой концепт).

**Диагностика.** При ошибке загрузки (`file not found`, `not a valid archcheck baseline`) — exit 2 + сообщение в stderr. Нарушение на конкретной строке файла включает номер строки в сообщение.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Matching по `(rule, file, line)`, не по `message` | Message может меняться при рефакторе правила; triple стабильнее |
| `--save-baseline` всегда exit 0 | Первый запуск фиксирует baseline, не падает CI |
| Suppressed count в отчёте | Пользователь видит что baseline активен и сколько нарушений скрыто |
| JSON, не custom format | Легко читать/редактировать руками; nlohmann или вручную (проект не тянет nlohmann — делать вручную, как baseline.cpp делает YAML вручную) |
| Отдельный модуль `report/violation_baseline` | Не смешивать с graph-baseline (#016); разные концепции с одинаковым словом в названии |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `include/archcheck/report/violation_baseline.h` | new |
| `src/report/violation_baseline.cpp` | new |
| `src/main.cpp` | + `--baseline`, `--save-baseline` в dispatch + print_help |
| `src/CMakeLists.txt` | + `violation_baseline.cpp` |
| `tests/unit/report/violation_baseline_test.cpp` | new |
| `tests/CMakeLists.txt` | + новый тест |
