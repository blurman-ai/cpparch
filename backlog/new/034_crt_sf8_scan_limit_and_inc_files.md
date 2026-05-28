# [RULES] SF.8: kScanLines слишком мал + .inc файлы не должны проверяться

**Дата создания:** 2026-05-28
**Дата старта:** —
**Статус:** new
**Модуль:** RULES
**Приоритет:** critical
**Сложность:** S
**Блокирует:** —
**Заблокирован:** —
**Related:** #028 (rules_engine_mvp)

## Цель

Устранить два бага SF.8, превращающих весь abseil-cpp (85 заголовков) в ложные срабатывания.

## Контекст

Прогон на abseil-cpp (commit `e7a10c8`) выдал 85 нарушений SF.8 — все ложные. Два независимых бага:

### Баг 1 — kScanLines = 30 слишком мал

Abseil использует Apache 2.0 лицензионный заголовок перед include guard:

```cpp
// Copyright 2017 The Abseil Authors.
// Licensed under the Apache License, Version 2.0 ...
// ... 16 строк лицензии + пустые строки ...
// (итого 47 непустых строк до #ifndef)
#ifndef ABSL_BASE_CONFIG_H_    ← строка 48, вне лимита!
#define ABSL_BASE_CONFIG_H_
```

Наш лимит `kScanLines = 30` в `sf8_include_guard.cpp`. Guard реально есть — мы его не видим. Любой проект с Apache/MIT copyright (большинство OSS) имеет аналогичную проблему.

**Фикс:** поднять `kScanLines` до 60 (покрывает все реальные copyright блоки, не замедляет сканирование заметно).

### Баг 2 — .inc файлы проверяются как заголовки

`src/scan/project_files.cpp` строка 20: `".inc"` входит в список header-расширений и проверяется SF.8. Но `.inc` файлы (например `spinlock_linux.inc`, `spinlock_posix.inc` в abseil) — это платформенные фрагменты без guard по дизайну: они подключаются ровно один раз через `#if PLATFORM ... #include "spinlock_linux.inc" #endif`. Guard в них противоречил бы назначению.

**Фикс:** убрать `.inc` из списка расширений, проверяемых SF.8 (оставить в `isHeaderFile` для сканирования include-графа — они участвуют в нём правомерно).

## План выполнения

- [ ] `sf8_include_guard.cpp`: поднять `kScanLines` с 30 до 60
- [ ] `sf8_include_guard.cpp`: добавить проверку — пропускать файлы с расширением `.inc`
- [ ] Убедиться: прогон на abseil → 0 SF.8 ложных срабатываний
- [ ] Убедиться: прогон на Catch2, fmt, spdlog не регрессирует
- [ ] Unit-тест: заголовок с Apache 2.0 copyright (>30 непустых строк) + корректным guard → pass

## Сделано

- (пусто)

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `src/rules/sf8_include_guard.cpp` | kScanLines → 60, skip .inc |
| `tests/unit/rules/sf8_test.cpp` | тест long-copyright guard |
