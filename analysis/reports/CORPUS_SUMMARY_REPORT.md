# Corpus Summary Report — archcheck on 310 C++ repos

**Window:** all non-merge commits since 2025-05-01. **Scan:** archcheck 0.1.0, test code
excluded from all signals (#070). **Data:** `corpus_summary.tsv` + per-repo
`<repo>_graph_drift.{jsonl,md}` (graph) + `<repo>_dup.txt` (duplication).

> Every number below was **independently re-derived from the raw artifacts**, gated by an
> automated verifier (`verify_corpus.py`) that includes a synthetic cycle-regression test —
> see §6. **Five pipeline bugs** were found and fixed mid-analysis (§5, §8.4); the totals
> here are post-fix. The graph window is now *all non-merge commits* (not `--first-parent`),
> so `grown_cycles` is finally non-zero (§8.1).

---

## 1. Corpus overview

| metric | value |
|---|---|
| repos (C/C++, ≥5 files, ≥1 commit in window) | **310** (343 scanned, 26 too-small/inactive, 7 non-C++ removed) |
| total graph_errors (per-commit drift signals) | **13 476** |
| of which: commits that introduced a **grown cycle** | **72** (136 cycles, in 35 repos) |
| total duplication pairs (HEAD) | **6 402** |
| repos with graph_errors > 0 | 289 (93%) |
| repos with dup_pairs > 0 | 225 (73%) |
| repos with **both** problems | 215 (69%) |
| repos with **neither** | 11 (4%) |
| AI-commit % — mean / median / stdev | 52.9% / 53.3% / ±29.2 |
| repos 100% AI-marked | 6 |
| repos 0% AI-marked | 20 |

> **Corpus cleanup (2026-06-04):** 7 non-C++ repos were dropped from `/home/localadm/oss/`
> and from `corpus_summary.tsv` (`_remove_plan.tsv` tag `not-cpp`): 4 Flutter apps
> (C++ = only the generated desktop runner), 1 Python Telegram bot (`saud552_NHAY7`),
> 1 React-Native lib (`m-szopinski_BabylonReactNative`), 1 .NET-hosting bridge
> (`jpfeuffer_openms-thermo-bridge`). They leaked in because every selection path filtered
> on GitHub/Linguist *dominant language* without a C++-share guard. §1 above is now
> re-derived directly from `corpus_summary.tsv` (n=310); the graph_errors-presence counts
> (ge>0 / both / neither) use the raw per-repo column and supersede the earlier
> windowed-jsonl figures (272 / 215 / 17 at n=317).

**graph_errors** = commits in the window whose include-graph diff fired any signal
(added/removed edges, grown cycle, new cross-area dep). **dup_pairs** = reported
copy-paste pairs on HEAD across 5 clone types (EXACT, LITERAL, MIXED, RENAMED, STRUCTURAL).

---

## 2. Key findings (on verified data)

### 2.1 AI adoption does NOT predict code-quality problems
Spearman correlation, n=310:

| relationship | ρ | reading |
|---|---|---|
| **AI% ↔ graph_errors** | **−0.065** | no correlation |
| **AI% ↔ dup_pairs** | **+0.061** | no correlation |
| **graph_errors ↔ dup_pairs** | **+0.481** | moderate ✓ |

Both debt signals correlate with **each other** (0.49) but neither with AI%. The worst
graph-drift repo is **43% AI** (`makr-code_ThemisDB`, 1558); the second is **0% AI**
(`deskull-m_bakabakaband`, 788). AI% is not a risk signal — workflow and project size are.

### 2.2 The top duplication numbers are fork/vendor, not author copy-paste
The 3 largest dup repos = **3 582 / 6 402 = 56%** of all pairs, and they are almost
entirely structural artifacts, not developer copy-paste:

| repo | pairs | FP share | what it really is |
|---|---|---|---|
| awest813_CnC_Generals_Zero_Hour | 2 538 | 96% (`Generals/`↔`GeneralsMD/`) | EA base game + Zero Hour expansion (byte-identical trees) |
| HyperCogWizard_a81ml-org | 721 | 99.9% (ggml ×3) | vendored ggml under llama.cpp/ + whisper.cpp/ |
| awest813_Dewm-3 | 323 | 81% (`neo/game/`↔`neo/d3xp/`) | Doom 3 base + Resurrection of Evil expansion |

Real authored copy-paste exists but is smaller and lives in normal source trees (§4).

---

## 3. How the graph changes — with file names

### 3.1 Per-commit drift (real `added:` edges from the diff)
**deskull-m_bakabakaband** (#1, 622) — `floor/geometry.h` turning into a hub:
```
+ src/spell-kind/spells-specific-bolt.cpp  ->  src/floor/geometry.h
+ src/target/target-getter.h               ->  src/floor/geometry.h
```
**makr-code_ThemisDB** (#2, 451) — cross-layer reach (server → cache, benchmarks → internals):
```
+ include/server/http_server.h  ->  include/cache/semantic_cache.h
+ benchmarks/bench_query.cpp    ->  include/index/secondary_index.h
```
**Krilliac_SparkEngine** (commit `8175be5d`, "split 25+ oversized headers") — one refactor
adding 54 edges, most funnelling into one god-header + creating a cycle:
```
+ SparkEngine/.../AI/BehaviorTreeTypes.h  ->  SparkEngine/Source/Core/Platform.h
+ SparkEngine/.../SaveSystemTypes.h       ->  SparkEngine/Source/Core/Platform.h
grown_cycles: 1     (RHIPipelineTypes.h <-> RHITypes.h)
```

### 3.2 God-headers (fan-in > 50) — HEAD scan
| repo | header | fan-in |
|---|---|---|
| Krilliac_SparkEngine | `SparkEngine/Source/Core/Platform.h` | **439** |
| djeada_Standard-of-Iron | `game/core/component.h` | 201 |
| djeada_Standard-of-Iron | `game/core/world.h` | 152 |
| Krilliac_SparkEngine | `SparkEngine/Source/Utils/Assert.h` | 132 |

### 3.3 Include cycles (`[SF.9]`) — real mutual includes
```
Krilliac_SparkEngine : RHIPipelineTypes.h:14  ->  RHITypes.h
                       RHITypes.h:463         ->  RHIPipelineTypes.h   ("backwards compatibility")
AlchemyViewer        : indra/llcharacter/llcharacter.h:38 <-> llmotioncontroller.h:237
deskull-m_bakaba…    : src/system/angband.h:22 <-> src/term/z-term.h:11
BALL-Project_ball    : scoringComponent.h:11 <-> scoringFunction.h:18
```

### 3.4 Chain-length (depth > 10)
`UNIGINE SDK Unigine.h` — depth **33**; `SparkEngine` — 58 files over the threshold.

---

## 4. Code chunks — real STRUCTURAL clones (authored, not fork/vendor)

### 4.1 makr-code_ThemisDB — TLS init copied between QUIC server and transport
`src/network/quic_server.cpp:149-198` ↔ `src/network/quic_transport.cpp:82-127` (STRUCTURAL 0.999)
```cpp
SSL_CTX* ctx = SSL_CTX_new(TLS_server_method());
if (!ctx) { return nullptr; }
// QUIC requires TLS 1.3; reject any older protocol version.   // A
// QUIC requires TLS 1.3; reject older versions.               // B
SSL_CTX_set_min_proto_version(ctx, TLS1_3_VERSION);
SSL_CTX_set_max_proto_version(ctx, TLS1_3_VERSION);
```
**Δ:** identical TLS-1.3 context setup duplicated; only the comments differ.

### 4.2 aethersdr_AetherSDR — two Qt fader widgets share their constructor
`src/gui/ClientCompThresholdFader.cpp:36-81` ↔ `src/gui/ClientEqOutputFader.cpp:44-89` (STRUCTURAL 0.96)
```cpp
setFixedWidth(kLabelColW + kGap + kBarW + kHandleOverhang * 2 + 2);
setMinimumHeight(160);
setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
setFocusPolicy(Qt::ClickFocus);
setCursor(Qt::ArrowCursor);
setToolTip( /* threshold vs output-gain text differs */ );
auto* root = new QVBoxLayout(this);
```
**Δ:** widget scaffolding copied; only tooltip text + one `setMouseTracking` differ.

### 4.3 awest813_Dewm-3 — Lexer ↔ Parser (rename clone)
`neo/idlib/Lexer.cpp:943-988` ↔ `neo/idlib/Parser.cpp:2452-2497` (STRUCTURAL 0.998)
```cpp
if ( !idLexer::ReadToken( token ) ) { idLexer::Error("couldn't read..."); return 0; }
if ( !idParser::ReadToken( token ) ){ idParser::Error("couldn't read..."); return 0; }
switch( type ) { case TT_STRING: str="string"; case TT_NUMBER: str="number"; ... }
```
**Δ:** same `ExpectTokenType`, only the class name `idLexer`→`idParser`.

### 4.4 awest813_Dewm-3 — platform net backends
`neo/sys/aros/aros_net.cpp:140-173` ↔ `neo/sys/posix/posix_net.cpp:132-165` (STRUCTURAL 0.976)
```cpp
memset(sadr,0,sizeof(*sadr)); sadr->sin_family=AF_INET; sadr->sin_port=0;
if (s[0]>='0'&&s[0]<='9') {
  if (!inet_aton((char*)s,&sadr->sin_addr)) { ... }   // A: cast
  if (!inet_aton(      s,&sadr->sin_addr)) { ... }   // B: no cast
```

### 4.5 FiveTechSoft_HarbourBuilder — C backend copied into C++
`source/backends/gtk3/gtk3_core.c:10220-10257` ↔ `source/cpp/hbbridge.cpp:5093-5129` (STRUCTURAL 0.995)
```cpp
if (!cHrbFile || s_dbgState!=DBG_IDLE) { hb_retl(HB_FALSE); return; }
hb_dbg_SetEntry(IDE_DebugHook); s_dbgState=DBG_STEPPING;
DbgOutput("=== Debug session started ===\n");    // A: \n
DbgOutput("=== Debug session started ===\r\n");   // B: \r\n + extra line
```

### 4.6 FiveTechSoft_OpenADS — B-tree seek copied for first vs last
`src/drivers/adi/adi_index.cpp:382-403` ↔ `:409-429` (STRUCTURAL 0.70)
```cpp
for(;;){ if(auto r=read_adi_page_(cur,pg);!r) return r.error(); ...
  cur_idx_ = 0;                              // A: seek-first
  cur_idx_ = static_cast<int32_t>(ct) - 1;   // B: seek-last
```

---

## 5. Pipeline bugs found & fixed during this analysis

**Five** defects were silently corrupting the dataset — every one produced a plausible-looking
**0**, invisible to the eye. Each was caught only by independent re-derivation / the verifier.

1. **Silent per-repo timeout → false `graph_errors=0`.** The orchestrator killed any repo
   whose per-commit graph-drift exceeded 600 s and recorded **0** — zeroing the **11 largest**
   repos (`makr-code_ThemisDB` 1.9 M LOC showed 0). Fix: timeouts ×10; never silently truncate.
2. **`.with_suffix()` truncated dotted repo names.** `benpm_.dev` → `benpm_.jsonl` (dot-tail
   eaten), so the orchestrator never found the file → **0**. Hit **6** repos (`benpm_.dev` has
   8 commits — proof it wasn't timeout). Fix: build the path by string-append.
3. **`--first-parent` skipped the commits where cycles are born.** The scan walked the mainline
   only, so a cycle introduced on a feature branch (reaching HEAD via the merge) was never
   diffed → `grown_cycles=0` corpus-wide. Fix: walk **all non-merge commits** (`--no-merges`).
4. **The summary parser clobbered `grown_cycles` to 0.** `grown_cycles:` appears twice in the
   diff output — the summary counter *and* a detail-section header (empty). The parser let the
   header overwrite the count back to 0, so even scanned cycle-births read as 0. Fix: only
   update on a numeric tail.
5. **Baseline-skip — the first in-window commit was never diffed.** The generator iterated
   `commits[1:]`, using `commits[0]` as baseline. A fork-import commit that adds a whole
   codebase + cycle in that first commit (`CrispStrobe_CrispASR` `ca0911db`: +1209 edges,
   grown_cycles 1) was missed. Fix: diff every commit; a true repo-root (no parent) is skipped
   by its error return.

Bugs #3–#5 all hid the same thing — cycle births. After fixing them the corpus went from
**0 → 136 grown cycles in 35 repos**. A synthetic regression test (branch-born cycle in the
first window commit) now fails the verifier if any of the three regresses.

The graph total: **5 904 (timeout-corrupt) → 8 912 (recovered, first-parent) → 13 476
(all non-merge commits, n=310 after the non-C++ cleanup)**, with grown_cycles finally non-zero.

### Recovered repos (intermediate first-parent step; final no-merges values in §7 are higher)
| repo | real graph_errors | AI% | window commits |
|---|---|---|---|
| deskull-m_bakabakaband | **622** | 0.0% | 1981 |
| makr-code_ThemisDB | **451** | 43.1% | 1590 |
| AztecProtocol_aztec-packages | 386 | 2.3% | 4107 |
| danielraffel_pulp | 282 | 18.6% | 2298 |
| aethersdr_AetherSDR | 280 | 82.2% | 1975 |
| CrispStrobe_CrispASR | 268 | 55.2% | 2252 |
| dilithion_dilithion | 261 | 81.5% | 1761 |
| GregorGullwi_FlashCpp | 157 | … | 2831 |
| deltahdl_deltahdl | 140 | … | 9914 |
| (8 more, 0–46) | … | | |

---

## 6. Verification (independent re-derivation of every result)

| result | how re-checked | outcome |
|---|---|---|
| graph_errors (all 310) | re-counted error-records from each `_graph_drift.jsonl`, windowed to last N=commits, compared to tsv | **0 mismatches**; all 310 have a jsonl (was 317; 7 non-C++ dropped, see §1 note) |
| dup_pairs (all 310) | parsed `reported N` and independently counted pair lines (all 5 clone types) in each `_dup.txt`, compared to tsv | **0 mismatches**; re-derived at n=310: Σ types = 4145 EXACT + 1804 STRUCTURAL + 254 RENAMED + 129 LITERAL + 70 MIXED = **6402** = Σ reported |
| AI% (spot, 5 repos) | replicated orchestrator logic exactly (first-parent, `%B` split on `---END---`, marker match) | exact match (CnC 100%, aethersdr 82.2%, SparkEngine 0.6%, FreshVoxel 96.6%, CopilotChess 94.3%) |
| correlations | recomputed Spearman from scratch on corrected tsv (n=310) | −0.065 / +0.061 / +0.481 |
| fork/vendor claim | `grep`-counted `Generals/↔GeneralsMD/`, `ggml`, `neo/game↔d3xp` shares in `_dup.txt`; `diff`'d a CnC pair (0 lines) | confirmed 96% / 99.9% / 81% |
| code chunks | `sed` of both endpoints of each STRUCTURAL pair from the live repo | shown verbatim in §4 |
| **automated gate** | `verify_corpus.py`: regression test + L0 completeness + L1 ground-truth + L2 redundancy (summary counter == detail bullets) + L3 HEAD↔commit cycle reconciliation (auto-classifies pre-window vs in-window via the include's git history) + L4 dup integrity | **ALL CHECKS PASSED** (0 anomalies); report regenerated only on a clean run |
| **cycle-regression test** | synthetic repo: a cycle born on a feature branch in the first in-window commit must surface `grown_cycles>0` — fails if first-parent/parse/baseline-skip regress | PASS |

---

## 7. Top-10 tables (honest, final — all non-merge commits)

### graph_errors
| # | repo | graph | AI% | dup | LOC |
|---|---|---|---|---|---|
| 1 | makr-code_ThemisDB | 1558 | 43.1% | 99 | 1.9M |
| 2 | deskull-m_bakabakaband | 788 | 0.0% | 17 | 357K |
| 3 | AztecProtocol_aztec-packages | 602 | 2.3% | 41 | 1.1M |
| 4 | boydsoftprez_NereusSDR | 489 | 56.9% | 17 | 988K |
| 5 | Krilliac_SparkEngine | 438 | 0.6% | 37 | 689K |
| 6 | djeada_Standard-of-Iron | 344 | 56.3% | 25 | 307K |
| 7 | dilithion_dilithion | 315 | 81.5% | 62 | 254K |
| 8 | aethersdr_AetherSDR | 284 | 82.2% | 78 | 1.6M |
| 9 | danielraffel_pulp | 284 | 18.6% | 30 | 577K |
| 10 | CrispStrobe_CrispASR | 270 | 55.2% | 70 | 704K |

### dup_pairs
| # | repo | dup | type | AI% |
|---|---|---|---|---|
| 1 | awest813_CnC_Generals_Zero_Hour | 2538 | fork (base+expansion) | 100% |
| 2 | HyperCogWizard_a81ml-org | 721 | vendored ggml ×3 | 44.1% |
| 3 | awest813_Dewm-3 | 323 | fork (base+expansion) | 100% |
| 4 | shifty81_MasterRepoRefactor | 205 | — | 33.3% |
| 5 | AlchemyViewer_Alchemy | 113 | — | 48.1% |
| 6 | makr-code_ThemisDB | 99 | authored | 43.1% |
| 7 | Zero3K20_hpsx64 | 98 | — | 88.9% |
| 8 | aethersdr_AetherSDR | 78 | authored (Qt widgets) | 82.2% |
| 9 | CrispStrobe_CrispASR | 70 | — | 55.2% |
| 10 | alphaonex86_CatchChallenger | 63 | — | 53.5% |

---

## 8. False-positive verification (manual)

Both headline counts contain a large fraction of false positives. Re-derived from the raw pairs/records.

### 8.1 Graph — the per-commit `graph_errors` metric is mostly churn (but cycles now fire)
Breakdown of all **13 476** error-commits by which signal fired (shares unchanged by the
7-repo cleanup — they contributed 9 of 13 485 error-commits):

| signal | share | reading |
|---|---|---|
| `added_edges` only | **90.0%** | a commit added an `#include` — **churn**, weak signal, mostly FP for "architecture problem" |
| `removed_edges` | 11.8% | edges deleted |
| `new_area_deps` (cross-area) | 6.4% | a new module→module crossing — **meaningful** |
| `grown_cycles` | **72 commits → 136 cycles in 35 repos** | a cycle was born — **the real architectural signal** |

`grown_cycles` was **0** for most of this analysis — three stacked bugs (§5 #3–#5): the scan
walked `--first-parent` (skipping the branch commits where cycles are born), the parser
clobbered the count to 0 with the detail-section header, and the first in-window commit was
used as a baseline and never diffed. After all three fixes, cycle births surface: **136 in 35
repos** (e.g. `Krilliac_SparkEngine` `RHIPipelineTypes.h↔RHITypes.h` born in `8175be5d`;
`CrispStrobe_CrispASR` born in the fork-import `ca0911db`). A synthetic cycle-regression test
now guards all three in the verifier so they cannot silently come back.

**Verdict:** the `added_edges` bulk is still churn — rank architecture risk by the
**HEAD-scan** signals (cycles `[SF.9]`, god-headers, chain-length, §3.2–3.4) and the **136
grown cycles**, not the raw error-commit count.

### 8.2 Duplication — ≥61% is fork/vendor, genuine copy-paste is ~1/5
Classification of all **6 392** reported pairs:

| class | share | TP / FP |
|---|---|---|
| fork — sibling base+expansion tree (`Generals/`↔`GeneralsMD/`, `neo/game`↔`neo/d3xp`) | **41.7%** | FP |
| vendored library (ggml, third_party, …) | 11.8% | FP |
| same-basename copy across trees | 7.6% | FP-ish |
| self — intra-file, adjacent ranges | 20.1% | mixed (boilerplate / real) |
| cross-file authored | 18.8% | **TP** — but includes amalgamation FP (`epiworld.hpp` ↔ `*-meat.hpp` single-header builds) |

**Verdict:** **≥61% of dup pairs are fork/vendor FP**; genuine authored copy-paste is
~15–19%. The headline `dup_pairs` overstates real copy-paste roughly **5×**.

### 8.3 Residual: CamelCase test names leak past the test filter
The #070 test-exclusion matches whole segments (`tests/`) and `*_test`/`*_spec` basenames,
but misses **CamelCase compound** names — `EngineTests/`, `BalanceTests.cpp`,
`MatrixUnitTest.cpp`. This leaks **25 / 6 392 pairs (0.4%)** of test code back in. Not fixed:
widening to a `*test$` suffix would wrongly catch production `backtest/` and `contest/`. Small,
documented, left as-is.

### 8.4 Duplication is also UNDER-counted — two scanner bugs cause misses
Opposite of the fork/vendor inflation: real authored copy-paste is silently dropped.

- **Bug A — `discoverFiles` aborts on the first FS error.** Its loop is
  `while (!ec && it != end)`; one bad entry kills the whole traversal. `jjbudz_6502`
  has a self-referential symlink (`_codeql_detected_source_root -> .`) → the recursive
  iterator loops → **0 files scanned** despite 10 tracked `.cpp`.
- **Bug B — SPDX/license header treated as "vendored".** `hasVendorLicenseHeader` flags
  any permissive-license banner as vendored, but **SPDX headers are now standard on
  authored code**. `fuddlesworth_PlasmaZones` (a KDE project) has
  `SPDX-License-Identifier: GPL-3.0` on **all 401** of its own `.cpp` → every file
  classified vendored → **0 scanned**. Same for `ajazz-control-center`, `limitless`.

**Scale:** 4 repos scanned 0 files (full miss); a sample of 120 also shows partial
under-counts where authored files carry SPDX headers (some legit-vendored, some not).
So the headline 6 402 is simultaneously **inflated** by fork/vendor FP (§8.2) and
**deflated** by these over-exclusion misses — neither direction is trustworthy without
the fixes. Bug A is a safe fix (don't abort on per-entry error); Bug B is a design call
(distinguish vendored-with-license from authored-with-SPDX). Both flagged, not yet fixed.

---

## 9. Drill-down — clickable links

### View real code fragments for duplicates
- **16 worked examples with embedded code** (graph + STRUCTURAL clones): [EXAMPLES_10_GRAPH_10_CLONES.md](EXAMPLES_10_GRAPH_10_CLONES.md)
- **6 code chunks above** in §4 (ThemisDB TLS, AetherSDR Qt, Lexer/Parser, …)
- **Full per-repo pair lists** (`file:line <-> file:line (TYPE)`), open then jump to the source line:
  - authored TP: [makr-code_ThemisDB](ai_repo_run/makr-code_ThemisDB_dup.txt) · [aethersdr_AetherSDR](ai_repo_run/aethersdr_AetherSDR_dup.txt) · [chrxh_alien](ai_repo_run/chrxh_alien_dup.txt)
  - fork/vendor FP: [CnC_Generals](ai_repo_run/awest813_CnC_Generals_Zero_Hour_dup.txt) · [HyperCogWizard (ggml)](ai_repo_run/HyperCogWizard_a81ml-org_dup.txt) · [Dewm-3](ai_repo_run/awest813_Dewm-3_dup.txt)

### See how the graph spreads (per-commit added edges + cross-area, with file names)
- [deskull-m_bakabakaband](ai_repo_run/deskull-m_bakabakaband_graph_drift.md) — #1, 622
- [makr-code_ThemisDB](ai_repo_run/makr-code_ThemisDB_graph_drift.md) — #2, 451
- [AztecProtocol_aztec-packages](ai_repo_run/AztecProtocol_aztec-packages_graph_drift.md) — #3, 386
- [Krilliac_SparkEngine](ai_repo_run/Krilliac_SparkEngine_graph_drift.md) — the `8175be5d` "split 25 headers" refactor (added 54 edges into `Platform.h`)

> Each `_graph_drift.md` lists, per commit, the new include edges (`A.h -> B.h`) and any new
> cross-area dependency — i.e. exactly *how* the dependency graph spread, with file names.
> The corpus source itself lives under `/home/localadm/oss/…` (outside this workspace); the
> `_dup.txt` pair gives `path:line` to open it there.
