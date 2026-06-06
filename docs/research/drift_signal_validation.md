# Drift signal validation — имеет ли право быть drift-чекер (corpus evidence)

_2026-06-06_

Цель документа — ответить **данными**, а не мнением, на вопрос: detection
архитектурного дрейфа в archcheck — это реальная ниша или бесполезная побочка.
Вывод: **узкие drift-правила (циклы, god-header shortcuts, взаимная связность)
— реальная ниша; сырая cross-area связность как gate — шум.** Ценность продукта
ровно в том, чтобы оставаться узким.

## Данные

- **Корпус:** 310 C++ репозиториев (`/home/localadm/oss/`), окно ~13 мес
  (май 2025 → июнь 2026), **135 092 коммита** проанализировано per-commit
  (`experiments/ai_repo_run/*_graph_drift.{jsonl,md}`,
  генератор `generate_per_commit_graph_drift.py`).
- **Метрики на коммит:** `added_edges`, `removed_edges`, `grown_cycles`,
  `new_cross_area_dependencies`.
- **Боевая валидация shipped-правила DRIFT.1** — отдельный прогон
  `archcheck --drift-baseline` на LibreSprite PR #581 (см. ниже).

### Сырое распределение

| Сигнал | Коммитов | Объём | Доля коммитов |
|---|---|---|---|
| `added_edges>0` | 13 036 | 232 262 рёбер | 9.6% |
| `new_area_deps>0` | 855 | 1903 cross-area события | 0.63% |
| **`grown_cycles>0`** | **72** | **136 циклов** | **0.05%** |

`added_edges` — почти весь бенайн (новый `.cpp` включает свой `.h` — нормальный
рост, не дрейф). Поэтому ниже разбираются только cross-area и циклы.

## Классификация 1903 cross-area событий

Эвристика: NOISE = build/vendor/test/header/codeql + артефакты переименований;
STRUCT = parent→child подкаталог; LEGIT = `→core/engine/common` (нормальная база);
ACTIONABLE = bidirectional `A↔B` (взаимная связь = smell цикла/связности);
REVIEW = distinct-area однонаправленные.

| Корзина | Кол-во | % | Интерпретация |
|---|---|---|---|
| NOISE | 622 | 32% | шум (build/vendor/test/header + renames типа `src/elab`→`src/elaboration`) |
| STRUCT | 178 | 9% | `src→src/ui` — нормальная структура |
| LEGIT | 182 | 9% | `→core/engine` — нормальная база, не дрейф |
| REVIEW | 627 | 33% | «человек глянет»; потенциал при лучшем area-партишене |
| **ACTIONABLE** | **294** | **15%** | **bidirectional — реальный layering-smell** |

**Ключевой вывод:** сырая cross-area-метрика как gate дала бы **~50%+ false-alarm**
(NOISE + STRUCT + LEGIT = 50% — это нормальная разработка). Гейтить на ней нельзя.
Это объясняет, почему archcheck шипит **узкие** правила, а cross-area держит
research-пробой, не shipped-гейтом. Это правильная дисциплина, не недоработка.

## Где ценность реальна

### 1. DRIFT.2 (циклы) — gate-grade

72 из 135 092 коммитов = **0.05%**. Срабатывает исключительно редко → почти нулевой
false-alarm. Новый цикл объективно плох (Lakos physical design). Примеры из корпуса:
«Refactor CodeGen to classical header/implementation split» (+1), японский
«ファイル分割» / split файлов (+1), «Fix build: add ESP8266WebServer.h guard» (+7),
«Restructured all source files» (+1). Это ровно класс регрессий, который хочется
ловить в PR.

### 2. DRIFT.1 (god-header shortcut) — точный, линтер-невидимый

Боевой прогон, LibreSprite PR #581 (Claude Opus 4.5 серия коммитов):

- before `60eed0f` → `--save-graph-baseline` (граф 1207 узлов) →
  after `276fdbd` → `--drift-baseline`.
