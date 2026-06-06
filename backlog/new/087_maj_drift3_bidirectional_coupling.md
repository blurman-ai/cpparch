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
модулями/областями на АГРЕГАТНОМ уровне** — когда модуль `A` начинает зависеть от
модуля `B`, при том что `B` уже зависит от `A`, **но через разные файлы**, так что
циклического include-пути НЕТ.

### Чем это НЕ DRIFT.2 (важно — иначе правило редундантно)

На уровне файлов/компонент `A→B` + `B→A` = 2-node цикл, и его **уже ловят SF.9 /
DRIFT.2**. DRIFT.3 — строго про **area/module-уровень**: разные файлы в каждом модуле
(`hal/gpio.h → ui/x.h` и `ui/btn.cpp → hal/gpio.h`) дают взаимную зависимость модулей
`hal ↔ ui` **без** циклического include. Это Lakos «не-levelizable design»: модули
нельзя собрать/тестировать раздельно, хотя ни один компонент не в цикле.

**DRIFT.3 обязан НЕ перекрывать DRIFT.2:** если взаимность образует реальный
file-cycle — это домен DRIFT.2, DRIFT.3 молчит (не двойной репорт).

## Обоснование (данными)

Corpus-валидация ([drift_signal_validation.md](../../docs/research/drift_signal_validation.md)):
из 1903 cross-area событий **294 (15%) — bidirectional `A↔B`**, и это **реальный
actionable-сигнал** (в отличие от сырой cross-area, где ~50% — норма). Примеры из
корпуса: `src/hal ↔ src/ui` (HAL не должен взаимно зависеть от UI), `Source/Game ↔
Source/Renderer`, `core ↔ inspect`, `editor ↔ engine`. Узкие правила DRIFT.1/DRIFT.2
этот класс сейчас НЕ покрывают.

**Доказательство не-редундантности с DRIFT.2 (corpus):** из **65 коммитов** с
bidirectional area-парой только **9** имели реальный file-cycle (`grown_cycles>0`,
уже пойманы DRIFT.2). Остальные **56 (86%)** — `grown_cycles==0`: взаимная связь
модулей через разные файлы, без циклического include → **DRIFT.2 их пропускает**.
Значит DRIFT.3 ловит 56 коммитов, которые DRIFT.2 не видит. (Перепроверяемо:
скрипт в `experiments/ai_repo_run/*_graph_drift.jsonl`, фильтр bidirectional ∧ grown_cycles==0.)

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
- [ ] **DRIFT.3 НЕ дублирует DRIFT.2:** если взаимность = file-cycle, репортит только DRIFT.2.
- [ ] Fixtures: есть кейс «area-coupling без file-cycle» (DRIFT.3 fires, DRIFT.2 молчит) и «file-cycle» (наоборот).
- [ ] Fixtures pass/fail; правило — отдельный класс/файл.
- [ ] Прогон по корпусу: измерена precision DRIFT.3, зафиксировано решение advisory/gate.

## Заметки

- Это **не** «cross-area gate» (тот ~50% FP, не делаем). DRIFT.3 узкий — только bidirectional.
- Реюзать граф/levelization из существующего `graph/` слоя, не строить новый движок.
