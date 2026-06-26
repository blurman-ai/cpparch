# [CHORE] Translate the whole repository RU → EN (public-release blocker)

**Created:** 2026-06-26
**Started:** 2026-06-26
**Status:** wip
**Module:** DOCS / INFRA
**Priority:** major
**Related:** —

## Goal

The project is preparing for a public release and **must not ship with Russian text**
(author's call). Translate every Russian artifact in the repo to English and switch the
project's working language to English (including `CLAUDE.md`'s "answer in Russian" directive
and the `architecture-spec.md` / `JOURNEY.md` narratives).

Code is already almost fully English: Cyrillic survives only in 3 test files (comments and
`TEST_CASE` labels — verified: not assertion fixture data). The weight is in documentation.

## Scope (≈388 files, ≈30 000 Cyrillic lines)

Driven by "can't go public with Russian" — public surface first, archive last.

- **P0 — public surface (release blocker).** `README.md`, `CHANGELOG.md`, `AGENTS.md`, root
  `dup_band_70_80.md`, `.github/workflows/ci.yml`, `CMakeLists.txt` + `src/CMakeLists.txt`,
  and comments/labels in `tests/integration/graph/end_to_end_test.cpp`,
  `tests/duplication_qt_slots_test.cpp`, `tests/integration/scan/scanner_fixtures_test.cpp`.
- **P1 — `docs/` (88 files, ~6.9k lines).** Authoritative `architecture-spec.md`,
  `config_format.md`, `ci_usage.md`, `ci_integration.md`, `duplication_architecture.md`,
  `code_style.md`, `code_quality.md`, `docs/research/*`, `docs/decisions/*`, `docs/dev/*`.
  Plus: flip `CLAUDE.md` (language directive → English; update "in Russian" descriptions) and
  `JOURNEY.md` (1056 lines).
- **P2 — operations.** `.claude/commands/*.md` (12) + comments in `analysis/*.py` (12 scripts).
- **P3 — archive (largest tail).** `backlog/` (173 files, incl. 125 in `completed/`) +
  `experiments/` (94). Low external value but visible in a public repo — translated last.

## Translation rules (quality-critical)

1. Preserve markdown structure: frontmatter, headings, tables, lists.
2. Do not touch code inside fenced blocks (```…```) — commands/configs/listings stay verbatim.
3. Do not rename files or change link targets — translate only the `text` in `[text](path)`;
   `path` stays. Internal cross-links hold because filenames are unchanged.
4. Keep proper nouns and terms: `archcheck`, Lakos, Martin, libclang, Core Guidelines, rules
   `SF.7/8/9/21`, metrics `CCD/ACD/NCCD`, task ids (`#127`), markers (`GCC8-COMPAT`).
5. `TEST_CASE("…")` — translate only the label text (it's a label, not fixture data).
6. `CLAUDE.md`: replace the "answer in Russian…" directive with an English equivalent; update "in Russian"
   doc descriptions. ⚠️ this also changes the assistant's future behavior.
7. JOURNEY / research: translate the narrative; verbatim author quotes — translate preserving
   meaning. Technical accuracy beats style in `architecture-spec.md`.

## Execution

Batched parallel subagents per wave (each agent owns a disjoint file set, edits in place).
Waves P0 → P1 → P2 → P3; verify and commit between waves.

## Acceptance

- `grep -rlP '\p{Cyrillic}'` over the whole repo (minus `build/`) → 0.
- Build Debug + ctest green; clang-format clean on the 3 test files.
- Dogfood: `./build/debug/src/archcheck` over `src include tests` → 0 violations.
- Internal links / anchors not broken (translate heading + its `#anchor` links together).

## Progress

- [x] P0 — public surface (verified: build+tests green, dogfood 0 violations)
- [x] P1 — docs/ (88 files) + CLAUDE.md flip + JOURNEY
- [x] P2 — .claude/commands (12) + analysis/*.py (12) + config dotfiles (4)
- [x] P3 — backlog/ (~170 tracked .md) + analysis/*.md (4) + fixtures/ (2)
- [x] Final repo-wide Cyrillic = 0 across all tracked files **except** the
      exclusions below

## Exclusions (deliberate, not gaps)

- `experiments/` — gitignored (never public); holds captured run-data/logs
  (localized git/date output), not prose. Translating would corrupt artifacts.
- `backlog/pending/027_maj_coverage_90_percent.md` — left untranslated per the
  standing "don't touch pending/" rule. ⚠️ It IS tracked, so it would ship with
  Russian; lift the pending exclusion if it must be translated before release.
- Untracked in-flight files (`backlog/new/145,146,148,149`,
  `docs/research/bot_review_drift.md`) — "don't translate what isn't under git".
