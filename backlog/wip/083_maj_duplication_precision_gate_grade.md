# [SCAN][DUPLICATION] Поднять precision `--duplication` до gate-grade

**Дата создания:** 2026-06-05
**Дата старта:** 2026-06-05
**Статус:** wip
**Модуль:** SCAN/DUPLICATION
**Приоритет:** major
**Сложность:** L
**Блокирует:** опциональный blocking `--duplication` gate в CI
**Заблокирован:** —
**Related:** #082 (alignment umbrella — родитель, Slice 5b), #070 (FP fix proposals + спека header-impl gate), #071 (FP/TP classification), #078 (co-change harm-signal / severity)

## Цель

Поднять precision детектора дубликатов до уровня, на котором `--duplication` можно
безопасно сделать **blocking CI-gate** (падать на регрессии дублирования), а не только
advisory report.

## Контекст (откуда задача)

В рамках #082 (Slice 5b) duplication сознательно шипнут как **advisory reporting
capability** (`--duplication` — report-only, всегда `exit 0`, не блокирует CI). Это
честная рамка: [docs/product_vision.md](../../docs/product_vision.md) прямо фиксирует,
что **precision недостаточен для trusted CI-gate** (idiom-floor ~40 FP, неустранимый
без семантики). Поднять precision — это **algorithm work**, который #082 вынесла из
scope («Не улучшать сам алгоритм duplication "заодно"»). Эта задача — про то, что #082
отложила.

## Что нужно

1. Реализовать честный **P1.3 header-impl gate** (сейчас удалён как no-op; готовая
   спека — в [#070](../wip/070_maj_checker_fp_fix_proposals.md): matched-span ≥70%
   decl-токенов → suppress).
2. Измерить precision на размеченном корпусе (см. также #084 — харнесс оценки).
3. Оценить **P2 semantic/LLM confirm** (в CHANGELOG помечен `⏳ v0.2`) как путь пробить
   idiom-floor.
4. Только при достижении gate-grade precision — добавить опцию gating
   (`--duplication --gate` или порог), с явным exit-контрактом.

## Acceptance criteria

- [ ] Измеримая precision на корпусе с зафиксированной методикой.
- [ ] P1.3 (или эквивалент) реализован и покрыт fixtures/тестами.
- [ ] Принято решение: достаточно ли precision для опционального gate; если да —
      gate-режим реализован с явным exit-кодом и документирован во всех слоях.
- [ ] Если precision всё ещё недостаточен — это явно зафиксировано, `--duplication`
      остаётся advisory, задача закрыта с честным выводом.

## Заметки

- Не ломать текущий advisory-контракт (`exit 0`) по умолчанию — gating только opt-in.
- fixtures обязательны (правило проекта).

## Scoping / разведка (2026-06-05, при старте)

Разобраны структуры данных сканера для реализации P1.3:

- `Fragment` ([include/archcheck/scan/duplication/fragmenter.h](../../include/archcheck/scan/duplication/fragmenter.h))
  даёт `seq` (упорядоченные нормализованные токены), `rawSeq` (сырые написания,
  выровнены с seq), `normLines`, `diversity`, `tokenCount`. Этого достаточно, чтобы
  считать declaration-маркеры (`virtual`/`override`/`final`/access-specifier/`Q_OBJECT`/
  pure-`= 0`) и контроль-флоу (`if`/`for`/`while`/`return`/`switch`).
- Существующие P1-классификаторы (`phase10DataTableClassifier`,
  `phase11BoilerplateDensity`) — чистые ~15-25-строчные функции: P1.1 down-weight
  (`p.weighted *= k`), P1.2 filter (drop). P1.3 встанет тем же паттерном.
- **Важно:** `extractFragments` эмитит тела `)`-`{` (function/control bodies), а не
  чистые декларации. Значит header-impl FP возникает в специфических формах (inline-тела
  в .h, init-list конструкторов, параллельные определения). Точную форму нужно увидеть
  на 6 размеченных header-impl примерах из корпуса.

### Почему имплементация не сделана при старте (честно)

1. **Recall-gate требует корпуса.** Спека #070 прямо требует прогон против 195 TP с
   метрикой TP-retention и откатом при падении recall. Корпус (`fp_corpus_r2.tsv`,
   6 header-impl FP) — в **untracked `experiments/`**, не hermetic. Влить precision-
   suppressor, не измерив recall, — нарушение этоса #082 (не шипать невалидируемое).
2. **Эвристика decl-vs-executable нетривиальна** на токенах (отличить `void foo();` от
   `foo();` без парсера). Ошибка → либо нет пользы, либо суппресс реальных TP.

### План реализации (когда есть корпус)

1. Посмотреть 6 размеченных header-impl FP → понять фактическую форму fragment'ов.
2. Реализовать `phase12HeaderImplGate` (имя свободно — no-op удалён в #082) как
   консервативный классификатор: declRatio ≥0.7 **И** <2 executable-statement → suppress.
3. Fixtures: `fixtures/duplication/header_impl/pass/` (реальный logic-клон — НЕ суппрессить),
   `fixtures/duplication/header_impl/fail_*/` (параллельные декларации — суппресс).
4. Прогон recall-retention против корпуса; откат при падении recall > порога.
5. Только при достаточной aggregate precision — решать про opt-in gate (item 4 выше).

**Статус:** стартована, заскоуплена; имплементация P1.3 ждёт доступа к корпусу для
валидации recall (или явного решения пользователя реализовать без corpus-гейта).
