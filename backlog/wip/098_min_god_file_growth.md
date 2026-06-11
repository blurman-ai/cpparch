# [CHEAP-DRIFT][HISTORY] God-File Growth

**Дата создания:** 2026-06-10
**Дата старта:** 2026-06-11
**Статус:** wip
**Модуль:** GIT / HISTORY / REPORT
**Приоритет:** minor
**Сложность:** medium
**Блокирует:** —
**Заблокирован:** —
**Related:** #048 (drift_clean_checkout_methodology), #075 (trusted_diff_workflow), #097 (test_co_evolution)

## Цель

Находить файлы, которые по истории коммитов устойчиво надуваются и становятся кандидатами в будущий god file.

Это report-only аналитика. Не путать с уже shipped `Lakos.GodHeader`, который измеряет fan-in заголовков в include graph.

## Контекст

- Источник постановки: `docs/codex_archcheck_cheap_drift_tasks.md`, Task 6.
- Название похоже на `GodHeader`, но смысл другой: здесь речь не о графовом хабе, а о файле-аккумуляторе по размеру и истории изменений.
- Для сигнала нужен `git log --numstat` плюс текущий LOC. Значит, задача лежит ближе к history/report слою, чем к обычным `IRule`.
- По позиции продукта это вероятностная аналитика. Default gate здесь противопоказан.

## План выполнения

### Detection contract

- Файл подозрителен, если одновременно выполнены условия:
  - уже достаточно большой;
  - за окно истории заметно вырос;
  - рос в нескольких подряд touching commits;
  - meaningful shrink не наблюдался.
- Конкретные дефолты v1 (алгоритм по `git log --numstat`, окно = последние
  `N=200` коммитов или 12 месяцев, что меньше):
  - «уже большой»: current LOC ≥ P75 по production-файлам репо (адаптивный порог,
    не абсолют — как пороги Arcan по частотному анализу проекта);
  - «заметно вырос»: net growth за окно ≥ +30% ИЛИ ≥ +300 строк;
  - «рос подряд»: ≥ 5 последних touching-коммитов с `added > deleted`;
  - «без shrink»: ни одного коммита с `deleted - added >= 50` в окне;
  - все четыре условия — И; редкость по построению, иначе поднять пороги.
- Итоговый finding:
  - `SIZE.1.god_file_growth`
  - `file = <path>`
  - `line = 0`
  - message с current LOC, net growth и числом growth touches.

### Runtime shape

- Делать один repository-wide проход по `git log --numstat`, а не отдельный вызов на каждый файл.
- Текущий LOC брать из current tree.
- Vendor/generated/test code исключать через уже существующую классификацию, где это применимо.
- Если история слишком дорогая на больших репо, задача должна зафиксировать лимиты окна и graceful degradation.

### Scope discipline

- Первый проход — только report/advisory analytics.
- Без line-level blame.
- Без ownership / bus-factor / SZZ.
- Без попытки вывести "истинную архитектурную причину" роста файла.

### Конкретный план в текущем коде

1. Историю не читать через N вызовов `git log` на файл:
   нужен один shared parser `include/archcheck/git/history_query.h` + `src/git/history_query.cpp` поверх вынесенного `git_exec.cpp`.
2. Формат запроса зафиксировать сразу:
   `git log --numstat --format=%H%x1f%s` с лимитом по числу коммитов.
   Этого достаточно и для #098, и для #100.
3. Current LOC считать один раз по текущему дереву:
   `DiskFileSource` + `collectNonVendoredSources()` дают уже очищенный от vendor/test список, по которому можно построить `path -> currentLoc`.
4. Сам сигнал держать отдельным file-level detector-ом, который агрегирует историю по path и на выходе даёт `Violation { ruleId, file, line=0, message }`.
5. Не пытаться впихнуть это в `RegressionReport`:
   история не связана с current structural diff model, а `RegressionReport` сейчас заточен под graph-vs-graph.
6. В `src/main.cpp` это лучше вешать как отдельный report path рядом с existing diff/duplication helpers.
   Если задачка реализуется раньше общего history CLI, допустим временный text-only subpath, но не через `IRule`.
7. Интеграционные git-сценарии сначала можно держать в [tests/integration/diff/git_diff_test.cpp](/home/localadm/projects/cpparch/tests/integration/diff/git_diff_test.cpp), потому что там уже есть `TempDir`, `initRepo`, `commitAll`.
   Вынос общего test helper делать только если history suite реально разрастётся.

## Fixtures & Tests

- `fixtures/god_file_growth/fail/steady_growth.log`
- `fixtures/god_file_growth/pass/growth_then_shrink.log`
- `fixtures/god_file_growth/pass/vendor_ignored.log`
- `fixtures/god_file_growth/pass/small_file.log`

Обязательные проверки:

- steady growth over history даёт finding;
- рост с последующим meaningful shrink не даёт false positive;
- vendor/generated/test файлы исключаются;
- message показывает window/growth summary;
- правило не влияет на exit code по умолчанию.

## Критерии готовности

- Report-only или advisory-only, но не gate.
- Ясно разведено по терминологии с `Lakos.GodHeader`.
- Нет N shell-вызовов `git log` на каждый файл.
- Есть synthetic fixtures на parser истории.

## Не делать

- SZZ.
- Blame/ownership map.
- AI scoring "насколько файл опасный".
- Попытку навешивать этот сигнал на default CI fail.

## Сделано

- `include/archcheck/git/history_query.h` + `src/git/history_query.cpp`: queryCommitHistory + parseHistoryOutput (single git log pass)
- `include/archcheck/scan/god_file_growth.h` + `src/scan/god_file_growth.cpp`: GodFileGrowthDetector с 4 условиями
- CLI: `archcheck --history <path>` (advisory-only, всегда return 0)
- Tests: 5 unit-тестов parseHistoryOutput + 1 end-to-end GodFileGrowthDetector + интеграционный тест
- Все 402 тестов green, archcheck self-check clean, форматирование OK
- Found real god-file: src/config/config_loader.cpp (413 LOC, +160 net, 5 growth commits)

## Следующие шаги

1. Спроектировать history parser на одном проходе `git log --numstat`.
2. Зафиксировать thresholds и окно истории для первого прохода.
3. Добавить fixtures steady-growth vs grow-then-shrink.
4. Проверить стоимость на среднем OSS-репо.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Repository-wide history pass | Иначе правило станет слишком дорогим |
| Разводить с `Lakos.GodHeader` и по id, и по docs | Это два разных класса сигнала |
| Только report/advisory | Метрика вероятностная, не gate-grade |
| `line = 0` | Это file-level history signal без точной строки |

## Планируемые файлы

| Область | Изменение |
|---------|-----------|
| `src/git/` или history analytics | Parser `git log --numstat` |
| `src/main.cpp` / optional report path | Подключение report-only сигнала |
| `tests/unit/` | History aggregation |
| `tests/integration/` | Стоимостные и end-to-end сценарии |
| `fixtures/god_file_growth/` | Synthetic history fixtures |
