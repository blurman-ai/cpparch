# [RULES] AI-assisted rule synthesis contract

**Дата создания:** 2026-05-26
**Дата старта:** —
**Статус:** new
**Модуль:** RULES
**Приоритет:** major
**Сложность:** M (дизайн 1-2 дня, реализация — отдельной таской)
**Целевой релиз:** v0.3+ (не v0.1)
**Блокирует:** —
**Заблокирован:** #008 (dependency_graph_foundation — synthesis может опираться на тот же graph contract)
**Related:** #009 (drift_regression_rules — пара synthesis ↔ drift)

## Цель

Зафиксировать **контракт** на процесс «AI-assisted rule synthesis» — без реализации, но достаточно конкретно, чтобы будущая имплементация была однозначной. Чтобы у читателя спеки, контрибьютора и пользователя была одна и та же картинка того, что archcheck делает, чего не делает, и где границы.

## Контекст

В спеке v2.1 появился термин **AI-assisted rule synthesis** — процесс, когда AI-агент читает репозиторий, строит архитектурную гипотезу и превращает её в проверяемые правила. Концептуально парный с DRIFT-правилами: synthesis выводит контракт, DRIFT следит, чтобы он не размывался.

В спеке зафиксировано:
- Operational shape: отдельный shell-флоу (`archcheck synthesize`), не магия внутри обычного запуска.
- archcheck-ядро **не** вызывает LLM само; synthesis — это либо локальные эвристики, либо отдельный wrapper-агент (Claude Code / Cursor / самописный скрипт), читающий тот же репозиторий и выдающий YAML.
- Output — `.archcheck.yml.draft`, который пользователь явно подтверждает.
- Risk-набор: false hypothesis, privacy, volatility (см. §«Ключевые риски» п.6).

Эта задача формализует это в полноценный контракт с примерами вызова, схемой output-а, режимами и интеграционными точками. Без него любая имплементация будет re-litigate базовые вопросы.

## План выполнения

- [ ] **CLI-интерфейс.** Зафиксировать subcommand:
   - `archcheck synthesize` — основное приглашение.
   - Какие флаги (`--out`, `--from-git`, `--mode=heuristic|wrapper-prompt`, `--accept-baseline`).
   - Exit codes.
- [ ] **Two modes.**
   - **Heuristic mode** (default, всегда доступен): локальные правила вывода без LLM. Источники — path-структура, common header detection, facade pattern по фактическим include-ам.
   - **Wrapper-prompt mode**: archcheck выдаёт structured prompt, который пользователь скармливает любому агенту (Claude Code, Cursor, локальная Llama). Output агента возвращается обратно через `archcheck synthesize --apply-wrapper-output <file>`. Это сохраняет приватность — пользователь сам решает, какому агенту доверить.
- [ ] **Output schema.** Что именно лежит в `.archcheck.yml.draft`:
   - YAML-валидный конфиг с теми же ключами, что и обычный (`modules`, `rules`, `defaults`, `thresholds`).
   - **Каждое synthesized правило помечено комментарием `# synthesized: <source heuristic> [confidence: low|med|high]`.**
   - Поле `version: 1` + `synthesized: true` в шапке.
   - Хеш source-репозитория (например, git HEAD), чтобы было видно от чего синтезировали.
- [ ] **User confirmation flow.** Как пользователь принимает synthesized config:
   - Default: synthesized rules идут в `report-only`-режиме. archcheck отчитывает «вот что synthesis считает, ничего не валит».
   - При явном `archcheck synthesize --confirm` или ручном переименовании `.draft → .archcheck.yml` правила становятся реальными.
   - В `report-only` отчёте — diff с тем, что уже в `.archcheck.yml` (если есть).
- [ ] **Heuristic catalog (минимальный для v0.3).** Какие именно эвристики реализуются в heuristic mode:
   - Путь-как-модуль: каталоги верхнего уровня под `src/` становятся модулями.
   - Facade detection: если `src/A/*.cpp` 90%+ обращается к `src/B/` через один заголовок — это inferred facade.
   - Layered hint: если `src/domain/` существует — предполагается `domain forbidden_deps: [infrastructure, ui]` если такие тоже есть.
   - Confidence rating для каждой эвристики.
- [ ] **Wrapper-prompt format.** Что archcheck даёт агенту:
   - Compact YAML-описание текущей структуры (modules с paths, существующие fan-in/fan-out, top hubs).
   - Списко-кандидаты модулей и предполагаемых boundaries.
   - Шаблон output (тот же YAML format, заполнить).
- [ ] **Privacy guarantees.** В документации явно:
   - archcheck heuristic mode полностью offline.
   - wrapper-prompt mode — пользователь сам выбирает кому передать prompt.
   - Никакой телеметрии в synthesize-команде.
- [ ] **Volatility mitigation.** Как synthesize обрабатывает повторные запуски:
   - Не перезатирает существующий `.archcheck.yml.draft`, выдаёт diff.
   - Heuristic mode полностью детерминированный (одинаковый репо → одинаковый output).
   - Wrapper-prompt mode — non-deterministic, но это явно помечается в output-е (`source: wrapper-agent <agent-id>` + дата).
- [ ] **Reference scenarios.** Маленькие test-проекты в `fixtures/synthesize/`:
   - `clean_layered/` — три каталога, очевидная иерархия → synthesis выводит соответствующий `forbidden_deps`.
   - `mixed_old_new/` — частично legacy, partially refactored → synthesis помечает unclear modules как `unknown`.
   - `monolith/` — нет очевидной структуры → synthesis возвращает пустой draft с warning.

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Зафиксировать CLI-интерфейс и output schema (это блокирующее решение для последующих шагов).
2. Решить, реализуем ли heuristic mode и wrapper-prompt mode оба сразу, или сначала heuristic.
3. После закрытия #010 — заводить отдельную implementation-задачу (на v0.3).

## Ключевые решения

| Решение | Причина |
|---------|---------|
| archcheck сам не вызывает LLM | Privacy + детерминизм ядра. LLM-логика — снаружи, через wrapper-prompt. |
| Synthesis — отдельный shell-флоу, не часть обычного запуска | Чтобы `archcheck` в CI оставался быстрым и offline. |
| Output — всегда `.archcheck.yml.draft`, не сразу в `.archcheck.yml` | Пользователь должен явно подтверждать, никакой auto-apply магии. |
| Каждое synthesized правило помечено комментарием с источником и confidence | Когда через полгода правило начнёт мешать — будет видно почему оно появилось. |
| Heuristic mode = детерминированный | Чтобы CI-результаты были воспроизводимы. |
| Wrapper-prompt mode = опциональный | Не каждый пользователь готов давать код LLM-у; heuristic должен покрывать базовые сценарии. |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `docs/architecture-spec.md` | секция «AI-assisted rule synthesis» расширится контрактом |
| `docs/dev/synthesis_contract.md` (новый) | подробный контракт CLI + output schema (если выйдет длинным) |
| `src/cli/synthesize.cpp` (будущее) | имплементация subcommand-а (отдельной задачей) |
| `fixtures/synthesize/*` (будущее) | reference-сценарии (отдельной задачей) |

## Fixtures (контрактный уровень, без кода)

- [ ] Образец `.archcheck.yml.draft` для clean_layered проекта (как выглядит output)
- [ ] Образец wrapper-prompt output schema (что агент должен вернуть)
- [ ] Образец diff-вывода при повторном synthesize (с уже существующим draft)
