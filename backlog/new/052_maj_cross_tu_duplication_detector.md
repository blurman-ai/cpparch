# [RULES][SCAN] Cross-TU duplication detector (missing reuse edge)

**Дата создания:** 2026-05-29
**Дата старта:** —
**Статус:** new
**Модуль:** RULES / SCAN
**Приоритет:** major
**Сложность:** L (multi-day, разбивается на Stage 0 → 1 → 2, gate после Stage 0)
**Целевой релиз:** v0.2
**Блокирует:** —
**Заблокирован:** #042 (clang_semantic_backend — Stage 2 садится на libclang-infra), #043 (spike_clang_perf — может изменить общий выбор бэкенда)
**Related:** #006 (spec_refactor — двух-бекендная схема), #033 (ai_drift_dataset — duplication = №1 AI-drift сигнал)

## Цель

Завести опциональный проход «cross-TU duplication detector» поверх libclang-бэкенда: находить функции/блоки, переписанные в одном модуле вместо переиспользования из другого. Классифицируется как **missing reuse edge** — дефект физического дизайна, который граф зависимостей по построению не ловит.

Атрибуция: **Lakos** (replication vs hierarchical reuse — почему плохо архитектурно) + **GitClear** (в 2024 копи-паст ×8, впервые обогнал рефакторинг — эмпирическая мотивация). Никакой «вкусовщины».

## Контекст

archcheck сейчас валит запрещённые рёбра графа. Дубликат — это **отсутствующее** ребро: модуль B переписал то, что есть в A, вместо того чтобы зависеть от A. Это отдельная категория, не покрываемая ни SF.*, ни Lakos.{Cycles,GodHeader,ChainLength}, ни Martin'ом. И это №1 измеренный сигнал AI-дрейфа — напрямую усиливает AI-guardrail-позиционирование инструмента (см. #033, [docs/research/constraint_decay.md](../../docs/research/constraint_decay.md)).

**Off by default**, opt-in через `--duplication` / `defaults.duplication: true`. НЕ в zero-config дефолтах v0.1.

## Что уже выяснено спайком (НЕ переоткрывать)

Стартовый спайк живёт в [experiments/clone_detector/clone_spike.cpp](../../experiments/clone_detector/clone_spike.cpp) — две in-memory TU, межмодульный Type-2 клон обнаружен.

1. **Механизм решён.** Встроенный `alpha.clone.CloneChecker` per-TU, бесполезен — большинство клонов меж-TU. Драйвим нижележащий `clang::CloneDetector` (`clang/Analysis/CloneDetection.h`) сами: каждое TU — отдельный `ASTUnit`, **все ASTUnit'ы держать живыми** (StmtSequence ссылается в их `ASTContext`); каждое тело функции через `analyzeCodeBody(FD)` в один общий детектор; `findClones` с пайплайном constraint-ов.
2. **Cross-TU детект подтверждён** спайком (Type-2 с консистентным переименованием, члены в разных файлах).
3. **Пайплайн constraint-ов (clang 18):**
   `RecursiveCloneTypeIIHashConstraint`, `MinComplexityConstraint(N)`, `MinGroupSizeConstraint(2)`, `RecursiveCloneTypeIIVerifyConstraint`, `OnlyLargestCloneConstraint`.
   **Не использовать** «suspicious»/variable-pattern ветку для подсказок про copy-paste — её suggestions точны на ~50% (FIXME в исходнике clang). `MatchingVariablePatternConstraint` через разные `ASTContext` не падает (проверено), но шум он НЕ режет — шум режет `MinComplexity`.
4. **Главный FP-риск — гранулярность.** Детектор репортит клоны на каждом уровне `StmtSequence`, не только функции. Мелкие фрагменты (голый `for`) дают ложные межфайловые «клоны». В спайке: `MinComplexity=5` → клон + шум; `MinComplexity=15` → только клон. Лечится `MinComplexity` + политикой «только клоны уровня функции».
5. **Память — главный НЕизмеренный риск.** Держим все AST сразу. На toy-входе мгновенно; на больших репах — неизвестно. Это и есть Stage 0.
6. **Сборка:** линковка против `libclang-cpp` + `libLLVM` (флаги в README рядом со спайком).

## Non-goals (YAGNI)

- Type-3 (gapped) и Type-4 (семантические) клоны.
- Свой token/winnowing-бекенд для fast-пути без `compile_commands.json` (возможный v2).
- GUI/визуализация.
- Включение в zero-config дефолты.
- Variable-pattern «suspicious copy-paste» подсказки.

## План выполнения

### Stage 0 — Замер перфа/памяти (DECISION GATE)

**Делать первым.** Без зелёного gate Stage 2 не начинать.

