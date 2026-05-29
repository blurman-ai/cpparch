# [SCAN][RULES] SF.9 молчит на проектах с `#ifndef`-guard'ом: include внутри include-guard ложно conditional

**Дата создания:** 2026-05-29
**Дата старта:** 2026-05-29
**Дата завершения:** 2026-05-29
**Статус:** completed
**Модуль:** SCAN / RULES
**Приоритет:** critical
**Сложность:** M
**Блокирует:** v0.1 release
**Заблокирован:** —
**Related:** #032 (conditional cycles suppression — источник регрессии), #028 (rules_engine_mvp)

## Цель

SF.9 должен репортить реальные циклы в проектах со стандартным include-guard `#ifndef X / #define X / ... / #endif`. Сейчас на любом таком проекте SF.9 даёт **0** даже при наличии цикла, потому что сканер считает include-guard условным контекстом и `allEdgesConditional` глушит SCC.

## Контекст

Перепрогон dogfood-корпуса 2026-05-29 (см. милистоны):

| Проект | Guard-стиль | `sccs_cyclic` (--graph) | SF.9 | Расхождение |
|---|---|---|---|---|
| fmt | `#pragma once` | 0 | 0 | — |
| spdlog | `#pragma once` | 22 | 0 | подавлено #032 корректно (все циклы `foo.h ↔ foo-inl.h` под `#ifdef SPDLOG_HEADER_ONLY`) |
| Catch2 | `#pragma once` | 0 | 0 | — |
| nlohmann/json | `#pragma once` | 0 | 0 | — |
| abseil-cpp | `#pragma once` | 0 | 0 | — |
| folly | `#pragma once` | 9 | 9 | — |
| **grpc** | **`#ifndef`** | **4** | **0** | **критично** |

В grpc 4 реальных SCC, среди них настоящий архитектурный цикл в самом grpc:

```
src/core/tsi/alts/handshaker/alts_handshaker_client.h ↔ alts_tsi_handshaker.h
```

Оба `#include` — на верхнем уровне файла, никакого `#ifdef` рядом нет, кроме включающего весь файл guard'а:

```cpp
// alts_handshaker_client.h
#ifndef GRPC_SRC_CORE_TSI_ALTS_HANDSHAKER_ALTS_HANDSHAKER_CLIENT_H
#define GRPC_SRC_CORE_TSI_ALTS_HANDSHAKER_ALTS_HANDSHAKER_CLIENT_H
...
#include "src/core/tsi/alts/handshaker/alts_tsi_handshaker.h"  // ← помечается conditional=YES
...
#endif
```

Диагностика через `/tmp/scc_dump` (одноразовый отладочный бинарь, исходник
`/tmp/scc_dump.cpp`) на grpc показала, что **все 4 SCC** содержат рёбра
`conditional=YES`. После этого `Sf9NoCycles::check` отбрасывает их через
`allEdgesConditional` и не репортит.

## Почему скрывалось

Все остальные проекты в корпусе используют `#pragma once` — в них никакого `#ifndef`-блока не открывается, и сканер не маркирует includes как conditional. grpc — первый проект в выборке с классическим guard-стилем, поэтому баг проявился только сейчас.

В существующих юнит-тестах SF.9 / scanner используются `#pragma once` либо упрощённые тестовые шапки → баг через них не отлавливается.

## Масштаб

Это **critical**: archcheck позиционируется как ловец циклов в CI. На любом проекте с `#ifndef`-guard'ом (большинство OSS C/C++ кода: grpc, LLVM, Linux-style, классические библиотеки) SF.9 сейчас бесполезен. До v0.1 release это надо закрыть.

## Решение

В `include_scanner`-е распознавать стандартный паттерн include-guard:
- первая не-комментарий / не-пустая директива в файле — `#ifndef X`
- сразу за ней `#define X`
- где X — un-shouty identifier (по соглашению `[A-Z_][A-Z0-9_]*`)

При обнаружении этого паттерна — не учитывать первый `#ifndef` в счётчике глубины `#if`, и зеркально игнорировать парный закрывающий `#endif` в конце файла. Тогда контент файла трактуется как «верхний уровень», и includes не помечаются conditional, кроме реально вложенных в `#ifdef`/`#if`.

Альтернатива: ослабить #032 (например, считать цикл all-conditional только если ВСЕ его include-edges под директивами `#ifdef X` или `#if defined(X)` с одинаковым X) — сложнее, и не покрывает кейс когда guard окружает реальный условный include.

## План

- [ ] Фикстура `fixtures/sf9_no_cycles/ifndef_guard_real_cycle/` — два `.h` файла с классическим `#ifndef` guard'ом, образующих цикл. Ожидаемое поведение: SF.9 репортит.
- [ ] Фикстура `fixtures/sf9_no_cycles/ifndef_guard_no_cycle/` — те же два файла, но циклического include нет. Ожидаемое поведение: SF.9 молчит, без false-positive.
- [ ] Unit-тест на сканер: `IncludeScanner` для файла с include-guard — все top-level includes должны иметь `conditional=false`.
- [ ] Реализовать распознавание include-guard в сканере (новая функция `detectIncludeGuard()` или подобная).
- [ ] Перепрогнать на grpc — должны проявиться 4 SF.9 нарушения (или сколько действительно есть после фильтрации не-наших циклов).
- [ ] Проверить регрессию на остальном корпусе (folly должен остаться 9, spdlog — 0, остальные — 0).
- [ ] Обновить milestones (§«Прогон 9 — grpc»: убрать «требует расследования», добавить ссылку на эту задачу и итог фикса).
- [ ] Обновить STATUS.md — убрать «один невыясненный mismatch» из шапки.

