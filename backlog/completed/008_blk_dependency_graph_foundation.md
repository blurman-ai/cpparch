# [GRAPH] Dependency graph foundation and diff primitives

**Created:** 2026-05-26
**Started:** 2026-05-26
**Completed:** 2026-05-27
**Status:** done
**Module:** GRAPH
**Priority:** blocker
**Complexity:** M (2-4 days) — actually implemented in a single day via parallel worktree agents
**Blocks:** #009 (ai_drift_regression_rules), #018 (git_diff_analysis)
**Blocked by:** —
**Related:** #006 (spec_refactor)

## Goal

Fix and implement the canonical file-level dependency graph and a
separate `graph-baseline` contract, without which the first prototype of
`DRIFT.*` cannot be reliably built.

## Final state

The `scan/` and `graph/` subsystems are fully implemented and verified on real
code. `archcheck --graph <path>` builds a graph end-to-end from an arbitrary
C++ repository without `compile_commands.json` and libclang.

**Pipeline:**
```
discover_files → scan_includes → resolve_includes → DependencyGraph
              → compute_scc / added_edges / grown_sccs
              → save_baseline / load_baseline
```

**Metrics:**
- 115/115 tests green (unit + integration + fixture).
- lizard clean (`--CCN 15 --length 30 --arguments 5`, zero warnings).
- On the real `gm` project (2192 files): 7.7 s wall, found 2 cycles in Unigine SDK headers.
- On archcheck itself: 0 cycles (lakos-clean).

See [docs/milestones.md](../../docs/milestones.md) for a detailed report.

## Done (by subtask)

Decomposition of #008 into 8 + 7 subtasks (textual scanner + the rest of the pipeline):

**Scanner (textual, without libclang):**
- **#008a** — API skeleton: `IncludeKind`, `IncludeDirective`, `scan_includes`. STATIC core.
- **#008b** — naive line-based extraction (quote + angle, multi, leading ws).
- **#008c** — skipping `//` and `/* */` comments (including multi-line).
- **#008d** — skipping raw strings `R"d(…)d"` with arbitrary delimiter.
- **#008e** — logical-line continuation (`\\\n` while preserving physical numbering).
- **#008f** — the rule "`#` is the first significant character" (tests — the implementation already existed via `skip_ws+compare`).
- **#008g** — macro-include diagnostic, API extension → `ScanResult{directives, diagnostics}`.
- **#008h** — integration fixtures from disk (6 files `fixtures/scan/include_scanner/`).

**Discovery + resolver:**
- **#011** — `discover_files` (`std::filesystem::recursive_directory_iterator` + excluded dirs), `ProjectIndex{exact_path_index, suffix_index}`.
- **#012** — `include_resolver`: `Project / External / Unresolved / Ambiguous`, quote/angle policies.

**Graph:**
- **#013** — `NodeId` (strong-typed `uint32_t`) + `DependencyGraph` (forward + reverse adjacency).
- **#014** — `compute_scc` (Tarjan iterative), `reachable_from`, `reverse_reachable_from`, `has_path`.
- **#015** — diff primitives: `added_edges`, `removed_edges`, `grown_sccs` (matching SCCs by member-set overlap).
- **#016** — `graph-baseline` format (YAML via ryml) + save/load with structured error handling. Specification — [docs/baseline_format.md](../../docs/baseline_format.md).

**Fixtures:**
- **#017** — 7 integration fixtures (minimal_dag, single_scc, new_edge, shortcut_edge, cycle_growth, unresolved_include, ambiguous_include) + end-to-end pipeline test.

**CLI (preview, for dogfooding):**
- `archcheck --scan <path>` — discovery + scan, prints a summary.
- `archcheck --graph <path>` — full pipeline, prints nodes/edges/sccs/etc., exit code 1 on cycles.

## How it works (end to end)

1. **Discovery** (`scan::discover_files`) walks the repo, filtering out
   `build/` / `.git/` / `cmake-build-*/` / `.cache/` / `.idea/` / `.vscode/` /
   `out/` at any depth. Extensions: `.c/.cc/.cpp/.cxx/.h/.hh/.hpp/.hxx/.ipp/.tpp/.inl/.inc`. Paths are normalized to POSIX repo-relative.

2. **Indexing** (`scan::build_project_index`) builds `exact_path_index`
   (repo-relative path → NodeId) and `suffix_index` (each `/`-suffix of a path → vector of candidates). Needed by the resolver.

3. **Scanning** (`scan::scan_includes`) — textual scanner. Pipeline:
   `join_continuations` (joining `\\\n` while preserving physical numbering) →
   `preprocess` (comments + raw strings → spaces) →
   `try_extract` (finding `#include` after skip_ws + classification).
   Result: `ScanResult{directives, diagnostics}`. Macro-includes go into diagnostics.

