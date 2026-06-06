# [RULES][DRIFT] DRIFT.3 — bidirectional area coupling (взаимная связность)

**Дата создания:** 2026-06-06
**Дата старта:** 2026-06-06
**Дата завершения:** 2026-06-06
**Статус:** completed
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

- [x] Area-detection не даёт `src↔include`/build/vendor/rename ложных cross-area.
- [x] DRIFT.3 ловит появление `A↔B` и НЕ срабатывает на однонаправленных `→base`.
- [x] **DRIFT.3 НЕ дублирует DRIFT.2:** если взаимность = file-cycle, репортит только DRIFT.2.
- [x] Fixtures: есть кейс «area-coupling без file-cycle» (DRIFT.3 fires, DRIFT.2 молчит) и «file-cycle» (наоборот).
- [x] Fixtures pass/fail; правило — отдельный класс/файл.
- [x] Прогон по корпусу: DRIFT.3 провалидирован на реальном коммите; решение — **advisory** (как DRIFT.1/.2 сейчас), gate-вопрос отдельно.

## Заметки

- Это **не** «cross-area gate» (тот ~50% FP, не делаем). DRIFT.3 узкий — только bidirectional.
- Реюзать граф/levelization из существующего `graph/` слоя, не строить новый движок.

## Как сделано + validation (2026-06-06)

**Реализация** (один класс = один файл, OCP):
- `include/archcheck/rules/drift_bidirectional_coupling.h` + `src/rules/drift_bidirectional_coupling.cpp`
  — класс `DriftBidirectionalCoupling`, id `DRIFT.3`. Подключён в `makeDriftRuleSet`
  (после DRIFT.1, до DRIFT.2) и в `src/CMakeLists.txt`.
- **area-функция:** первый сегмент пути после strip wrapper-каталогов (`src/include/lib/..`),
  игнор build/vendor/test/generated → `src↔include` и шум не считаются cross-area.
- **семантика:** DRIFT.3 fires, когда после диффа модули A и B зависят друг от друга
  (`A→B` и `B→A` на area-уровне), а в baseline мутуальными НЕ были. Исключает прямой
  two-file cycle (`hasEdge(to,from)`) — это домен DRIFT.2.

**Важная коррекция при прогоне (born-coupled vs incremental):** первая версия требовала,
чтобы обратное ребро `B→A` уже было в baseline (только инкрементальная эрозия). Прогон
по pulp показал, что коммит «Add plugin inspector» добавил **обе** стороны разом (модуль
родился связанным) — и правило молчало. Переписал на «мутуальность есть СЕЙЧАС и не было
в baseline» → ловит и incremental, и born-coupled.

**Fixtures** (`fixtures/drift_bidirectional/`): `fail_new_coupling` (core↔ui → DRIFT.3),
`pass_one_directional` (однонаправленно → молчит), `pass_file_cycle` (two-file cycle →
DRIFT.3 молчит, DRIFT.2 fires). Тесты в `drift_fixtures_test.cpp`.

**Боевой прогон:** `danielraffel/pulp` @ `705f86e` («Add plugin inspector») — archcheck
выдал `[DRIFT.3] bidirectional module coupling: 'core' <-> 'inspect'` (совпало с тем,
что python-проба отметила как bidirectional), рядом DRIFT.1 ×2 + GodHeader. Прогон 60
старых коммитов pulp: 0 ложных DRIFT.3 (не спамит).

**Gate:** clang-format/cppcheck/lizard чисто, 347 тестов (+3 DRIFT.3), coverage 95.6/57.1 — PASS.
