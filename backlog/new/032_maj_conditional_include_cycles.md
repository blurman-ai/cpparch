# [SCAN][RULES] Conditional includes: не репортить циклы из #ifdef-охраняемых рёбер

**Дата создания:** 2026-05-28
**Дата старта:** —
**Статус:** new
**Модуль:** SCAN / RULES
**Приоритет:** major
**Сложность:** M
**Блокирует:** —
**Заблокирован:** —
**Related:** #028 (rules_engine_mvp), #031 (godheader_pch_exclusion)

## Цель

Сканер должен помечать `#include`-рёбра внутри `#ifdef`/`#if`-блоков как условные, чтобы SF.9 не репортил циклы, существующие только в одном из режимов сборки.

## Контекст

Прогон на spdlog (commit `2e71fdf`) выдал 22 нарушения SF.9. Все 22 — пары `foo-inl.h ↔ foo.h`, охраняемые макросом `SPDLOG_HEADER_ONLY`:

```cpp
// foo.h, последние строки:
#ifdef SPDLOG_HEADER_ONLY
#include "foo-inl.h"
#endif
```

```cpp
// foo-inl.h, первая строка:
#include <spdlog/foo.h>
```

В compiled-режиме (дефолт) этого include нет — цикла нет. В header-only режиме он есть намеренно: это классический dual-mode паттерн C++ библиотек (spdlog, Abseil, part of Boost).

Текстовый сканер `#ifdef` не раскрывает → все 22 ребра попадают в граф как обычные → SF.9 репортит правомерно по букве, но ложно по смыслу.

**Это типичный false positive на реальных OSS-проектах.** Без исправления tool будет встречать эти нарушения в spdlog, Abseil и других известных библиотеках, что подрывает доверие к результатам.

## Решение (выбранный подход)

Пометить условные рёбра в графе флагом `conditional: true`.  
SF.9 (и другие правила на циклах) игнорируют циклы, все рёбра которых условны.

### Шаги реализации

- [ ] В `IncludeToken` / ребре графа добавить поле `conditional: bool`
- [ ] В `include_scanner` отслеживать глубину `#ifdef`/`#if`/`#ifndef` — любой include внутри помечать `conditional = true`
- [ ] В `DependencyGraph` хранить флаг на ребре (или отдельный `conditional_edges` set)
- [ ] В SF.9 (`cycle_rule`): при обходе SCC пропускать циклы, где все рёбра `conditional`
- [ ] Fixtures: `fixtures/sf9/pass_conditional_ifdef/` — пара `foo.h` + `foo-inl.h` через `#ifdef`
- [ ] Тест: убедиться, что spdlog выдаёт 0 SF.9 нарушений после правки

## Альтернативы (отклонены)

**Вариант 1 — exclude-паттерн в конфиге** (`exclude_cycles: ["**/*-inl.h"]`):
- Проще реализовать; пользователь управляет явно.
- Минус: требует конфига — ломает zero-config; не защищает от тех же паттернов с другим именованием.

**Вариант 3 — built-in эвристика `-inl.h`**:
- Если `foo.h` включает `foo-inl.h` и наоборот — считать known pattern и не репортить.
- Минус: hardcoded эвристика, скрывает реальные случайные циклы с `-inl.h` в имени; хрупко.

## Сделано

- **2026-05-28** — Ручная верификация паттерна на spdlog commit `2e71fdf`. Проверены исходники: `logger.h:385`, `common.h:373`, `base_sink.h:50`, `registry.h:130` и др. Все 22 пары подтверждены — каждый `.h` включает `*-inl.h` строго внутри `#ifdef SPDLOG_HEADER_ONLY`. Ложные срабатывания воспроизводимы, механика понята.

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `include/archcheck/scan/include_token.h` | поле `conditional` |
| `src/scan/include_scanner.cpp` | отслеживание `#ifdef`-глубины |
| `include/archcheck/graph/dependency_graph.h` | conditional-флаг на ребре |
| `src/rules/sf9_no_cycles.cpp` | пропуск all-conditional циклов |
| `fixtures/sf9/pass_conditional_ifdef/` | новая фикстура |
| `tests/unit/rules/sf9_test.cpp` | тест conditional-цикла |
