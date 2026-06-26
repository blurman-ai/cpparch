# [SCAN][GRAPH] Exclude vendored FILES (outside third_party) from dedup and the graph

**Created:** 2026-06-01
**Started:** 2026-06-01
**Completed:** 2026-06-01
**Status:** done
**Module:** SCAN / GRAPH / RULES
**Priority:** major
**Related:** #064 (vendor folders in #056), #068 (vendor folders in the archcheck graph), #067 (verification exposed the class)

## Goal

Exclude single vendored libs lying NOT in `third_party/` (but right in `src/`,
`plots/`, next to the code) from (a) the #056 deduplicator and (b) the archcheck include graph.
They pollute ALL signals: within-file copy-paste (`qcustomplot.cpp` 30k lines),
cross-file, cycles. #064/#068 catch only folders — files slip through.

## Context — found in the corpus (#054, outside vendor folders)

`json.hpp` ×17, `stb_image*.h` and other stb ×~18, `catch.hpp`/`doctest.h` ×8,
`imgui.cpp` ×3, `httplib.h` ×3, `qcustomplot.*` ×2, `tinyxml2`/`pugixml`/`miniz`/
`sqlite3`/`lua` ×1-2. All — third-party code, not author drift.

## Three layers (per user decision)

1. **Curated list of names — DO.** Base: `nothings/single_file_libs` (the reference
   list of single-file C/C++ libs) + a corpus scan. Match by basename (case-insensitive):
   `qcustomplot.*`, `sqlite3.[ch]`, `stb_*.h`, `json.hpp`, `httplib.h`, `miniz.[ch]`,
   `imgui*.cpp`, `tinyxml2.*`, `pugixml.*`, `lua.c`, `doctest.h`, `catch.hpp`,
   `lodepng.*`, `cJSON.*`, `glad.c`, … Catches ~80%.
2. **License header — DO.** The first ~40 lines contain the name of a third-party lib + a permissive
   license (MIT/BSD/zlib/Apache/public domain / "Copyright (c) <non-author>"). Catches
   renamed/unfamiliar vendors. Stateful header parse (like SF.7/generated-skip).
3. **Size heuristic — ASSESSED: size ALONE is dangerous, only size+license.**
   A scan of >5000 lines (outside vendor folders) showed: among the large ones there is REAL author
   code — `R5900_Recompiler.cpp` 52k and `PS2_Gpu.cpp` 40k (the hand-written core of the PS2 emulator
   hps2x64), `mainwindow.cpp` 32k / `DecodiumBridge.cpp` 35k (author Qt). Pure
   size would have thrown them out. What's more, `mainwindow.cpp` 32k is ITSELF the drift signal
   (god-file, Lakos.GodHeader), it must be CAUGHT, not excluded. → layer 3 only as
   "size >5000 AND a license header of a third-party lib" (layer 2 mandatory), or not needed at all
   with a good layer 1+2.

### List extensions (from the corpus scan)

- **Curated names (layer 1) to add:** `entt.hpp`, `miniaudio.h`, `uni_algo.h`,
  `cutlass/cute/*`, `vulkan.hpp`, `td_api.h` (TDLib), `nuklear.h`.
