# [SCAN] extensionless headers are not scanned (stdlib-style: `gsl/span`, `<vector>` convention)

**Created:** 2026-06-14
**Status:** new
**Module:** SCAN
**Priority:** major
**Difficulty:** unknown
**Blocks:** —
**Blocked by:** —
**Related:** #122 (corpus growth — the same third-party repos), demo/dogfooding targets (GSL/mlpack/pcl)
**Verification:** #131 (corpus run: GSL graph 0→~10 nodes, Eigen vendor didn't leak in)

## Goal

Teach the scanner to see **extension-less headers** — the stdlib convention
(`<vector>`, `<span>`) and a number of libraries (GSL: `include/gsl/span`, `gsl/pointers`,
parts of Boost). Right now they silently fall out of the scan → a false "0 violations".

## Context (how it was found)

2026-06-14, a run of archcheck on **microsoft/GSL** (the reference implementation
of the C++ Core Guidelines — the very authority our default rules cite). Result:
`No violations found`, exit 0 — but **not because it's clean**,
rather because the tool **didn't see it**:

- All 8 GSL headers are extension-less (`include/gsl/span`, `gsl/pointers`, …).
- The recognized extension set is hardcoded in [file_classification.h:25](../../include/archcheck/scan/file_classification.h#L25):
  `.c .cc .cpp .cxx .h .hh .hpp .hxx .ipp .tpp .inl .inc` — extension-less is not included.
- The scanner saw only the 14 test `.cpp` files; their `#include <gsl/...>` was treated as system
  → empty graph → nothing to violate.

## Consequence

On any stdlib-style repo (extension-less project headers) archcheck **silently
shows "0 violations"**. For the goal of demo/contribution on third-party code (#1/#3 from
the discussion) this is a failure: we either embarrass ourselves with "clean" where we didn't look, or
emit a false green in someone else's CI.

## Priority calibration (corpus measurement, 2026-06-14)

A run over the corpus `~/oss` (1686 repos) — how wide is the blind spot:

- 69,093 extension-less files (excluding `.git`) → 973 look like C/C++ → **425 clear
  headers** (`#pragma once`/guard) in **27 repos**.
- Of those 425: **324 (76%) are Eigen** (vendored in cvxpy/taskflow/…), 153 in
  vendored paths (`third_party`/`3rd-party`/`libs`). Plus stdlib clones
  (`ROCm/libhipcxx` = a libc++ port, Qt/lol shims — there extension-less is *the meaning*,
  not a mistake). One FP (Bazel `BUILD`).
- **First-party extension-less headers are almost nonexistent**: essentially 1 repo
  (`taigongzhaihua_F__K_UI`, 17 of them) out of 1686.

**Conclusion:** the bug is real (confirmed on GSL: graph 0→10 nodes, [Bash replay]), but as a
first-party pattern across the corpus it is **marginal**. The value of the fix is not corpus
coverage but the **demo/contribution case** (#1/#3): GSL and stdlib-style libraries.
Priority can stay major only in conjunction with the demo goal; for corpus metrics it's
low. Decide during planning.

## How to fix (sketch — not a decision)

A fork in the road, care needed not to scoop up garbage (files with no dot at all —
`Makefile`, `LICENSE`, a binary):

- ⚠️ **A vendor filter is mandatory IN PAIR with the fix.** The measurement showed: 76% of extension-less
  headers are vendored Eigen, currently invisible. A naive fix would drag 324 Eigen files
  into the graph → instead of a clean result, noise. Path-based vendor exclusion is part of the task, not an option.
- **Don't** just "take any file without an extension" — it would flood the scan with non-code.
- Option A: recognize an extension-less file as C++ if it lies in an include path
  AND its content looks like C++ (has `#include` / `#pragma once` / templates).
  Content-sniff on top of a path heuristic.
- Option B: opt-in — the user declares include directories/patterns in the config
  (config — v0.2, see ADR), then extension-less files inside them = headers.
- Open question: how to avoid pulling `<gsl/...>` back into the graph by double
  counting (right now it's "system"; it would become "project" — check the resolver
  [include_resolver.cpp:39](../../src/scan/include_resolver.cpp#L39)).

## Measure performance (mandatory before choosing an option)

Right now file classification is cheap: a string extension check, the file isn't
opened. To see an extension-less header, you'll have to **open and read the
content** (content-sniff for `#include`/`#pragma once`/templates). This changes the
cost of the scan: across the corpus there were **69,093** such files (and C++ markers exist in
only ~973) — i.e. in the worst case we open tens of thousands of files in vain.

What to measure before choosing option A/B:
- [ ] baseline: time of a full scan of a large repo (pcl/godot) on the current code;
- [ ] the same with content-sniff of all extension-less files enabled → delta;
- [ ] how much a path filter (sniff only inside include directories, not the whole tree)
      cuts the cost — probably the key optimization;
- [ ] whether to limit the sniff to the first N bytes of the file (a C++ header is visible in
      the first lines), so as not to read multi-megabyte non-code files in full.

If the delta is significant — that's an argument for **opt-in** (option B): by default
extension-less files are left alone, the cost is zero; it's enabled only when the user
declared include directories in the config.

## Verification (fixtures mandatory)

- [ ] `fixtures/extensionless_headers/pass/` — extension-less headers with no violations → 0
- [ ] `fixtures/extensionless_headers/fail_cycle/` — a cycle through extension-less headers → fires
- [ ] negative: `Makefile`/`LICENSE` nearby must not enter the scan
- [ ] regression: a repeat run on a clone of GSL gives a non-empty graph

## Self-check

"0 violations" from the scanner ≠ "clean". Before concluding — make sure the graph is non-empty
(how many files were actually pulled in). This bug is exactly the case where an empty
result was taken for a correct one.
