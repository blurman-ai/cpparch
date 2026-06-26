# [SCAN][GRAPH] archcheck: exclude third_party/vendor from the include graph (cycles/metrics)

**Created:** 2026-06-01
**Started:** 2026-06-01
**Completed:** 2026-06-01
**Status:** done
**Module:** SCAN / GRAPH
**Priority:** major
**Related:** #064 (vendor-exclude in #056), #067 (nightly verification — exposed the bug)

## Goal

archcheck built the include graph (and counted SF.9 cycles / SCCs / Lakos metrics) over the WHOLE
tree, including vendored code. Cycles inside `third_party/borealis` (fmt, SDL)
were counted as project architectural drift → an inflated share of "structural drift"
in the corpus run (#054). Vendor must be excluded unconditionally, in all variations.

## ⚠️ Correction (2026-06-01, #069)

The code claimed below (`kExcludedVendor` in `project_files.cpp`) **was absent from the
repository** — `git log -S"kExcludedVendor"` across all branches is empty, the graph was built over
the whole tree including vendor. The work was lost / not committed. The dir-vendor-exclude
was **actually implemented and covered by tests in #069**, but at a different point: not in `discoverFiles`
(where the test requires surfacing `third_party/`), but in `buildGraphForSource` (`filterVendored`
→ `pathHasVendoredDir`), with the list `kVendoredDirNames` in `include/archcheck/scan/file_classification.h`.
The description below is kept as the original intent; for the actual implementation — see #069.

## How it works

`src/scan/project_files.cpp` → `is_excluded_dir_name`: to the existing build artifacts
(`.git`, `build`, `cmake-build-*`, …) an exclusion of vendor directories was added.
The directory name is normalized (lowercase, `_`/`-`/space stripped) and checked against
the canonical list `kExcludedVendor`: `thirdparty`, `3rdparty`, `vendor`, `vendored`,
`vendors`, `external`, `externals`, `extern`, `deps`, `dependencies`, `submodules`,
`submodule`, `nodemodules`, `contrib`. This catches all spelling variants
(`third_party` / `thirdParty` / `ThirdParty` / `3rd-party` / `node_modules` …).
The whole directory is skipped (`disable_recursion_pending`), the subtree is not scanned.

## What controls it

An unconditional default (like `.git`/`build`). There is no flag — vendor is always excluded.

## Verified

- `beiklive/GBAStation`: was `sccs_cyclic=4` (all in `third_party/borealis`), became **0**
  (nodes 1037→128). The authored `src/` had no cycles to begin with.
- build OK (GCC8), lizard clean (function ≤30 lines).
- Corpus re-measure (#054): the "structural drift" share recomputed (it was inflated by vendor).

## What it relates to

<!-- TODO -->

## Diagnostics

If a cycle is suspicious — `archcheck --graph <repo>` gives `sccs_cyclic`; compare with
a run over the authored subdirectory. The list of vendor variations — `kExcludedVendor` in
`project_files.cpp`. Extend it when new names are encountered.
