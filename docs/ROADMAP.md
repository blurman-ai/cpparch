# archcheck — ROADMAP

_2026-05-29 · phase: **v0.1 (ready to tag)**_

## Current focus

**v0.1 release** — функционально готов; критичных blocker'ов нет.

Открытых критичных багов нет (#049 закрыт 2026-05-29). Решение по SF.21 зафиксировано: перенесён в v0.3 (preview в v0.2 через `--with-clang`), см. [#050](../backlog/wip/050_min_sf21_anonymous_namespace.md).

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

## v0.2 — config + libclang semantic backend

- `.archcheck.yml` v1: spec закрыт; loader в работе (#051) — modules + typed rules (`layers` / `independence` / `forbidden`)
- `--with-clang` opt-in libclang backend (спайк #043 → backend #042)
- Semantic SF rules: SF.2, SF.5, SF.10, SF.11
- SF.21 — preview через `--with-clang` (default-ON в v0.3, см. [#050](../backlog/wip/050_min_sf21_anonymous_namespace.md))
- SARIF output (для GitHub Code Scanning)
- `archcheck init`

## v0.3 — расширение правил + AI loop

- C / I / NL секции Core Guidelines: C.121, C.133, C.134, I.2, I.3, I.22, NL.27
- SF.21 — default-ON (см. [#050](../backlog/wip/050_min_sf21_anonymous_namespace.md))
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
