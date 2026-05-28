# [BUILD/TESTS] Поднять пороги coverage gate до 90% по всем метрикам

**Дата создания:** 2026-05-28
**Дата старта:** 2026-05-28
**Статус:** blocked (toolchain constraint — ждёт смены окружения)
**Модуль:** BUILD / TESTS
**Приоритет:** major
**Блокирует:** —
**Заблокирован:** lcov 1.13 + GCC 8.3 (Astra Linux 1.7) — см. `docs/dev/coverage_constraints.md`
**Related:** —

## Цель

Поднять пороги в `scripts/check_coverage.sh` с `70/60/40` до `90/90/90`
и обеспечить стабильное прохождение gate на этих порогах.

## Текущее состояние (2026-05-28)

| Метрика   | Было (старт задачи) | Сейчас | Цель | Разрыв |
|-----------|---------------------|--------|------|--------|
| Lines     | 70%                 | ~96%   | 90%  | ✓ (порог поднят до 90%) |
| Functions | 60%                 | ~95%   | 90%  | ✓ (порог поднят до 90%) |
| Branches  | 40%                 | ~63%   | 90%  | **−27 п.п. — заблокировано** |

Lines и Functions: **частично выполнено** — пороги подняты до 90%, реальные значения выше.
Branches: **заблокировано** — достичь 90% на текущем toolchain невозможно, порог оставлен на 40%.

## Почему branches заблокировано

GCC 8.x + lcov 1.13 (Astra Linux 1.7): каждый вызов потенциально бросающей функции
порождает throw arc — ветвь «вернулся нормально» + «бросил исключение». Throw arcs
физически непокрываемы без симуляции OOM.

**lcov 1.13** — нет `--exclude-unreachable-branches` (появилось в 1.15).
**lcov 2.4** с `--filter exception` — проверено, на данных GCC 8.x **эффекта нет**:
GCC 8 не размечает throw-arcs в формате, который lcov 2.4 распознаёт.
Установлены `libcapture-tiny-perl`, `libdatetime-perl` — Perl-зависимости работают,
но сам фильтр не срабатывает на GCC 8 gcov data.

Подробности: `docs/dev/coverage_constraints.md`.

## Точный срез по файлам (lcov --list, 2026-05-28)

| Файл | Branches% | Всего веток | Непокрыто |
|------|-----------|-------------|-----------|
| `regression_report.h` | 25.0% | 24 | 18 |
| `scan/disk_file_source.cpp` | 50.0% | 8 | 4 |
| `report/violation_baseline.cpp` | 53.3% | 240 | 112 |
| `graph/graph_builder.cpp` | 54.1% | 37 | 17 |
| `rules/sf9_no_cycles.cpp` | 55.9% | 68 | 30 |
| `git/git_object_file_source.cpp` | 57.2% | 138 | 59 |
| `rules/lakos_chain_length.cpp` | 57.7% | 26 | 11 |
| `rules/lakos_god_headers.cpp` | 57.7% | 26 | 11 |
| `git/git_state.cpp` | 58.6% | 232 | 96 |
| `graph/baseline.cpp` | 59.6% | 280 | 113 |
| `graph/diff.cpp` | 69.3% | 140 | 43 |
| `graph/algorithms.cpp` | 70.1% | 154 | 46 |
| `scan/include_scanner.cpp` | 76.1% | 184 | 44 |

## Сделано

- `MIN_LINES=90`, `MIN_FUNCTIONS=90` подняты в `scripts/check_coverage.sh` — проходят
- `MIN_BRANCHES=40` оставлен — выше не поднять без смены toolchain
- `LCOV_EXCL_START/STOP` вокруг post-fork child-process кода:
  - `src/git/git_state.cpp` — execChild + LCOV_EXCL_BR_LINE на drainFd/collectChild
  - `src/git/git_object_file_source.cpp` — execGitChild, execCatFileBatch, runGitOneShot, spawnCatFileBatch
- `LCOV_EXCL_BR_LINE` на ostream-вызовы в `src/diff/regression_report.cpp`
- Новые тесты `tests/unit/diff/regression_report_test.cpp` — +5: writeAdded, writeGrown, writeEmpty, metric regressions
- Новые тесты `tests/unit/graph/baseline_test.cpp` — +6: MalformedSchema errors, empty graph
- Фикс бага в `src/graph/baseline.cpp`: пустой граф → `nodes: []` вместо YAML null
- `docs/dev/coverage_constraints.md` — новый файл, документирует проблему и что пробовалось
- `CLAUDE.md` — ссылка на coverage_constraints.md

## Разблокируется при

- Переход на Ubuntu 22.04+ / Debian Bookworm → lcov 1.16+: добавить `--exclude-unreachable-branches`
- Или `pip3 install gcovr` + переписать `check_coverage.sh` под gcovr (`--exclude-throw-branches`)

## Ключевые решения

| Решение | Причина |
|---------|---------|
| LCOV_EXCL_START/STOP только для fork-child | Единственное законное место: gcov не видит дочерний процесс |
| LCOV_EXCL_BR_LINE только для явных throw-arc строк | Не трогать строки с логическими ветвями |
| Branches порог НЕ поднят | toolchain-ограничение, не обходится без смены gcc/lcov |
| lcov 2.4 эксперимент провален | `--filter exception` на GCC 8 gcov data не работает |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `scripts/check_coverage.sh` | MIN_LINES/FUNCTIONS → 90 (branches оставлен на 40) |
| `src/git/git_state.cpp` | LCOV_EXCL_START/STOP + LCOV_EXCL_BR_LINE |
| `src/git/git_object_file_source.cpp` | LCOV_EXCL_START/STOP + LCOV_EXCL_BR_LINE |
| `src/diff/regression_report.cpp` | LCOV_EXCL_BR_LINE на STL-вызовы |
| `src/graph/baseline.cpp` | фикс write_nodes_block/write_edges_block для пустого графа |
| `tests/unit/diff/regression_report_test.cpp` | +5 новых тестов |
| `tests/unit/graph/baseline_test.cpp` | +6 новых тестов |
| `docs/dev/coverage_constraints.md` | новый файл — объяснение branches-ограничения |
| `CLAUDE.md` | ссылка на coverage_constraints.md |
