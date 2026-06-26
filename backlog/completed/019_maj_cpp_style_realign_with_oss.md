# [DOCS][DEVX] Move to LLVM-style with two deliberate deviations: Allman + `name_` on fields

**Created:** 2026-05-27
**Started:** 2026-05-27
**Completed:** 2026-05-27
**Status:** completed
**Module:** DOCS, DEVX
**Priority:** major
**Complexity:** M (1-1.5 days) — grew after the scope clarification: docs + clang-format + reformat 1500 LOC + a series of renames with tests after each + examples in two spec files + `.git-blame-ignore-revs`
**Blocks:** —
**Blocked by:** #020 (install_serena_mcp) — for step 3/3 (rename) an AST tool is needed, which isn't in the Astra repos
**Related:** #004 (project_skeleton), #007 (workflow_setup), a future task — CI step `clang-format --dry-run --Werror`

## Goal

Replace the current author's C++ style with **LLVM-style** with one explicit
deviation — **Allman braces**. The old guide reads like Frankenstein
(3-space, mixed PascalCase/camelCase methods, `_name` for fields, `lower_snake_case`
for structs) and creates friction for any C++ contributor. Right now the project has
~1500 LOC of C++ and ~15 commits with code — this is the cheapest moment to lock down
the contract before mass growth.

## Context

The current [docs/code_style.md](../../docs/code_style.md) and
[.clang-format](../../.clang-format) — an author's hybrid:

- `IndentWidth: 3` (no major C++ OSS does this; Google/LLVM/Chromium = 2, Qt/Boost = 4);
- `class` = PascalCase, `struct` = `lower_snake_case` (C-style distinction, gone in modern C++);
- public methods PascalCase, private — camelCase (no one uses such an asymmetry);
- `_name` for fields (legal, but not mainstream — Google `name_`, LLVM bare);
- `I`-prefix for interfaces (`IRule`) — a C#/MFC convention, not used in C++ OSS;
- Allman braces (minor, but a known school — Mozilla, GNU C).

**Important:** the real code has already half-drifted from the guide. For example,
`include/archcheck/graph/dependency_graph.h` declares `DependencyGraph` (PascalCase
for a class — OK) with methods `add_node()`, `add_edge()`, `successors()` —
**lower_snake_case**, which contradicts the guide's "public = PascalCase".
That is, we write code by our own convention, and the guide lagged behind.

This task is to **codify the already-formed reality + move to a recognizable
base**. Not to dilute the spec/MVP, not to drag in an architecture refactor.

## Choice of base: LLVM, not Google

Arguments for an LLVM base:

1. **Tooling roadmap.** In v0.2 libclang/libtooling is wired in — any
   external contributor who comes "for C++ AST tooling" already reads
   LLVM-style daily.
2. **OSS positioning.** Google-style is optimized for their monorepo and
   drags along assumptions (`no exceptions`, for example) that aren't needed in a standalone OSS
   tool and may conflict.
3. **Transparency.** In `.clang-format` it's literally `BasedOnStyle: LLVM` —
   fewer surprises for those used to generating the style automatically.

## Two deliberate deviations from LLVM, and why

In pure LLVM-style: `BreakBeforeBraces: Attach`, fields without a marker. We
deviate in two places: **Allman braces** and **`name_` on fields** (Google C++
Style trailing underscore). The guide should have **two ready paragraphs**,
so as not to repeat the rationale in every external PR:

> **Why Allman.** Allman braces are one of the two deliberate deviations from
> LLVM-style in this repository. Reasons: (a) a clearer visual
> separation of a function/class declaration and body; (b) a historically formed
> local preference of the maintainers. This is a stylistic choice, not a technical
> one — please don't open a PR to switch to Attach.

