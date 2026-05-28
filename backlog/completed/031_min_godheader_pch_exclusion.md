# [RULES] GodHeader: исключать precompiled headers из проверки fan-in

**Дата создания:** 2026-05-28
**Дата старта:** 2026-05-28
**Дата завершения:** 2026-05-28
**Статус:** completed
**Модуль:** RULES
**Приоритет:** minor
**Сложность:** small
**Блокирует:** —
**Заблокирован:** —
**Related:** —

## Цель

Перестать выдавать ложный позитив `Lakos.GodHeader` на precompiled header файлах (`pch.h`, `stdafx.h` и т.п.), которые по определению включаются во все единицы трансляции.

## Контекст

При прогоне archcheck на реальных проектах (`gm/rmi`, `samastra_itain`) правило `Lakos.GodHeader` срабатывало на `pch.h` / `rmi/pch.h` с fan-in 60–111. Это ложный позитив: precompiled header намеренно включается в каждый `.cpp`.

## Как работает

В `LakosGodHeaders::check()` перед подсчётом fan-in вызывается `isPchName(path)` — статический метод, который берёт `filename()` через `std::filesystem::path` и сверяет со статическим `unordered_set` из четырёх хардкодных имён. Если совпало — узел пропускается. Дополнительно в конструктор передаётся `extraExcludes_` (для нестандартных PCH-имён, будущая конфиг-интеграция).

## Сделано

- Добавлен статический метод `LakosGodHeaders::isPchName()` с хардкодом: `pch.h`, `stdafx.h`, `precompiled.h`, `precompiled_header.h`
- Добавлен параметр `extraExcludes` в конструктор (для пользовательских исключений без YAML-конфига)
- Фильтрация по `extraExcludes_` — берётся `filename()`, не полный путь (PCH может лежать в любом подкаталоге)
- Fixture `fixtures/lakos_god_headers/pass/pch_excluded/`: `pch.h` + 31 `user*.h` с `#include "pch.h"` (fan-in = 31 > threshold 30)
- Два новых тест-кейса: все 4 PCH-имени, плюс `extraExcludes` с путём `sub/my_pch.h`
- 193/193 тестов зелёных (`build+tests`)

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Хардкод 4 имён в статическом `unordered_set` | Покрывает 95% реальных проектов без конфига |
| Фильтр по `filename()`, не по пути | PCH может лежать в любом подкаталоге дерева |
| `extraExcludes` в конструкторе, не в YAML | YAML-конфиг-инфраструктура ещё не существует; параметр уже тестируем и заготовлен под будущую интеграцию |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `include/archcheck/rules/lakos_god_headers.h` | + `isPchName()`, + `extraExcludes_`, + `<unordered_set>` |
| `src/rules/lakos_god_headers.cpp` | реализация фильтрации PCH |
| `tests/unit/rules/lakos_god_headers_test.cpp` | +2 теста (known PCH names, extra excludes) |
| `fixtures/lakos_god_headers/pass/pch_excluded/` | 32 новых fixture-файла |
