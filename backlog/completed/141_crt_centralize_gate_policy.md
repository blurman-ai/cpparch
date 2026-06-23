# [CLI][RULES] Централизовать gate/advisory policy для check/drift/diff

**Дата создания:** 2026-06-23
**Дата старта:** 2026-06-23
**Статус:** completed
**Модуль:** CLI / RULES / DIFF
**Приоритет:** critical
**Сложность:** M
**Блокирует:** устойчивость CLI-контракта: один источник истины вместо ручной синхронизации кода, help и comments; код-предпосылка для #140 и код-части #136
**Заблокирован:** —

> **Приоритет поднят major→critical (2026-06-23):** задача жёстко блокирует два `critical` (#140, #136); `major`-предпосылка двух critical мислейблена. Если решено, что #140/#136 не обязаны ждать helper — вернуть major и снять их блок-статус.
**Related:** #075 (mvp_v1_trusted_diff_workflow), #118 (drift4_lateral_rule), #133 (first_run_noise_floor), #136 (cli_docs_contract_after_advisory_wave), #140 (check_json_gate_advisory_schema)

## Цель

Убрать размазанную gate/advisory логику из разных мест кода и завести один небольшой code-level источник истины для того, какие finding-и ломают CI в каждом режиме.

## Контекст

Сейчас gate policy уже не простая, но живёт строковыми проверками и комментариями в разных местах.

- Check-mode гейтит только `SF.9`:
  [src/cli/check_command.cpp#L90-L107](../../src/cli/check_command.cpp#L90-L107).
- Drift-baseline гейтит `DRIFT.1`, `DRIFT.2`, `DRIFT.4.CYCLE`:
  [src/cli/check_command.cpp#L74-L87](../../src/cli/check_command.cpp#L74-L87).
- Diff-mode гейтит grown cycles и new god-headers:
  [include/archcheck/diff/regression_report.h#L77-L79](../../include/archcheck/diff/regression_report.h#L77-L79).
- Help и headers уже отстали от кода:
  [src/main.cpp#L31-L47](../../src/main.cpp#L31-L47),
  [include/archcheck/rules/rule_set.h#L15-L21](../../include/archcheck/rules/rule_set.h#L15-L21),
  [src/cli/check_command.h#L25-L31](../../src/cli/check_command.h#L25-L31).

Это ровно тот механизм, который уже породил `#136`: DRIFT.4.CYCLE добавили в код, но не во
все описания. Если оставить строковые проверки россыпью, следующий rule-wave повторит дрейф.

## План выполнения

- [ ] Ввести маленький helper/тип для classification: `gating` vs `advisory` по режиму (`check`, `drift`, `diff`).
- [ ] Перевести `reportCheckGate` и `reportDriftGate` на этот helper.
- [ ] Дать JSON-репортёру из #140 готовую classification, а не заставлять его знать rule IDs.
- [ ] Обновить stale comments в `rule_set.h` и `check_command.h`.
- [ ] Добавить unit-тесты policy: `SF.9` gates check, `SF.7` advisory; `DRIFT.4.CYCLE` gates drift, `DRIFT.4.NEW` advisory.
- [ ] Убедиться, что diff-mode не смешан с rule-id policy: у него structural `RegressionReport::gates()`, это отдельная ветка.
- [ ] Развести в helper-е и комментариях НЕоднородный gate: check/drift гейтят только циклы, а diff гейтит ещё и new god-headers ([include/archcheck/diff/regression_report.h#L79](../../include/archcheck/diff/regression_report.h#L79)). Поправить врущий комментарий [src/cli/check_command.cpp#L95](../../src/cli/check_command.cpp#L95) («Mirrors the --diff/drift gating model (gate = cycles)» — для diff это неверно). Централизация не должна зафиксировать ложную симметрию (синхронно с #139).

## Сделано

- Добавлен `rules/gate_policy`: единая classification `gating` / `advisory` для `check` и `drift`.
- `check` gate теперь централизованно считает только `SF.9`.
- `drift` gate теперь централизованно считает `DRIFT.1`, `DRIFT.2`, `DRIFT.4.CYCLE`.
- Обновлены stale-comments в `rule_set.h` и `check_command.h`.
- Добавлены unit-тесты gate policy.

## В работе

- (пусто)

## Следующие шаги

- Нет. Diff-mode gate остаётся отдельным `RegressionReport::gates()` по структурным полям.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Это не generic rule engine | нужна только CI-severity classification, не новая архитектура правил |
| Diff-mode оставить отдельным structural report | там gate определяется не ruleId, а структурными полями report-а |
| Комментарии обновлять вместе с helper-ом | stale comments уже стали частью проблемы |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `include/archcheck/cli/` или `include/archcheck/rules/` | новый минимальный helper для gate/advisory policy |
| `src/cli/check_command.cpp` | использовать helper вместо inline string checks |
| `include/archcheck/rules/rule_set.h` | обновить drift-rule комментарий |
| `src/cli/check_command.h` | обновить baseline/drift комментарий |
| `tests/unit/cli/` или `tests/unit/rules/` | unit-тесты policy |
