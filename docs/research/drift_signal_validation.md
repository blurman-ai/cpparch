# Drift signal validation — имеет ли право быть drift-чекер (corpus evidence)

_2026-06-06_

Цель документа — ответить **данными**, а не мнением, на вопрос: detection
архитектурного дрейфа в archcheck — это реальная ниша или бесполезная побочка.
Вывод: **узкие drift-правила (циклы, god-header shortcuts, взаимная связность)
— реальная ниша; сырая cross-area связность как gate — шум.** Ценность продукта
ровно в том, чтобы оставаться узким.

## Данные

- **Корпус:** 310 C++ репозиториев (`~/oss/`), окно ~13 мес
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

## Грань польза / бесполезность

```
УЗКО (циклы + god-header shortcuts + bidirectional)
   → редко, объективно, линтер-невидимо               → ПОЛЕЗНО
ШИРОКО (любая cross-area dependency как gate)
   → ~50% false-alarm, тонет в норме                  → БЕСПОЛЕЗНО
```

archcheck сейчас на правильной стороне грани. Бесполезным он станет только если
погнаться за широтой.

## Рекомендации

1. **DRIFT.2 (циклы) → default blocking-gate** — частота 0.05% и объективность
   созрели для жёсткого гейта. (задача #086)
2. **DRIFT.3 (bidirectional coupling)** — добавить как узкое правило: 15% реального
   сигнала, сейчас не покрыто. (задача #087)
3. **Cross-area raw — НЕ гейтить**, держать advisory/research (как duplication
   после #082). Перед любым «area»-правилом — чинить area-detection (renames,
   build/output-dirs, `src↔include` дают половину шума).
4. **Категория REVIEW (33%)** — резерв: при лучшем area-партишене часть станет
   ACTIONABLE.

## Границы этого анализа

- Cross-area/cycle метрики — из python-пробы `generate_per_commit_graph_drift.py`,
  не 1:1 shipped-правила. `grown_cycles` ≈ DRIFT.2; DRIFT.1 (god-header shortcut)
  валидирован отдельно (LibreSprite).
- Бакетизация 1903 событий — эвристическая (калибрована ручным чтением выборок,
  не полная ручная разметка всех). Порядки величин надёжны; точные проценты ±неск.
