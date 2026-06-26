# [SCAN] one pass over the tree + a shared authored/vendored/generated view for all rules

**Created:** 2026-06-18
**Started:** 2026-06-18
**Status:** wip — core (steps 1–6) committed; remainder = step 7
**Module:** SCAN
**Priority:** major
**Difficulty:** hard
**Blocks:** consistency of rule verdicts; report applicability (#124)

## Progress (as of 2026-06-19)

The core of the plan landed in commits `ec5988b` (unify vendored/generated into a read-once
snapshot), `9cc349b` (check-mode reads the tree via SourceSnapshot), `b01707a`
(one snapshot per ref for graph+advisories in `--diff`), + docs `83127ca`/`7e1de3c`.
Remainder — **step 7**: final dogfood + per-repo corpus vs golden + update
`docs/research/agent_drift_within_repo.md` (the numbers shifted from steps 3–5: clone
recall ↑, graph −generated). The file was in `new/` by mistake — moved to `wip/`.
A perf nit from this task's review was split out into #130 (the findFile index is already done there).
**Blocked (closure):** #131 — step 7 (the final corpus golden) is executed there.
**Verification:** #131 (Group 1: clone recall ↑, graph −generated, complexity rmm fixed, foundationdb 0)
**Related:** #127 (precision of the vendored/generated predicate — slots in here), #068 (graph vendor exclude), #069 (vendored file exclude), #081 (over-exclusion), #113 (apache-banner dominant-guard — reuse), #124 (corpus run that surfaced the mismatch)

## Goal

Scan the source tree **once** per snapshot: read+classify the
files (authored / vendored / generated / test) into a **shared view** consumed by
all rules (clone, complexity, graph), instead of each rule walking the tree
itself and filtering by its own copy of the logic.

## Context — three diverged implementations of one idea

Each check vendor-filters **in its own method**, with a different subset of common
predicates → **inconsistent verdicts on the same file**:

| rule | method | what it uses |
|---|---|---|
| clone | `collectNonVendoredSources` (project_files.cpp) | path + basename + license-header (`isVendoredFile`) |
| graph | `filterVendored` (graph_builder.cpp) | path + basename + **license-header with dominant-banner guard** (#113) |
| complexity | `collectFilePairs` (local_complexity_drift.cpp) | path + basename **WITHOUT license-header** (deliberately disabled) |

**Evidence (corpus FP audit #124, `docs/research/agent_drift_within_repo.md` §5):**
- rmm `cpp/benchmarks/utilities/rapidcsv.h` (vendored single-header outside a vendor dir):
  "vendor" for clone/graph, but "authored" for complexity → a false **CCN 64**.
- MS-Teams `objectmodel_wrap.cpp` (SWIG-generated): generated isn't excluded
  by any rule → a false clone.

**Dogfood irony (to record in the pitch):** `archcheck src/ include/` → `No violations`.
Our own clone detector **doesn't see** this duplication — it's **conceptual**
(three reimplementations written apart), not a token clone. The honest boundary of token detection:
the most important duplication (diverging reimplementations of a concept) is caught by the architectural
view, not by "find identical tokens". A strong admission for the report, not a weakness.

## Why they diverged (not pure laziness — but no excuse either)

The license-header signal requires a **whole-tree review** (dominant-banner guard #113/#109
foundationdb: if >50% of files carry the banner — it's the project's own license, turn the layer off).
This guard exists **only in graph** (it scans the whole tree). complexity is **diff-scoped**
(parses only changed files) → doesn't compute the tree ratio → **disabled license-header
entirely** (dumb-safe, at the cost of the rmm FP). BUT `FileSource` (memory `GitObjectFileSource` /
disk worktree) can `list()`/`read()` over the **whole** tree — the data is available, complexity
just doesn't read it. So the ratio **is computable**; what's needed is one shared pass that computes it
once and hands it to everyone.

## Design

- A shared scan stage: `AuthoredSourceView` (or extend `project_files`), built once
  from `FileSource`: `list()` → `read()` each file **once** → classification
  (the predicate from #127: .gitmodules / path tokens / nested-LICENSE+code / copyright-mismatch /
  generated markers + dominant-banner guard #113).
- Rules consume the view: clone — over authored files; graph — over authored; complexity
  parses changed **authored** files, but takes the classification and the whole-tree banner-ratio
  from the shared view.
- This is the `scan → rules` from the spec; it removes the triple IO, the triple filter, and the
  per-rule drift (one place for generated patterns and thresholds).
- `#127` supplies the PRECISION of the predicate; `#129` — the fact that the predicate is computed ONCE and
  SHARED. Do them together (or #127 first — the predicate, then #129 — the single pass).

## Plan

- [ ] Extract a unified `classifySource()` / `AuthoredSourceView` with the richest logic (from `filterVendored`)
- [ ] Thread the whole-tree banner-ratio into the diff-scoped complexity
- [ ] Move clone / graph / complexity onto the shared view (remove the 3 separate filters)
- [ ] Add generated patterns in one place (`*_wrap.cpp`, `*.pb.{h,cc}`, `moc_`/`ui_`/`qrc_`, `*.tab.*`/`*.yy.*`, `@generated`/`DO NOT EDIT`)
- [ ] Fixtures (below) + dogfood `archcheck src/ include/ tests/` = 0 violations
- [ ] Transparency: `excluded N files (vendored/generated, reason: ...)` (as in #127)

## Done

- (empty)

## In progress

- (empty)

## Next steps

1. First close/account for #127 (the predicate), then the single pass here.
2. Remove the regression on rmm (CCN 64 goes away) and MS-Teams (the SWIG clone goes away).

## Key decisions

| Decision | Reason |
|----------|--------|
| One pass + a shared view, not a per-rule filter | three implementations have already diverged in behavior (rmm/MS-Teams) — the root is in separate walks |
| Base it on `filterVendored` logic (graph) | it's the only one with the dominant-banner guard (#113), the others are stripped down |

## Changed files

| File | Change |
|------|--------|
| src/scan/project_files.{h,cpp} | unified `AuthoredSourceView` / `classifySource` |
| src/scan/new_clone_drift.cpp | consume the shared view |
| src/scan/local_complexity_drift.cpp | consume the shared view + whole-tree banner-ratio |
| src/graph/graph_builder.cpp | consume the shared view (remove the `filterVendored` duplicate) |

## Fixtures (mandatory)

- [ ] `fixtures/source_classification/pass_consistent/` — the same vendored file yields the SAME verdict to all rules
- [ ] `fixtures/source_classification/fail_rmm_style/` — a single-header lib outside a vendor dir does NOT fail complexity
- [ ] `fixtures/source_classification/fail_generated_swig/` — `*_wrap.cpp` does NOT fail clone
- [ ] `fixtures/source_classification/pass_self_licensed/` — a project with its own banner in every file is NOT excluded entirely (anti-over-exclude, #113)

## Self-check

The risk is OVER-exclusion (eating our own code) and a regression in rule behavior. Verify by
enumeration: the same set of files → the same verdict from all rules; compare the
clone/complexity/graph output before and after on 2-3 real repos (rmm, foundationdb,
MS-Teams). "Fewer violations" — a reason to check whether we threw out too much.

## Converged design (design-workflow, 2026-06-18)

5 Discover lenses + 3 designs + a judge. Chosen **Design 3 — `scan::AuthoredScope` + one
source per ref**; a full TreeSnapshot (read/token dedup) rejected as scope bloat
(split out into a separate follow-up). The root is wider than thought: the AND-chain "is this project code?"
is openly hardcoded in **7 places with 5 formulas** — `collectNonVendoredSources`
(unguarded banner), `filterVendored` (>50%-dominant-guarded banner), `collectFilePairs`
(banner OFF, #109), `god_file_growth.cpp:36` (the comment promises generated-exclude,
the **code doesn't do it**), `defect_attractor`/satd/test (path-only). Plus the per-diff tree is
read 2× per side by three separate `cat-file` children; in check, headers up to 3× from disk.

**API (a clean value-type over (path,content), backend-agnostic — the two-backend split stays intact):**
`AuthoredScope::fromFiles(files)` (computes the dominant-banner ratio once) /
`changedFilesMode()` (banner=no-op, preserves the #109-safe complexity behavior) /
`excluded(path,content)`; + `scan::isGeneratedPath()` (lift from `duplication_scanner.cpp:173`
+ SWIG `_wrap.{cpp,cxx,c}`). NOT migrated: god_file_growth/defect_attractor/satd/test —
they classify commit paths without content, they stay on the path trio (a documented gap).

**Twist #109:** complexity until step 6 — `changedFilesMode()` (rmm still fires CCN 64,
foundationdb 0); step 6 gives the full tree → `fromFiles(fullList)` → rmm is fixed via the
shared ratio WITHOUT a foundationdb regression. Naively "turning the banner on" — forbidden.

**Plan (~120–160 LOC, each step builds+tests on its own, reverts by 1 file):**
0. Golden baseline (no code): dogfood=0 + clone/complexity/graph on rmm/SWIG/foundationdb/Apache → save per-repo.
1. `isGeneratedPath`+SWIG → repoint `phasePathBasedFpSuppress`, delete the private copy. Fixture `fixtures/generated/swig_wrap/`.
2. `AuthoredScope` (pure addition, 0 call-sites) + equivalence test: `excluded()` == each of the 5 formulas. `tests/scan/authored_scope_test.cpp` + fixtures.
3. graph `filterVendored` → AuthoredScope (delta: graph now drops generated).
4. clone `collectNonVendoredSources` → AuthoredScope (delta: clone gets the dominant-banner guard → recall ↑).
5. complexity `collectFilePairs` → `changedFilesMode()` (rmm is NOT fixed yet — by design; foundationdb stays 0).
6. `diff_command` — one baseline+current source per ref, passed into graph+complexity+clone (borrow by ref, git-orphan hygiene); complexity → `fromFiles(fullList)` → **rmm fixed**.
7. Final dogfood + per-repo corpus vs golden + lizard.

**Human decisions (judge flag):** (1) step 6 in scope — YES (this is exactly the "centralized run" request). (2) **steps 3–5 shift the published report numbers** (clone recall ↑, graph −generated) → re-run the corpus + update `docs/research/agent_drift_within_repo.md`. (3) SWIG only exact suffixes. (4) generated-removal globally (changes cycle/god/chain on repos with generated code). (5) history-advisories stay path-only.

**Risk:** med. The main one — reintroducing #109 (mitigated: banner no-op until step 6); per-repo golden (not an aggregate — the aggregate lies); git-orphan hygiene in step 6. Out of scope (follow-up TreeSnapshot): comment/literal strippers, brace-walkers, diverging lex variants — there include_scanner needs a byte-offset that the token-stream loses.
