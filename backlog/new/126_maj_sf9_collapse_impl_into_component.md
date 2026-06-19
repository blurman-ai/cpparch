# [RULES][SF.9] схлопывать header+impl в один Lakos-компонент до детекции циклов

**Дата создания:** 2026-06-14
**Статус:** new
**Модуль:** RULES / GRAPH
**Приоритет:** major
**Сложность:** unknown
**Блокирует:** demo/контрибьюшн на header-only C++ (#1/#3)
**Заблокирован:** —
**Related:** #088 (inline-split FP-фильтр — этот тикет его обобщает), quick-fix `_impl.hpp` маркеры (эта сессия)
**Верификация:** #131 (корпус-прогон: mlpack 16→~0, PCL 206→~0, реальные циклы держатся)

## Цель

SF.9 должен считать `foo.hpp` + `foo_impl.hpp` (и `.inl`/`.ipp`/…) **одним
компонентом** (определение компонента у Лакоса: .h + его реализация с тем же
stem). Сейчас они — два узла графа, и их include-петля горит как цикл.

## Контекст (как нашли)

2026-06-14, dogfooding на **mlpack** (header-only template-библиотека из
goodfirstissue). Сырой прогон: **259 «actionable» SF.9-циклов**. Разбор поштучно:
**248 из 259 (96%) — идиома `X.hpp → X_impl.hpp → X.hpp`**, не дефект
(`foo.hpp` в конце делает `#include "foo_impl.hpp"`, тот включает `foo.hpp` ради
объявлений).

**Quick-fix этой сессии** (`sf9_no_cycles.cpp`): добавил `_impl.hpp`/`_impl.h`/
`_impl.hh`/`_impl.hxx` в `kImplMarkers`. Итог: **259 → 16**. Тесты 528/528,
dogfood archcheck = 0.

**Почему 16 осталось и нужен этот тикет:** фильтр `isInlineSplitScc` срабатывает
только на **строгом 2-узловом SCC** (`scc.size() != 2 → не фильтруем`). Часть
`_impl`-пар сидит в **большом SCC** и не схлопывается. Подтверждено:
`core/util/backtrace.hpp ↔ backtrace_impl.hpp` горит, потому что
`backtrace_impl.hpp` тянет ещё `log.hpp`, а `log_impl.hpp` — обратно `backtrace.hpp`
→ клубок backtrace+log из 4 узлов. Внутри него пары идиоматичны, но 2-узловой
спецкейс их не видит.

## ⚠️ Важное расширение из dogfood (PCL, 2026-06-14): impl в ПОДКАТАЛОГЕ `impl/`

Прогон archcheck по **PointCloudLibrary/pcl** дал **206 SF.9-циклов, ~все — idiom**
`dir/X.h ↔ dir/impl/X.hpp` (`common/centroid.h ↔ common/impl/centroid.hpp`,
`2d/edge.h ↔ 2d/impl/edge.hpp`). Захваченная выборка 60/206 = 100% этот паттерн.

Мой quick-fix (коммит `2690a34`) их НЕ ловит по двум причинам:
1. `isInlineSplitScc` требует `dirName(a)==dirName(b)` → `common` ≠ `common/impl`;
2. маркер у меня `_impl.hpp` (суффикс), а у PCL — `.hpp` с тем же stem, лежащий в
   `impl/`-подкаталоге (нет суффикса).

PCL-конвенция `include/.../X.h` + `include/.../impl/X.hpp` ОЧЕНЬ распространена
(Eigen `detail/`, Boost, многие template-либы). Это, вероятно, БОЛЬШЕ кейсов, чем
same-dir `_impl.hpp`. Компонентная склейка ОБЯЗАНА это покрывать.

**Определение компонента уточняется:** `A` и `B` — один компонент, если совпадает
stem (без extension и без impl-суффикса) И они в одном каталоге ЛИБО один в
`impl/`-/`detail/`-подкаталоге другого. Т.е. canonical-путь = `dir без хвостового
/impl|/detail` + stem.

## Суть фикции (эскиз — не решение)

Перейти от band-aid «спецкейс 2-узлового SCC» к **компонентной модели**:
- Перед детекцией циклов **слить** узлы с одинаковым canonical-ключом (dir-без-
  /impl|/detail + stem, один — impl-маркер ИЛИ в impl/detail-подкаталоге) в один
  логический компонент (Лакос). Рёбра внутри компонента выкидываются,
  межкомпонентные — остаются.
- Тогда `backtrace`-клубок схлопнется до `backtrace`-компонент ↔ `log`-компонент;
  если между ними реальный цикл — он *должен* гореть (это уже честно), а
  внутрипарные `_impl` рёбра исчезнут.
- Аккуратно: stem-склейка не должна слить разные компоненты (`foo.hpp` и
  `foo_bar.hpp` — разные; маркер должен быть суффиксом-реализацией, а не префиксом).

## Остаток 16 на mlpack (для приёмки) — разобрать каждый

После quick-fix остались (классифицировать при реализации, не скопом):
- `_impl` в большом SCC: `backtrace`, `gan`, `rbm`, `rp_tree_max_split`,
  `r_tree_split` → должны уйти после компонентной склейки.
- forwarding-umbrella: `binary_space_tree.hpp → binary_space_tree/binary_space_tree.hpp`,
  `cover_tree`, `spill_tree` → отдельный класс (umbrella ↔ same-name subdir header).
- deprecated-split: `load.hpp ↔ load_deprecated.hpp`, `save ↔ save_deprecated`.
- typedef back-edge: `neighbor_search.hpp ↔ typedef.hpp`, `ra_search ↔ ra_typedef`.
- umbrella back-edge (возможно реальный smell): `core.hpp → data.hpp →
  one_hot_encoding.hpp → core.hpp` — leaf включает зонтик. Решить: FP или TP.

## Проверка (фикстуры обязательны)

- [ ] `fixtures/sf9_no_cycles/pass_impl_in_larger_scc/` — две header+impl-пары,
      связанные крест-накрест (как backtrace+log) → 0 (главный кейс тикета)
- [ ] негатив: реальный межкомпонентный цикл A↔B (каждый — header+impl) → горит
- [ ] regression: `pass_impl_hpp_split` (quick-fix) и `fail_simple` без изменений
- [ ] mlpack: sf9_cycles ощутимо < 16 (разобрать, что осталось и почему)

## Самопроверка

«16 осталось» ≠ «16 реальных циклов». Каждый из остатка разобрать поштучно
(вход→ожидание), как разобрали 259. Идиома в большом SCC выглядит как цикл, но им
не является.
