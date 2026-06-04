# [DIFF][GRAPH] Zero-config drift signal: новые межзональные зависимости в `--diff`

**Дата создания:** 2026-06-02
**Дата старта:** 2026-06-02
**Дата завершения:** 2026-06-02
**Статус:** done
**Модуль:** DIFF / GRAPH / REPORT
**Приоритет:** major
**Related:** #018 (git_diff_analysis), #009 (drift_regression_rules), #057 (lakos_fanout_coupling_checks), #075 (mvp_v1_trusted_diff_workflow)

## Цель

Добавить в основной diff path zero-config сигнал:

> появился новый межзональный канал зависимости `A -> B`, которого в baseline не было.

Это уровень выше обычного `addedEdges`: не “новый include между файлами”, а
“появилась новая связь между крупными областями проекта”.

## Контекст

До задачи `archcheck` уже умел:

- новые file-level рёбра;
- новые/выросшие циклы;
- рост chain length / NCCD / появление god-header.

Но в коде не было сигнала на вопрос:

> “возникла ли новая связь между двумя областями проекта, которые раньше не зависели друг от друга?”

При этом в исследовательском `graph_probe.py` уже существовала близкая идея
`[MODULE]` (`directory = module`) как snapshot-эвристика. Значит сигнал полезен,
но жил мимо продуктового diff/report core.

## Как работает

В `RegressionReport` добавлено поле:

- `newCrossAreaDependencies`

Каждый элемент хранит:

- `fromArea`
- `toArea`
- `edgeCount` — сколько file-level рёбер образуют этот новый канал в current
- `sampleFrom` / `sampleTo` — конкретный пример для ручной проверки

### Эвристика области (`area`)

Это **zero-config area**, а не config-defined module:

- если в объединении `baseline + current` несколько top-level директорий, область = первый сегмент пути (`src`, `tests`, `plugins`);
- если весь проект живёт под одним общим top-level, область = первые два сегмента (`src/core`, `src/net`) — иначе всё схлопнулось бы в один `src`.

Классификатор строится **по объединению путей baseline и current**, чтобы новая
top-level директория не меняла гранулярность между снимками и не порождала ложные
"новые" пары.

### Семантика

- если в baseline уже существовал любой канал `A -> B`, новые file-level рёбра
  внутри этой пары не считаются новым area-signal;
- если в current впервые появился любой канал `A -> B`, репортится один drift-hit
  на пару областей;
- внутризональные рёбра (`A -> A`) игнорируются.

## Вывод в отчёт

Сигнал включён в обычный `buildRegressionReport()` и текстовый diff-отчёт.

Новые поля отчёта:

- `new_area_deps: N`
- секция `new_cross_area_dependencies:`

Дополнительных CLI-флагов не требуется.

## Проверено

Без запуска сборки/тестов в этой сессии добавлены:

- unit-тесты в `tests/unit/diff/regression_report_test.cpp`
- integration-тест в `tests/integration/diff/git_diff_test.cpp`

Покрыты случаи:

- первая связь `tests -> src` репортится один раз на пару областей;
- рост уже существующей пары не считается “новой” связью;
- текстовый отчёт печатает новую секцию;
- git-based diff ловит новый межзональный канал на temp-repo сценарии.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Делать zero-config **areas**, а не ждать runtime `modules` | нужен полезный сигнал уже в `v0.1` diff-core |
| Классификатор строить на объединении `baseline + current` | новая top-level директория не должна ломать сравнение |
| Один hit на пару областей, а не N hits на file-level рёбра | меньше шума, сигнал остаётся архитектурным |
| Сохранять `sampleFrom/sampleTo` | finding должен открываться и проверяться руками |
| Не смешивать это с `layers/forbidden/independence` | настоящий policy layer остаётся задачей `v0.2` |

## Как работает

- [include/archcheck/diff/regression_report.h](../../include/archcheck/diff/regression_report.h)
  — `NewCrossAreaDependency` и новое поле в `RegressionReport`
- [src/diff/regression_report.cpp](../../src/diff/regression_report.cpp)
  — area-classifier, агрегация cross-area рёбер, детекция новых пар и текстовый вывод
- [tests/unit/diff/regression_report_test.cpp](../../tests/unit/diff/regression_report_test.cpp)
  — unit coverage сигнала
- [tests/integration/diff/git_diff_test.cpp](../../tests/integration/diff/git_diff_test.cpp)
  — git-based integration scenario

## Чем управляется

- обычным `archcheck --diff <revspec> [path]`
- zero-config path heuristic, без `.archcheck.yml`

## С чем связана

- `graph::addedEdges()` / `graph::grownSccs()` — уже существующий diff core
- `graph_probe.py` — исследовательский предшественник идеи
- будущий config-policy layer (`layers` / `forbidden` / `independence`) — точный successor этого эвристического сигнала

## Диагностика

Если сигнал выглядит шумным:

1. проверить, как именно эвристика нарезала области по путям;
2. открыть `sampleFrom -> sampleTo` и убедиться, что include реальный;
3. помнить, что `include/` + `src/` одной библиотеки может выглядеть как новая area-pair связь в zero-config режиме;
4. для точного boundary enforcement переходить на config-defined modules в `v0.2`.
