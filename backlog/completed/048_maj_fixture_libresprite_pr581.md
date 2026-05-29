# [TEST][DRIFT] DRIFT.1 fixture: LibreSprite PR #581 shortcut edge

**Дата создания:** 2026-05-29
**Дата старта:** 2026-05-29
**Дата завершения:** 2026-05-29
**Статус:** completed
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

- [x] Создать `fixtures/drift_real_world/libresprite_pr581/`
- [x] After-состояние как набор source-файлов в директории fixture'а
      (`app/ui/toolbar.cpp` с include'ами `toolbar.h` + `preferences.h`,
      `app/ui/toolbar.h`, `app/pref/preferences.h` — все stub'ы `#pragma once`)
- [x] Before-состояние через `baseline.graph.yml` (конвенция из соседних
      DRIFT-fixtures: nodes/edges YAML, не отдельный `before/` дир) — узлы
      все три, ребро только `toolbar.cpp -> toolbar.h`, без preferences
- [x] `README.md` в fixture-папке: SHA-пара, ссылка на PR, ссылка на
      milestones.md, скептик-фрейминг (upstream Aseprite vs LibreSprite fork)
- [x] Integration-тест в `tests/integration/rules/drift_fixtures_test.cpp` —
      assert `v.size()==1`, `ruleId=="DRIFT.1"`, message содержит
      `app/ui/toolbar.cpp -> app/pref/preferences.h`
- [x] Fixture compile-free — только include-директивы + `#pragma once`

## Критерий приёмки

`ctest` зелёный, fixture-тест воспроизводит ровно тот hit, что записан в
milestones.md прогон 10. Папка fixture'а самодостаточна — не зависит от
sandbox-клонов.

## Сделано

Fixture `fixtures/drift_real_world/libresprite_pr581/` собран по схеме
существующих DRIFT-fixtures (`drift_shortcut_edge/fail_new_coupling/`):

- **After-state** — файлы в директории: `app/ui/toolbar.cpp` (включает
  `app/ui/toolbar.h` + `app/pref/preferences.h`), `app/ui/toolbar.h`,
  `app/pref/preferences.h`. Все header'ы — `#pragma once` stub'ы.
- **Before-state** — `baseline.graph.yml`: те же 3 узла, единственное ребро
  `toolbar.cpp -> toolbar.h`. Преднамеренно НЕ кладём ребро в `preferences.h`.
- **Test** — `tests/integration/rules/drift_fixtures_test.cpp` добавлен
  `TEST_CASE "drift fixture: real_world/libresprite_pr581 — DRIFT.1 fires on
  toolbar -> preferences"`. Проверяет `v.size()==1`, ruleId DRIFT.1, и текст
  сообщения содержит ожидаемое ребро.
- **README.md** — контекст + skeptic-фрейминг (upstream Aseprite держит edge,
  LibreSprite-форк его терял, AI-коммит вернул).

Прогон: `archcheck_tests "[drift][fixtures]"` → 5 test cases, 18 assertions,
все зелёные (вместо 4/15 до добавления). Lizard на `src/ include/ tests/` —
зелёный.

Принцип: для DRIFT.1 fixture достаточно `baseline.graph.yml` + источников
after-state'а. Не нужно компилировать, не нужен `compile_commands.json`,
не нужен полный клон репозитория. Из реального hit'а на 1253-узловом графе
LibreSprite получается воспроизводимый 3-узловой срез, который держит
ровно одно скептик-стойкое утверждение: «AI-коммит добавил ребро между
двумя pre-existing модулями, archcheck это поймал».

Демо-фрейминг (для README/HN): edge `app/ui/toolbar.cpp ->
app/pref/preferences.h` присутствует в upstream `aseprite/aseprite`, но
LibreSprite на before-SHA `60eed0f` его не нёс. Это снимает возражение
«просто re-convergence к upstream» — граф **этого** репо действительно
изменился, и AI-коммит его изменил. Полная git-верификация (before-grep /
after-grep / introducing commit) — в `docs/research/ai_drift_cases.md`
секция «Верификация» и `docs/milestones.md` Прогон 10.
