# [GRAPH][RULES] Дешёвые граф-чеки: fan-out god-component + связность/blast/scc-size в отчёт

**Дата создания:** 2026-05-31
**Дата старта:** —
**Статус:** new
**Модуль:** GRAPH / RULES / REPORT
**Приоритет:** major
**Сложность:** S (по пунктам; каждый — отдельный маленький коммит)
**Блокирует:** —
**Заблокирован:** —
**Related:** #029 (metric_regression_detection — diff-дельты этих же метрик), #037 (godheader_structural_threshold — порог fan-in 50), #028 (rules_engine_mvp), #054 (ai_repo_duplication_run — эмпирика, откуда взялись эти сигналы)

## Цель

Добавить набор архитектурных проверок/метрик, которые **почти бесплатны** — число
уже вычисляется граф-движком (или достаётся одной строкой из уже построенного
графа), не хватает только «провода» до правила/отчёта. Никакого нового backend'а,
никакого libclang, никакого git — всё на уже работающем fast-include-графе.

## Контекст

Корпусный прогон #054 (85 реп, обе метрики по истории) показал, какие граф-сигналы
реально различают дрейф — и **два из них archcheck считает, но не выставляет как
чек/метрику**:

- **§7.3 «тихий дрейф» — связность `edges/nodes`.** stellar-core 1.9→6.3,
  opentelemetry 2.1→5.7, FastLED 1.3→4.8 — **ноль циклов, низкий копипаст, но
  средняя связность утроилась**. Сейчас этот сигнал нигде не виден: счётчик рёбер
  есть (`GraphBuildResult.edges`, [graph_builder.cpp:27](../../src/graph/graph_builder.cpp#L27)),
  но `edges/nodes` не печатается и не входит в `GraphMetrics`.
- **§7.2 размер клубка.** acts `hpp↔ipp` — это 2-узловые циклы (косметика),
  FastLED — один SCC из 47 компонент (настоящий клубок). `largest_scc` уже
  считается (`compute_scc_stats`, [main.cpp:271](../../src/main.cpp#L271)) и
  печатается в `--graph`, но SF.9 в обычном отчёте про размер молчит.
- **§9 приёмы дедупа** — копипаст вдоль осей вариативности (платформы, бэкенды)
  рождает компоненты, тянущие десятки сиблингов = **высокий fan-out**. Ловим
  перегруз *входа* (god-header, fan-in), но не перегруз *выхода*.

Что уже есть и **не дублируем**:
- `largest_scc` **рост** между ревизиями → **DRIFT.2** (shipped, spec:332
  «размер существующего SCC растёт»).
- diff-дельты NCCD / chain length / новые god-headers → **#029**.
- CCD/ACD/NCCD, god-header (fan-in, порог 50), chain length → shipped.

Spec уже цитирует Lakos «минимизация **fan-in/fan-out**» (spec:191) и
«In-degree / **Out-degree** для каждого компонента» (spec:185) — то есть fan-out
авторитетом покрыт, просто не реализован. Это снимает риск «правило-мнение»
(CLAUDE.md: authority over opinion).

## Скоуп

### A. Новое правило (absolute, single-snapshot) — главный пункт

- [ ] **`Lakos.GodComponentFanOut`** — компонент с out-degree (числом прямых
      `#include` на компоненты проекта) выше порога. Зеркало `LakosGodHeaders`:
      та же форма, `predecessors()` → `successors()`. Один файл-правило
      (`src/rules/lakos_god_component.cpp` + хедер), регистрация в `rule_set.cpp`,
      **не трогая существующие правила** (OCP).
  - Атрибуция: Lakos, «minimize fan-out» (тот же источник, что god-header).
  - Порог: НЕ копировать 50 вслепую — у fan-out другое распределение (`.cpp` с
    30 инклюдами — норма). Откалибровать на корпусе `_aidev_run/` (он уже скачан,
    см. #054). До калибровки — высокий дефолт / report-only, чтобы не дать «5000
    нарушений на первом прогоне» (CLAUDE.md).
  - Fixtures (обязательны): `fixtures/god_component_fanout/pass/` +
    `fixtures/god_component_fanout/fail_fanout/`.

### B. Новые числа в отчёте (report-only, не pass/fail) — почти даром

- [ ] **Средняя связность `edges/nodes`** — добавить `edgeCount` + `avgCoupling`
      в `GraphMetrics` ([algorithms.h:25](../../include/archcheck/graph/algorithms.h#L25));
      `edgeCount` уже посчитан при сборке графа. Печатать рядом с CCD/ACD/NCCD.
- [ ] **Max blast radius** — `max_n |reachableFrom(n)|`. Уже вычисляется в
      CCD-цикле ([algorithms.cpp:273](../../src/graph/algorithms.cpp#L273)) и
      **выбрасывается** (суммируется в CCD). Удержать максимум — ~3 строки. Это
      абсолютная версия планируемого DRIFT.6 (blast_radius_growth).
- [ ] **SF.9: размер клубка в сообщении.** При репорте цикла дописать размер SCC
      (сколько компонент в клубке). Делает находку actionable: «цикл из 2» vs
      «клубок из 47». Правка только в формировании сообщения SF.9, не в логике.

### C. Drift-версии — НЕ здесь (вынесено осознанно)

Гейты «связность не должна расти» / «blast radius не должен расти» — это метрик-
регрессия, территория **#029** (diff/baseline). После реализации B добавить
`avgCoupling` и `maxBlastRadius` в `GraphMetrics`-дельту #029 одной строкой каждый
(там уже есть nccdDelta). В этой задаче — только абсолютные числа.

### D. Доки

- [ ] **spec §Lakos**: дописать `edges/nodes` (avg coupling) и max blast radius в
      список метрик; god-component (fan-out) — в список правил рядом с god-header.
- [ ] **Решение по нормировке связности** зафиксировать в spec (см. Ключевые
      решения): нормируем `edges/nodes` (структурно), НЕ per-KLOC. Это
      единственное, чего в доках сейчас нет.
- [ ] ROADMAP/CHANGELOG — по завершении (не на создании), как требует
      `backlog/README.md`.

## План выполнения

1. [ ] B-метрики (`edgeCount`/`avgCoupling`/`maxBlastRadius` в `GraphMetrics` +
       печать) — самый дешёвый, разблокирует и отчёт, и будущий #029-гейт.
2. [ ] SF.9 message enrichment (размер SCC).
3. [ ] `Lakos.GodComponentFanOut` + fixtures + unit-тест + регистрация.
4. [ ] Калибровка порога fan-out на `_aidev_run/`.
5. [ ] spec-правки (A/B + решение по нормировке).
6. [ ] (после) дописать `avgCoupling`/`maxBlastRadius` дельты в #029.

## Критерий приёмки

- [ ] `Lakos.GodComponentFanOut` есть, проходит fixtures pass/fail, зарегистрирован,
      существующие правила не тронуты (OCP).
- [ ] `--graph` и обычный отчёт показывают `edges/nodes` и max blast radius.
- [ ] SF.9 в сообщении указывает размер клубка.
- [ ] Порог fan-out обоснован числами с корпуса, а не взят с потолка.
- [ ] Dogfooding: archcheck на самом себе остаётся зелёным (CLAUDE.md).
- [ ] Spec отражает новые метрики/правило + решение по per-KLOC.
- [ ] Каждый пункт — отдельный коммит ≤50 строк, ≤2 новых файла (code_quality.md).

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Связность нормируем `edges/nodes`, НЕ per-KLOC | per-KLOC мешает объём кода в структурную метрику (раздул LOC без рёбер → «улучшил» per-KLOC). `edges/nodes` чисто структурна; именно она дала сигнал в #054 §7.3. per-KLOC потребовал бы счётчика LOC, которого в графе нет |
| fan-out god-component — отдельное правило, не расширение god-header | OCP: одно правило = один класс = один файл; god-header (fan-in, бутылочное горло) и god-component (fan-out, переусложнённый потребитель) — разные дефекты |
| Порог fan-out калибруем на корпусе, по умолчанию консервативный | Избегаем «5000 нарушений на первом прогоне»; `.cpp` с 30 инклюдами — норма |
| blast radius / coupling — сначала report-only | абсолютные пороги требуют настройки; сперва показать число, гейт-версия (рост) — в #029/DRIFT |
| largest_scc growth НЕ берём | уже DRIFT.2 (shipped) |

## Сделано

- (пусто)

## Pointers

- Эмпирика сигналов: [experiments/ai_repo_run/DRIFT_RUN_REPORT.md](../../experiments/ai_repo_run/DRIFT_RUN_REPORT.md) §7.2/7.3/9
- Граф-метрики: [src/graph/algorithms.cpp](../../src/graph/algorithms.cpp) (`computeGraphMetrics`), [include/archcheck/graph/algorithms.h](../../include/archcheck/graph/algorithms.h)
- God-header (образец для зеркала): [src/rules/lakos_god_headers.cpp](../../src/rules/lakos_god_headers.cpp)
- `--graph` вывод (уже печатает largest_scc/edges): [src/main.cpp:258](../../src/main.cpp#L258)
- Diff-дельты метрик (куда уйдут C-гейты): #029, [src/diff/regression_report.cpp](../../src/diff/regression_report.cpp)
