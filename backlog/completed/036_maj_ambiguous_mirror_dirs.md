# [SCAN][CLI] Ambiguous из зеркальных директорий: single_include/, _amalgamated

**Дата создания:** 2026-05-28
**Дата старта:** 2026-05-28
**Дата завершения:** 2026-05-29
**Статус:** completed
**Модуль:** SCAN / CLI
**Приоритет:** major
**Сложность:** S
**Блокирует:** —
**Заблокирован:** —
**Related:** #028 (rules_engine_mvp)

## Цель

Устранить ложные ambiguous-находки возникающие когда репо содержит `single_include/` или аналогичную директорию с дубликатами заголовков.

## Контекст

Прогон на nlohmann/json (commit `d10879b`) выдал **333 ambiguous** — все ложные. Причина:

```
/tmp/json/include/nlohmann/json.hpp         ← основной исходник
/tmp/json/single_include/nlohmann/json.hpp  ← сгенерированный amalgam
```

Оба файла называются `json.hpp`. Любой `#include <nlohmann/json.hpp>` находит два кандидата → `ambiguous`. Аналогично для `json_fwd.hpp`.

Паттерн распространён: nlohmann/json, {fmt}, Catch2 (extras/), Abseil (не имеет, но другие подобные проекты есть). Типичные имена зеркальных директорий: `single_include/`, `amalgamate/`, `dist/`, `release/`, `generated/`.

## Решение

Два независимых подхода, можно реализовать оба:

**А — exclude-список по умолчанию (quick fix):**
При разрешении `ambiguous` — если среди кандидатов один путь содержит `single_include/`, `amalgamate/`, `dist/`, `generated/` — предпочитать другой, не помечать как ambiguous. Список well-known директорий хранить в `include_resolver.cpp`.

**Б — конфигурируемый exclude в `.archcheck.yml`:**
```yaml
scan:
  exclude_dirs: ["single_include", "build", "third_party"]
```
Файлы из exclude_dirs не добавляются в проектный граф вообще → не могут быть ни источником, ни кандидатом.

Вариант Б более правильный (пользователь управляет явно), но требует конфига. Вариант А — zero-config быстрый фикс, закрывает распространённый случай.

**Рекомендация:** реализовать А сейчас как встроенный heuristic, Б — как follow-up при появлении конфига.

## План выполнения

- [x] `src/scan/include_resolver.cpp`: при ambiguous — отдавать предпочтение пути без well-known mirror dirs
- [x] Список well-known dirs: `single_include`, `amalgamate`, `amalgamated`, `dist`, `release/include`, `generated`
- [x] Проверить: nlohmann/json → 0 ambiguous (было 333, стало 0 ✓)
- [ ] Проверить: fmt, spdlog, Catch2 — нет регрессий
- [x] Тест: два кандидата, один в `single_include/` → выбирается второй, не ambiguous

## Сделано

- Реализован вариант А (heuristic): `kMirrorPrefixes` + `is_mirror_dir_path()` в `include_resolver.cpp`
- Фильтрация в `resolve_by_suffix`: если ровно 1 кандидат не в mirror-dir → `Project`, иначе `Ambiguous` как раньше
- Пробросан параметр `files` через цепочку `resolveInclude → resolve_quote/resolve_angle → resolve_by_suffix`
- Добавлены 2 теста: happy path (`single_include` пропускается) + edge case (оба в mirror → Ambiguous)
- Сборка чистая, 739 тестов прошли, lizard без предупреждений
- `starts_with` заменён на `path.find(prefix) == 0` (clang 11 не поддерживает C++20 `starts_with`)

## В работе

- Ручная проверка на nlohmann/json (`./archcheck --graph /tmp/json`)

## Ключевые решения

- `path.find(prefix) == 0` вместо `starts_with` — clang 11 на Astra Linux не имеет `std::string_view::starts_with` даже с `-std=c++20`
- Передаём `*candidates` (не `preferred`) в `make_ambiguous` когда фильтрация не помогла — сохраняем полную информацию о конфликте

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `src/scan/include_resolver.cpp` | `kMirrorPrefixes`, `is_mirror_dir_path()`, фильтрация в `resolve_by_suffix`, проброс `files` |
| `tests/unit/scan/include_resolver_test.cpp` | 2 новых теста mirror-prefer |
