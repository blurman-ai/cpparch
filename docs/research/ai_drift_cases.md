# AI-assisted commits as DRIFT-rule test cases

Дата прогона: 2026-05-29
Бинарь: `build/debug/src/archcheck` (DRIFT.1 + DRIFT.2)
Метод: `--save-graph-baseline` на before-SHA → `--drift-baseline` на after-SHA.

Связано с задачей [backlog/future/033_maj_ai_drift_dataset.md](../../backlog/future/033_maj_ai_drift_dataset.md).
Реализация DRIFT.1/DRIFT.2 — [backlog/completed/009_maj_ai_drift_regression_rules.md](../../backlog/completed/009_maj_ai_drift_regression_rules.md).

## Сводка

| Repo | PR | Before SHA | After SHA | DRIFT.1 | DRIFT.2 | Вердикт |
|------|----|------------|-----------|---------|---------|---------|
| LibreSprite/LibreSprite | #581 macOS / menu search / toolbar badges | `60eed0f` | `276fdbd` | **1** | 0 | hit — shortcut edge через границу `ui → pref` |
| proximafusion/vmecpp | #360 asymmetric infra | `df63271` | `5eabd51` | 0 | 0 | clean — крупный рефактор без дрейфа |
| proximafusion/vmecpp | #340 consolidate constants | `b44fb7f` | `a7797dc` | 0 | 0 | clean — рефакторинг сделан корректно |
| bambulab/BambuStudio | #10794 color cutting / dovetail / sculpting | `2263815` | `a206a95` | **2 real + 1 fp** | 0 | hit — два UI→Widgets shortcut'а + найден баг в сканере |

DRIFT-правила сработали только там, где это оправдано. На вмecpp PR'ах — clean. На BambuStudio найдены два реальных нарушения **и один баг в нашем сканере** (UTF-8 BOM).

## LibreSprite PR #581 — найденное нарушение

```
app/ui/toolbar.cpp: [DRIFT.1] shortcut edge: app/ui/toolbar.cpp -> app/pref/preferences.h
```

Источник: коммит `0aa57ad` "Add keyboard shortcut badges to toolbar icons"
(Co-Authored-By: Claude Opus 4.5).

Diff:
```cpp
// src/app/ui/toolbar.cpp
+#include "app/pref/preferences.h"
```

Что произошло: агент добавил визуальные бейджи с горячими клавишами на иконки тулбара
и завёл новую preference `show_tool_shortcuts`. Чтобы прочитать её, он добавил прямой
include `app/pref/preferences.h` из `toolbar.cpp` — модуля, который раньше с
preferences-слоем напрямую не общался.

Почему это именно тот класс находки, под который DRIFT.1 проектировался:
- локально удобно (один include, и preference доступен)
- глобально размывает архитектуру (UI-виджет начинает зависеть от слоя
  preferences без явного посредника)
- не ловится обычными линтерами — синтаксически и семантически код валиден

Это эталонный shortcut edge в формулировке `docs/research/constraint_decay.md`:
правдоподобный AI-шаг через границу модуля, не создающий очевидного мусора, но
накапливающий структурный дрейф.

### Верификация (skeptic pre-empt, 2026-05-29)

LibreSprite — форк Aseprite. Upstream `aseprite/aseprite`
`src/app/ui/toolbar.cpp` уже включает `app/pref/preferences.h`, поэтому
естественный skeptic-вопрос: «это не AI-drift, а просто re-convergence к
upstream, где edge всегда был». Аргумент слабый по сути (DRIFT.1 считает дельту
графа **этого** репо, а не upstream), но он способен унести punch из публичного
демо. Закрыто тремя командами `git` против sandbox-клона LibreSprite:

```
$ git show 60eed0f:src/app/ui/toolbar.cpp | grep -n 'app/pref/preferences.h'
ABSENT at before-SHA

$ git show 276fdbd:src/app/ui/toolbar.cpp | grep -n 'app/pref/preferences.h'
15:#include "app/pref/preferences.h"

$ git log --oneline 60eed0f..pr-581 -- src/app/ui/toolbar.cpp
0aa57ad Add keyboard shortcut badges to toolbar icons
```

`git log -p -S 'app/pref/preferences.h' 60eed0f..pr-581 -- src/app/ui/toolbar.cpp`
подтвердил единственное `+#include "app/pref/preferences.h"` в этом коммите,
отката внутри PR нет.

**Вердикт: CONFIRMED.** Форк LibreSprite на 60eed0f действительно не нёс ребра;
AI-коммит 0aa57ad его (re)introduced. Фрейминг для демо: «edge присутствует в
upstream Aseprite; LibreSprite по своей истории его терял; AI-коммит вернул его
именно в той ситуации, которую DRIFT.1 и должен ловить».

## Обновление 2026-05-29 (вторая сессия)

