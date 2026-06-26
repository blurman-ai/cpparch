# [DOCS] README → shipped reality (CLI + features)

**Created:** 2026-05-29
**Started:** 2026-05-29
**Completed:** 2026-05-29
**Status:** done
**Module:** DOCS
**Priority:** critical
**Complexity:** S (in practice: ~1 hour with a smoke test of all commands)
**Blocks:** —
**Blocked by:** —
**Related:** #6 (gh — audit Issues 1+2), #028 (rules_engine_mvp — pinned the v0.1 zero-config scope), #045 (docs_sync_roadmap_mvp_spec — internal slice of the same synchronization)

## Goal

Bring `README.md` in line with what is really shipped in the current binary: remove advertising of a non-existent CLI (`check --config`, `init`, `baseline create`) and unshipped features (config-loader, SARIF), splitting them out into a Planned/Roadmap section.

## Context

Audit #6 showed that the README references a CLI and features that are deferred to v0.2 by decision #028 (config rules) or simply not implemented (SARIF). A user copy-pasting the first example from the README would fail on the very first command.

**Issue 1 — non-existent CLI:**
- `archcheck check --config arch.yaml` — there is no `check` subcommand, no `--config`
- `archcheck init > arch.yaml` — there is no `init`
- `archcheck baseline create --config ... --output ...` — there is no `baseline create`
- The real CLI (see `src/main.cpp` usage): `archcheck [path]` (zero-config), `--format json`, `--baseline`, `--save-baseline`, `--save-graph-baseline`, `--drift-baseline`, `--diff`, `--scan`, `--graph`.

**Issue 2 — unshipped features in present tense:**
- "Applies declarative rules from a YAML config" — the config loader is deferred to v0.2 (#028).
- SARIF in Output formats — there is only `text_reporter` and `json_reporter`.

## Done

- **2026-05-29** — Reconciled against the real binary via `archcheck --help`. Every command from the rewritten README was run against `build/debug/src/archcheck`: bare check, `--format json`, `--save-baseline`/`--baseline` (full round-trip with suppression), `--save-graph-baseline`/`--drift-baseline`, `--diff HEAD~1..HEAD`. All 7 commands return the expected exit codes and outputs.
- **2026-05-29** — Rewrote README:
  - "What it does (today, v0.1)" section — the actual list of the five shipped rules (SF.7/8/9, Lakos.GodHeader threshold 50, Lakos.ChainLength threshold 10), drift and diff.
  - "Quick start" section — real commands with zero-config first, the baseline flow, drift and diff.
  - The output example is taken from a run on `fixtures/sf9_no_cycles/fail_simple`: `a.h: [SF.9] cycle: a.h → c.h → b.h → a.h`.
  - Output formats — text and json only.
  - "What it is NOT" expanded to the CLAUDE.md formulations (clang-tidy / PVS-Studio / clang-format / IWYU explicitly).
  - A "Planned (v0.2+)" section appeared — config rules, `init`, `--with-clang`, SARIF, color TTY moved there with a link to the spec and the ADR trail.

## How it works

The README now has three clear zones:

1. **"What it does (today, v0.1)"** — a closed list of what really works. Each bullet is run against the binary with a single command. If a bullet is here, it must work without a footnote.
2. **"Quick start"** — seven commands covering three use cases: static check, the baseline flow (legacy adoption), and drift/diff (CI on PRs).
3. **"Planned (v0.2+)"** — an open list of what the spec promises but isn't shipped. Each item references an ADR or a backlog task — the reader sees that the feature isn't forgotten, but deliberately deferred.

The main invariant: **any command from "Quick start" must work on the current binary without build flags not mentioned in the README.** On the next release or when a flag is removed — the README is updated in the same commit.

The source of truth for the shipped CLI is `archcheck --help` (see [src/main.cpp](src/main.cpp)). On a README ↔ `--help` discrepancy — the README is wrong, not the binary.

## Controlled by

The document is not controlled by anything at runtime. Of the control bindings:

- The list of default rules in the README must match the registration in `src/rules/rule_set.cpp` (5 rules in v0.1).
- The thresholds in the "Lakos.GodHeader" (50) and "Lakos.ChainLength" (10) bullets are copies of `kDefaultThreshold` from [include/archcheck/rules/lakos_god_headers.h:14](include/archcheck/rules/lakos_god_headers.h#L14) and from lakos_chain_length.h respectively. When a threshold changes — update the README in the same edit.
- The SF.9 output example in the README is the format that `text_reporter.cpp` writes. When the format changes — regenerate the example.

## Related to

- **#028** — the closed decision "config rules → v0.2". The README Planned section references it indirectly via the ADR trail.
- **#045** — a parallel sync for MVP.md / architecture-spec.md / ADR. The README was done first so that #045 could rely on the finished texts.
- **#046** — color TTY decision; while the track is undecided, color is mentioned as Planned.
- **#042 + #043** — `--with-clang` Planned. The README promises no timelines — only direction.

## Diagnostics

- When in doubt "does the README match the binary" — `diff <(grep -E '^archcheck' README.md) <(archcheck --help | grep -E '^\s*archcheck')`. Crude, but it catches a discrepancy in the set of flags.
- If a new user gets stuck on a command from Quick start — that is a README regression, not a binary bug. Recover by running the command on a clean machine and updating the README.
- If something that is in fact already shipped shows up in Planned — move it to "What it does", update the bullet, remove it from Planned.

## Changed files

| File | Change | Commit |
|------|--------|--------|
| `README.md` | rewrite: Usage / What it does / Output formats / Planned | (commit pending) |
