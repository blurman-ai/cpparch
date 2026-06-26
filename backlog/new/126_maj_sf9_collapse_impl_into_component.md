# [RULES][SF.9] collapse header+impl into a single Lakos component before cycle detection

**Date created:** 2026-06-14
**Status:** new
**Module:** RULES / GRAPH
**Priority:** major
**Complexity:** unknown
**Blocks:** demo/contribution on header-only C++ (#1/#3)
**Blocked by:** —
**Related:** #088 (inline-split FP filter — this ticket generalizes it), quick-fix `_impl.hpp` markers (this session)
**Verification:** #131 (corpus run: mlpack 16→~0, PCL 206→~0, real cycles hold)

## Goal

SF.9 must treat `foo.hpp` + `foo_impl.hpp` (and `.inl`/`.ipp`/…) as **one
component** (Lakos's definition of a component: .h + its implementation with the same
stem). Right now they are two graph nodes, and their include loop fires as a cycle.

## Context (how we found it)

2026-06-14, dogfooding on **mlpack** (a header-only template library from
goodfirstissue). Raw run: **259 "actionable" SF.9 cycles**. Going through them one by one:
**248 of 259 (96%) are the idiom `X.hpp → X_impl.hpp → X.hpp`**, not a defect
(`foo.hpp` at the end does `#include "foo_impl.hpp"`, which includes `foo.hpp` for the
declarations).

**Quick-fix this session** (`sf9_no_cycles.cpp`): added `_impl.hpp`/`_impl.h`/
`_impl.hh`/`_impl.hxx` to `kImplMarkers`. Result: **259 → 16**. Tests 528/528,
dogfood archcheck = 0.

**Why 16 remain and this ticket is needed:** the filter `isInlineSplitScc` fires
only on a **strict 2-node SCC** (`scc.size() != 2 → not filtered`). Some
`_impl` pairs sit in a **large SCC** and do not collapse. Confirmed:
`core/util/backtrace.hpp ↔ backtrace_impl.hpp` fires because
`backtrace_impl.hpp` also pulls in `log.hpp`, and `log_impl.hpp` pulls `backtrace.hpp` back
→ a backtrace+log tangle of 4 nodes. Within it the pairs are idiomatic, but the 2-node
special case does not see them.

## ⚠️ Important extension from dogfood (PCL, 2026-06-14): impl in the `impl/` SUBDIRECTORY

Running archcheck on **PointCloudLibrary/pcl** gave **206 SF.9 cycles, ~all idiom**
`dir/X.h ↔ dir/impl/X.hpp` (`common/centroid.h ↔ common/impl/centroid.hpp`,
`2d/edge.h ↔ 2d/impl/edge.hpp`). Captured sample 60/206 = 100% this pattern.

My quick-fix (commit `2690a34`) does NOT catch them for two reasons:
1. `isInlineSplitScc` requires `dirName(a)==dirName(b)` → `common` ≠ `common/impl`;
2. my marker is `_impl.hpp` (suffix), whereas PCL uses `.hpp` with the same stem, lying in
   an `impl/` subdirectory (no suffix).

The PCL convention `include/.../X.h` + `include/.../impl/X.hpp` is VERY widespread
(Eigen `detail/`, Boost, many template libs). It is probably MORE cases than
same-dir `_impl.hpp`. Component merging MUST cover this.

**The component definition is refined:** `A` and `B` are one component if the
stem matches (without extension and without the impl suffix) AND they are in the same
directory OR one is in an `impl/`-/`detail/` subdirectory of the other. That is, the canonical
path = `dir without trailing /impl|/detail` + stem.

## The essence of the fix (sketch — not a solution)

Move from the band-aid "2-node SCC special case" to a **component model**:
- Before cycle detection, **merge** nodes with the same canonical key (dir-without-
  /impl|/detail + stem, one being an impl marker OR in an impl/detail subdirectory) into one
  logical component (Lakos). Edges within a component are discarded,
  inter-component ones remain.
- Then the `backtrace` tangle collapses to `backtrace` component ↔ `log` component;
  if there is a real cycle between them — it *must* fire (that is now honest), while the
  intra-pair `_impl` edges disappear.
- Carefully: stem merging must not merge different components (`foo.hpp` and
  `foo_bar.hpp` are different; the marker must be an implementation suffix, not a prefix).

## The remaining 16 on mlpack (for acceptance) — go through each

After the quick-fix, what remained (classify when implementing, not in bulk):
- `_impl` in a large SCC: `backtrace`, `gan`, `rbm`, `rp_tree_max_split`,
  `r_tree_split` → should disappear after component merging.
- forwarding-umbrella: `binary_space_tree.hpp → binary_space_tree/binary_space_tree.hpp`,
  `cover_tree`, `spill_tree` → a separate class (umbrella ↔ same-name subdir header).
- deprecated-split: `load.hpp ↔ load_deprecated.hpp`, `save ↔ save_deprecated`.
- typedef back-edge: `neighbor_search.hpp ↔ typedef.hpp`, `ra_search ↔ ra_typedef`.
- umbrella back-edge (possibly a real smell): `core.hpp → data.hpp →
  one_hot_encoding.hpp → core.hpp` — a leaf includes the umbrella. Decide: FP or TP.

## Verification (fixtures mandatory)

- [ ] `fixtures/sf9_no_cycles/pass_impl_in_larger_scc/` — two header+impl pairs,
      cross-linked (like backtrace+log) → 0 (the ticket's main case)
- [ ] negative: a real inter-component cycle A↔B (each a header+impl) → fires
- [ ] regression: `pass_impl_hpp_split` (quick-fix) and `fail_simple` unchanged
- [ ] mlpack: sf9_cycles noticeably < 16 (go through what remains and why)

## Self-check

"16 remained" ≠ "16 real cycles". Go through each of the remainder one by one
(input→expectation), as we did with 259. An idiom in a large SCC looks like a cycle, but is
not one.