- [ ] Harness: взять `experiments/clone_detector/clone_spike.cpp`, заменить строковые исходники на реальный обход `compile_commands.json` (через `clang::tooling::ClangTool` / `buildASTs`, либо `buildASTFromCodeWithArgs` по списку файлов из БД). Детектор/пайплайн/отчёт оставить.
- [ ] Прогнать по возрастанию: (1) `fmt` — baseline, (2) `spdlog` — средняя, (3) `folly` или `llvm-project` — стресс.
- [ ] Записать в таблицу: `repo | LOC | #TU | peak RSS | parse time | findClones time | #cross-component function clones`.
- [ ] Прогнать `findClones` на 2–3 значениях `MinComplexity` (10 / 20 / 40) → как меняется число групп (сигнал/шум).
- [ ] Зафиксировать **решение gate** в `experiments/clone_detector/PERF.md` явно.

**GATE (пороги — согласовать с мейнтейнером, ниже дефолты-заглушки):**
- peak RSS на `spdlog` **> ~4 GB** ИЛИ `findClones` суперлинейно/неюзабельно (> десятков секунд на средней) → **СТОП**. Whole-repo удержание AST отклоняется → Stage 1 в тяжёлом варианте (индекс/fingerprint) или пересмотр фичи.
- В рамках бюджета → Stage 1.

### Stage 1 — Решение по scope: whole-repo vs changed-files / baseline

**Важно:** корпус для сравнения = ВСЯ репа всегда. «Анализировать только дифф» невозможно — клон парный, второй член в неизменённом коде, не с чем сравнивать. `--changed-files` — это **фильтр отчёта** (репортим группу только если хоть один член в changed set), не сужение анализа. `--baseline` — это **заморозка** существующих дублей.

- [ ] На основе цифр Stage 0 выбрать:
  - **whole-repo дешёвый** → анализируем весь корпус; добавить `--baseline` (консистентно с baseline-моделью archcheck) и/или `--changed-files <list>` (PR-режим).
  - **whole-repo тяжёлый** → персистентный индекс фингерпринтов (хеши тел функций), кэшируемый в CI по base-commit; перехеширование только изменённых TU. Тяжёлый путь — только если Stage 0 вынудил.
- [ ] Референс для diff/инкрементальной CI-модели: **Ericsson CodeChecker** (реанализ только изменённых файлов, дифф «new vs fixed»). Копировать модель, не код.
- [ ] **Deliverable:** ≈1 страница design-note (`docs/dev/design_duplication_scope.md`) с рекомендацией одного варианта и обоснованием цифрами Stage 0. Согласовать с мейнтейнером ПЕРЕД интеграцией.

### Stage 2 — Интеграция как опциональный проход (только если gate зелёный)

