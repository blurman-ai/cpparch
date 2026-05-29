# [TEST][DRIFT] DRIFT.1 fixture: LibreSprite PR #581 shortcut edge

**Дата создания:** 2026-05-29
**Дата старта:** —
**Статус:** new
**Модуль:** TEST / DRIFT
**Приоритет:** major
**Сложность:** S (полдня)
**Блокирует:** публичное демо DRIFT.1 (README / HN-пост / `docs/research/ai_drift_cases.md`)
**Заблокирован:** —
**Related:** #009 (DRIFT.1/2 impl), #033 (AI-drift dataset), docs/milestones.md прогон 10 (2026-05-29)

## Цель

Завести минимальный воспроизводимый fixture
`fixtures/drift_real_world/libresprite_pr581/` под единственный реальный DRIFT.1
hit, верифицированный 2026-05-29: новое ребро
`app/ui/toolbar.cpp -> app/pref/preferences.h` в LibreSprite PR #581
(before `60eed0f` → after `276fdbd`, commit `0aa57ad`, Co-Authored-By: Claude
Opus 4.5).

## Контекст

Верификация (см. milestones.md, прогон 10, секция «Верификация») закрыла
основное возражение: include **отсутствовал** в `toolbar.cpp` на before-SHA
форка LibreSprite и был **добавлен** AI-коммитом 0aa57ad, не откатывался.
Upstream `aseprite/aseprite` это ребро держит — фрейминг для демо: «форк
дрейфом потерял зависимость, AI-коммит её вернул, archcheck зафиксировал
изменение графа этого репо».

Это первый и пока единственный CONFIRMED hit в DRIFT-корпусе (3 прогона:
1 hit / 2 clean), поэтому он же — headline-кейс для публичного материала.

## План выполнения

Per #033: fixture = **минимальный воспроизводимый slice графа**, не клон репо.

- [ ] Создать `fixtures/drift_real_world/libresprite_pr581/`
- [ ] `before/` — два input-файла, отражающих состояние на 60eed0f:
      `src/app/ui/toolbar.cpp` со списком его include-директив **без**
      `app/pref/preferences.h` + пустой stub `src/app/pref/preferences.h`
      (только для разрешения include-пути из других файлов слайса, если они
      нужны для DRIFT.1)
- [ ] `after/` — то же `toolbar.cpp` с добавленной строкой
      `#include "app/pref/preferences.h"` (как в коммите 0aa57ad)
- [ ] `README.md` в fixture-папке: 6–10 строк — SHA-пара, ссылка на PR,
      ссылка на milestones.md «Прогон 10», что должен поймать DRIFT.1
- [ ] Integration-тест: `--save-graph-baseline` на `before/`, потом
      `--drift-baseline … after/` — assert ровно одно DRIFT.1 hit с edge
      `toolbar.cpp -> preferences.h`, DRIFT.2 = 0
- [ ] Убедиться, что fixture compile-free (никаких `.cpp` тел, только
      include-строки + минимальные header-stubs) — DRIFT.1 работает на
      include-only backend, libclang не нужен

## Критерий приёмки

`ctest` зелёный, fixture-тест воспроизводит ровно тот hit, что записан в
milestones.md прогон 10. Папка fixture'а самодостаточна — не зависит от
sandbox-клонов.

## Сделано
