# [RESEARCH] Run over new AI sources: graph + duplication

**Created:** 2026-05-30
**Started:** 2026-05-30
**Status:** wip
**Module:** RESEARCH
**Priority:** major
**Difficulty:** unknown
**Blocks:** —
**Blocked by:** — (the clone detector is standalone spike #053; the cross-component ratio from P0-B is desirable but not a blocker: a cross-file proxy on the first run)
**Related:** #033 (ai_drift_dataset — first corpus, DRIFT over 33 PRs), #053 (fast_backend_line_duplication_pass — detector), #052 (cross_tu_duplication_detector — AST layer), #048 (drift_clean_checkout_methodology), #029 (metric_regression_detection)

## Goal

Run **both** archcheck metrics over a corpus of AI repositories (a ready-made public dataset, not a homemade
grep) — the structure/drift of the include graph (DRIFT.1/DRIFT.2
from #009 + absolute graph metrics) and duplication (line-dup from #053) — and
record the measured numbers without interpretation. The corpus is new, so the
graph run is done from scratch: an honest re-check of the past narrow result (#033)
on an unbiased set and a direct comparison of "where the signal is stronger — graph or clones".

## Context

The first corpus (#033) searched for AI PRs by 1-2 git markers and measured the **drift
of the include graph** (DRIFT.1/DRIFT.2). The catch turned out narrow: 12 DRIFT.1 hits on
7 of 33 PRs, DRIFT.2 — zero. A review of the field (2026-05-30) showed two things:

1. **Ready-made AI-repo sources exist** — no need to grep GitHub by hand. See
   [docs/research/ai_code_detection_landscape.md](../../docs/research/ai_code_detection_landscape.md):
   - **Configs dataset** ([2605.08435](https://arxiv.org/html/2605.08435)) —
     4,738 repos with AI configs; 71.6% of them have an AI commit.
   - **AIDev** — 932k agentic PRs / 116k repos; there are also human PRs (for within-repo
     control).
   - **Debt Behind the AI Boom** ([2603.28592](https://arxiv.org/abs/2603.28592))
     — 304k AI commits by 4 git signals (29 tools).
2. **We measure BOTH metrics.** On the first corpus (#033) graph drift was almost silent, but
   the corpus was narrow and manual. On an unbiased dataset we need to re-check the graph
   from scratch AND add duplication (where the field signal is stronger — GitClear: blocks of 5+
   clones ×8 over 2024; "AI Code in the Wild": AI code concentrates in
   glue/tests/boilerplate). The goal is a direct comparison of signal strength on one corpus.

This task is **only the run and number collection**. Hypotheses and project conclusions
("duplication as a detector of AI drift", archcheck positioning) are taken
separately and not written here, so the data stays neutral.

All methods/datasets/links are already gathered in
[ai_code_detection_landscape.md](../../docs/research/ai_code_detection_landscape.md)
— it is the reference entry point for the task.

## Reconnaissance (2026-05-30, done — the numbers are real)

The source chosen is **AIDev** ([hao-li/AIDev](https://huggingface.co/datasets/hao-li/AIDev),
HF, CC BY 4.0). Downloaded `all_repository.parquet` (**116,211 repos**), counted
by the `language` column (pandas + pyarrow):

| Language | Repos |
|------|------|
| Python | 25 660 |
| TypeScript | 19 709 |
| JavaScript | 15 247 |
| (none) | 11 986 |
| HTML | 10 336 |
| Java | 3 481 |
| C# | 3 463 |
| Go | 2 511 |
| PHP | 2 483 |
| **C++** | **2 445** |
| Rust | 1 958 |

C++ by stars: `≥10` → 233, `≥50` → 150, **`≥100` → 119**, `≥500` → 73,
`≥1000` → 64, `≥5000` → 33. (Of 2,445 C++ repos only 233 have ≥10 stars —
the rest are small/forks/educational.)

Top C++ by stars (to understand the composition): bitcoin/bitcoin (84.8k),
ggml-org/llama.cpp (83.7k), opencv/opencv (83.3k), tesseract-ocr/tesseract
(68.5k), x64dbg/x64dbg, ggml-org/whisper.cpp, microsoft/WSL, gabime/spdlog,
keepassxreboot/keepassxc, PaddlePaddle/Paddle, Tencent/ncnn,
microsoft/onnxruntime, …

**Reconnaissance conclusion:** the corpus is viable but more modest than expected — the working core is
~119 repos (≥100★) / ~233 (≥10★). The top is large mature projects (opencv/bitcoin
clone slowly and are heavy). The Configs dataset (2605.08435) is not needed: there C++ is
hidden in the "remaining 27.5%" without a separate number, while AIDev gives an exact language + stars
+ human/agentic PRs for a future within-repo step.

`all_repository` columns: `id, url, license, full_name, language, forks, stars`.
The dataset also has `pr_commit_details` / `pr_commits` / `human_pull_request`
— for a within-repo AI/human slice at the next stage.

## Resolved forks

- **Source:** AIDev (per the reconnaissance above).
- **Slice:** first **whole-tree** (cheap, the tool exists); within-repo
  AI/human — as a separate next stage, once whole-tree shows a signal.
- **Duplication detector:** standalone spike
  `experiments/line_duplication/main.cpp` (removed from the tree, in git history — see #053)
  with an explicit "cross-file proxy" note (we don't wait for the #053 port into `src/`).
- **Graph detector:** DRIFT.1/DRIFT.2 already in `src/` (#009). For whole-tree
  we take absolute graph metrics (cycles, CCD/ACD/NCCD, god-headers, chain
  length); DRIFT delta — where applicable (on the AI-PR range).

## Corpus selection (decided 2026-05-30)

- **Sample:** ~50 **random** C++ repos from the `≥100★` layer (119 of them), with a fixed
  seed for reproducibility.
- **Size limit:** take only repos **< 500 MB** (size via
  `gh api repos/{full_name}` → the `size` field in KB, threshold < 512000). Mega-projects
  (opencv/bitcoin/paddle) are cut off by this limit naturally.
- **Partial checkout:** pull only the analyzable text/code files, exclude
  binaries/images/documents (`*.png/jpg/gif/ico/pdf/zip/bin/...`), which
  archcheck and the spike don't read anyway. Method: `git clone --filter=blob:none`
  + sparse-checkout by code extensions, or post-clone cleanup. Saves disk.

## Execution plan

- [ ] Pin down N and the selection criterion for C++ repos from AIDev (see open question).
- [ ] Export the list of `full_name`, clone into `~/oss` (permanent storage, not sandbox).
- [ ] **Graph metrics:** run archcheck (`src/`) over each repo — absolute (cycles, CCD/ACD/NCCD, god-headers, chain length) + DRIFT delta where applicable.
- [ ] **Duplication:** run the line-dup spike over each repo (whole-tree).
- [ ] Apply default excludes + auto-exclusion of nested git repos/vendoring (lesson #053 P0-A).
- [ ] Record per repo: sig LOC; graph — cycles/CCD/god-headers/chain; clones — total ratio, cross-file proxy ratio, top-N blocks; time/RSS.
- [ ] Spot-check the findings of both metrics by hand (valid signal / noise).
- [ ] Write the result into `docs/research/` as a separate run log (like `ai_drift_runlog.md`), **only numbers + manual verification**.

## Acceptance criteria

- [ ] The corpus is assembled from AIDev (a ready-made dataset), the selection criterion and N are documented.
- [ ] The run is reproducible: commands + archcheck version + spike version + excludes are pinned down.
- [ ] The run log has **both graph and duplication numbers** for each repo (a signal-strength comparison is possible).
- [ ] The findings of both metrics are spot-checked by hand.
- [ ] The limitations are named explicitly: language bias of the dataset; cross-file proxy ≠ cross-component; whole-tree does not separate AI from human (within-repo — the next stage; correlation ≠ causation).
- [ ] The file contains no project conclusions — only measurements.

## Done

- **2026-05-31 CHEAP GRAPH DETECTORS over the corpus (HEAD snapshot, 68 repos).** Report:
  `experiments/ai_repo_run/GRAPH_PROBE_FINDINGS.md`. Without prod changes: the graph is
  exported by the existing `--save-graph-baseline`, analysis — `graph_probe.py`
  (8 detectors: cycle-members / mutual-pairs / self-include / backdoor / fan-in /
  fan-out / longest-chain / orphan). The run — `graph_probe_all.sh`, raw data in
  `runs_history/probe/`. Findings (checked by hand):
  - **mc2** — a tangle of **56 components** (`mclib/stuff/*.hpp`, 47 hpp↔hpp pairs).
  - **acts** — **45 `hpp↔ipp`** pairs by name (the same class as §7.2).
  - **`[SELF]` exposed a resolver bug → fixed in #058**: an angled
    `#include <basename.h>` collapsed onto a same-named project header →
    a false self-loop (bitcoin `<cpuid.h>`, esphome `<md5.h>`, FastLED `<alloca.h>`).
    The single-candidate variant of #036. Fix: a guard against self-edge in `include_resolver.cpp`
    + 3 tests; after the fix the corpus has self-edges 0/68, 8 phantom cycles gone.
  - Corpus: cycles in 25/68 (after the self-edge fix); backdoor (hdr→.cpp) = 0;
    fan-in champion esphome `core/log.h` 1049. Candidates for real checks — see #057.
  - **Module boundaries (2nd pass, closed the 5th original chat metric):** `[MODULE]`
    cycles between module directories — **10/68 repos** (≠ 25 file cycles!), the genuine ones
    were verified (FastLED src↔examples, stellar-core lib↔src). `[ENCAP]` breach with a prod/test split:
    hundreds of raw hits (acts 124), after the split prod→prod in 3 repos; manual check:
    **acts (13) and otel (2) — REAL breaches in `Core/.../detail`**, cpprestsdk (15) —
    an artifact of src↔include of one lib. Lesson: the boundary detector requires a config definition
    of modules (#051), otherwise it confuses a lib's layout with a breach. Along the way a
    probe bug was fixed: `parse()` did not strip quotes → the numbers were off.
- **2026-05-31 CLEANUP OF THE CORPUS ON DISK.** By the user's criterion, clones were removed
  on which archcheck found nothing or found only false positives (vendoring /
  fork-in-repo / data-blob / meta-aggregator / codegen / header-only phantom),
  including borderline ones; repos with high absolute dup or a real signal were kept.
  Classification by `runs_history/*.tsv` (`classify_remove.py`) → removal
  (`do_remove.py --apply`). Result: **116 repos, ~6.0 GB** (`~/oss`
  26→20 GB); high-star 29 / ~2.6 GB, low-star 87 / ~3.6 GB. List+reasons —
  `REMOVED.md`. **Evidence TSVs and series were NOT touched** — the reports are reproducible,
  only re-clonable clones were removed. README §1 (previously "DO NOT delete") and
  DRIFT_RUN_REPORT §7.5bis were updated.
- **2026-05-31 CORPUS DRIFT RUN (85 repos, for the article).** `drift_all.sh`
  over all full-history repos in `_aidev_run/`, 25 snapshots/repo, both metrics.
  Data: `runs_history/all/*.tsv` + `drift_summary_v2.tsv`. Report §7. Outcomes:
  - **Copy-paste: 39 repos grow (median +8.2%, max +69%), 28 stable, 18 DECREASED.**
    Drift prevails, but is reversible (not universal).
  - **Cycles: 15 repos gave birth to cycles (0→N), 20 grew them.**
  - **4 categories of causes** with localized culprit commits (verified with git show):
    A) copy-paste — opencv-mobile (600-line JPEG encoder cix↔rpi), OptiScaler cc5d695e;
    B) graph/cycles — **acts `233ba90c` (2025-06-02): ".ipp include .hpp" refactoring
       closed hpp↔ipp → cycles 3→43 in ONE commit**; OptiScaler d3e1c13a;
    C) connectivity — stellar-core e/n 1.9→6.3, otel 2.1→5.7, FastLED 1.3→4.8;
    D) counter-examples — alpaka (dup 40%→19% fixed, but cycles 0→3 + edges ×100):
       copy-paste and graph drift INDEPENDENTLY → both checks are needed.
  - Artifacts excluded (CnC/GacUI fork-in-repo, idx0, data-blob) — §7.5.
  - Run infra: `gc.auto=0` on heavy repos, `checkout_watchdog.sh` (killed 1
    hung checkout on ice), baseline = the first snapshot with code (not idx0),
    snapshots sorted by date (first-parent ≠ chronology).
- **2026-05-30 FULL RUN (history drift + discipline-vs-stars).** Report:
  `experiments/ai_repo_run/DRIFT_RUN_REPORT.md`. In brief:
  - **The framing was changed:** we measure not "the commit author (AI/human)", but "in which commit
    the architecture went off" — the delta of metrics over history (graph + copy-paste).
  - **OptiScaler drift (first-parent, 30 snapshots):** dup ratio ×2.2 (12.6%→27.6%,
    peak 31%), cross-file ×3.8, edges ×10 with nodes ×4, **a cycle was born** —
    localized to commit `d3e1c13a` (2025-04-18, "Merge refactor-hooks"):
    splitting `dllmain` into `proxies/`+`hooks/` closed a 4-component cycle.
    Series: `runs_history/optiscaler_n30_fp/`.
  - **#056 diff mode implemented** (agent): `--diff <sha>|<A>..<B> --repo` catches
    "a commit copied existing code". 5/30 sampled commits introduced partial clones;
    the largest `cc5d695e` (23 hits, max_sim=1.0, copy-paste of shader-pass dispatch bodies
    DI↔FT — verified by hand). Series: `runs_history/optiscaler_partial/`.
  - **Discipline-vs-stars (130 repos, 5 buckets):** the "fewer stars → worse" hypothesis
    was NOT confirmed — median dup is flat 10–15% across all buckets.
    `discipline.tsv`.
  - **Anomalies (verified):** sampling artifact (side branches → fix with first-parent);
    ggml-org-central = meta-aggregator (80% = vendoring); ReP_AL camera_index.h =
    data-blob #056.
  - **Corpus (permanent):** `_aidev_run/` 50→119 ≥100★ with full history;
    `_aidev_lowstar/` low-star by buckets. Scripts: `history_drift.sh`,
    `discipline_vs_stars.sh`, `partial_history_drift.sh`, `clone_*.sh`.
- **2026-05-30 reconnaissance:** downloaded AIDev all_repository.parquet (116211 repos);
  C++=2445, ≥100★=119. Artifacts in `experiments/ai_repo_run/`: corpus_50.tsv
  (50 random C++ ≥100★ <500MB, seed=42), skipped_big.tsv, sample_cpp.py.
- **2026-05-30 pilot (5 repos), real numbers** (binary
  `/tmp/line_dup_build/line_duplication`, `--top 8`, text output; the spike
  does NOT support `--json`). Raw data: `experiments/ai_repo_run/pilot_raw/*.txt`.

  | repo | eligible files | sigLOC | dupLOC | ratio% | blocks | cross-file |
  |------|---:|---:|---:|---:|---:|---:|
  | mqtt_client | 2 | 1395 | 184 | 13.19 | 13 | 1 |
  | rii | 6 | 6798 | 586 | 8.62 | 33 | 0 |
  | guetzli | 47 | 5982 | 289 | 4.83 | 18 | 1 |
  | pict | 42 | 8027 | 485 | 6.04 | 37 | 14 |
  | implot3d | 7 | 7806 | 490 | 6.27 | 32 | 1 |

  Time <0.01 s/repo, RSS ~4 MB. The pipeline works. Real cross-file findings
  (verbatim): mqtt_client `MqttClient.hpp:2 <-> MqttClient.cpp:2` (19 lines,
  header↔impl, low value); guetzli `jpeg_data_encoder.cc:56 <->
  jpeg_data_writer.cc:56` (5); pict `api/resource.h:7 <-> clidll/resource.h:7`
  (8) +13; implot3d `implot3d.cpp:3286 <-> implot3d_demo.cpp:1975` (10).
- **Noted:** (1) `rii` ratio 8.62% is inflated by the vendored `src/extern/sse2neon.h`
  — the default excludes don't catch the `extern` folder; (2) `mqtt_client` 13.19% — almost
  the entire dup is header↔impl of one component, no signal for cross-component
  (whole-tree inflates it).
- **2026-05-30 excludes fix (per the decision "just add patterns").** Verified
  empirically (see `PILOT_NOTES.md`): nested-git=0 in all 5 repos, .gitignore doesn't catch
  vendoring (it's committed) — so patterns only. In
  `experiments/line_duplication/main.cpp` the defaults gained: `extern`,
  `external`, `external_libs`, `vendor`, `vendored`, `deps`, `3rdparty`,
  `3rd_party`. Rebuilt (binary mtime 15:35), re-run (raw data
  `pilot_raw/*_v2.txt`):

  | repo | eligible | sigLOC | ratio% | blocks | cross-file | Δ vs v1 |
  |------|---:|---:|---:|---:|---:|---|
  | mqtt_client | 2 | 1395 | 13.19 | 13 | 1 | unchanged |
  | rii | 5 | 769 | 3.90 | 2 | 0 | was 6798 sigLOC / 8.62% |
  | guetzli | 47 | 5982 | 4.83 | 18 | 1 | unchanged |
  | pict | 42 | 8027 | 6.04 | 37 | 14 | unchanged |
  | implot3d | 7 | 7806 | 6.27 | 32 | 1 | unchanged |

  The fix is valid: only rii changed. Telling: its sigLOC dropped 6798→**769**
  — `extern/sse2neon.h` made up ~90% of the repo's "code".

- **2026-05-30 corpus cloned in full.** All 50 repos in
  `~/oss/_aidev_run/` (2.8 GB, 0 failures, `git clone --depth 1
  --filter=blob:none`). Log: `experiments/ai_repo_run/clone.log`.

- **2026-05-30 infrastructure for repeated runs** in `experiments/ai_repo_run/`:
  `run_dup.sh <tag>` (run over the whole corpus → `runs/<tag>/` raw+summary+context),
  `diff_runs.sh <a> <b>` (comparison of runs), `README.md` (step-by-step instruction).

- **2026-05-30 first full run `v1_excludes` (50 repos).** Summary:
  `runs/v1_excludes/summary.tsv`, raw `runs/v1_excludes/<repo>.txt`. Perf is fine
  (seconds per repo). Outliers visible: CnC_Generals_Zero_Hour 90.56%,
  ReP_AL-3D-Lawn-Mower 84.55%, ncnn 68.40%, alpaka 62.58% — candidates for
  uncleaned vendoring/codegen (check via `<repo>.txt`).

- **🐞 REAL spike BUG (proven on data, a credibility blocker).**
  `isCommentOnly` (main.cpp:275) filters comments **line by line** by the start
  of the line (`//`, `/*`, `*`), but does NOT track state inside a `/* ... */` block.
  License headers `/* ... */`, where lines start with ordinary text
  (`MIT License`, `Copyright...`), pass as "significant code".
  Proof: the `mqtt_client` top block "19 lines MqttClient.hpp:2 <->
  MqttClient.cpp:2" = verbatim the MIT license text (verified with `sed`), lines 1=`/*`,
  25=`*/`. Inflates sigLOC and cross-file. This is the same class of bug fixed in
  the production SF.7 (#038 block-comment stripping) — the spike did not inherit it.

- **2026-05-30 block-comment fix CLOSED + run v2.** Reused the
  proven pair from the production SF.7 (`src/rules/sf7_using_namespace.cpp`):
  `stripLineComment` + `updateBlockCommentState(raw, inBlockComment)` — not a homegrown
  reinvention. Rebuilt (mtime 16:34, exit 0; clangd IDE diagnostics were
  false). Run `runs/v2_blockcomment/`, comparison `diff_runs.sh v1 v2`.
  Effect (examples): mqtt_client cross-file 1→0 (license gone);
  HexRaysCodeXplorer 11.29→6.92%; itksnap 11.38→8.34%; HPCC cross-file 3066→2725.
- **A NEW class of noise surfaced (proven).** FTXUI ratio grew 8.45→10.45% —
  not a bug: the fix now also strips **trailing** comments (`foo(); // A`), whereas
  the old code left them. Because of this, `#include` blocks with different
  `// for ...` comments matched. Proof: a new cross-file
  `input.cpp:9 <-> radiobox.cpp:5` (11 lines) = an identical list of
  `#include "ftxui/..."` with different trailing comments (verified with `sed`).
  Technically a clone, architecturally — shared dependencies, not a "missing reuse edge".
  **Separate task: filter include blocks** (not a blocker, but inflates the signal).

- **2026-05-30 outlier analysis + vendoring fix (v3_vendor_ci).** Checked
  the top blocks of the outliers via `<repo>.txt` — they turned out to be THREE different classes, not one:
  - **alpaka 62%, AMICI 28%** — vendoring `thirdParty/catch2` and the like. NOISE.
  - **ncnn 68%** — `src/layer/{loongarch,mips,arm,riscv}/` SIMD backends.
    A REAL clone of the project's code, NOT noise — don't touch.
  - **CnC_Generals 90%** — `Generals/` vs `GeneralsMD/` (identical files of
    3000+ lines). Fork-in-repo (two versions of the game). Real, not vendoring.
  - **ReP_AL 84%** — ESP32 camera boilerplate (Espressif), copied into 2 places.
    Semi-vendoring.

  Conclusion: **you cannot cut by ratio blindly** — ncnn 68% is real, alpaka 62% is junk.
  The fix — only unambiguous vendoring: the excludes matching was made
  **case-insensitive** (vendor folders in the corpus: `thirdParty/ThirdParty/
  3rdParty/External/Submodules` — all case variants), + added
  `vendors/submodules/subprojects`. Rebuilt (mtime 16:52). Effect (diff v2→v3):
  alpaka 61.9→**20.3%**, AMICI 28.1→**23.9%** (blocks 1296→297), PcapPlusPlus
  10.6→9.4%. ncnn/CnC did not move (correctly — there is no vendoring there).

- **Another class of noise surfaced (proven, NOT fixed):** Effekseer — `ShaderHeader/
  *.h` are GLSL shaders embedded into C++ as a string (`static const char ...[] =
  R"(#version 120 ...)"`). Clones between them are generated shader code, not
  architecture. There is NO universal pattern by name (each project has its own
  names) → detect-by-content is needed, that's a BIG decision, deferred. Not a blocker
  (1 repo). Recorded as a known limitation.

## State of v3 (after 3 fixes: excludes+comment+vendor-CI)

Distribution of ratio over 50 repos: ≥40% — 3 (CnC/ReP_AL/ncnn, all explained),
20-40% — 8, 10-20% — 18, 3-10% — 17, <3% — 4. Summary `runs/v3_vendor_ci/summary.tsv`.
Known residual distortions: shader-headers (Effekseer), fork-in-repo (CnC),
include blocks (FTXUI), header↔impl (cross-file proxy, not cross-component).

- **2026-05-30 fixes v4: skip `#include` + carve out raw-string `R"(...)"`.**
  Per the user's instruction: include lines are dependencies, not code (identical
  include blocks and header↔impl headers = shared deps, not a reuse edge); raw-string
  content is data (shader/SQL/script), not code. Helpers by analogy with
  block-comment (`updateRawStringState`, `isIncludeLine`), order:
  raw-string → comment → include-skip. Rebuilt (mtime 17:14).
  Effect (diff v3→v4): opentelemetry cross-file 608→349; scenario_simulator
  19.5→15.3%; AMICI 24→17.3% (xfile 179→88); FTXUI xfile 61→28; acts 723→569.
  Control: ncnn 68% / CnC 90% untouched (real code). Run `runs/v4_include_rawstr/`.
- **header↔impl analyzed (at the user's request — these are NOT signatures).** Check
  on sources: AMICI `rdata.h↔cpp` = a multiline list of constructor
  parameters; opentelemetry `.h↔.cc` = an `#include` block (gone in v4); acts
  `SurfaceArray` = whole function bodies. The include part is cured by v4; parameters/bodies
  remain — a cross-**component** mapping is needed (#053 P0-B), not a quick fix.
- **Effekseer-DX remains (another class, NOT fixed).** GL shaders = raw-string
  (v4 catches), but DX11/12/Metal/Vulkan = `fxc` output — bytecode as arrays of numbers
  (`0, 0, 124, 6, ...`) inside `#if 0` disassembly. There's no R"( there. Catching
  "arrays of numbers" with a heuristic is risky (it would snag constant tables); the clean move
  — skip `#if 0` dead code. Deferred (1 repo, not a blocker).

## Noise cleanup result (5 fixes, all verified on sources)

| Fix | Noise class | Proof |
|------|-----------|----------------|
| excludes vendor folders | sse2neon, catch2 | rii sigLOC 6798→769 |
| block-comment stripping (from SF.7) | licenses `/* MIT */` | mqtt cross-file 1→0 |
| case-insensitive matching | thirdParty/ThirdParty/3rdParty | alpaka 62→20% |
| skip `#include` | include blocks, header headers | otel xfile 608→349 |
| raw-string `R"(...)"` | embedded GL shaders | Effekseer 33→29% |

Deliberately NOT touched (real signal): ncnn 68% (SIMD backends), CnC 90%
(fork Generals/GeneralsMD). Residual known distortions: Effekseer-DX
(fxc bytecode), header↔impl bodies/parameters (cross-component #053 P0-B needed).

- **2026-05-30 fix v5: skip `#if 0` / `#if false` dead blocks** (accounting for
  `#if`/`#endif` nesting). Per the user's instruction. Helper
  `isDeadPreprocessorLine`, the check on the raw line before raw-string/comment.
  Rebuilt (mtime 17:28). Effect (diff v4→v5): Effekseer 28.85→27.92%
  (cross-file 1660→1486, fxc disassembly listings under `#if 0` gone);
  onnxruntime-genai 10.55→9.50%; rocky/HPCC minor. ncnn/CnC stable.
  Run `runs/v5_if0/`.
- **Numeric data arrays handed off to #056.** The remainder of Effekseer (196-line
  cross-file blocks) is `const BYTE g_main[] = { 69, 70, 88, ... }` (fxc
  bytecode, NOT under `#if 0`). Per the user's decision "no need to compare numbers,
  but it's #056". A note was added in `backlog/wip/056` §"Numeric data blocks".
  In #053/#054 line-based this is not curable (numbers are indistinguishable from code without tokenization).

## Cleanup result (v1→v5, current baseline = v5_if0)

5 spike fixes, all verified on sources; the control (ncnn 68% SIMD backends,
CnC 90% fork-in-repo) deliberately untouched — there's a real code clone there.
Residual known distortions, placed out of scope of #054: numeric data blocks
(#056), header↔impl bodies/parameters (cross-component, #053 P0-B).

## In progress (2026-05-31)

- **Drift run ×3** on 160 low-star repos (`runs_history/lowstar/`) —
  in progress, expanding the corpus statistics from 85 to ~245 projects.
- **AIDev PR tables downloaded** (`/tmp/aidev_pr/`: pull_request 17MB,
  human_pull_request, pr_task_type) — ground-truth labels of AI-PR/human-PR for
  the next stage (within-repo test of AI drift).

## ⚠️ Honest framing (recorded 2026-05-31)

**Proven:** the tool catches drift; the drift is real (39/85 grow in copy-paste,
15/85 gave birth to cycles) and is localized to a commit. **NOT proven:** that the cause is
AI. The whole-tree run measures the drift of the project as a whole, without separating AI code. Attribution
by git is impossible (0/11 culprit commits with AI markers — that's a method failure,
not a conclusion about AI). To prove AI drift — a within-repo AI-PR vs human-PR
test on the ground-truth labels of AIDev is needed (the next stage, the labels are already downloaded).

## Strategy change and why (2026-05-31) — for history and the article

Three user remarks in a row turned the approach around. I record both the reasons and
the facts they exposed — this is the research narrative itself.

### Remark 1: "we're blurring the goal — AI drift is not proven"

In §0 of the report I changed the framing to "which commit broke it, regardless of who the author is" —
this proved the tool works, but **removed the project's original hypothesis**
(constraint decay FROM AI). I split two questions: (1) the tool catches drift —
proven; (2) is AI to blame — NOT verified. The result "0/11 culprit commits
with AI markers" is a **failure of git attribution**, not an exoneration of AI. Conclusion: a
within-repo test AI-PR vs human-PR on ground-truth labels is needed, not on git trailers.

### Remark 2: "projects with high quality requirements were chosen — it's not AI there"

Verified with data (AI markers × stars over local repos): bitcoin (84k★),
keepassxc (23k★), z3 (11k★), acts (CERN) — flagships with strict review, where
the AI contribution is **cleaned out** before merge → AI drift can't be seen there. Objective
selection criteria instead of "fame": **(a) many AI PRs** (AI actually worked),
**(b) not a flagship** (stars as a rough proxy for review strictness). In the current corpus
almost nobody satisfied this — most have an AI share of 0-2%, and where it's high
(gizmosql 23%) — single repos.

### Remark 3: the change of selection strategy (the main one)

1. **Window ≥ May 2025** — we treat it as the start of mass agentic development; take
   only PRs/commits of this period (earlier — "pre-agentic" code, noise).
2. **Increase coverage** — look at ALL C++ repos of AIDev, not only the downloaded 50/160.
3. **First a quick triage by metadata** (WITHOUT downloading) → assess "how
   agentic the development is there" → then decide whether to download. The goal of the first run is to
   find which repos are interesting for the research at all.

### Triage done read-only on the downloaded AIDev PR tables (2026-05-31)

AI labels are taken from `pull_request.parquet` (33596 agentic PRs; column `agent`:
OpenAI_Codex 21799, Copilot 4970, Devin 4827, Cursor 1541, Claude_Code 459).
Filter `merged_at ≥ 2025-05-01` + repo language = C++ (from `all_repository`).

**Facts found (all — real pandas output, not estimates):**
- Agentic PRs since May 2025: **22,786** of 33,596; but in **C++** repos — only
  **208** (66 repos). C++ is a thin slice of AIDev's agentic activity.
- Repos with **≥8 AI PRs** (the threshold for statistics): only **6**:
  | repo | AI-PR | ★ |
  |---|---|---|
  | forntoh/LcdMenu | 18 | 216 |
  | shader-slang/slang | 17 | 4349 |
  | chrxh/alien | 11 | 5211 |
  | miguel5612/MQSensorsLib | 9 | 196 |
  | zeroc-ice/ice-demos | 9 | 333 |
  | log4cplus/log4cplus | 8 | 1701 |
- **`human_pull_request.parquet` — NOT a full human baseline** (6618 rows /
  818 repos, a sparse sample; all 6 candidates have 0 human PRs in the window). Conclusion:
  for the within-repo contrast, take the human side from the **git history**
  (non-AI commits of the same repo), the AI side — from the PR table (ground truth).
- **`pr_task_type` gives the task type** (feat/fix/docs/refactor/test/chore) for both AI
  and human PRs → the confounder "AI is put on boilerplate" is closed by data
  (compare within a type).

**Main triage conclusion (for the article):** the AIDev dataset is biased toward non-C++ languages;
there are few dense "agentic" C++ repos. This is a result in itself — it couldn't have been
seen without doing a quick metadata triage instead of blind downloading. The threshold
≥8 AI PRs + the May-2025 window gives a manageable shortlist of 6 repos.

### Breakthrough: we went past the AIDev ceiling — a working search method (2026-05-31, evening)

The user's remark "12 is too few, we need at least 100; look up how people actually search
GitHub, maybe you don't know the parameters" lifted the ceiling. AIDev is just a slice; a live search
via `gh search` gives an order of magnitude more. The method was worked out and validated, see
[METHODOLOGY_FINDINGS.md](../../experiments/ai_repo_run/METHODOLOGY_FINDINGS.md).

- **What does NOT work:** `gh search commits` is useless for breadth — the index
  clusters (62 of 100 commits from one repo), there's no language filter,
  secondary rate-limit. It's only good for highlighting already-dense vibe repos.
- **What works (scalable):** `gh search prs` supports `--language=cpp`
  AND `--author=app/<bot>` simultaneously. Trap: the repo field in PR-search is
  `repository.nameWithOwner` (not `fullName`, as in commit-search).
- **Coverage by cloud agents** (merged, created>2025-05-01): Copilot **230**,
  Devin **143**, Jules 20, Cursor 13, Codegen 6 → **410 unique C++ repos**.
  The "≥100 candidates" threshold is taken with margin.
- **Limitation:** Codex (the largest in AIDev, 21799 PRs) bot-search does NOT catch —
  Codex-cloud opens PRs under a human account, not from a bot.
- **Concentration** is computed over the git history of a blobless clone: numerator — AI commits
  via native `--author/--committer/--grep` (union by SHA, method D),
  denominator — all commits after May 2025; threshold **> 50%**. On the first 39 of 410
  the threshold was passed by 11 repos (~28%) → expected ~100+ of 410.

## Next steps

1. Wait for the ×3 run to finish → recompute the corpus summary on ~245 projects.
2. Download the shortlist (6 repos with ≥8 AI PRs), match AI PRs by PR number with
   merge commits; the human baseline — from the git history (≥May-2025).
3. A design document for the within-repo test (method + confounders: PR size, task type
   from `pr_task_type`, selection bias, time window) → pilot, both metrics.
4. Expand the triage: if desired, lower the AI-PR threshold or include "May+"
   commits by git trailers on top of the PR table (to gather more candidates).

## Key decisions

| Decision | Reason |
|---------|---------|
| Corpus from AIDev, not grep | the field already assembled the lists; a homemade grep is biased toward "tidy" (signing) repos; AIDev gives exact language + stars |
| Measure BOTH metrics (graph + clones) | the graph was silent on the first corpus, but the corpus was narrow; on an unbiased one we need to re-check the graph AND compare with duplication on one set |
| First whole-tree, within-repo later | cheap and the tool exists; the AI/human slice is more expensive (commit-level) — a separate stage if a signal is present |
| Only numbers, conclusions separately | keep the data neutral; interpretation — a separate task |
| The corpus is stored locally, not recreated | we will keep refining the tool (the excludes/ratio spike) and run it over the corpus MANY times; re-downloading 50 repos every run is wasteful. Canonical path: `~/oss/_aidev_run/` (permanent storage). |
| Runs are versioned (`runs/<tag>/`) | a tool change → a new tag → `diff_runs` shows the shift; don't overwrite past numbers |

## Changed files

| File | Change |
|------|-----------|
| `experiments/line_duplication/main.cpp` | excludes: added extern/vendor/deps/3rdparty/...; (later) block-comment stripping fix |
| `experiments/ai_repo_run/corpus_50.tsv` | corpus list (AIDev C++, 50 repos, seed=42) |
| `experiments/ai_repo_run/sample_cpp.py`, `skipped_big.tsv`, `clone.log` | sampler, filtering, clone log |
| `experiments/ai_repo_run/run_dup.sh`, `diff_runs.sh`, `README.md` | runner, run comparison, instruction |
| `experiments/ai_repo_run/runs/v1_excludes/` | first full run (50 repos): summary + raw |
| `experiments/ai_repo_run/PILOT_NOTES.md` | working notes/context restoration |
| `docs/research/ai_code_detection_landscape.md` | field reference (methods/datasets/numbers) |
| `experiments/ai_repo_run/DRIFT_RUN_REPORT.md` | **full report** (§7 corpus, §8 AI/human, §9 dedup, §10 links) |
| `experiments/ai_repo_run/SUMMARY.md` | short report (1 page) with the honest framing of what's proven/not |
| `experiments/ai_repo_run/DRIFT_COMMITS_AUTHORSHIP.md` | 11 culprit commits: AI/human + texts (0 AI, 9 human, 2 unclear) |
| `experiments/ai_repo_run/DEDUP_TECHNIQUES.md` | deduplication techniques (top: extraction into a shared header) |
| `experiments/ai_repo_run/drift_all.sh` | corpus drift run (parametrizable ROOT/GLOB/TAGDIR) |
| `experiments/ai_repo_run/history_drift.sh` | drift of one project over history (first-parent, both metrics) |
| `experiments/ai_repo_run/checkout_watchdog.sh` | watchdog against hung git checkout |
| `experiments/ai_repo_run/{discipline_vs_stars,clone_lowstar,unshallow_*}.sh` | discipline-vs-stars + corpus download/history |
| `experiments/ai_repo_run/runs_history/{all,lowstar}/*.tsv`, `drift_summary_v2.tsv` | metric series by project + corpus summary |
| `experiments/ai_repo_run/METHODOLOGY_FINDINGS.md` | **working method for finding AI repos** (PR-search by bot author + concentration method D) |
| `experiments/ai_repo_run/graph_probe.py` | cheap graph detectors over the `--save-graph-baseline` export (8 of them) |
| `experiments/ai_repo_run/graph_probe_all.sh` | run detectors over the corpus → `runs_history/probe/` |
| `experiments/ai_repo_run/GRAPH_PROBE_FINDINGS.md` | graph-detector findings (mc2 56-tangle, acts hpp↔ipp, resolver bug) |
| `experiments/ai_repo_run/DENSITY_SIGNALS.md` | signs of dense agentic development + the closed >50% threshold |
| `/tmp/airepo/` (not in git) | 410 candidates, `conc_one.sh`/`run_conc.sh`, `conc_results.tsv` |

## Pointers

- Field reference: [docs/research/ai_code_detection_landscape.md](../../docs/research/ai_code_detection_landscape.md)
- Dataset: [hao-li/AIDev](https://huggingface.co/datasets/hao-li/AIDev) (HF; `all_repository.parquet` downloaded to `/tmp/aidev_repo.parquet` at the time of reconnaissance; `pyarrow` is needed to read it)
- Clone detector: `experiments/line_duplication/main.cpp` (standalone, in git history — see #053) / #053 (port into `src/`)
- Graph detector: DRIFT.1/DRIFT.2 (#009) + absolute graph metrics in `src/`
- Past corpus and methodology: [ai_drift_runlog.md](../../docs/research/ai_drift_runlog.md), #048