## Изменённые файлы

Всё в коммите `595bdf2`:

| Файл | Изменение |
|------|-----------|
| `src/scan/include_scanner.cpp` | `pp_argument()`, `is_shouty_ident()`, `is_blank_line()`, `next_significant_line()`, `detect_include_guard_offset()`; основной цикл скипает `++depth` для guard-открытия |
| `tests/unit/scan/include_scanner_test.cpp` | +5 тестов с тегом `[guard]` |
| `fixtures/sf9_no_cycles/fail_ifndef_guard_real_cycle/{alpha,beta}.h` | new — два `#ifndef`-guarded файла с циклом |
| `fixtures/sf9_no_cycles/pass_ifndef_guard_no_cycle/{alpha,beta}.h` | new — два guarded файла без цикла |
| `docs/milestones.md` | §«Прогон 9 — grpc»: помечен Resolved 2026-05-29 со сводкой |
| `docs/STATUS.md` | убран mismatch из шапки и из «Открытых багов» |

Тест SF.9-уровня (`tests/unit/rules/sf9_no_cycles_test.cpp`) не добавлялся: правило работает на `DependencyGraph`-уровне и уже покрыто (включая `allEdgesConditional`-кейс). Регрессия защищена сканер-тестами и fixture-парой.

## Сделано

- Фикстуры `fixtures/sf9_no_cycles/fail_ifndef_guard_real_cycle/` и `pass_ifndef_guard_no_cycle/`.
- 5 unit-тестов в [tests/unit/scan/include_scanner_test.cpp](../../tests/unit/scan/include_scanner_test.cpp) с тегом `[guard]`: положительный кейс, кейс с лидирующими комментариями, вложенный `#ifdef` внутри guard'а, два негативных (нет `#define` / mismatched ident).
- Реализованы `pp_argument()`, `is_shouty_ident()`, `detect_include_guard_offset()` в [src/scan/include_scanner.cpp](../../src/scan/include_scanner.cpp); основной цикл пропускает `++depth` на guard'овом `#ifndef`.
- ctest: **235/235 passed**, lizard --warnings_only: чисто.
- Перепрогон корпуса: **grpc SF.9 = 4** (включая реальный архитектурный цикл `alts_handshaker_client.h ↔ alts_tsi_handshaker.h`), folly = 9 (без изменений), spdlog = 0 (корректное подавление по #032), fmt/Catch2 = 0.
- Обновлён `docs/milestones.md` §«Прогон 9 — grpc» (Resolved 2026-05-29) и `docs/STATUS.md` (убран mismatch из шапки и из «Открытых багов»).

## Принцип работы

**Проблема.** `include_scanner` помечал `IncludeDirective.conditional = true` для всех директив внутри любого открытого `#if`/`#ifdef`/`#ifndef`-блока — включая классический include-guard `#ifndef X / #define X / ... / #endif`, который оборачивает весь файл. После #032 правило SF.9 фильтровало SCC через `allEdgesConditional`, и подавляло реальные циклы в любых проектах с такими guard'ами.

**Решение.** Перед основным проходом сканер один раз ищет «шапку файла»:

1. Первая не-пустая строка — `#ifndef X`, где `X` — ALL_CAPS identifier (`[A-Z_][A-Z0-9_]*`).
2. Сразу следующая не-пустая строка — `#define X` с тем же `X`.

Если шапка распознана, сохраняется offset строки с `#ifndef`. В основном цикле эта одна строка не инкрементирует `depth`. Парный закрывающий `#endif` тогда оказывается в позиции `depth == 0`, и существующая защита `depth > 0` его молча игнорирует — никакой отдельной логики на закрытие не требуется.

**Что не сломалось.** Comments и blank lines уже превращаются в whitespace в `preprocess()`, поэтому детекция «первая non-blank строка» автоматически пропускает лицензионные шапки. Вложенные `#ifdef OPT` внутри guard'а работают как раньше — депт там стартует с 0, инкрементируется на `#ifdef`, includes внутри получают `conditional=true`. `#pragma once`-файлы детектор отвергает на первом же шаге (первая директива не `#ifndef`), и поведение для них не меняется.

**Граница приложения.** Эвристика консервативна — требует ALL_CAPS ident и сразу следующий `#define`. Это сознательно: ловим стандартный паттерн (LLVM, grpc, Boost, классические библиотеки) и не пытаемся объять нестандартные guards с `#if !defined()` / lowercase / отложенным `#define`. Такие проекты можно добавлять в эвристику отдельно при необходимости.

## Как работает

<!-- TODO: канонизировать раздел из «Принцип работы» выше -->

## Чем управляется

<!-- TODO -->

## С чем связана

<!-- TODO -->

## Диагностика

<!-- TODO -->