Корпус расширен до **33 PR'ов на 10 репозиториях**. Полная таблица —
[ai_drift_runlog.md](ai_drift_runlog.md). Дополнительные find'ы:

| Repo | PR | DRIFT.1 | Архетип |
|------|----|---------|---------|
| jakildev/IrredenEngine | #727 render LOD Phase 1 | 2 | system→component_lod, system→lod_utils |
| EtherAura/Kartend | #27 promote uiconstants | 5 | data/settings → ui/uiconstants ×4 + data→utils/view |
| community-shaders/skyrim | #2326 fix singleton ptr | 1 | State.cpp → Features/InteriorSun |
| community-shaders/skyrim | #2207 refactor common Util | 1 | VRStereoOpt → Utils/UI |

В ходе прогона найден **методологический баг** (#048): partial git checkout
без `git clean -fdx` оставляет файлы из других ревизий → массовые
false-positive в DRIFT. Все цифры выше пересчитаны через `scripts/drift_run.sh`
с full clean checkout.

Итого: 12 подтверждённых DRIFT.1 hit'ов на 7 PR'ах из 33 (21%).
Архетипы: UI→widgets, generic→features, system→component, ui-config→core-data.

## BambuStudio PR #10794 — два UI→Widgets shortcut + один false-positive (баг)

```
slic3r/GUI/FilamentMapDialog.hpp: [DRIFT.1] shortcut edge: ... -> slic3r/GUI/Widgets/CheckBox.hpp
slic3r/GUI/FilamentMapPanel.hpp:  [DRIFT.1] shortcut edge: ... -> slic3r/GUI/Widgets/SwitchButton.hpp
slic3r/GUI/MsgDialog.cpp:         [DRIFT.1] shortcut edge: ... -> slic3r/GUI/MsgDialog.hpp   ← FALSE POSITIVE
```

Источник изменений: коммит `126aa51` "Integrate AG changes into root source tree" (author: adamgasoft).
Этот PR явно не помечен Co-Authored-By: Claude, но в задаче #033 он включён в Tier 2 — PR
содержит инструкцию AI workflow всегда добавлять Claude attribution. Берём как кейс
крупного архитектурного изменения с возможным AI-присутствием.

**Реальные находки (2):**
- `FilamentMapDialog.hpp -> Widgets/CheckBox.hpp` — диалог-уровень тащит include
  низкоуровневого виджета напрямую. Классический shortcut UI-слоя.
- `FilamentMapPanel.hpp -> Widgets/SwitchButton.hpp` — то же самое для panel.

**False-positive (1) — баг в сканере, заведена задача [#047](../../backlog/new/047_crt_scan_utf8_bom.md):**
- `MsgDialog.cpp -> MsgDialog.hpp` — `MsgDialog.cpp` ВСЕГДА начинался с `#include "MsgDialog.hpp"`.
  В before-ревизии первая строка имеет UTF-8 BOM (`EF BB BF`), наш `include_scanner`
  не зачищает BOM → первая строка не матчит regex → в graph-baseline ребра нет →
  в after-ревизии (без BOM) ребро видится как новое → DRIFT.1.

Этот false-positive — реально важная находка прогона: блокер для надёжности DRIFT.1
на любых wxWidgets / Windows-проектах. Зафиксирован в #047.

## vmecpp PR #360 и #340 — clean

Оба merged PR с явным Claude-attribution. Тысячи строк изменений (PR #360 — infrastructure
для asymmetric VMEC, PR #340 — консолидация констант в `vmec_constants.h`). DRIFT-правила
не сработали ни разу — это полезный сигнал, что DRIFT.1/DRIFT.2 не превращаются в
шумогенератор на нормальном рефакторинге.

Lakos.GodHeader флагирует 3 заголовка — это pre-existing состояние, не дрейф.

## Reproduction

```bash
cd sandbox/drift_repos/LibreSprite
git checkout 60eed0fd3e39104d50d67c366dd0f312ac45329c
archcheck --save-graph-baseline /tmp/libresprite_before.graph.json src

git fetch origin pull/581/head:pr-581
git checkout pr-581
archcheck --drift-baseline /tmp/libresprite_before.graph.json src
# → 1 DRIFT.1 violation
```

## Следующие шаги

1. Превратить LibreSprite-кейс в integration fixture `fixtures/drift_real_world/libresprite_pr581/`
   — минимальный воспроизводимый срез графа (без полного клона репо).
   Заведена задача [#048](../../backlog/new/048_maj_fixture_libresprite_pr581.md);
   skeptic-проверка (включая «PR целиком, а не только merge-range») закрыта
   секцией «Верификация» выше — CONFIRMED.
2. Добавить ещё 2-3 кейса из Tier 2 (nodos-dev/sys-device, ...) для
   расширения корпуса.
4. Когда корпус >= 5 PR с подтверждёнными hit'ами — обновить README демонстрацией
   на реальных данных (см. `backlog/future/033` §"Целевой вид демонстрации").