- **Signal directories (cheap in the #068 dir-exclude):** `single_include`, `*-source`
  (e.g. `qcustomplot-source`), `*_bundled` / `*-bundled`, `amalgamation`, `cutlass`.

## Precedent — GitHub Linguist (reference, researched 2026-06-01)

GitHub uses Linguist for language highlighting; it solves exactly this task:
- `lib/linguist/generated.rb` — markers for generated code (protobuf `DO NOT EDIT!`, Go
  `Code generated`, Thrift, Cython…) = our layer 2/#065; plus **minified = average
  line length >110 chars**; plus path patterns (node_modules, lock files…).
- `lib/linguist/vendor.yml` — ~130-150 vendor patterns = layer 1/#064/#068, BUT
  geared to web/JS — specific C/C++ libs (qcustomplot/sqlite/stb) by name are NOT there.

**What Linguist does NOT have → our additions:**
- **A numeric heuristic (data tables)** — absent. Linguist catches minified by
  line length, but not numeric tables (`JSC_map.cpp`/`inv.cpp` it would miss).
- **C/C++ single-file libs by name** — needs its own list (base `nothings/single_file_libs`).

### Layer 4 (NEW) — numeric density (data tables). VALIDATED.

If the digit/letter ratio is high — the file is data, not code, don't check it. Measured on the corpus
(first 4000 lines): code 0.03–0.13 (pii_detector, mainwindow, PS2 emulator), data
0.97–**448** (JSC_map, JSC_list, GNFS `inv.cpp` — 80 letters per 35851 digits!). A threshold of
**digits/letters > ~0.4** cleanly separates them. Cheap, catches what markers/names miss.
This closed the contentious `inv.cpp` (turned out to be a numeric table).

## Implementation

A shared `isVendoredFile(path, headerBytes)` (curated names + license heuristic) →
- #056 `collectFromSource`: skip (like generated-skip #065);
- archcheck `discoverFiles`: skip the file (next to `isExcludedDirName`).
The list of known names — in data/config, extensible. Keep the list found over the corpus
as an artifact for exclusion in the research (#054).

## Acceptance criteria

- [x] Layer 1 (names) in #056 and archcheck; pass/fail fixtures.
- [x] Layer 2 (license header) in #056 and archcheck; fixtures.
- [x] Layer 3: decision made in the task body (§3) — only "size+license", no separate code needed.
- [x] Corpus remeasure (2026-06-01) — A/B via `--no-skip-vendored` / a stash binary:
  - **#056 dedup, DataPlotter** (qcustomplot.cpp 35529 lines in `src/plots/`): fragments
    502→1147 without the skip; reports 34→68 — **33 pairs** of pure qcustomplot within-file noise
    suppressed; the default doesn't miss a single vendor pair (0 leaks).
  - **archcheck graph, nodes:** DataPlotter 99→**97**; BambuStudio 3176→**3001** (175 vendor files
    imgui/miniz/nlohmann removed from fan-in/god-header/CCD metrics).
  - There were no cycles in the vendor of these repos (single-file libs) → `sccs_cyclic` unchanged;
    node noise removed. Vendor *directories* with cycles (the original #068 case) didn't appear in this slice.

---

## Done

- Infrastructure recon; **finding: #068 — a phantom** (the claimed code was absent, see below).
- Canonical vendor classifier in the header-only `file_classification.h`:
  `isVendoredFile` (layer 1 curated names + layer 2 license header), `isVendoredDirName` /
  `pathHasVendoredDir` (#068, dir-exclude), `baseName`, `toLowerAscii`.
- archcheck graph: filter `filterVendored` in `buildGraphForSource` — drops vendor files AND
  subtrees of vendor directories before `addNode` (reads content once, caches it for the include scan).
- #056 spike: `isVendoredFile(baseName(label), src)` in `collectFromSource`, gated by
  `Options::skipVendored` (`--no-skip-vendored`); shared header via `target_include_directories`.
- Tests: 12 cases (`file_classification_test.cpp` unit + `vendor_exclude_test.cpp` integration),
  all 272 tests green; lizard clean. Spike smoke: vendored json.hpp → 0 fragments by default.

## Next steps

1. (Opt.) Corpus remeasure (#067) — confirm the cleanup of cycles/copy-paste on real repos.
2. Commit(s) of ≤50 lines: (a) header+unit test, (b) graph filter+integration, (c) spike insertion.
3. Close #069 → completed/; sync #068 (phantom fixed here).

## Key decisions

- **D1. Storing the list (layer 1):** a constexpr array in `file_classification.h` (like
  `kExcludedDirNames`/`kMarkers`), not a data file. SSOT, zero runtime IO, single for both paths.
- **D2. The filter point in archcheck — `buildGraphForSource`, NOT `discoverFiles`.** In
  `file_classification.h:13-16` there's a NOTE "vendor is a graph/diff-layer concern, not enumeration",
  and the test `project_files_test.cpp:110` requires that `discoverFiles` SURFACES `third_party/`.
  A filter at the graph layer doesn't break that and affects only the graph/metrics.
- **D3. Iteration scope:** layer 1+2, both paths. Layer 3/4 — as a separate step.
- **D4. #068 is closed in #069:** the dir-vendor-exclude goes into the same graph filter (point B).

## Finding — #068 marked `done`, but there's no code

`backlog/completed/068_maj_graph_vendor_exclude.md` describes `kExcludedVendor` in
`project_files.cpp` (the graph cleaned of vendor cycles). **The code is nowhere:** `git log -S"kExcludedVendor"`
across all branches is empty; `discoverFiles` excludes only `.git/build/cmake-build-*`;
`buildGraphForSource` builds the graph over ALL files. The #068 work is lost/not committed →
the archcheck include graph currently includes vendor. #069 closes this both by files AND by directories.

## Changed files

- `include/archcheck/scan/file_classification.h` — vendor classifier (layers 1+2 + dir #068).
- `src/graph/graph_builder.cpp` — `filterVendored` in `buildGraphForSource`.
- `experiments/partial_duplication/{main.cpp,CMakeLists.txt}` — skip + flag + include-path.
- `tests/unit/scan/file_classification_test.cpp`, `tests/integration/graph/vendor_exclude_test.cpp` — new.
- `tests/CMakeLists.txt` — registration of the two test files.
- _(not committed — awaits `/commit`)_
