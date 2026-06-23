# [REPORT][CLI] Добавить gate/advisory semantics в JSON отчёт check-mode

**Дата создания:** 2026-06-23
**Дата старта:** 2026-06-23
**Статус:** completed
**Модуль:** REPORT / CLI
**Приоритет:** critical
**Сложность:** S
**Блокирует:** безопасное использование `archcheck --format json` в CI после advisory-first check-mode
**Заблокирован:** #141 (centralize_gate_policy) — JSON-репортёр должен получать готовую gate/advisory classification из code-level helper, а не зашивать строковые проверки заново
**Related:** #055 (dogfood_json_escape_dedup), #075 (mvp_v1_trusted_diff_workflow), #133 (first_run_noise_floor), #136 (cli_docs_contract_after_advisory_wave)

## Цель

Сделать JSON-отчёт обычного `check` режима честным для CI: JSON должен явно говорить, какие findings гейтят exit code, а какие advisory.

## Контекст

После `#133` plain check-mode стал advisory-first: exit 1 только на `SF.9`, а `SF.7`,
`SF.8`, `Lakos.GodHeader`, `Lakos.ChainLength` печатаются, но не ломают CI:
[src/cli/check_command.cpp#L90-L107](../../src/cli/check_command.cpp#L90-L107).

Text-output это объясняет строкой `note: ... advisory finding(s) ... gate fails only on dependency cycles`.
JSON-output такой информации не содержит:
[src/report/json_reporter.cpp#L11-L33](../../src/report/json_reporter.cpp#L11-L33).

Итоговый конфликт:

- команда `archcheck --format json <repo>` может вернуть `exit 0`;
- при этом JSON содержит `"violations": [...]`;
- внутри JSON нет поля `gate`, `severity`, `advisory`, `gating`, или другого способа
  понять, почему violations есть, а CI зелёный.

Для машинного потребителя это хуже, чем текстовый режим: stable schema есть, но в ней нет
самого важного CI-смысла.

## План выполнения

- [ ] Зафиксировать check-mode JSON contract: top-level `gate: ok|fail` и/или раздельные `gating`/`advisory` списки.
- [ ] Не ломать без нужды существующий schema version: если меняется форма, явно поднять `version` или добавить backward-compatible поля.
- [ ] Прокинуть gate classification из CLI/report layer в `writeJsonReport`, не дублируя строковые проверки по всему коду.
- [ ] Добавить e2e: advisory-only fixture → JSON содержит findings + `gate: ok` + exit 0.
- [ ] Добавить e2e: SF.9 fixture → JSON содержит `gate: fail` + exit 1.
- [ ] Обновить README/CI docs в рамках #136: machine-readable output нельзя описывать просто как список violations.

## Сделано

- `archcheck --format json` получил top-level `gate: ok|fail`.
- Каждая finding в check JSON получила `disposition: advisory|gating`.
- JSON reporter использует `rules/gate_policy`, не дублирует rule-id checks.
- E2E покрывает advisory-only JSON (`SF.7`, exit 0, `gate: ok`) и gating JSON (`SF.9`, exit 1, `gate: fail`).
- `CHANGELOG.md` фиксирует additive JSON schema change.

## В работе

- (пусто)

## Следующие шаги

- Нет. При будущем schema-breaking изменении отдельно поднять/обсудить JSON version.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| JSON должен нести gate verdict | иначе machine-readable CI output противоречит exit code |
| Advisory findings остаются в отчёте | скрывать их из JSON нельзя: пользователь потеряет полезный сигнал |
| Classification не копировать в reporter | reporter должен получать уже рассчитанную семантику, иначе drift повторится |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `include/archcheck/report/json_reporter.h` | расширить контракт JSON report |
| `src/report/json_reporter.cpp` | добавить gate/advisory поля |
| `src/cli/check_command.cpp` | передавать classification в reporter |
| `tests/integration/cli/cli_smoke_e2e_test.cpp` | e2e для JSON advisory/gating semantics |
| `tests/unit/report/json_reporter_test.cpp` | unit-контракт схемы, если отдельный тест уместен |
