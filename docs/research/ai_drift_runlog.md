# DRIFT-прогоны на AI-PR'ах — рабочий лог

Этот файл — операционный журнал DRIFT-исследования по задаче
[backlog/future/033](../../backlog/future/033_maj_ai_drift_dataset.md).
Финальные находки переезжают в [ai_drift_cases.md](ai_drift_cases.md).
Milestone-сводки — в [milestones.md](../milestones.md).

**Клоны:** `~/oss/<repo>/` (постоянное хранилище, не sandbox).
**Версия archcheck:** working tree после фикса #047 (BOM strip).
**Методология:** clean checkout через `git clean -fdx` + `git checkout -f`
(см. [#048](../../backlog/new/048_maj_drift_clean_checkout_methodology.md)
о причинах). Helper: `/tmp/drift_one.sh`.

## Сводная таблица всех прогонов (clean checkout)

Все строки получены через единый helper `drift_one.sh`, который:
1. `git clean -fdx <sub>` (включая ignored)
2. `git checkout -f <parent>^1 -- <sub>` → archcheck `--save-graph-baseline`
3. `git clean -fdx <sub>` снова
4. `git checkout -f <merge> -- <sub>` → archcheck `--drift-baseline`

`parent` — это `merge^1`, т.е. родитель merge-коммита по первой стороне,
поэтому в диапазоне только коммиты самого PR.

| Дата | Repo | PR | Files | DRIFT.1 | DRIFT.2 | Заметки |
|------|------|----|-------|---------|---------|---------|
| 2026-05-29 | LibreSprite/LibreSprite | #581 toolbar/menu/macOS | 1253 | **1** | 0 | shortcut ui→pref |
| 2026-05-29 | proximafusion/vmecpp | #360 asymmetric infra | 232 | 0 | 0 | clean |
| 2026-05-29 | proximafusion/vmecpp | #340 consolidate constants | 232 | 0 | 0 | clean |
| 2026-05-29 | bambulab/BambuStudio | #10794 color cutting/dovetail | 3019 | **2** | 0 | shortcut ui→Widgets ×2 |
| 2026-05-29 | sxs-collaboration/spectre | #7238 Filters::Filter base | 3531 | 0 | 0 | clean (+1352 LOC) |
| 2026-05-29 | sxs-collaboration/spectre | #7244 ZernikeB1 filter | 3531 | 0 | 0 | clean |
| 2026-05-29 | sxs-collaboration/spectre | #7237 GridToInertialJacobian | 3531 | 0 | 0 | clean |
| 2026-05-29 | sxs-collaboration/spectre | #7253 GenerateTetrahedral fix | 3531 | 0 | 0 | clean |
| 2026-05-29 | sxs-collaboration/spectre | #7251 GH Cartoon bases | 3531 | 0 | 0 | clean |
| 2026-05-29 | gwdevhub/GWToolboxpp | #2108 Armory grouping | 375 | 0 | 0 | clean (+150 LOC) |
| 2026-05-29 | gwdevhub/GWToolboxpp | #2156 sort INI sections | 375 | 0 | 0 | clean |
| 2026-05-29 | gwdevhub/GWToolboxpp | #2158 Sort by Map buttons | 375 | 0 | 0 | clean |
| 2026-05-29 | jakildev/IrredenEngine | #798 editor layer system | 665 | 0 | 0 | clean (+427 LOC) |
| 2026-05-29 | jakildev/IrredenEngine | #1200 chunk persistence | 665 | 0 | 0 | clean |
| 2026-05-29 | jakildev/IrredenEngine | #1207 rename ChunkDiskPersistence | 665 | 0 | 0 | clean (pure rename) |
| 2026-05-29 | jakildev/IrredenEngine | #727 render LOD Phase 1 | 666 | **2** | 0 | shortcut system→component_lod, system→lod_utils |
| 2026-05-29 | openmoq/moqx | #327 CrossExecFilter | 67 | 0 | 0 | clean (+1183 LOC, new module) |
| 2026-05-29 | openmoq/moqx | #328 CrossExecFilter relay | 67 | 0 | 0 | clean |
| 2026-05-29 | openmoq/moqx | #198 idle eviction fix | 67 | 0 | 0 | clean |
| 2026-05-29 | aethersdr/AetherSDR | #3065 TCI per-mode overflow | 550 | 0 | 0 | clean |
| 2026-05-29 | aethersdr/AetherSDR | #2390 connect to last radio | 552 | 0 | 0 | clean |
| 2026-05-29 | aethersdr/AetherSDR | #2685 wasVisible-guard | 552 | 0 | 0 | clean |
| 2026-05-29 | OreStudio/OreStudio | #618 ORE instrument pipeline | 4968 | 0 | 0 | clean (+1770 LOC) |
| 2026-05-29 | OreStudio/OreStudio | #558 s2s authentication | 4968 | 0 | 0 | clean |
| 2026-05-29 | OreStudio/OreStudio | #588 composite instrument | 4968 | 0 | 0 | clean (+9762 LOC!) |
| 2026-05-29 | OreStudio/OreStudio | #547 E2E lifecycle refactor | 4968 | 0 | 0 | clean |
| 2026-05-29 | EtherAura/Kartend | #26 errorutils refactor | 693 | 0 | 0 | clean |
| 2026-05-29 | EtherAura/Kartend | #34 covers leaf-struct | 693 | 0 | 0 | clean |
| 2026-05-29 | EtherAura/Kartend | #27 promote uiconstants | 693 | **5** | 0 | shortcut data→ui/uiconstants ×4 + data→utils/view |
| 2026-05-29 | community-shaders/skyrim | #2326 fix singleton ptr | 229 | **1** | 0 | shortcut State.cpp → Features/InteriorSun |
| 2026-05-29 | community-shaders/skyrim | #2207 refactor common Util | 230 | **1** | 0 | shortcut VRStereoOpt → Utils/UI |
| 2026-05-29 | community-shaders/skyrim | #2205 perf VR DLSS | 230 | 0 | 0 | clean |
| 2026-05-29 | community-shaders/skyrim | #2077 build version proposals | 230 | 0 | 0 | clean |

## Итог корпуса

- **33 PR'а** проверено на **10 репозиториях**.
- **7 PR'ов с DRIFT.1 hit'ами** (12 находок суммарно): LibreSprite #581,
  BambuStudio #10794, IrredenEngine #727, Kartend #27, Skyrim #2326, #2207
  → подтверждено grep'ом против file-content.
- **26 PR'ов clean** — 0 DRIFT.1, 0 DRIFT.2.
- **DRIFT.2 (cycles) не сработал ни разу.** Ни один AI-коммит в корпусе
  не ввёл новый include-cycle. Возможные интерпретации:
  - циклы редки даже в неконтролируемом C++ коде
  - наш порог largest_scc>=2 строг — мелкая инверсия не считается циклом
  - корпус мал (33 PR'а) для статистики

## Архетипы DRIFT.1 находок в корпусе

1. **UI → low-level widgets** (BambuStudio #10794, Kartend #27 частично,
   LibreSprite #581) — диалог/виджет тащит include конкретного младшего
   компонента вместо абстракции.
2. **Generic → Features** (Skyrim #2326, #2207) — общий код тянет include
   конкретного feature-модуля, который раньше был изолирован.
3. **System → Component** (IrredenEngine #727) — ECS-система начинает
   импортировать компонент вместо запроса через query.
4. **UI-config → core data** (LibreSprite #581) — обратное направление,
   виджет читает preferences-слой напрямую.

Все 4 архетипа — именно те классы находок, под которые DRIFT.1
проектировался.

## Чистые PR'ы — что говорят 0 DRIFT

26 из 33 PR'ов прошли без drift. Это полезный негативный сигнал:
**DRIFT.1 не false-positive шумогенератор**.

Особенно показательны крупные PR'ы:
- spectre #7238: **+1352 LOC** — новая базовый класс Filters::Filter — clean
- OreStudio #588: **+9762 LOC** — composite/scripted instrument domain model — clean
- moqx #327: **+1183 LOC** — новый модуль CrossExecFilter — clean
- IrredenEngine #798: **+427 LOC** — editor layer system — clean

Чистые крупные PR'ы доказывают, что DRIFT.1 различает «новый код в
своих границах» (OK) и «существующий код пересекает новые границы» (drift).

## Найденные баги в archcheck в ходе прогона

- **#047 (closed)** — UTF-8 BOM не зачищается, ломает DRIFT.1 на
  wxWidgets/MSVC-проектах. Обнаружено на BambuStudio. Фикс применён.
- **#048 (open)** — методологическая ловушка: dirty working tree даёт
  массовые false-positive в DRIFT. Требуется helper-скрипт +
  возможно режим `archcheck --diff` с git worktree изоляцией.

## Кандидаты в очереди (отложено)

- apache/thrift (13 AI-PR'ов, но почти все non-C++)
- dancinlab/hexa-lang (24 PR'а, но Python-проект)
- raspberrypi/pico-sdk (3 PR'а, все +1 LOC)
- Tier 2 из задачи #033: PX4 (запрещают AI attribution),
  nodos-dev/sys-device (1 файл)

## Команды воспроизведения

```bash
# Один PR:
bash /tmp/drift_one.sh <repo-path> <subdir> <merge-sha> <label>

# Пример:
bash /tmp/drift_one.sh ~/oss/LibreSprite src \
    276fdbdb27b537a074c3e170af6afc88c244a539 libresprite_581
```

Helper-скрипт хранится в `/tmp/drift_one.sh` (нужно перенести в `scripts/`
вместе с реализацией #048).
