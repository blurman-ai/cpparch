# archcheck — STATUS

_Обновлено: 2026-05-29 · commit `cc5cca9` · фаза: **v0.1 (близко к завершению)**_

## Состояние одним абзацем

archcheck сегодня сканирует C++ проекты **без `compile_commands.json`**, строит include-граф (узлы / рёбра / SCC через iterative Tarjan), проверяет правила SF.7 / SF.8 / SF.9, GodHeader, ChainLength и DRIFT.1 / DRIFT.2, поддерживает baseline-режим (`--baseline` / `--save-baseline`). Работа подтверждена девятью dogfood-прогонами от 28 до 4493 файлов (0.1–4.9 s wall clock). v0.1 почти закрыт — открыта только SF.21 (рассмотреть, нужно ли в MVP), плюс один невыясненный mismatch между графом и SF.9 на grpc.

## Работает end-to-end

- `archcheck --scan <path>` — discovery файлов + резолюция include в `Project / External / Unresolved / Ambiguous`.
- `archcheck --graph <path>` — include-граф (nodes / edges / sccs_cyclic / largest_scc) + Tarjan.
- `archcheck <path>` — прогон default rules с отчётом `file:line:column`.
- `archcheck --save-baseline <file>` и `archcheck --baseline <file>` — снимок и регрессионная проверка (#030).
- DRIFT.1 (no_shortcut_edge) + DRIFT.2 (no_cycle_growth) — детектят ухудшение архитектуры между прогонами (#009 #040).
- CI: build matrix (gcc, clang-18 + libc++), static analysis (cppcheck + lizard как гейты, clang-tidy strict как warning-only), coverage (lines / functions ≥ 90 %, branches ≥ 40 %), sticky PR-comments (#025).

## Чек-лист v0.1

Из `docs/architecture-spec.md` § «MVP v0.1»:

- ✅ **SF.7** (`using namespace` в global scope) — [src/rules/sf7_using_namespace.cpp](../src/rules/sf7_using_namespace.cpp); brace-depth tracking + block-comment stripping (#035 #038).
- ✅ **SF.8** (include guard / `#pragma once`) — [src/rules/sf8_include_guard.cpp](../src/rules/sf8_include_guard.cpp); `kScanLines = 60`, `.inc`/ObjC skip (#034).
- ⬜ **SF.21** — не реализован, задачи в backlog нет. Решить, тянем ли в v0.1.
- ✅ **SF.9 / cycles** — [src/rules/sf9_no_cycles.cpp](../src/rules/sf9_no_cycles.cpp); conditional-cycle suppression — продолжение в #032.
- ✅ **Include chain length** (default 10) — [src/rules/lakos_chain_length.cpp](../src/rules/lakos_chain_length.cpp).
- ✅ **God-headers** (default fan-in 50) — [src/rules/lakos_god_headers.cpp](../src/rules/lakos_god_headers.cpp); PCH-exclusion (#031), порог поднят 30 → 50 (#037).
- ✅ **CCD / ACD / NCCD** — вычисляются в [include/archcheck/graph/algorithms.h](../include/archcheck/graph/algorithms.h); NCCD-delta в `RegressionReport`.
- ✅ **`--baseline` режим** — `--baseline` + `--save-baseline` (#030).

## Открытые баги корректности

- **grpc: `sccs_cyclic = 1` в `--graph`, но SF.9 рапортует 0 нарушений.** Граф нашёл цикл (предположительно в `third_party/upb` между `.c`/`.h`), правило молчит. Задача в backlog не заведена — нужно открыть перед закрытием v0.1. См. [docs/milestones.md](milestones.md) §«Прогон 9 — grpc» (строки 425–428).

## Известный шум

Таблица B1–B7 в [docs/milestones.md](milestones.md) §«Известный шум — пропускаем». Закрыто: B1 / B2 / B3 / B5 / B6 / B7 (#034 #035 #037 #038). Открыто и тянет на v0.1+: #032 (conditional cycles в `-inl.h`-парах). НЕ копировать таблицу сюда.

## Сейчас в работе / следующие 1–3 шага

- `backlog/wip/` **пуст** — активной задачи нет.
- Кандидаты из `new/`: #032 (conditional cycles, закрывает класс false-cycle на spdlog/folly), #041 (аудит захардкоженных строк, гигиена перед v0.1).
- Перед закрытием v0.1: открыть задачу под grpc-mismatch (см. секцию выше) и принять решение по SF.21.

## Указатели

- [docs/milestones.md](milestones.md) — лог боевых прогонов, source of truth для «что измерено».
- [backlog/](../backlog/) — очередь задач, source of truth для «что в работе».
- [docs/architecture-spec.md](architecture-spec.md) — дизайн и роадмап (v0.1 → v0.5).
- [CHANGELOG.md](../CHANGELOG.md) — публичные изменения к релизу; **обновлять только при релизе / заметной фиче**.
- [docs/MVP.md](MVP.md) — критерии приёмки MVP.

## Как обновлять этот файл

1. После задачи, которая меняет возможности / фиксит баг / закрывает пункт чек-листа / делает dogfood-прогон — обновить релевантные секции и шапку (`Обновлено`, `commit`, `фаза`).
2. Держать в пределах ~2 экранов. Ссылаться на milestones / backlog / spec, не дублировать.
3. В конце дня — `/status-review` сверит документ с git-реальностью и предложит патчи.