- Результат: **DRIFT.1 = 1** — `app/ui/toolbar.cpp -> app/pref/preferences.h`
  (UI-виджет полез в god-header настроек, fan-in 74, ради одного `bool`
  `showToolShortcuts()`); DRIFT.2 = 0.
- Из 260 нарушений 259 — pre-existing legacy (ChainLength/GodHeader/SF.8),
  drift-режим их игнорирует. **Сигнал отделён от шума.**
- Скептик-проверка (тремя `git`): ребро (re)introduced AI-коммитом `0aa57ad`,
  не унаследовано из upstream Aseprite. Вердикт CONFIRMED. Зафиксировано
  hermetic-фикстурой `fixtures/drift_real_world/libresprite_pr581/`
  (тест `drift_fixtures_test.cpp`). Подробно: [ai_drift_cases.md](ai_drift_cases.md),
  [../milestones.md](../milestones.md) §«Прогон 10».

Обычные линтеры этот класс не ловят — diff чистый, код компилируется.

### 3. Bidirectional coupling — 15% сигнала, в основном НЕ покрыто

294 события `A↔B` — настоящие layering-нарушения. Примеры из корпуса:
`src/hal ↔ src/ui` (HAL — низкоуровневый leaf — не должен взаимно зависеть от UI),
`Source/Game ↔ Source/Renderer` (игра и рендер сцепились), `core ↔ inspect`,
`editor ↔ engine`, `src/hal ↔ {input,math,ui,display}`.