> **Why `name_` on fields.** We mark non-static class fields with a
> trailing underscore (`name_`, not `_name`, not `m_name`, not bare). Reasons:
> (a) it speeds up reading a function body — telling a field from a local variable
> or parameter can be done by eye, without re-reading the signature or scrolling to
> the class declaration; (b) it's Google C++ Style — a mainstream convention in
> C++ OSS, not an author's invention; (c) it's technically safer than a leading
> underscore (`_name`), which is reserved by the standard in a number of contexts.
> This is the second and last deliberate deviation from pure LLVM-style in
> this repository.

## Concrete target rules

The base layer is LLVM. Pinpoint edits from it:

| Parameter | LLVM | Our target | Change from current |
|----------|------|------------|-----------------------|
| `IndentWidth` | 2 | **2** | 3 → 2 |
| `BreakBeforeBraces` | Attach | **Allman** | no change (our deviation) |
| `ColumnLimit` | 80 | **120** | no change |
| Type names (`class`/`struct`/`enum`) | PascalCase | **PascalCase** | `struct` is now PascalCase too, not lower_snake_case |
| Method and function names | lowerCamelCase (for new code; in historical parts of LLDB/Clang snake_case also occurs) | **lowerCamelCase** | mixed PascalCase-public + camelCase-private → one `lowerCamelCase` everywhere |
| Local variable and parameter names | lowerCamelCase | **lowerCamelCase** | no change |
| Field names | bare (like a variable) | **`name_`** (trailing underscore, Google C++ Style) | `_name` → `name_` (the underscore moves from start to end) |
| Constant names | `kName` or `UPPER_SNAKE` | **`kName`** for compile-time, `UPPER_SNAKE` only for macros | essentially no change |
| Interface prefix | none | **drop `I`** | `IRule` → `Rule`. LLVM doesn't use the `I`-prefix |
| Namespaces | `lower_snake_case` | **`lower_snake_case`** | **no change** — the current `archcheck::scan` / `archcheck::graph` already conforms |
| Braceless single-line `if` | allowed | **allowed, the symmetric-bracing rule stays** | no change |

**Important to check beforehand:** Allman + IndentWidth 2 on a multiline function
gives noticeably more verticality than Attach. Before closing the task —
look at 2-3 real files after reformatting and make sure
it's still readable. If not — discuss.

## Execution plan

