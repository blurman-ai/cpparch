# archcheck — ROADMAP

_2026-06-01 · phase: **v0.1 (in progress — duplication pass)**_

## Current focus

**v0.1** — граф-правила готовы; **duplication pass внесён в MVP-scope** и пока не готов.

Открытый блокер — **рост дубликатов**: по предв. оценке на dense-корпусе (#054) класс не менее серьёзный, чем циклы графа. Дедуп пока живёт спайками (`experiments/line_duplication`, `experiments/partial_duplication`), в бинарь не интегрирован; precision `--diff` дедуп-детектора 16.5% (iter1-4, surface- и essence-LCS подходы исчерпаны — #060/#063). Hardening-loop #060 (≤5 итераций) активен.

Граф-часть открытых блокеров не имеет (#049 закрыт 2026-05-29). SF.21 → v0.3 (preview в v0.2 через `--with-clang`), см. [#050](../backlog/future/050_min_sf21_anonymous_namespace.md).

«Что в работе прямо сейчас» — `backlog/wip/`. Очередь — `backlog/new/`.
«Что уже зашипилось» — [CHANGELOG.md](../CHANGELOG.md).
«Боевые прогоны на чужом коде» — [milestones.md](milestones.md).

---

## v0.1 — zero-config intrinsic rules + drift (current)

**Cover-story:** пользователь запускает `archcheck <path>` без конфига, получает осмысленные нарушения, замораживает baseline, встраивает в CI.

- Default intrinsic rules: SF.7, SF.8, SF.9, Lakos.GodHeader, Lakos.ChainLength
- Drift rules: DRIFT.1 (shortcut edges), DRIFT.2 (cycle growth)
- Baseline mode: `--baseline`, `--save-baseline`, `--save-graph-baseline`, `--drift-baseline`
- PR diff mode: `--diff <revspec>`
- Output: text + JSON; exit codes 0 / 1 / 2 / 3
- Fast preprocessor backend (без `compile_commands.json`, без libclang)
- Duplication pass (fast backend): line-level (#053) + token/partial (#056) с precision-фильтрами (#059); интеграция в бинарь + hardening (#060) — в работе

## v0.2 — config + libclang semantic backend

- `.archcheck.yml` v1: spec закрыт; loader в работе (#051) — modules + typed rules (`layers` / `independence` / `forbidden`)
- `--with-clang` opt-in libclang backend (спайк #043 → backend #042)
- Semantic SF rules: SF.2, SF.5, SF.10, SF.11
- SF.21 — preview через `--with-clang` (default-ON в v0.3, см. [#050](../backlog/future/050_min_sf21_anonymous_namespace.md))
- SARIF output (для GitHub Code Scanning)
- `archcheck init`

## v0.3 — расширение правил + AI loop

- C / I / NL секции Core Guidelines: C.121, C.133, C.134, I.2, I.3, I.22, NL.27
- SF.21 — default-ON (см. [#050](../backlog/future/050_min_sf21_anonymous_namespace.md))
- BDE: no-inter-component-friendship, external-linkage-in-header
- DRIFT расширения (intrinsic-only)
- AI rule synthesis contract — `archcheck synthesize` (#010)

## v0.4 — Martin metrics + distribution

- Ce / Ca / I / A / D на уровне namespace (опционально, по флагу)
- Кастомные regex pattern-правила
- Pre-commit hook, Docker image, static binary releases (linux x64/arm64, macOS arm64, win x64)

## v0.5 — templates + community

- Layer / hexagonal / onion templates (готовые `.archcheck.yml`)
- Регрессионный CI на топ-N OSS (fmt, Catch2, spdlog, abseil, folly, grpc)
- Полная документация, migration guides

## Long-term (опционально, по запросу)

- Plugin API для кастомных правил
- Опциональная визуализация графа (graphviz output, не GUI)
- Поддержка C
- clangd index bridge для скорости на больших проектах

## Что НЕ делаем

- `--suggest-config` (авто-вывод модульной структуры).
- GUI / web dashboard / IDE extension.
- AST-rules в v0.1.
- Замена линтеров / formatter / bug finder / include optimizer.

---

Детальные обоснования — [architecture-spec.md](architecture-spec.md).
