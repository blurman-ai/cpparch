# #081: Dup scanner silently skips files — 2 over-exclusion bugs

**Creation date:** 2026-06-04
**Start date:** 2026-06-04
**Status:** completed
**Completion date:** 2026-06-05
**Priority:** maj
**Type:** [BUG][scan]

---

## Goal

The dup scan (`--duplication`) silently reports "scanned 0 files" on repos with real code —
copy-paste isn't found at all. Found during corpus verification (#079/#080): 4 repos out of
317 scanned 0 files, plus partial undercounts. Two independent causes.

## Bug A — `discoverFiles` breaks the entire walk on the first FS error

[src/scan/project_files.cpp](../../src/scan/project_files.cpp) `discoverFiles`:
```cpp
while (!ec && it != end) { ... it.increment(ec); }
```
`!ec` means: any error on ONE entry (a broken/looped symlink, no permissions)
stops the entire walk. `jjbudz_6502` has `_codeql_detected_source_root -> .`
(a self-symlink) → `recursive_directory_iterator` loops → `ec` → 0 files
(although there are 10 tracked `.cpp`).

**Fix:** don't abort the walk on a per-entry error — reset `ec` and continue; don't
follow directory symlinks (or catch loops). Options:
`directory_options::skip_permission_denied`. Also affects the graph (the same `discoverFiles`).

## Bug B — an SPDX header is classified as "vendored"

[include/archcheck/scan/file_classification.h](../../include/archcheck/scan/file_classification.h)
`hasVendorLicenseHeader` marks any permissive-license banner as vendored. But
**SPDX headers are now standard on authored code**, not only in vendor. The comment in the
file ("archcheck's own sources carry no per-file license header") doesn't hold on the corpus.

`fuddlesworth_PlasmaZones` (a KDE project) — `SPDX-License-Identifier: GPL-3.0` on **all 401**
of its `.cpp` → all marked vendored → 0 files. Likewise `ajazz-control-center`, `limitless`.

**This is a design decision:** how to distinguish vendor-with-license from authored-with-SPDX. Options:
1. remove the license-header heuristic, rely on dir/basename (#068/#069 layer 1);
2. fire only if license-header AND a vendored location/basename;
3. narrow the markers (SPDX by itself — not a vendor signal).

## Scale

- **Full zero:** 4 repos (PlasmaZones 0/401, jjbudz 0/10, ajazz, limitless).
- **Partial undercount:** where part of the files have SPDX (a sample of 120 repos showed onnx-light
  43/376, SpecusGL 34/174 — part may be legit-vendor, check on site).
- Effect: the dup signal is **understated** by over-exclusion (the opposite of inflation from fork/vendor).

## Acceptance

- [x] Fixture: a repo with a self-symlink → the dup scan doesn't fall to 0 (Bug A).
- [x] Fixture: an authored file with an SPDX-GPL header is NOT excluded as vendored (Bug B).
- [x] Re-scan of an "affected" repo → a non-zero result (the corpus repos are locally unavailable,
      verified with an equivalent temp-repo using the real binary — see Decision).

## Decision (2026-06-04)

**Bug A — robust walk** ([src/scan/project_files.cpp](../../src/scan/project_files.cpp) `discoverFiles`):
- `recursive_directory_iterator` now with `directory_options::skip_permission_denied` —
  one unreadable subtree doesn't take down the whole walk.
- Loop `while (it != end)` (without `!ec`); after each `increment(ec)` we do `ec.clear()` —
  a per-entry error doesn't interrupt the walk.
- New helper `should_skip_dir`: don't descend into **symlink directories** (any) —
  a self/loop symlink (`_codeql_detected_source_root -> .`) no longer loops the iterator.
  Per-entry statuses are read into a local `item_ec`, not the shared `ec`.

**Bug B — SPDX is no longer a vendor signal** (strategy: option 3, chosen by the user)
([include/archcheck/scan/file_classification.h](../../include/archcheck/scan/file_classification.h)
`hasVendorLicenseHeader`): removed the `spdx-license-identifier` marker from `kLicenseMarkers`
(7→6). Only the **full verbatim texts** of licenses remain (MIT/BSD/public-domain/
Apache/Boost/zlib) — authored projects don't insert these per-file, but write one SPDX line.
Layer 1 (basename/stem) and layer 1-dir (#068) untouched.

**Verification:** all 344 tests green; lizard clean; a temp-repo (self-symlink + 2 files with
`SPDX-License-Identifier: GPL-3.0`) gives `scanned 2 files` instead of the buggy `scanned 0 files`.

**Changed files:**
- `src/scan/project_files.cpp` — Bug A (skip_permission_denied + no-follow symlinks + no-halt).
- `include/archcheck/scan/file_classification.h` — Bug B (drop SPDX marker).
- `tests/unit/scan/project_files_test.cpp` — test that a self-symlink doesn't zero out the walk.
- `tests/unit/scan/file_classification_test.cpp` — a bare SPDX is not vendor; layer-2 on full-text.
- `tests/integration/graph/vendor_exclude_test.cpp` — a layer-2 fixture on full-text MIT.

## Related
- `[[backlog/completed/069_maj_vendored_file_exclude.md]]` — vendor-exclude (the source of Bug B).
- `[[backlog/new/070_maj_checker_fp_fix_proposals.md]]` — general FP tracking.
- `experiments/CORPUS_SUMMARY_REPORT.md` §8.4 — the finding and its scale.
