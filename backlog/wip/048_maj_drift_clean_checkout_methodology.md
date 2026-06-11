# [DRIFT/METHODOLOGY] DRIFT-прогоны на чужих репо требуют clean checkout, не partial

**Дата создания:** 2026-05-29
**Дата старта:** — (staged для Haiku 2026-06-11, ждёт команды)
**Статус:** wip (staged)
**Исполнитель:** Haiku (только скрипт + методология; перепрогоны PR — отдельный research, НЕ Haiku)
**Модуль:** RESEARCH/FIXTURES (методология #033)
**Приоритет:** major (искажает результаты dogfood'а на чужих репо)
**Сложность:** S (документация + helper-скрипт, ≤ 1 ч)
**Целевой релиз:** v0.1
**Блокирует:** надёжность выводов из любого drift-прогона на чужих репозиториях
**Related:** #033 (ai_drift_dataset), #047 (BOM)

## Симптом

При прогоне DRIFT.1 на серии PR из одного репо (последовательно в одном
working tree) получаются ложные срабатывания.

Конкретный кейс: **EtherAura/Kartend PR #26** (refactor errorutils, 4 файла,
+13/-5 строк, ни одного из них — `settingsdialogform.cpp`):
- Сначала прогон с `git clean -fd src + git checkout $parent -- src + ... + git checkout $sha -- src`:
  **26 DRIFT.1** в `ui/dialogs/settings/core/settingsdialogform.cpp`, все указывают
  на файлы которые НЕ изменялись в PR.
- Тот же PR с `git clean -fdx src + git checkout -f $parent -- src` (и так же для after):
  **0 DRIFT.1**.

Аналогично OreStudio: dirty прогон 4 PR'ов давал **7 DRIFT.1**, clean — **0**.

## Причина

`git checkout <sha> -- <path>` обновляет только файлы, которые существуют в
`<sha>`. Файлы, которые присутствуют в working tree (с прошлой итерации цикла
или с master HEAD) но отсутствуют в `<sha>` — **остаются в working tree**.

`git clean -fd` удаляет только untracked. Tracked файлы из других ревизий,
которые числятся в индексе HEAD, не убираются.

Итог: при scan'е `archcheck --save-graph-baseline` видит файлы, которых не
должно быть в `parent` ревизии, и сохраняет их рёбра. Дальше при checkout'е
`after` эти файлы могут пропасть (если `after` тоже не содержит их) или
остаться (если содержит). Если меняется список файлов или их edge'и — DRIFT
ложно срабатывает.

## Эмпирическое подтверждение

| Repo | PR | Dirty | Clean | Дельта |
|------|----|-------|-------|--------|
| Kartend | #26 errorutils refactor | 26 DRIFT.1 | 0 | -26 (все FP) |
| Kartend | #27 promote uiconstants | 5 DRIFT.1 | 5 DRIFT.1 | 0 (real) |
| Kartend | #34 covers leaf-struct | 0 | 0 | 0 |
| OreStudio | #547 service-to-service auth | 1 DRIFT.1 | 0 | -1 (FP) |
| OreStudio | #558 sql isolation | 0 | 0 | 0 |
| OreStudio | #588 composite instrument | 2 DRIFT.1 | 0 | -2 (FP) |
| OreStudio | #618 ORE instrument | 4 DRIFT.1 | 0 | -4 (FP) |
| IrredenEngine | #727 render LOD | 2 DRIFT.1 | 2 DRIFT.1 | 0 (real) |
| vmecpp | #360, #340 | 0 | 0 | 0 |

Все ложные срабатывания исчезают при использовании `git clean -fdx` + `git checkout -f $ref -- path`.

## Фикс

### Краткосрочный: helper-скрипт для безопасного прогона

Положить в `scripts/drift_run.sh`:

```bash
#!/usr/bin/env bash
# Usage: drift_run.sh <repo-path> <subdir> <before-sha> <after-sha> [graph-baseline-path]
set -euo pipefail
repo=$1; sub=$2; before=$3; after=$4
graph=${5:-/tmp/drift_$(basename $repo)_${before:0:7}.graph.json}

cd "$repo"
git -C "$repo" clean -fdx "$sub" >/dev/null
git -C "$repo" checkout -f "$before" -- "$sub"
archcheck --save-graph-baseline "$graph" "$repo/$sub"
git -C "$repo" clean -fdx "$sub" >/dev/null
git -C "$repo" checkout -f "$after" -- "$sub"
archcheck --drift-baseline "$graph" "$repo/$sub"
```

Гарантирует чистое состояние перед каждой ревизией.

### Долгосрочный: режим archcheck со встроенной git-логикой

Уже есть `archcheck --diff <revspec>`. Расширить до режима, который
сам делает clean checkout двух ревизий (или использует `git worktree`
для полной изоляции), не требуя ручной работы с working tree.

`git worktree add /tmp/before <sha>` создаёт чистый WT в отдельной папке.
Это устранит весь класс проблем со стейтом working tree.

## Влияние на milestones

Все DRIFT-прогоны из раздела «Прогон 14 — DRIFT re-run после BOM-фикса»
требуют пересмотра: использовался `git checkout -- src` без `-f` и без `-fdx clean`,
кроме первого LibreSprite-кейса где делался полный `git checkout SHA`.

**Подтверждены чистыми re-run'ом:**
- LibreSprite #581 → 1 DRIFT.1 (full checkout)
- BambuStudio #10794 → 2 DRIFT.1 (full checkout)
- vmecpp #360, #340 → 0 (clean re-run)
- IrredenEngine #727 → 2 DRIFT.1 (clean re-run)
- Kartend #27 → 5 DRIFT.1 (clean re-run)

**Сомнительные (нужен re-run):**
- spectre 5 PR'ов (все были 0; нужна верификация что 0 — не FN)
- GWToolboxpp 3 PR'ов
- IrredenEngine #798, #1200, #1207
- moqx 3 PR'ов
- AetherSDR 3 PR'ов
- OreStudio: clean re-run уже сделан → все 0
- Kartend #26, #34: clean re-run сделан → 0, 0

## Сделано

- ✅ Шаг 1: dovesti `scripts/drift_run.sh` к контракту (2026-06-11)
  - Изменена usage-строка на `drift_run.sh <repo-path> <subdir> <before-sha> <after-sha> <label>`
  - Добавлена шапка-комментарий (3 строки) с объяснением причины clean checkout
  - Изменён `set -e` на `set -euo pipefail`
  - Скрипт теперь принимает `<before-sha>` и `<after-sha>` явно (не вычисляет parent)
  - Последовательность: clean → checkout before → archcheck baseline → clean → checkout after → archcheck drift
  - Абсолютный путь к бинарю: `/home/localadm/projects/cpparch/build/debug/src/archcheck`
- ✅ Шаг 2: дописана методология в `docs/research/ai_drift_runlog.md` (2026-06-11)
  - Добавлена секция «Методология: clean checkout обязателен»
  - Эмпирическая таблица Dirty/Clean скопирована целиком
  - Правило: все DRIFT-прогоны на чужих репо — только через `scripts/drift_run.sh`

## В работе

- (пусто)

## Следующие шаги

1. Перепрогнать сомнительные PR с full clean checkout — **research, вне Haiku-скоупа**, отдельный заход.
2. ✅→⚠️ `scripts/drift_run.sh` **уже существует** (проверено 2026-06-11), но не соответствует контракту — довести (план ниже).
3. Документировать методологию в `docs/research/ai_drift_runlog.md` (файл существует) — план ниже.
4. `--git-worktree` режим для archcheck — **future, вне скоупа этой задачи**.

## План для Haiku (2026-06-11)

Перед стартом ОБЯЗАН прочитать целиком: эту задачу, [docs/dev/haiku_task_guide.md](../../docs/dev/haiku_task_guide.md) §2.

### Разрешённые развилки (факты, проверены 2026-06-11)

- `scripts/drift_run.sh` существует, но: usage-строка врёт (`drift_one.sh`), принимает только `<after-sha>` и сам выводит parent, `set -e` вместо `set -euo pipefail`. Привести к контракту ниже — **редактировать существующий файл**, не создавать новый.
- `docs/research/ai_drift_runlog.md` существует — методологию **дописать секцией**, файл не переписывать.
- Флаги `--save-graph-baseline` / `--drift-baseline` существуют (`src/main.cpp:53-54`) — контракт скрипта на них корректен.
- C++ код в этой задаче НЕ трогается вообще.

### Шаг 1 — довести `scripts/drift_run.sh`

Контракт: `drift_run.sh <repo-path> <subdir> <before-sha> <after-sha> <label>`. Обязательно:
- `set -euo pipefail`;
- шапка-комментарий (3–4 строки): зачем clean checkout — `git checkout <sha> -- <path>` НЕ удаляет файлы, отсутствующие в `<sha>`, а `git clean -fd` не трогает tracked; без `-fdx` + `checkout -f` DRIFT.1 даёт FP (см. backlog #048);
- последовательность для каждой из двух ревизий сохранить как в текущем скрипте: `git clean -fdx "$sub"` → `git checkout -f "$ref" -- "$sub"` → archcheck;
- бинарь — абсолютный путь `/home/localadm/projects/cpparch/build/debug/src/archcheck` (как сейчас);
- выводить `=== $label ===` + DRIFT-строки + строку `violation(s)` (как сейчас).

### Шаг 2 — методология в `docs/research/ai_drift_runlog.md`

Дописать секцию `## Методология: clean checkout обязателен (2026-06-11)`:
- причина (формулировка из секции «Причина» этой задачи, 1 абзац);
- эмпирическая таблица Dirty/Clean из секции «Эмпирическое подтверждение» (скопировать как есть);
- правило: любой DRIFT-прогон на чужом репо — только через `scripts/drift_run.sh`.

### Контрольные кейсы / DoD

| Проверка | Ожидание |
|----------|----------|
| `bash -n scripts/drift_run.sh` | exit 0 |
| self-test (см. ниже) | exit 0, вывод содержит `=== selftest ===` и строку `violation(s)` или `(no violations)` |
| `grep -c "drift_one" scripts/drift_run.sh` | 0 |

Self-test — **ТОЛЬКО на одноразовом клоне**:
```bash
git clone --quiet /home/localadm/projects/cpparch /tmp/drift_selftest
scripts/drift_run.sh /tmp/drift_selftest src be56245 cb6e09d selftest
rm -rf /tmp/drift_selftest
```

**ЗАПРЕЩЕНО запускать скрипт на `/home/localadm/projects/cpparch` напрямую**: `git clean -fdx` сотрёт незакоммиченные файлы, `checkout -f` собьёт `src/` на старую ревизию. Только клон в `/tmp`.

### Не делать

- НЕ перепрогонять «сомнительные PR» из секции «Влияние на milestones» — это research-шаг, отдельный заход.
- НЕ трогать C++ код, CMake, тесты.
- НЕ реализовывать `--git-worktree` режим.
- НЕ коммитить без явной команды.

### Эскалация (когда остановиться и передать старшей модели)

Остановись, запиши сюда «Заблокировано: <что/почему/что пробовал>» и доложи, если: self-test падает дважды по непонятной причине; контракт скрипта конфликтует с чем-то, чего нет в этой задаче; нужен файл вне `scripts/drift_run.sh` + `docs/research/ai_drift_runlog.md`. Дальше — Sonnet, затем Opus.
