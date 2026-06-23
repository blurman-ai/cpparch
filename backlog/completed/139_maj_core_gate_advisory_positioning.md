# [DOCS][PRODUCT] Пересобрать позиционирование вокруг core gate и advisory-слоя

**Дата создания:** 2026-06-23
**Дата старта:** 2026-06-23
**Статус:** completed
**Модуль:** DOCS / PRODUCT / CLI
**Приоритет:** major
**Сложность:** M
**Блокирует:** непротиворечивый внешний нарратив о том, что такое archcheck и чем он не является
**Заблокирован:** —
**Related:** #075 (mvp_v1_trusted_diff_workflow), #093 (flag_argument), #096 (satd_delta), #097 (test_co_evolution), #098 (god_file_growth), #100 (defect_attractor), #101 (local_complexity_drift), #123 (diff_new_clone_gate)

## Цель

Определить и зафиксировать честную продуктовую рамку: как объяснять `archcheck`, если core gate остаётся архитектурным, но рядом уже живёт широкий advisory-слой по новому коду и истории.

## Контекст

Старое позиционирование было простым: «Lakos physical design + Core Guidelines SF.* checks в CI», не линтер, не багфайндер, не IDE-tool. Код уже сложнее этой формулы.

- В `--diff` сегодня есть не только graph drift, но и SATD, test co-evolution,
  local complexity drift, new clone drift и flag-argument drift:
  [src/cli/diff_command.cpp#L165-L211](../../src/cli/diff_command.cpp#L165-L211).
- В CLI уже есть отдельный advisory-режим `--history`:
  [src/main.cpp#L36-L42](../../src/main.cpp#L36-L42).
- Changelog честно показывает эту эволюцию, но README и spec всё ещё в основном
  говорят языком «architecture only»:
  [CHANGELOG.md#L31-L62](../../CHANGELOG.md#L31-L62),
  [README.md#L123-L131](../../README.md#L123-L131).

Пока это не оформлено, у пользователя возникает естественное ощущение противоречия:
инструмент говорит «не лinter», но при этом сигналит про TODO/HACK, тесты, флаги-були и копипаст.

## План выполнения

- [ ] Разложить текущие сигналы на слои: core gate, structural advisory, hygiene advisory, history analytics.
- [ ] Решить, что входит в headline продукта, а что живёт как secondary/advisory capability.
- [ ] Переписать README/spec/CI copy так, чтобы advisory-слой не выглядел scope creep или случайным набором smell-checks.
- [ ] Явно объяснить, почему это всё ещё не clang-tidy replacement: diff-scoped, advisory-first, regression-oriented, без претензии на style/semantic linting.
- [ ] Проверить названия секций и терминов: возможно, нужен устойчивый термин для не-гейтящих сигналов (`advisories`, `delta heuristics`, `PR hygiene layer`).
- [ ] Зафиксировать в нарративе, что gate НЕ однороден между режимами: check/drift гейтят только циклы (SF.9 / DRIFT.1·2·4.CYCLE), а `--diff` гейтит ещё и new god-headers ([include/archcheck/diff/regression_report.h#L79](../../include/archcheck/diff/regression_report.h#L79)). Комментарий [src/cli/check_command.cpp#L95](../../src/cli/check_command.cpp#L95) сейчас врёт («gate = cycles» про diff) — позиционирование не должно повторить эту ложную симметрию (синхронно с #141).

## Сделано

- README и architecture spec получили явную модель слоёв: core gate, structural advisories, PR hygiene advisories, history analytics.
- Объяснено, почему advisory-layer не делает archcheck заменой clang-tidy: diff-scoped, regression-oriented, advisory-first.
- Gate различен по режимам: check = `SF.9`, drift = `DRIFT.1/2/4.CYCLE`, diff = new/grown cycles + new god-headers.
- CI-док описывает advisory blocks vs gating verdict.

## В работе

- (пусто)

## Следующие шаги

- Нет. Новые signal waves должны явно выбирать слой: gate или advisory.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Не прятать advisory-слой под ковёр | он уже shipped и виден пользователю в `--diff`/`--history` |
| Держать core gate отдельно от advisory | именно узкий gate сохраняет trust floor и отличает archcheck от broad smell-lint |
| Аргументировать рамку через режим работы, а не через список тем | SATD/clone/test сигналы допустимы, если остаются diff-scoped и advisory-only |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `README.md` | переписать headline / feature framing |
| `docs/architecture-spec.md` | развести core product и advisory layers |
| `docs/MVP.md` | при необходимости уточнить acceptance language вокруг trusted narrow gates |
| `docs/ci_integration.md` | объяснить advisory blocks vs gating verdict |
