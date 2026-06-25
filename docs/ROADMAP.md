# archcheck — ROADMAP

_2026-06-25 · phase: **v0.1.0 released — stabilising; v0.2 (dependency policy rules) next**_

## Product framing

`archcheck` — это **dependency-diff / drift guardrail для C++ pull requests**.

Он нужен, чтобы поймать до merge:

- новые include-зависимости;
- новые циклы;
- рост dependency knot / SCC;
- новые связи между областями проекта;
- позже — нарушения явно заданных модульных границ.

Главный продуктовый тезис:

> This PR added a dependency you probably did not want.

Это **не**:

- AI attribution tool;
- duplication-first checker;
- универсальная quality platform;
- bug finder / formatter / include optimizer.

«Что в работе прямо сейчас» — `backlog/wip/`.
Очередь — `backlog/new/`.
«Что уже зашипилось» — [CHANGELOG.md](../CHANGELOG.md).
Боевые прогоны и исследовательские выводы — [milestones.md](milestones.md).
Продуктовая рамка подробнее — [product_vision.md](product_vision.md).

---

## Current focus

**v0.1.0 зарелизен** (2026-06-25): trusted graph/drift/diff core зашипился, prebuilt
Linux-бинарь живёт в GitHub Releases для pinned CI-install. Фаза сменилась с «довести
core» на **стабилизацию + переход к v0.2**.

Приоритеты текущей фазы:

- стабилизировать `--diff`-в-CI: shallow base-fetch (#143), exit 2 на нерезолвящемся
  ref (#144);
- public-readiness: outward-facing GitHub demo-репо для new-clone drift (#123);
- начать v0.2 — enforcement модульных правил из `.archcheck.yml` (ADR-001).

Что зашипилось и больше не «в фокусе доведения» (детали — в [CHANGELOG.md](../CHANGELOG.md)):

- graph/drift/diff — продуктовый wedge, в релизе;
- duplication — advisory reporting (`--duplication`, new-clone drift в `--diff`), не blocking gate;
- cheap-drift advisories (SATD, test co-evolution, local complexity, flag-argument ARG.1,
  bool-field accretion) — в `--diff`, advisory-first;
- AI-vs-human drift — исследование (вывод: agentic-подписи дрейфа нет, гибнет под repo FE),
  не продуктовый promise.

---

## v0.1 — Trusted Dependency Diff for PRs (released 2026-06-25)

**Цель:** показать архитектурные dependency changes, внесённые PR, почти без настройки.

**Cover-story:** пользователь запускает `archcheck` в CI и получает понятный
ответ, ухудшил ли этот diff dependency graph.

### Product core

- include graph extraction без обязательного `compile_commands.json`;
- baseline save/load;
- graph baseline + drift comparison;
- PR/diff workflow;
- deterministic text/json output;
- documented exit codes `0 / 1 / 2 / 3`;
- advisory-first default;
- строгий gate только для самых надёжных regressions;
- advisory-вывод копипаста, внесённого коммитом, в `--diff` (решение 2026-06-13:
  показывать его уже в v0.1; блокирующий duplication-gate остаётся v0.2).

### Trusted signals

- intrinsic include-graph checks (`SF.7`, `SF.8`, `SF.9`, Lakos-style defaults);
- `DRIFT.1`, `DRIFT.2`;
- `DRIFT.4.CYCLE` (новая латеральная связь между модулями, образующая цикл с
  baseline-обратным ребром);
- новые нежелательные dependency edges;
- новые циклы;
- рост SCC / dependency knot;
- новые cross-directory / cross-area зависимости.

### Что доделываем в v0.1

- убрать ложные и недоведённые user-facing контракты;
- сделать diff/drift output стабильным и explainable;
- привести roadmap/docs/CLI к реальному состоянию;
- удержать zero-config сигнал полезным и безопасным для первого включения.

### Что не является центром v0.1

- duplication как blocking gate;
- AI attribution;
- AI rule synthesis;
- широкая AST semantic platform;
- visualization / plugin API / broad C support.

Duplication в `v0.1` шипнута как **advisory report** (`--duplication`,
report-only, exit 0) — поддерживаемая фича, но не blocking gate и не центр v0.1.

---

## v0.2 — Dependency Policy Rules

**Цель:** превратить `archcheck` из zero-config dependency diff в dependency diff
плюс enforceable project policy.

### Scope

- `.archcheck.yml` как реальная runtime-фича;
- modules / path mapping;
- `forbidden` dependencies;
- `layers`;
- `independence`;
- более понятные violation explanations;
- CI examples;
- optional SARIF там, где он помогает adoption.

### Supporting expansion

- fan-out growth;
- blast-radius growth;
- coupling growth;
- selective `libclang` backend только там, где он улучшает enforceable checks.

Важно: продуктовая история `v0.2` — не “теперь у нас libclang”, а
“теперь можно проверять мои архитектурные границы”.

---

## v0.3+ — Selective Semantic Expansion

Семантическое расширение идёт только после того, как product core и policy layer
стабильны.

Потенциальный scope:

- AST-backed уточнение существующих checks;
- SF.2 / SF.5 / SF.10 / SF.11;
- SF.21 preview/default-ON по готовности;
- дополнительные intrinsic drift metrics, если они выдерживают trust bar.

Принцип: semantic expansion нужна только там, где она даёт проверяемую
пользовательскую ценность, а не ради самого backend-а.

---

## Preview / Research

Эти направления важны, но **не входят в trusted CI gate**, пока не доказали
стабильность и product-fit.

- duplication-as-blocking-gate (сам `--duplication` уже shipped как advisory report; gate-grade precision — ещё нет);
- AI-vs-human drift comparison;
- AI-assisted rule synthesis;
- semantic dependency extraction beyond current product core;
- Martin metrics;
- visualization;
- plugin API;
- C support.

Их можно развивать в `research/`, `preview/` и `future/`, но не надо продавать
как ядро ближайшего продукта.

---

## What We Are Not Doing

- не строим “общую платформу качества кода”;
- не делаем AI detector / AI attribution tool;
- не двигаем duplication в mandatory gate до нужной precision;
- не размываем MVP широким AST-scope;
- не делаем GUI / web dashboard / IDE extension;
- не заменяем линтеры, formatter-ы и bug finder-ы.

---

Детальные продуктовые аргументы — [product_vision.md](product_vision.md).
Архитектурный контекст и long-form spec — [architecture-spec.md](architecture-spec.md).
