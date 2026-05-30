# partial_duplication — OSS-wide sweep (#056)

**Date:** 2026-05-30
**Tool:** `experiments/partial_duplication` spike (token-overlap, parser-free, no
libclang). Sweep added two flags to the spike: `--exclude <substr>` (repeatable)
and `--rare-df-pct P` (relative rare-token cutoff).
**Corpus:** every C/C++ tree under `/home/localadm/oss/` — 19 repos
(`_aidev_run` skipped: it is a multi-repo meta-corpus, cross-matching unrelated
sub-repos would be meaningless).

Goal: run the Type-3 detector on real, diverse code and see (a) what clean
cross-component duplication it surfaces, and (b) whether the spike's findings
(#1–#5 in `SPIKE_REPORT.md`) hold at scale.

Shared excludes for every run (vendored / generated / build / tests):
`/build /_deps/ /third_party /thirdparty /3rdparty /vendor /bundled /extern
/external /deps/ /packages/ single_include amalgamate /dist/ moc_ qrc_ _autogen
ui_ .pb. /test /examples /benchmark /fuzz /node_modules /gen/`.

---

## Headline methodological result — `rare_df` must be relative (spike finding #4, confirmed at scale)

Two passes, identical except the rare-token cutoff:

- **Pass 1** — absolute `--rare-df 50`, `--min-shared 3`.
- **Pass 2** — relative `--rare-df-pct 8` (cutoff = 8 % of fragment count, floored at 8), `--min-shared 4`.

| repo | fragments | pass1 reported (df≤50) | pass2 cutoff | pass2 candidates | pass2 reported |
|------|----------:|----------------------:|-------------:|-----------------:|---------------:|
| BambuStudio | 27243 | **0** | df≤2179 | 76441 | **180** |
| grpc | 26494 | 3 | df≤2119 | 33382 | 7712 |
| OreStudio | 18821 | 1 | df≤1505 | 46758 | 13170 |
| spectre | 5845 | **0** | df≤467 | 2418 | 31 |
| abseil-cpp | 6838 | 1 | df≤547 | 2339 | 50 |
| folly | 5346 | 1 | df≤427 | 3030 | 30 |
| AetherSDR | 4335 | 1 | df≤346 | 458 | 19 |
| ttcg | 3338 | 4 | df≤267 | 306 | 23 |
| LibreSprite | 3318 | 6 | df≤265 | 280 | 13 |
| GWToolboxpp | 3653 | 1 | df≤292 | 328 | 6 |
| Kartend | 2500 | **0** | df≤200 | 75 | 2 |
| skyrim-community-shaders | 1928 | 1 | df≤154 | 237 | 1 |
| IrredenEngine | 1747 | 1 | df≤139 | 207 | 4 |
| pico-sdk | 1161 | 5 | df≤92 | 82 | 20 |
| vmecpp | 859 | 12 | df≤68 | 216 | 11 |
| Catch2 | 398 | 7 | df≤31 | 40 | 4 |
| nlohmann_json | 389 | 59 | df≤31 | 30 | 2 |
| moqx | 296 | 4 | df≤23 | 6 | 0 |
| fmt | 272 | 4 | df≤21 | 7 | 0 |
| spdlog | 214 | 12 | df≤17 | 10 | 1 |

**Reading it:**

- The absolute cutoff is **blind on large repos**: Bambu/spectre/Kartend reported
  0 not because they are clean, but because `df≤50` on tens of thousands of
  fragments prunes every candidate (Bambu: 0 of 371 M possible pairs). Even
  distinctive normalized tokens recur in hundreds of fragments at that scale.
- The relative cutoff fixes the blind spot — Bambu 0→180, spectre 0→31, abseil
  1→50, OreStudio 1→13170.
- **But it is not a free lunch.** `--min-shared 4` (raised to keep the big-repo
  candidate sets bounded) over-prunes *small* repos whose fragments are short:
  nlohmann 59→2, spdlog 12→1, fmt/moqx → 0. And on monorepos the loosened recall
  **over-reports** (grpc 7712, OreStudio 13170) — dominated by generated /
  boilerplate, not real copy-paste (see noise section).

No single `(rare_df, min_shared)` is right for both a 214-fragment header lib and
a 27 k-fragment monorepo. This is exactly why the spike's verdict is **relative
recall + order-sensitive LCS confirm + per-repo excludes**, not a one-shot bag
gate.

---

## 🟢 Clean cross-component duplicates (the signal worth shipping)

Different files / different modules → genuine *missing reuse edge*. All token-LCS
or plain = 1.0 unless noted; renamed copies show low line-overlap (line-based
would miss them).

- **AetherSDR** — `gui/ClientChainWidget.cpp:617-672` ↔ `gui/StripChainWidget.cpp:617-672`
  (identical 55-line block); `gui/ClientReverbApplet.cpp` ↔ `gui/StripReverbPanel.cpp`.
  Whole **Client*/Strip*** UI family duplicated.
- **GWToolboxpp** — `GWToolboxdll/Utils/TextUtils.cpp:222-259` ↔
  `plugins/Base/PluginUtils.cpp:223-260`: a util copied from the main dll into the
  plugin base (cross-module). (Matches the historical PacketLogger finding.)
- **folly** — `compression/Zlib.cpp:317-327` ↔ `compression/Zstd.cpp:53-63`, and
  `compression/Compression.cpp` ↔ `Zlib.cpp`: parallel codec backends.
- **OreStudio** — `ores.compute.wrapper/.../archiver.cpp` ↔ `ores.storage/.../archiver.cpp`
  (cross-project, 1.0); plus a family of Qt `*DetailDialog.cpp` copied across
  `ores.qt` / `ores.qt.analytics` / `ores.qt.refdata`.
- **ttcg** — `Triangulation` ↔ `Triangulation_int`, `PolyPolygon` ↔ `PolyPolygon_int`
  (float/int parallel trees, plain=1.0 / line≈0.18 — pure rename copies); cross-module
  `Utils/mesh_splitter.h` ↔ `Src/NTPROSurfaceSplitSandbox/mesh_splitter.h` (line=1.0).
- **spdlog** — `details/tcp_client-windows.h` ↔ `udp_client-windows.h`;
  `sinks/daily_file_sink.h` ↔ `hourly_file_sink.h`.
- **moqx** — `relay/PublisherCrossExecFilter.cpp` ↔ `SubscriberCrossExecFilter.cpp`;
  `MoqxRelay.cpp` ↔ `NamespaceTree.cpp`.
- **vmecpp** — `fourier_basis_fast_poloidal_test.cc` ↔ `fourier_basis_fast_toroidal_test.cc`
  (poloidal/toroidal parallel implementations).
- **spectre** — `ParallelAlgorithms/Amr/Actions/AdjustDomain.hpp` ↔ `InitializeChild.hpp`.
- **skyrim-community-shaders** — `Features/TerrainHelper.cpp` ↔ `TruePBR.cpp`.
- **Kartend** — `scraperesultdialogunified.cpp` ↔ `scraperesultdialogunified_runtime.cpp`
  (base vs `_runtime` variant — a `_int`-style split).
- **pico-sdk** — CMSIS `cmsis_iccarm.h` ↔ `m-profile/cmsis_iccarm_m.h`.

The recurring shape is a **parallel family** — `Client*/Strip*`, `*_int`, `_runtime`,
`Zlib/Zstd`, `tcp/udp`, `poloidal/toroidal` — where one variant was copied and
locally edited. Token normalization sees through the renames; line matching does
not. This is the class #056 exists for.

## 🟡 Within-file / adjacent twins (mostly overloads — lower value)

Same file, adjacent blocks: nlohmann_json (`json.hpp` parallel type handlers,
`binary_reader.hpp`), fmt (`format-inl/base/chrono`), Catch2 (`textflow`,
`template_test_registry`), grpc (`call_filters.h`), folly (`AtomicUtil-inl`,
`Future-inl`), LibreSprite (`rotate.cpp` variants), abseil (test files),
IrredenEngine (`modifier_compose.hpp`). Usually intentional overload families,
not copy-paste debt.

## 🔴 What leaked as noise (sharpens the exclude story)

Pass 2's biggest "reported" counts are **not** real findings — they expose
excludes the generic set missed:

- **grpc (7712)** — almost entirely `src/core/ext/upb-gen/**` generated protobuf
  (`*.upb.h`). The `.pb.` exclude misses upb's `.upb.h` naming. Generated code is
  identical by construction; LCS confirm would *not* help (it is genuinely
  identical) — the fix is an exclude (`upb-gen`, `.upb.h`, `.upbdefs.h`).
- **OreStudio (13170)** — Qt CRUD dialog boilerplate replicated per entity across
  projects. Borderline: arguably real duplication, but at a volume that needs the
  confirm phase + a "boilerplate family" judgment, not a raw dump.
- **BambuStudio (180)** — dominated by bundled libs not caught by name
  (`src/mcut/**`, `src/minilzo/**`, embedded `src/nlohmann/**`): real internal
  dups, but inside vendored code, not Bambu's own.

Lesson, straight into the config-agent exclude rules (#056 task §"шум-файлы"):
generated-code naming is project-specific (`upb-gen`, `*.g.cpp`, `*_autogen`,
`*.upb.h`), and vendored libs are often **un-namespaced subfolders** (`mcut`,
`minilzo`) with no `third_party/` marker. A path-substring exclude list is
necessary but must be tunable per repo.

---

## How this validates the #056 spike findings

1. **Bag catches rename/cross-component copies that line-based misses** — confirmed
   across ttcg, AetherSDR, GWToolboxpp, folly, vmecpp, Kartend (low line-overlap,
   high token overlap).
2. **Bag over-reports without an order check** — grpc/OreStudio at thousands.
   Re-affirms the mandatory token-LCS confirm phase (precision) on top of bag
   recall. (Note: for *generated-identical* code even LCS=1.0 — that class is an
   exclude problem, not a metric problem.)
3. **`rare_df` must be relative** — demonstrated directly: absolute cutoff → 0 on
   large repos; relative cutoff → real findings.
4. **Excludes dominate first-run signal** — without them the top results are
   boost / upb-gen / minilzo. Per-repo, generated-naming-aware exclude lists are
   load-bearing, exactly the guidance now in the config-agent task.

## Type-3 reference — partial / diverged matches (the actual #056 target)

The clean (plain=1.0) hits above are Type-1 / pure-rename — `#053` largely covers
those. The **partial** class is what `#056` exists for: a procedure copied, then
**edited in place** in one copy (operator flipped, template→scalar, SIMD/version
variant). Found with `--partial-precise` (token-LCS + diff), `--min-tokens 80`,
keeping pairs with `lcs < 0.99` **and** a non-empty diff.

Per-repo partial count (lcs<0.99, non-empty diff): folly 58, IrredenEngine 74,
LibreSprite 60, GWToolboxpp 57, vmecpp 50, ttcg 48, Kartend 48, pico-sdk 18,
skyrim 17, moqx 16, Catch2 6, spectre 5, fmt 2, spdlog 1. (Recall-limited — see
note below; a looser broad pass surfaces far more — folly 367, spectre 341,
abseil 274, GWToolboxpp 227, grpc 215, AetherSDR 210, BambuStudio 183, ttcg 166,
at `--threshold 0.70 --min-tokens 45 --rare-df-pct 10`, big repos included.)

### What a "partial" score actually measures — rename is free, structure costs

Crucial, and the reason partial counts *look* sparse next to plain=1.0 hits.
Normalization collapses **every identifier to `id`** and every literal to `lit`.
So renaming variables changes nothing in the token stream. Direct test — a
10-line function, a rename-only copy, and a rename+operator-edit copy:

| copy | what changed | lcs | bucket |
|------|-------------|----:|--------|
| `meanOf` | only variable names (`values→items`, `total→acc`, …) | **1.000** | **full** — not partial |
| `scaled` | names **+** `+=`→`=…-`, `+=1`→`*=2`, inserted `if…continue` | 0.840 | partial (3 changed, 13 added) |

So **rename ≠ partial here, by design**: a rename-only diverged copy is a 100 %
match — that is the Type-2 robustness this pass exists for (catch the copy *through*
the rename). A partial score (`lcs<1.0`) appears only when the edit touches what
survives normalization — **operators, keywords, brackets, literals, structure**.

This is exactly the complementarity with #053:

- **#053 (line-based):** rename *is* a difference → sees a renamed copy as ~12 % line overlap.
- **#056 (token-normalized):** rename is transparent → sees the same copy as 100 %.

Consequence for counting: most real diverged copies diverge by *renaming* (parallel
components — `_int`, `Client/Strip`, `V1/V2`), so they land in the **full** bucket,
not partial. "Partial" is the narrower class of *logic-edited* copies. The score is
not "how many lines changed" — it is "how much of what survives normalization
changed". Names are free; operators are not.

### Worked examples — the diff shows exactly what diverged

- **Catch2 — two reporters, boolean logic edited** (textbook operator-flip):
  `reporters/catch_reporter_junit.cpp:254-315` ↔ `catch_reporter_sonarqube.cpp:99-160`,
  `lcs=0.909`. diff: `~ && -> ||`, `~ != -> ==`, `+ !`, `+ if (`, `- return false ;`.
- **ttcg — float/int copy, edited (not just renamed):**
  - `Triangulation/SurfaceOps.h:441-454` ↔ `Triangulation_int/SurfaceOpsInt.h:76-89`,
    `lcs=0.986`. diff: `~ id -> lit`, removed `id ::` (a templated call replaced by an int literal).
  - `Triangulation/LineDataCallbackImpl.h:133-153` ↔ `…_int/LineDataCallbackImplInt.h:100-119`,
    `lcs=0.975`, 2 changed: `id < … >` → `( … - lit )` (template → cast/arithmetic).
  - `PolyPolygon/contours_layout.h:224-243` ↔ `PolyPolygon_int/contours_layout_int.h:23-43`,
    `lcs=0.976`: `~ [ -> )` + indexing edits.
- **folly — diverged variants:**
  - `crypto/detail/MathOperation_AVX2.cpp:164-188` ↔ `MathOperation_SSE2.cpp:161-187`,
    `lcs=0.984` (same routine, diverged on the SIMD intrinsics).
  - `hash/SpookyHashV1.cpp:165-219` ↔ `SpookyHashV2.cpp:167-220`, `lcs=0.964` (V1→V2).
  - `fibers/Semaphore.cpp:139-188` ↔ `SemaphoreBase.cpp:141-189`, `lcs=0.970`.

The recurring profile: one procedure spread across components and then locally
edited — `&&↔||`, template↔scalar, `AVX2↔SSE2`, `V1↔V2`. Plain=1.0 would miss the
*edit*; line-based misses the *whole pair* (line-overlap here is 0.06–0.49). Only
token granularity + an order-sensitive diff surfaces and explains it.

### Recall is the binding constraint (finding #4, again)

Partial counts read low because the rare-token inverted index **drops diverged
copies before scoring**: the harder the edit, the fewer rare tokens the copies
still share, so they never become candidates. This is exactly the case for a
k-gram / winnowing fingerprint fallback on the recall side — a tight rare-token
gate optimizes precision at the cost of the very Type-3 copies the pass targets.
Reproduce partials: `partial_duplication <repo> --partial-precise --min-tokens 80
--rare-df 8 --rare-df-pct 8 --threshold 0.82 <shared excludes>`.

## Caveats

- This is an **explorer**, not a gate: thresholds/recall were swept, not
  calibrated per repo; counts above are not "duplication scores".
- No token-LCS confirm in the sweep (would cut idiom-FPs, not generated-identical).
- Single-setting recall can't fit both tiny libs and monorepos — by design the
  product wants relative recall + confirm + tuned excludes.
- Reproduce: `partial_duplication <repo> --metric plain --threshold 0.90
  --rare-df 8 --rare-df-pct 8 --min-shared 4 <shared excludes>`.
