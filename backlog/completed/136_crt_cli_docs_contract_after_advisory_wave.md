# [DOCS][CLI] Синхронизировать help/README с реальным CLI-контрактом после advisory-first wave

**Дата создания:** 2026-06-23
**Дата старта:** 2026-06-23
**Статус:** completed
**Модуль:** DOCS / CLI
**Приоритет:** critical
**Сложность:** S
**Блокирует:** публичный README/help как доверенный контракт для первого запуска и CI
**Заблокирован:** #141 (centralize_gate_policy) для код-части (help) — описывать gate-контракт лучше после code-level SSOT, иначе help снова разойдётся с россыпью строковых проверок. README-часть может идти в docs-кластере (#139 → #137 → #136)
**Related:** #044 (docs_readme_sync_shipped), #045 (docs_sync_roadmap_mvp_spec), #075 (mvp_v1_trusted_diff_workflow), #118 (drift4_lateral_rule), #133 (first_run_noise_floor)

## Цель

Привести `--help`, `README.md` и CI-facing описание CLI к тому, что бинарь реально делает сегодня: что gate, что advisory, и какие режимы вообще существуют.

## Контекст

После волны `#075/#096/#097/#101/#118/#123/#133` код и пользовательские тексты снова разъехались.

- `--drift-baseline` в help и README всё ещё описан как gate только для `DRIFT.1/DRIFT.2`, но код уже гейтит и `DRIFT.4.CYCLE`:
  [src/main.cpp#L31-L47](../../src/main.cpp#L31-L47),
  [src/cli/check_command.cpp#L74-L87](../../src/cli/check_command.cpp#L74-L87).
- Обычный `check` после `#133` advisory-first: exit 1 только на `SF.9`, а `SF.7/SF.8/Lakos.*` остаются advisory.
  Это зафиксировано в коде и changelog, но в README всё ещё читается как более общий
  «non-zero on failure» без чёткого контракта:
  [src/cli/check_command.cpp#L90-L107](../../src/cli/check_command.cpp#L90-L107),
  [CHANGELOG.md#L76-L89](../../CHANGELOG.md#L76-L89),
  [README.md#L23-L35](../../README.md#L23-L35).
- `--diff` давно шире, чем «graph regressions only»: кроме графа он уже печатает SATD,
  test co-evolution, local complexity, new clone drift и flag-argument drift:
  [src/cli/diff_command.cpp#L143-L211](../../src/cli/diff_command.cpp#L143-L211).
  В README это отражено частично и несистемно.
- В help уже есть `--history`, но публичный нарратив README вокруг него почти отсутствует:
  [src/main.cpp#L36-L42](../../src/main.cpp#L36-L42).

Итог: сейчас пользователь не может по одним только `README` и `--help` надёжно понять,
какие сигналы ломают CI, а какие лишь советуют.

## План выполнения

- [ ] Снять фактическую матрицу CLI-поведения: режим, exit-semantics, gating/advisory, формат вывода.
- [ ] Обновить `src/main.cpp::print_help()`, чтобы `--drift-baseline` и `check` описывали текущий gate-контракт.
- [ ] Переписать `README.md` разделы `What it does`, `Quick start`, `Status` под advisory-first модель.
- [ ] Проверить `docs/ci_integration.md` и пример workflow: нет ли там старого контракта «любое нарушение = fail».
- [ ] Дожать e2e/CLI-тесты на help-текст и статусные формулировки, чтобы следующая волна снова не уехала молча.

## Сделано

- `src/main.cpp::print_help()` синхронизирован с gate/advisory контрактом check, drift and diff.
- README переписан под advisory-first модель и check JSON `gate` / `disposition`.
- `docs/ci_integration.md` описывает `--diff` как gating verdict + advisory blocks, JSON как shipped, SARIF как future.
- CLI E2E проверяет help-текст для `SF.9`, `DRIFT.4.CYCLE` и diff gate формулировки.

## В работе

- (пусто)

## Следующие шаги

- Нет. При изменении gate policy сначала менять `rules/gate_policy`, затем help/docs/tests.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Править help и README вместе | это один пользовательский контракт; частичная синхронизация быстро протухает |
| Описывать gate/advisory явно по каждому режиму | формула `exit non-zero on failure` уже недостаточна и вводит в заблуждение |
| Проверять ещё и CI-доки | именно там старый контракт больнее всего бьёт по ожиданиям пользователей |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `src/main.cpp` | обновить текст `--help` под фактические gate/advisory правила |
| `README.md` | синхронизировать публичное описание CLI |
| `docs/ci_integration.md` | выровнять CI-нарратив с advisory-first моделью |
| `tests/integration/cli_smoke_e2e_test.cpp` | закрепить help/status-контракт тестом |
