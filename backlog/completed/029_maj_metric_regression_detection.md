# [GRAPH][DIFF] Metric regression detection in RegressionReport

**Дата создания:** 2026-05-28
**Дата старта:** 2026-05-28
**Статус:** completed
**Модуль:** GRAPH / DIFF
**Приоритет:** major
**Сложность:** medium
**Блокирует:** —
**Заблокирован:** —
**Related:** #009 (ai_drift_regression_rules), #027 (coverage_90_percent)

## Цель

Расширить `RegressionReport` метрическими регрессиями (рост max chain length, появление новых god-headers,
дельты CCD/ACD/NCCD) и добавить тесты, которые обнаруживают эти регрессии через git-сравнение двух ревизий.

## Контекст

Сейчас `RegressionReport` умеет только сравнивать рёбра и SCC-циклы (`addedEdges`, `removedEdges`,
`grownCycles`). В MVP заявлены три дополнительные категории нарушений:

- **Lakos chain length** — порог по умолчанию 10; при его превышении или росте между ревизиями — регрессия.
- **God-headers (fan-in)** — порог по умолчанию 30; узлы с fan-in выше порога, которых не было в baseline —
  регрессия.
- **CCD/ACD/NCCD** — сводные метрики Lakos; рост ACD или NCCD между ревизиями — регрессия.

`computeIncludeDepths` (chain depth) уже есть в `algorithms.h`; fan-in и CCD/ACD/NCCD нужно добавить.

Интеграционные тесты через реальные git-репозитории уже работают (см. `tests/integration/diff/git_diff_test.cpp`);
новые сценарии дописываются туда же.

### Стратегия тестовых git-репозиториев

Исследование паттернов из libgit2, gitoxide, go-git показало три подхода:

1. **Программное создание (наш подход)** — `mkdtemp` + `git init/commit` через `std::system`. Уже применяется
   в `git_diff_test.cpp`. Достаточно для всех тестов #029: каждый сценарий — 2 коммита (before/after),
   создаётся в `TempDir` и удаляется после теста. Ключевые предосторожности: фиксировать `user.email/name`,
   отключать `commit.gpgsign`.

2. **Pre-built bare repo в `tests/resources/`** (libgit2 style) — бинарный `.git`-каталог коммитится
   в репо. Быстро, но непрозрачно и сложно менять. **Не используем.**

3. **Fixture-скрипт + `.bundle` файл** (gitoxide style) — bash-скрипт создаёт сложную историю,
   результат упаковывается в `git bundle create`. Скрипт и бандл коммитятся; тест клонирует бандл.
   **Резервный вариант:** применить, если в будущем понадобится сканирование N последних коммитов
   (feature "scan git log"), где программное создание 50+ коммитов станет слишком медленным.
   Разместить в `tests/fixtures/bundles/`.

**Вывод для #029:** программный подход, патерн `TempDir` + `initRepo` + `commitAll` уже готов.

## План выполнения

### Алгоритмы (`graph/algorithms`)

- [ ] Добавить `computeFanIn(const DependencyGraph&) → vector<size_t>` (обратная степень каждого узла).
- [ ] Добавить `computeCCD(const DependencyGraph&) → size_t`
      (∑ |reachableFrom(n)| + 1 для всех n; +1 считает сам узел).
- [ ] Добавить `computeGraphMetrics(const DependencyGraph&) → GraphMetrics` — агрегирует
      maxChainLength, maxFanIn, CCD, ACD (`double = CCD / N`), NCCD (`double = ACD / log2(N+1)`).

### Структуры (`diff/regression_report.h`)

- [ ] Добавить `struct MetricDelta { size_t baseline; size_t current; }` (для целых метрик).
- [ ] Расширить `RegressionReport`:
  ```cpp
  std::optional<MetricDelta>  chainLengthGrown;   // set only when current.max > baseline.max
  std::vector<std::string>    newGodHeaders;       // nodes crossing fan-in threshold (default 30)
  std::optional<double>       nccdDelta;           // set when NCCD grew
  ```
- [ ] Обновить `hasRegression()`: добавить проверку `chainLengthGrown`, `!newGodHeaders.empty()`,
      `nccdDelta > 0`.

### Реализация (`diff/regression_report.cpp`)

- [ ] Принять `MetricThresholds { size_t chainLengthLimit = 10; size_t godHeaderFanIn = 30; }` вторым
      аргументом `buildRegressionReport` (со значениями по умолчанию — zero-config остаётся).
- [ ] Вычислять `GraphMetrics` для baseline и current; заполнять новые поля.

### Текстовый отчёт (`report/writeTextReport`)

- [ ] Добавить строки в вывод:
  ```
  chain_length:   <N>  [+<delta>]     (только если выросло)
  god_headers:    <K>  new: h1, h2    (только при наличии)
  nccd:           <F>  [+<delta>]     (только если выросло)
  ```

