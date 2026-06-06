# [RULES][DRIFT] DRIFT.2 (cycle growth) → default blocking gate

**Дата создания:** 2026-06-06
**Дата старта:** —
**Статус:** new
**Модуль:** RULES/DRIFT
**Приоритет:** major
**Сложность:** M
**Блокирует:** —
**Заблокирован:** —
**Related:** #009 (DRIFT-правила), #082 (alignment umbrella), [docs/research/drift_signal_validation.md](../../docs/research/drift_signal_validation.md) (corpus-доказательство)

## Цель

Сделать DRIFT.2 (рост/появление циклов зависимостей) **default blocking gate**
(exit `1`), а не просто репорт.

## Обоснование (данными)

Corpus-валидация ([drift_signal_validation.md](../../docs/research/drift_signal_validation.md)):
DRIFT.2 срабатывает на **72 из 135 092 коммитов = 0.05%** по 310 репозиториям.
Это исключительно низкий false-alarm rate, а новый цикл — объективная архитектурная
регрессия (Lakos physical design). Сочетание «редко + объективно» = созрело для
жёсткого гейта. Обычные линтеры этот класс не ловят.

## Что нужно

1. Подтвердить текущее поведение DRIFT.2 в `--drift-baseline` (сейчас репортит; какой
   exit code?).
2. Сделать DRIFT.2 blocking по умолчанию в drift-режиме (exit `1` при росте циклов).
3. Проверить, что pre-existing циклы (в baseline) НЕ ломают — гейтим только **новые/
   выросшие** циклы, не legacy.
4. Fixtures: `fixtures/drift_cycle_growth/` уже есть — дополнить pass/fail под gate-семантику.
5. Документировать exit-контракт во всех слоях (help/docs/CHANGELOG).

## Acceptance criteria

- [ ] DRIFT.2 ломает сборку (exit 1) при появлении/росте цикла в drift-режиме.
- [ ] Pre-existing циклы из baseline не вызывают ложного fail.
- [ ] Fixtures pass/fail покрывают gate-поведение.
- [ ] Контракт отражён в help/docs/CHANGELOG (не остаётся «advisory» формулировок про DRIFT.2).

## Заметки

- Не трогать DRIFT.1 семантику в этой задаче (отдельный сигнал).
- Cross-area raw НЕ гейтить (см. валидацию — ~50% FP).
