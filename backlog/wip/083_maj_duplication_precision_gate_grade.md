# [SCAN][DUPLICATION] Поднять precision `--duplication` до gate-grade

**Дата создания:** 2026-06-05
**Дата старта:** —
**Статус:** new
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
