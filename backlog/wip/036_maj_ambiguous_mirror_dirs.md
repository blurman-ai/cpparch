# [SCAN][CLI] Ambiguous из зеркальных директорий: single_include/, _amalgamated

**Дата создания:** 2026-05-28
**Дата старта:** —
**Статус:** new
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

- [ ] `src/scan/include_resolver.cpp`: при ambiguous — отдавать предпочтение пути без well-known mirror dirs
- [ ] Список well-known dirs: `single_include`, `amalgamate`, `amalgamated`, `dist`, `release/include`, `generated`
- [ ] Проверить: nlohmann/json → 0 ambiguous
- [ ] Проверить: fmt, spdlog, Catch2 — нет регрессий
- [ ] Тест: два кандидата, один в `single_include/` → выбирается второй, не ambiguous

## Сделано

- (пусто)

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `src/scan/include_resolver.cpp` | mirror-dir heuristic |
| `tests/unit/scan/include_resolver_test.cpp` | тест mirror-prefer |
