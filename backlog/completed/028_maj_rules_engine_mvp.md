# [RULES][REPORT][CLI] Zero-config rules engine MVP

**Date created:** 2026-05-28
**Date completed:** 2026-05-28
**Status:** completed
**Module:** RULES, REPORT, CLI
**Priority:** major
**Complexity:** L (removed YAML config from scope → implemented as zero-config)
**Related:** plan `yaml-fuzzy-knuth`

## Goal

Implement the zero-config rules engine for v0.1 MVP: `archcheck [path]` with no
settings at all immediately gives a useful result from the 5 default rules.

## Decision (config deferred to v0.2)

YAML config removed from the MVP — high barrier to entry, nobody will describe
modules on first run. The zero-config experience matters more for adoption.
MVP = `archcheck [path]` without config → useful result.

SF.21 (anonymous namespace) removed from the MVP — it requires libclang for precise
detection, an approximate text scan gives false positives.

## Done

### Rules infrastructure
- `include/archcheck/rules/violation.h` — `struct Violation{ruleId, file, line, message}` + `ViolationList`
- `include/archcheck/rules/i_rule.h` — `class IRule` (pure abstract), `check(graph, readFile lambda)`
- `include/archcheck/scan/project_files.h` + `src/scan/project_files.cpp` — `isHeaderFile()` (8 extensions: .h .hh .hpp .hxx .ipp .tpp .inl .inc)

### Rules
- **SF.9 NoCycles** — SCC ≥ 2 → violation with the format "cycle: a → b → c → a" (DFS within the SCC)
- **SF.7 UsingNamespace** — line-by-line scan of headers, strip `//` comments, detect `using namespace`
- **SF.8 IncludeGuard** — check the first 30 non-empty lines for `#pragma once` or `#ifndef`
- **Lakos.GodHeader** — `predecessors(id).size() > 30` → violation
- **Lakos.ChainLength** — `computeIncludeDepths()` (condensation + memoised DFS) > 10 → violation

### Graph
- `computeIncludeDepths(graph)` — longest path per node via condensation DAG

### Reporters
- `writeTextReport(violations, ostream)` — `file:line: [rule] message` + summary
- `writeJsonReport(violations, ostream)` — schema version 1, `{"version":1,"violations":[...],"summary":{...}}`

### CLI
- `run_check(root, fmt)` — builds the graph, runs all rules, prints the report
- `archcheck [path]` — zero-config entry point (was `print_help()`)
- `archcheck --format json [path]` — JSON output
- Exit 1 on violations, 0 if clean

### Fixtures
- `fixtures/sf7_using_namespace/pass/`, `fail_global/`
- `fixtures/sf8_include_guard/pass/`, `fail_missing/`
- `fixtures/sf9_no_cycles/pass/`, `fail_simple/`
- `fixtures/lakos_chain_length/pass/` (5 files), `fail_deep/` (12 files)
- `fixtures/lakos_god_headers/pass/` (5 users), `fail_high_fanin/` (31 + god.h)

## Verification

- **164/164 tests** — 100% pass
- **Coverage**: lines 90.4%, functions 93.2%, branches 58.9% (all thresholds PASS)
- **Dogfood**: `archcheck .` finds violations only in fixture files, production code clean
- **JSON**: `archcheck --format json .` produces valid JSON, exit 1

## Key decisions

| Decision | Reason |
|---------|---------|
| readFile lambda in IRule::check() | Tests do not hit the disk — they pass strings directly |
| Eager string resolution (NodeId → path) when building a Violation | No dependency on the graph's lifetime |
| computeIncludeDepths via condensation | Cyclic SCCs get the same depth without infinite recursion |
| YAML config → v0.2 | Zero-config adoption matters more than flexibility at the start |
| SF.21 → v0.2 | Requires libclang; approximate text scan is unreliable |

## Changed files

| File | Change |
|------|-----------|
| `include/archcheck/rules/violation.h` | new |
| `include/archcheck/rules/i_rule.h` | new |
| `include/archcheck/rules/rule_set.h` | new |
| `include/archcheck/rules/sf{7,8,9}_*.h` | new |
| `include/archcheck/rules/lakos_*.h` | new |
| `include/archcheck/report/{text,json}_reporter.h` | new |
| `include/archcheck/scan/project_files.h` | + `isHeaderFile()` |
| `include/archcheck/graph/algorithms.h` | + `computeIncludeDepths()` |
| `src/rules/*.cpp` | new (6 files) |
| `src/report/*.cpp` | new (2 files) |
| `src/graph/algorithms.cpp` | + `computeIncludeDepths()` |
| `src/CMakeLists.txt` | + new sources, fix stdc++fs |
| `src/main.cpp` | + run_check, --format, zero-arg entry |
| `tests/CMakeLists.txt` | + new tests, fix stdc++fs |
| `tests/unit/rules/*.cpp` | new (5 files) |
| `fixtures/` | new — 5 pass/fail sets |