### Unit-тесты (`tests/unit/diff/regression_report_test.cpp`)

- [ ] `chain length grew → chainLengthGrown set, hasRegression() true`
- [ ] `chain length did not grow → chainLengthGrown nullopt`
- [ ] `new god-header added → newGodHeaders non-empty, hasRegression() true`
- [ ] `NCCD grew → nccdDelta > 0, hasRegression() true`
- [ ] `writeTextReport emits chain_length / god_headers / nccd lines only when grown`

### Интеграционные тесты (`tests/integration/diff/git_diff_test.cpp`)

- [ ] `git: PR adds deep include chain → chainLengthGrown detected`
  Сценарий: baseline = `a→b→c` (depth 2); PR добавляет `b→d→e→f→...` (depth > baseline max).
- [ ] `git: PR creates god-header → newGodHeaders non-empty`
  Сценарий: baseline = граф без высокого fan-in; PR добавляет 31+ входящих рёбер в один узел.
- [ ] `git: PR increases NCCD → nccdDelta detected`
  Сценарий: baseline = разреженный граф; PR уплотняет зависимости, NCCD растёт.
- [ ] `git (memory variant): chain length regression detected via GitObjectFileSource`
  Повторяет первый сценарий через in-memory путь (без worktree checkout).

### Fixtures (опционально, если понадобятся отдельные fixture-проекты)

- [ ] `fixtures/chain_length/pass/` — цепочка в пределах порога
- [ ] `fixtures/chain_length/fail_deep/` — цепочка превышает порог
- [ ] `fixtures/god_header/pass/`
- [ ] `fixtures/god_header/fail_fanin/`

## Сделано

- **2026-05-28** — весь план реализован коммитом `c480e39` (тем же коммитом, что создал этот файл; секция «Сделано» тогда не была заполнена — файл задачи ошибочно остался в `new/` со статусом `new` до бэклог-ревью 2026-06-11):
  - `computeFanIn` O(E) + `GraphMetrics` (maxChainLength / maxFanIn / CCD / ACD / NCCD) + `computeGraphMetrics` в `graph/algorithms`;
  - `MetricDelta`, `MetricThresholds` (chainLengthLimit=10, godHeaderFanIn=30), поля `chainLengthGrown` / `newGodHeaders` / `nccdDelta` в `RegressionReport`, `hasRegression()` расширен;
  - `writeTextReport` выводит chain_length / god_headers / nccd только при регрессии;
  - unit-тесты (+160 строк `regression_report_test.cpp`) и 4 git-интеграционных теста (+93 строки `git_diff_test.cpp`), coverage gate пройден (lines 95.9%).

## Следующие шаги

- Закрыто. Опциональные fixture-проекты (`fixtures/chain_length/`, `fixtures/god_header/`) не создавались — сценарии покрыты программными git-тестами, отдельные фикстуры не понадобились.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| `MetricThresholds` как аргумент со значениями по умолчанию | Zero-config остаётся, но пороги можно переопределить из YAML-конфига позже |
| Отдельная `computeGraphMetrics` вместо прямых вызовов | Избегает двойного вычисления depths и fan-in в `buildRegressionReport` |
| `newGodHeaders` — список имён, не NodeId | Имена нужны для текстового отчёта; NodeId — деталь реализации |
| `nccdDelta` — `optional<double>`, не bool | Позволяет показать числовое изменение в отчёте |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `include/archcheck/graph/algorithms.h` | + `computeFanIn`, `GraphMetrics`, `computeGraphMetrics` |
| `src/graph/algorithms.cpp` | Реализация новых функций |
| `include/archcheck/diff/regression_report.h` | + `MetricDelta`, `MetricThresholds`, новые поля в `RegressionReport` |
| `src/diff/regression_report.cpp` | Заполнение новых полей |
| `src/report/text_reporter.cpp` | Новые строки вывода |
| `tests/unit/diff/regression_report_test.cpp` | +5 unit-тестов |
| `tests/integration/diff/git_diff_test.cpp` | +4 git-интеграционных теста |

## Как работает

`buildRegressionReport` считает `computeGraphMetrics` для baseline и current графов и заполняет три
опциональных поля: `chainLengthGrown` (рост max include depth), `newGodHeaders` (узлы, впервые
пересёкшие fan-in порог 30), `nccdDelta` (рост Normalized ACD — ловит уплотнение графа без единого
большого ребра). Пороги — `MetricThresholds` со значениями по умолчанию, zero-config сохранён.

## Итог

**Статус:** completed
**Дата завершения:** 2026-05-28 (фактическая, коммит `c480e39`); файл закрыт 2026-06-11 по итогам бэклог-ревью.
**Причина задержки закрытия:** реализация и файл задачи попали в один коммит, чекбоксы и статус не были обновлены — задача 2 недели выглядела неначатой. Дублей работы не возникло, но #057 и TASK_TRACKER ссылались на неё как на несделанную.