4. **Resolution** (`scan::resolve_includes`) turns each directive into a
   `ResolvedInclude{Resolution}`:
   - Quote: dir-relative → exact → unique suffix → Project / Ambiguous / Unresolved.
   - Angle: exact → unique suffix → Project / Ambiguous / External.

5. **Graph build** — for each `Resolution::Project` an edge is built in `DependencyGraph::add_edge`. The container stores forward + reverse adjacency for cheap reverse-reachable / Ca metrics.

6. **Analysis** (`graph::compute_scc` + friends) — iterative Tarjan (no recursion, so it doesn't blow up on 100k+ files), deterministic output.

7. **Diff** (`graph::added_edges/removed_edges/grown_sccs`) — compares two graphs by path strings (NodeIds are session-local).

8. **Persistence** (`graph::save_baseline/load_baseline`) — deterministic YAML with `format_version`, sorted nodes, and edges as `[from_idx, to_idx]`. The loader returns `optional<BaselineLoadError>` instead of exceptions.

## What controls it

- At the library level — not a single flag / env. All behavior is set by function arguments.
- CLI: `--scan <path>` and `--graph <path>`. Without a config file, without `compile_commands.json`.
- All components are built into `archcheck_core` (STATIC). The `archcheck` binary is a thin wrapper over core.

## What it connects to

- **Unblocks #009** (drift-regression rules) — can rely on the ready diff primitives and baseline format.
- **Unblocks #018** (git-based PR analysis) — the lower layer is fully ready, only git integration and a diff-reporter are needed.
- **Unblocks the class of Lakos rules** in the v0.1 plan: cycles (SF.9) are effectively already implemented (`grown_sccs` on top of `compute_scc`), god-headers and chain length — separate rules on top of the ready graph algorithms.
- Does not block the `--baseline` mode of v0.1 — the format + save/load are ready, only a CLI subcommand is needed.

## Diagnostics

- If `archcheck --graph <path>` crashes or hangs — check that the compiler has `<filesystem>` (for GCC<9 an explicit `-lstdc++fs` is needed, already wired in CMake).
- If on large projects there are suddenly many `unresolved` — look at which paths are not found, usually these are generated headers / templates outside the repo.
- If there are many `ambiguous` — typical for a monorepo with per-module precompiled headers (`pch.h`) or shared names (`utils.h`); solved by exclude-pattern (future) or baseline.
- If the SCC counter doesn't match expectations — `compute_scc` returns an SCC even of size 1 (singletons). Cycles are `size >= 2`.
- ryml parser of the baseline: see the trap with the `[[noreturn]]` error-callback in #016.

## Mini-design: fast extraction v0.1 (historical design document)

Below is the original working draft of extraction semantics. Kept as a historical
record: all the semantics are implemented in subtasks 008a-h, 011, 012. There are no
discrepancies with the final implementation.

### 1. What we scan

The graph pipeline includes **all project files with suitable extensions**, not
only `.cpp`.

Starting set of extensions:

- translation units: `.c`, `.cc`, `.cpp`, `.cxx`
- headers / include-like: `.h`, `.hh`, `.hpp`, `.hxx`, `.ipp`, `.tpp`, `.inl`, `.inc`

By default, discovery excludes only explicitly non-target directories:

- `.git/`
- `build/`
- `cmake-build-*/`
- `.cache/`
- `.idea/`
- `.vscode/`
- `out/`

That is, v0.1 does **not** try to automatically drop `third_party/`,
`vendor/`, or `external/`: that is a separate decision that cannot be silently
baked into a heuristic.

### 2. How we scan include directives

The fast backend in v0.1 is a **textual include scanner**, not a full
preprocessor and not an AST.

The scanner must:

- read the file as text;
- understand logical lines with `\\`-continuation;
- ignore `#include` inside line comments, block comments, string literals, char literals, and raw string literals;
- recognize a directive only when `#` is the first significant character of a logical line;
- extract only literal includes: `#include "path"` / `#include <path>`.

A macro-based include (`#include SOMETHING`) goes into diagnostics as `macro_include`, but does not produce an edge.

### 3. Conditional compilation

For zero-config mode we have no reliable set of `-D` and compile target context. v0.1 accepts an honest limitation:

- a literal `#include` in any branch of `#if/#ifdef/#ifndef` is considered a candidate include;
- the fast graph is an over-approximation;
- branch-sensitive refinement is a task for adapters in v0.2+.

### 4. Resolution algorithm

Indexes are built in advance: `exact_path_index` (repo-relative path → file), `suffix_index` (`/`-suffix → candidates).

**For `#include "x"`:** dir-relative → exact → unique suffix → Project / Ambiguous / Unresolved.

**For `#include <x>`:** exact → unique suffix → Project / Ambiguous / External.

The resolver does not "guess" when there are multiple candidates — it honestly returns Ambiguous.

### 5. What goes into the canonical graph

**Nodes:** all discovered project files.
**Edges:** only successfully resolved project includes.

`external` / `unresolved` / `ambiguous` / `macro_include` are diagnostics, not edges. `graph-baseline` stores only nodes + project edges; warnings don't blur the meaning of DRIFT.*.

### 6. Diagnostics

Minimal record: source file + line + raw token + kind (quote/angle) + resolution kind + target/candidates. Repeated includes are deduplicated at the graph edge level, but locations are kept for the report.

### 7. Where off-the-shelf tools fit (v0.2+)

- **depfile adapter** — gcc/clang `-M*` output as a build-resolved source.
- **clang-scan-deps adapter** — a refined source for projects with compile context.
- **cross-check mode** — comparing fast vs refined graph to measure the discrepancies.

### 8. Open questions (for v0.2+)

- Whether to store metadata about "include found inside a conditional block".
- Whether a separate `vendored/generated` classification layer is needed.
- How to design suppression for chronically ambiguous includes.

## Key decisions

| Decision | Reason |
|----------|--------|
| Graph primitives first, rules later | Otherwise `DRIFT.*` and Lakos checks would rely on an unstable model |
| Canonical graph is file-level | Simplifies semantics, eliminates duplication of the primary models |
| Fast extraction core we build ourselves | Zero-config v0.1 requires a repo-local include resolver without a compile database |
| Off-the-shelf dependency extractors treated as adapters, not the core | `depfiles` and `clang-scan-deps` are useful but tied to build context |
| We need exactly a graph diff, not just a snapshot | AI-drift rules compare the "before/after" state |
| baseline stores a snapshot, not derived metrics | Makes the format more stable, doesn't tie it to the current rule set |
| Minimum dependencies, no external graph library | Matches the project's architectural constraints |
| Parallel implementation of subtasks via worktree agents | Drastically sped up #008 (a day instead of 2-4) while keeping changes isolated |

## Changed files (final list)

| File | Change |
|------|--------|
| `include/archcheck/scan/include_directive.h` | new — IncludeKind, IncludeDirective, DiagnosticKind, IncludeScanDiagnostic, ScanResult |
| `include/archcheck/scan/include_scanner.h` | new — `scan_includes` API |
| `include/archcheck/scan/project_files.h` | new — ProjectFile, ProjectIndex, discover_files, build_project_index |
| `include/archcheck/scan/resolved_include.h` | new — Resolution, ResolvedInclude |
| `include/archcheck/scan/include_resolver.h` | new — resolve_include / resolve_includes |
| `include/archcheck/graph/node_id.h` | new — strong-typed NodeId + std::hash |
| `include/archcheck/graph/dependency_graph.h` | new — DependencyGraph class |
| `include/archcheck/graph/algorithms.h` | new — compute_scc, reachable_from, reverse_reachable_from, has_path |
| `include/archcheck/graph/diff.h` | new — EdgeRef, added_edges, removed_edges, GrownScc, grown_sccs |
| `include/archcheck/graph/baseline.h` | new — save_baseline, load_baseline, BaselineLoadError |
| `src/scan/*.cpp` | 4 implementation files for the scan layer |
| `src/graph/*.cpp` | 4 implementation files for the graph layer |
| `src/main.cpp` | preview CLI: `--scan`, `--graph` |
| `src/CMakeLists.txt` | STATIC core, 8 sources, stdc++fs for GCC<9 |
| `tests/unit/scan/*` | 4 test source files |
| `tests/unit/graph/*` | 4 test source files |
| `tests/integration/scan/scanner_fixtures_test.cpp` | integration tests for the scanner |
| `tests/integration/graph/end_to_end_test.cpp` | end-to-end tests for the pipeline |
| `tests/CMakeLists.txt` | wiring all sources + ARCHCHECK_FIXTURES_DIR + stdc++fs |
| `fixtures/scan/include_scanner/*.cpp` | 6 scanner fixtures |
| `fixtures/graph/**/*` | 7 graph-pipeline fixtures |
| `docs/baseline_format.md` | new — baseline YAML specification |
| `docs/milestones.md` | new — log of real dogfood runs |
| `docs/architecture-spec.md` | + "Development progress" section with a link to milestones |

## Fixtures

- [x] Minimal DAG without cycles
- [x] Graph with one SCC
- [x] "A new edge appeared" scenario
- [x] "shortcut edge" scenario
- [x] "cycle growth" scenario
- [x] "unresolved include" scenario
- [x] "ambiguous include" scenario