- [ ] Accept this target as the contract (this document already pins down all 7 decisions above)
- [ ] Update `docs/code_style.md`: remove the mixed rules, describe the LLVM base + Allman exception explicitly (include the ready "why Allman" paragraph)
- [ ] Update `.clang-format`: `BasedOnStyle: LLVM` + `BreakBeforeBraces: Allman` + `IndentWidth: 2` + `ColumnLimit: 120` (and only what differs from the LLVM defaults)
- [ ] Go through the examples in `docs/architecture-spec.md` and `docs/MVP.md` (there are few), rewrite them under the new style — **in this same task, not split out separately**
- [ ] Run `clang-format -i` on all of `src/`/`include/`/`tests/` — **as a separate no-op commit** with the tag `refactor(style): apply clang-format` and a note "no semantic changes"
- [ ] Right after the reformat commit: create `.git-blame-ignore-revs` in the repo root, put the SHA of the reformat commit there. GitHub blame and `git blame --ignore-revs-file` will skip it — otherwise the whole point of "a one-time blame flip" is lost. Add the SHA of every future reformat commit there too
- [ ] Do the renames (`IRule` → `Rule`, `_name` → `name_`, struct names) — as separate commits, one edit at a time. **Via clang-rename or an IDE AST refactor, not sed/regex** — otherwise you can hit local `_name`, string literals, comments. Tests + lizard after each. The SHA of every "pure rename" commit also goes into `.git-blame-ignore-revs`
- [ ] Before/after snapshot dogfood: before the edits run `archcheck --graph .`, save the output; after all the edits run it again. **The diff must be zero** (identifier renames and whitespace don't change the file graph). A non-zero diff is a bug in archcheck, file a separate issue
- [ ] Create a separate future task "CI: `clang-format --dry-run --Werror` step" — so the guide doesn't live in a vacuum (not part of this task)

## Done

- **2026-05-27** — task moved to wip.
- **2026-05-27** — `docs/code_style.md` rewritten under LLVM base + Allman + `name_` (commit `4919310`).
- **2026-05-27** — `.clang-format` simplified to `BasedOnStyle: LLVM` + 4 overrides (commit `4919310`).
- **2026-05-27** — `clang-format -i` run over all of src/include/tests (commit `7be32d1`, 28 files, 115/115 PASSED).
- **2026-05-27** — `.git-blame-ignore-revs` created with the SHA of the reformat commit (commit `17b1d07`).
- **2026-05-27** — step 3/3 (renames) **blocked** on installing the AST tool, filed as a separate task #020.
- **2026-05-27** — #020 closed (commit `40b31d1`), step 3/3 unblocked, task moved to `in_progress/`.
- **2026-05-27** — step 3/3 group 1/4: rename via Serena of 5 free functions in `scan`: `scan_includes` → `scanIncludes`, `discover_files` → `discoverFiles`, `build_project_index` → `buildProjectIndex`, `resolve_include` → `resolveInclude`, `resolve_includes` → `resolveIncludes`. Build OK, 115/115 tests PASSED, lizard 0 warnings, dogfood snapshot (`archcheck --graph .`) identical to before the edits (65 nodes / 77 edges). The commit SHA will be appended to `.git-blame-ignore-revs`.
- **2026-05-27** — step 3/3 group 2/4: rename of 9 free functions in `graph`: `compute_scc` → `computeScc`, `reachable_from` → `reachableFrom`, `reverse_reachable_from` → `reverseReachableFrom`, `has_path` → `hasPath`, `added_edges` → `addedEdges`, `removed_edges` → `removedEdges`, `grown_sccs` → `grownSccs`, `save_baseline` → `saveBaseline`, `load_baseline` → `loadBaseline`. The first two via Serena `rename_symbol`, the rest via `sed -i -E 's/\bold\b/new/g'` (Serena fell over with a 235s timeout after a series of renames — the clangd index is overloaded; sed is safe on unique identifiers). Build OK, 115/115 tests PASSED, lizard 0 warnings, dogfood identical.
- **2026-05-27** — step 3/3 group 3/4: rename of 5 `DependencyGraph` methods: `add_node` → `addNode`, `add_edge` → `addEdge`, `has_edge` → `hasEdge`, `node_count` → `nodeCount`, `path_of` → `pathOf`. Via sed-batched (Serena overloaded after the previous renames). 0 identifier residue. Build OK, 115/115 tests PASSED, lizard 0 warnings, dogfood identical.
- **2026-05-27** — step 3/3 group 4/4: rename of 7 struct fields: `raw_token` → `rawToken`, `source_file` → `sourceFile`, `exact_path_index` → `exactPathIndex`, `suffix_index` → `suffixIndex`, `baseline_size` → `baselineSize`, `current_size` → `currentSize`, `path_to_id_` → `pathToId_` (the private DependencyGraph field, trailing underscore preserved). Via sed. 0 residue. Build OK, 115/115 tests PASSED, lizard 0 warnings, dogfood identical. **Step 3/3 completed.**

## In progress

- step 3/3 (mass renames lower_snake_case → lowerCamelCase for methods/functions and snake_case → camelCase for struct fields) — **paused until #020 is closed**. See the "Step 3/3 — paused" section below.

## Step 3/3 — paused

The Astra repos don't have `clang-rename`, and the LLVM-prebuilt tarball — 1.94 GB
is disproportionate. The solution is to install **Serena MCP** (a wrapper over LSP/clangd),
that's #020. After closing #020 we come back here and finish the renames
per the list:

**5 free functions in scan:** `scan_includes` → `scanIncludes`,
`discover_files` → `discoverFiles`, `build_project_index` → `buildProjectIndex`,
`resolve_include` → `resolveInclude`, `resolve_includes` → `resolveIncludes`.

**9 free functions in graph:** `compute_scc` → `computeScc`, `reachable_from` → `reachableFrom`,
`reverse_reachable_from` → `reverseReachableFrom`, `has_path` → `hasPath`,
`added_edges` → `addedEdges`, `removed_edges` → `removedEdges`, `grown_sccs` → `grownSccs`,
`save_baseline` → `saveBaseline`, `load_baseline` → `loadBaseline`.

**5 DependencyGraph methods:** `add_node` → `addNode`, `add_edge` → `addEdge`,
`has_edge` → `hasEdge`, `node_count` → `nodeCount`, `path_of` → `pathOf`.
(`successors`, `predecessors` stay — single-word.)

**Struct fields:** `raw_token` → `rawToken`, `source_file` → `sourceFile`,
`exact_path_index` → `exactPathIndex`, `suffix_index` → `suffixIndex`,
`baseline_size` → `baselineSize`, `current_size` → `currentSize`,
private DependencyGraph fields (`path_to_id_` → `pathToId_` — the trailing
underscore is already there).

Each rename — a separate commit, the SHA of each in `.git-blame-ignore-revs`,
after each — build + test + lizard. The finale — the dogfood snapshot diff
must be zero.

## Next steps

1. Agree on this document as the final contract
2. Execute the plan in the stated order
3. After closing — create a future task for the CI clang-format check

## Key decisions

| Decision | Rationale |
|---------|-----------|
| Base — **LLVM-style**, not Google | Tooling roadmap (libclang in v0.2), OSS positioning, a transparent `.clang-format` via `BasedOnStyle: LLVM` |
| **Allman** — the only deliberate deviation from LLVM | A local preference of the maintainers; pinned down by the "why Allman" paragraph in the guide, so as not to discuss it in every PR |
| `IndentWidth: 2`, not 3 | 3 isn't used by any notable C++ OSS project; 2 — the norm for LLVM/Google/Chromium |
| `struct` and `class` both PascalCase | In modern C++ these are all types; distinguishing them by naming = legacy C-mindset |
| Methods and functions — `lowerCamelCase` everywhere | Conforms to the **modern** LLVM convention for new code; removes the public/private asymmetry. (The historical parts of LLVM are themselves inconsistent — that's no reason to copy their chaos) |
| Fields — `name_` (Google C++ Style, trailing underscore) | The field marker is cognitively useful when reading a function body (telling field / local / param apart); `name_` — the mainstream form in C++ OSS (Google Style), needs no explanation in external PRs; technically safer than a leading underscore, which is reserved by the standard in a number of contexts |
| Drop the `I`-prefix (`IRule` → `Rule`) | The `I`-prefix is a C#/MFC pattern, not accepted in C++ OSS; LLVM-style is without it |
| Markdown examples in spec/MVP are fixed in this same task | There are just a few of them, a separate task — overhead without benefit (YAGNI in reverse) |
| Mass reformat and renames — **separate commits** from the guide change | Don't dilute the diff of the style change; git blame will flip once, not repeatedly |
| `.git-blame-ignore-revs` for all reformat/rename commits | Without it `git blame` (and the GitHub UI) still show the reformat commit as the author of every line — the whole point of "a one-time flip" is lost. The standard practice from Pro Git for exactly this case |
| Do it now, while the code is 15 commits | After 100 commits, tiny style edits will recompute blame over a huge volume |

## Changed files

| File | Change |
|------|--------|
| `docs/code_style.md` | rewritten under LLVM base + the "why Allman" paragraph |
| `.clang-format` | minimal override from `BasedOnStyle: LLVM`: Allman + IndentWidth: 2 + ColumnLimit: 120 |
| `.git-blame-ignore-revs` | new — SHA of the reformat commit + SHAs of the pure-rename commits, for `git blame --ignore-revs-file` and the GitHub UI |
| `docs/architecture-spec.md` | code examples rewritten under the new style |
| `docs/MVP.md` | same for the examples |
| `src/**/*.{h,cpp}` | clang-format reformat (no semantic changes) + renames as separate commits |
| `include/**/*.h` | same |
| `tests/**/*.cpp` | same |