**Размещение (OCP):**
- [ ] Новый класс `DuplicationRule` под `src/rules/duplication/`, реализующий интерфейс семантического правила (`ISemanticRule` из #042). Регистрация через статическую таблицу. Добавление = новый файл; существующее не правится.

**Переиспользование (DRY):**
- [ ] `scan/clang_scanner` — AST (бэкенд только libclang; нужен `compile_commands.json`).
- [ ] `graph/component_graph` — маппинг `SourceLocation` каждого члена клон-группы → компонент.
- [ ] Существующие `report/{text,json,sarif}_reporter`.

**Логика:**
- [ ] Репортить **только кросс-компонентные** группы; формулировка в словаре графа:
  `"missing reuse edge: components A and B share fragment → candidate shared component"`.
- [ ] **Политика гранулярности:** клоны уровня функции; под-statement шум отфильтровывать (порог `MinComplexity` + отбрасывание под-последовательностей, вложенных в более крупную группу).
- [ ] **Метрика duplication ratio** — в отчёт.

**Конфиг и флаги:**
- [ ] **Off by default.** Включение: CLI `--duplication` ИЛИ `defaults.duplication: true` в `.archcheck.yml`. НЕ в zero-config дефолтах.
- [ ] `thresholds.duplication_min_complexity` (tunable, дефолт подобрать по Stage 0, ориентир из спайка ≥15).
- [ ] Exclude paths для генерёнки/third_party — **обязательно** (protobuf/grpc/generated полны легального дублирования).
- [ ] Уважать `// archcheck: allow`.
- [ ] Вносит в exit code `1` только когда проход включён.

**Тесты (integration-фикстура с известным кросс-компонентным клоном):**
- [ ] Переиспользовать two-module fixture из спайка.
- [ ] Ассерт: планированный клон найден.
- [ ] Ассерт: контрольная не-клон функция НЕ сгруппирована.
- [ ] Ассерт: под-statement шум отфильтрован при выбранном `MinComplexity`.

**Документация:**
- [ ] Запись в rules-доке с атрибуцией (**Lakos** physical design + **GitClear** эмпирика).
- [ ] Явно: проход opt-in и требует `compile_commands.json` (AST-бекенд).

## Критерий приёмки

- [ ] Stage 0: `experiments/clone_detector/PERF.md` с таблицей закоммичен, решение gate зафиксировано.
- [ ] Stage 1: `docs/dev/design_duplication_scope.md` согласован.
- [ ] Проход за флагом, **off by default**.
- [ ] Находит планированный кросс-компонентный клон в фикстуре; нет ложной группы от контрольной функции; под-statement шум отфильтрован.
- [ ] Generated/third_party исключаются; `// archcheck: allow` уважается.
- [ ] Только кросс-компонентные группы; отчёт в словаре графа; duplication ratio в отчёте; json/sarif работают.
- [ ] Нет новых runtime-зависимостей сверх libclang/LLVM.
- [ ] Один класс правила, существующий код не тронут (OCP).

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Дождаться зелёного #043 (общая судьба libclang-бэкенда). Если #043 свернул фичу — переоценить и эту тоже.
2. После #042 фазы 1 (скелет `ISemanticRule` + `SemanticContext` + загрузка `compile_commands.json`) — стартовать Stage 0 на этой инфре.
3. Если Stage 0 — gate red, обсудить fingerprint-индекс с мейнтейнером ДО Stage 1.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Драйвим `clang::CloneDetector` сами, не используем `alpha.clone.CloneChecker` | встроенный чек — per-TU, наш кейс — meж-TU |
| Без `MatchingVariablePatternConstraint` | suggestions ~50% точность (FIXME в clang); шум всё равно режется `MinComplexity` |
| Только клоны уровня функции в отчёт | под-statement матчи дают ложные кросс-файловые группы |
| Off by default | дорогой проход (libclang + удержание AST), AI-drift аудит — opt-in |
| Атрибуция Lakos + GitClear | проектное правило: каждое default-правило с атрибуцией; здесь — авторитет + эмпирика |
| Stage 0 gate ОБЯЗАТЕЛЕН | память на whole-repo AST — главный неизмеренный риск; без цифр в Stage 2 не идём |

## Альтернативы (отклонены / отложены)

- **`alpha.clone.CloneChecker`** — per-TU, для меж-TU кейса бесполезен.
- **Type-3/Type-4** (gapped / семантические) — вне скоупа; точность падает резко, FP-цена выше выгоды.
- **Token/winnowing-бэкенд** для fast-пути без `compile_commands.json` — отложено в возможный v2; AST-бэкенд достаточно для opt-in PoC.
- **«Анализировать только дифф»** — невозможно: клон парный, второй член в неизменённом коде. См. Stage 1 пояснение.
- **Включить в zero-config дефолты v0.1** — нарушает «zero-config first»: дорого, требует `compile_commands.json`, риск шума на legacy-репах.
- **Variable-pattern «suspicious copy-paste» подсказки** — отдельная задача от меж-TU clone detection, ~50% точность.

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `experiments/clone_detector/clone_spike.cpp` | starter — уже лежит, доказывает cross-TU детект (Stage 0 берёт за основу) |
| `experiments/clone_detector/CMakeLists.txt` | сборка спайка против libclang-cpp + libLLVM |
| `experiments/clone_detector/PERF.md` | результат Stage 0 (таблица + gate-решение) |
| `docs/dev/design_duplication_scope.md` | результат Stage 1 (whole-repo vs index, ≈1 стр.) |
| `include/archcheck/rules/duplication/duplication_rule.h` + `src/rules/duplication/duplication_rule.cpp` | Stage 2 — `ISemanticRule` impl |
| `src/rules/rule_set.cpp` | регистрация (gated за `--duplication` / `defaults.duplication`) |
| CLI (entry point) | флаг `--duplication`; YAML — `defaults.duplication`, `thresholds.duplication_min_complexity`, exclude paths |
| `fixtures/duplication/cross_component_clone/` | two-module fixture (повторяет схему спайка как реальные файлы) |
| `tests/...` | integration-тесты на фикстуре |
| `docs/rules/duplication.md` | запись правила, атрибуция Lakos + GitClear |

## Fixtures

- [ ] `fixtures/duplication/cross_component_clone/` — два модуля, один Type-2 клон + одна контрольная не-клон функция; ожидаемый отчёт = 1 кросс-компонентная группа.
- [ ] `fixtures/duplication/sub_statement_noise/` — пара функций с одинаковым голым `for`-циклом; при `MinComplexity ≥ default` — 0 групп.
- [ ] `fixtures/duplication/generated_excluded/` — сгенерированные файлы под exclude-паттерном; 0 групп.

## Pointers

- **Старт:** `experiments/clone_detector/clone_spike.cpp` — доказывает cross-TU детект + пайплайн constraint-ов и содержит свип по `MinComplexity`. Для Stage 0 заменить строковые исходники на обход `compile_commands.json`.
- **API:** `clang/Analysis/CloneDetection.h`. Ключевое: `CloneDetector::analyzeCodeBody(const Decl*)`, `CloneDetector::findClones(result, constraints...)`, `StmtSequence::{getBeginLoc,getContainingDecl}`.
- **Перф-референс:** Ericsson CodeChecker (инкрементальная CI-модель).
- **Эмпирика:** GitClear 2024 — копи-паст ×8, впервые обогнал рефакторинг (см. [docs/research/constraint_decay.md](../../docs/research/constraint_decay.md), [docs/research/ai_drift_runlog.md](../../docs/research/ai_drift_runlog.md)).
