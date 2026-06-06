# [RULES][DRIFT] DRIFT.3 — bidirectional area coupling (взаимная связность)

**Дата создания:** 2026-06-06
**Дата старта:** —
**Статус:** new
**Модуль:** RULES/DRIFT + GRAPH
**Приоритет:** major
**Сложность:** L
**Блокирует:** —
**Заблокирован:** —
**Related:** #009 (DRIFT-правила), #082 (alignment umbrella), [docs/research/drift_signal_validation.md](../../docs/research/drift_signal_validation.md) (corpus-доказательство)

## Цель

Добавить узкое правило DRIFT.3, ловящее **появление взаимной зависимости между
модулями/областями** (`A → B` при уже существующем `B → A`) — классический smell
связности, ведущий к циклам и расцеплению-невозможности.

## Обоснование (данными)

Corpus-валидация ([drift_signal_validation.md](../../docs/research/drift_signal_validation.md)):
из 1903 cross-area событий **294 (15%) — bidirectional `A↔B`**, и это **реальный
actionable-сигнал** (в отличие от сырой cross-area, где ~50% — норма). Примеры из
корпуса: `src/hal ↔ src/ui` (HAL не должен взаимно зависеть от UI), `Source/Game ↔
Source/Renderer`, `core ↔ inspect`, `editor ↔ engine`. Узкие правила DRIFT.1/DRIFT.2
этот класс сейчас НЕ покрывают.

## Что нужно

1. Определить «область» (area) для графа. **ПЕРЕД правилом** — починить area-detection:
   - не считать `src↔include` cross-area (заголовки своей же области);
   - исключить build/output/vendor/test/generated-каталоги;
   - не плодить fantom-деп от переименований каталогов.
   (Без этого DRIFT.3 утонет в шуме — см. валидацию.)
2. Реализовать DRIFT.3: новое ребро `A → B`, когда в baseline уже есть путь `B → A`
   на уровне областей → bidirectional coupling. Один класс = один файл (OCP).
3. Решить режим: advisory (репорт) vs gate. Рекомендация — стартовать **advisory**,
   поднять до gate после прогона по корпусу и замера precision (как DRIFT.2).
4. Fixtures: `fixtures/drift_bidirectional/pass|fail_*`.

## Acceptance criteria

- [ ] Area-detection не даёт `src↔include`/build/vendor/rename ложных cross-area.
- [ ] DRIFT.3 ловит появление `A↔B` и НЕ срабатывает на однонаправленных `→base`.
- [ ] Fixtures pass/fail; правило — отдельный класс/файл.
- [ ] Прогон по корпусу: измерена precision DRIFT.3, зафиксировано решение advisory/gate.

## Заметки

- Это **не** «cross-area gate» (тот ~50% FP, не делаем). DRIFT.3 узкий — только bidirectional.
- Реюзать граф/levelization из существующего `graph/` слоя, не строить новый движок.