**Важно — это НЕ то же, что цикл (DRIFT.2).** На уровне файлов `A↔B` = 2-node цикл и
ловится SF.9/DRIFT.2. Но bidirectional здесь — **аггрегатный (area) уровень**: разные
файлы в каждом модуле, циклического include нет. Проверка по корпусу: из **65 коммитов**
с bidirectional area-парой лишь **9** имели реальный file-cycle (`grown_cycles>0` →
домен DRIFT.2); остальные **56 (86%)** — area-coupling без цикла, **которое DRIFT.2 не
видит**. Это Lakos «не-levelizable design» (модули нельзя собрать раздельно). Кандидат
на отдельное узкое правило DRIFT.3 (задача #087), строго не перекрывающее DRIFT.2.

## DRIFT.3 — ручной разбор хитов (eyeball, 2026-06-06)

Прогон реализованного DRIFT.3 по выборке из 56 corpus-коммитов с bidirectional
area-парой, с ручной проверкой каждого хита:

**Шум фильтруется корректно (silent):** `subdir↔src` (vibe-requirements split),
`<root>`-каша (Decodium «Initial commit»), born-at-empty initial structure
(NeoCalculator). Area-функция (strip `src/include/..`, ignore root/noise) убирает
артефакты, на которых тонула сырая cross-area проба.

**Ловит реальные smell'ы (TP):**
- `hal ↔ ui/apps/math/display/input` (NeoCalculator) — дырявый abstraction layer:
  HAL должен быть leaf, а он взаимно зависит от UI. Сильнейший сигнал.
- `common ↔ duckdb`, `common ↔ sqlite` (gizmosql) — «база» зависит обратно от бэкенда.
- `game ↔ render` (Standard-of-Iron), `terrain ↔ world` (bakabakaband),
  `engine ↔ game` (teaching engine), `core ↔ semantic` (mantra-lang) — entanglement
  на **feature-коммитах** (настоящий инкрементальный дрейф).
- `model/persistence ↔ ui` (MaximumTrainer) — layering-нарушения (persistence не
  должен зависеть от ui).

**Найденные глазами проблемы → одна пофикшена:**
1. **FP `build_overrides`** (UnleashedRecomp) — exact-noise-set не ловил `build_overrides`.
   **Исправлено** prefix-фильтром (`build_*`/`mock*`/`*override*`) → стало silent.
2. **Шумит на больших restructure-коммитах** (MaximumTrainer — 8 пар разом, UE5 «Refactor
   scene» — 3). Когда коммит реорганизует модули, много мутуальностей всплывает разом.
   Семантически отделить «намеренный рефактор» от «дрейф» нельзя → **подтверждает: DRIFT.3
   должен быть advisory, не blocking gate.**
3. **Грубая гранулярность area** (Lightpad `App/ui↔App/core` — silent): «первый сегмент»
   схлопывает всё под одним top-dir (`App`). Это under-reporting (miss), не FP; уточнение
   до 2 уровней — отдельная итерация (риск нового шума).

Итог eyeball: DRIFT.3 ловит реальную связность с приемлемой precision после area-фильтра;
основной FP (build dirs) закрыт; остаток (restructure-шум, грубая гранулярность) —
осознанные trade-off'ы, оправдывающие advisory-режим.

## Грань польза / бесполезность

```
УЗКО (циклы + god-header shortcuts + bidirectional)
   → редко, объективно, линтер-невидимо               → ПОЛЕЗНО
ШИРОКО (любая cross-area dependency как gate)
   → ~50% false-alarm, тонет в норме                  → БЕСПОЛЕЗНО
```

archcheck сейчас на правильной стороне грани. Бесполезным он станет только если
погнаться за широтой.

## Рекомендации → статус реализации

1. **DRIFT.2 (циклы) → default blocking-gate** — ✅ **сделано** (#086). `--drift-baseline`
   теперь regression-gate: exit определяется только DRIFT.1/DRIFT.2; legacy intrinsic
   (SF/Lakos) и advisory DRIFT.3 репортятся, но не гейтят. Live-верификация на LibreSprite:
   реальный дрейф (DRIFT.1=1) → exit 1; та же версия (259 legacy, 0 дрейфа) → exit 0
   (раньше падал на legacy).
2. **DRIFT.3 (bidirectional coupling)** — ✅ **реализован** (#087): узкое правило,
   ловит мутуальную связь модулей `A↔B` (через разные файлы, без file-cycle), которой
   не было в baseline; не перекрывает DRIFT.2. Провалидирован на `danielraffel/pulp`
   @ `705f86e` (`core ↔ inspect`) + ручной разбор 56 corpus-коммитов (см. eyeball-секцию).
   Остаётся **advisory** (шумит на restructure-коммитах).
3. **Cross-area raw — НЕ гейтить**, держать advisory/research (как duplication
   после #082). Перед любым «area»-правилом — чинить area-detection (renames,
   build/output-dirs, `src↔include` дают половину шума).
4. **Категория REVIEW (33%)** — резерв: при лучшем area-партишене часть станет
   ACTIONABLE.

### Итог: drift-ядро как CI-gate (2026-06-06)

Три узких правила с явной, corpus-обоснованной gating-политикой:

| Правило | Сигнал | Режим | Обоснование |
|---|---|---|---|
| **DRIFT.1** | shortcut к god-header | **gate** | точный, линтер-невидимый (LibreSprite) |
| **DRIFT.2** | новый/выросший цикл | **gate** | редкий (0.05%), объективный (Lakos) |
| **DRIFT.3** | взаимная связность модулей | advisory | реальный, но шумит на restructure |

Это и есть ответ на вопрос «имеет ли право быть чекер»: **да** — он гейтит ровно то, что
данные назвали gate-grade (редкие объективные регрессии), и не тонет в legacy-долге и
шуме нормальной разработки. Бесполезным он стал бы только при попытке гейтить сырую
cross-area связность (~50% FP) — этого сознательно не делаем.

## Границы этого анализа

- Cross-area/cycle метрики — из python-пробы `generate_per_commit_graph_drift.py`,
  не 1:1 shipped-правила. `grown_cycles` ≈ DRIFT.2; DRIFT.1 (god-header shortcut)
  валидирован отдельно (LibreSprite).
- Бакетизация 1903 событий — эвристическая (калибрована ручным чтением выборок,
  не полная ручная разметка всех). Порядки величин надёжны; точные проценты ±неск.
