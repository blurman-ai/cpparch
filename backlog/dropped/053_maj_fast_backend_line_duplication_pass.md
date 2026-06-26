# [RULES][SCAN] Fast-backend line-based duplication pass (port of a proven prototype)

**Created:** 2026-05-29
**Started:** 2026-05-30
**Status:** wip
**Module:** RULES / SCAN
**Priority:** major
**Difficulty:** L (core: port + plumbing + fixtures + perf note). Normalization mode, file-twin rate, and P1/P2 — as separate iterations.
**Target release:** v0.2
**Blocks:** #054 (ai_repo_duplication_run — the cross-component ratio from P0-B is desirable but not a blocker: a cross-file proxy on the first run)
**Blocked by:** —
**Related:** #006 (spec_refactor — fast backend as part of the two-backend scheme), #033 (ai_drift_dataset — duplication as a strong AI-drift signal), #052 (cross_tu_duplication_detector — AST layer, precise and expensive; **complementary** to this task), #054 (ai_repo_duplication_run — consumer of the signal), #055 (dogfood_json_escape_dedup — our own helper duplicate `jsonEscape`, found by this pass, see §"Reference findings"), #056 (fast_backend_partial_duplication_pass — **fast Type-3 / diverged-copy layer on tokens**; takes "full Type-3" and diverged copies that this line pass does NOT do — boundary: #053 = Type-1 verbatim, #056 = Type-3 diverged)

## Goal

Add to the fast backend a cheap snapshot pass for Type-1 line duplication: without
libclang, without `compile_commands.json`, reporting cross-component
duplicated blocks as a **missing reuse edge**. In text mode the headline signal is
**concrete findings (long blocks), not an overall `%`**; and they should be ranked
**not by raw length** (length pushes intentional twins to the top — see the lessons
below) but by the heuristic "large partial transfer". Ratio remains a secondary
diagnostic metric. **Off by default.**

**The detector is already proven by a spike + sweep + manual intent check.** The remaining
work is operational: modes, excludes, ranking, the metric, the move into the product.

## Why / place in the layered scheme

Duplicate detection in archcheck is built in three layers by cost and precision:

1. **This task:** line-based Type-1, cheap, instant, on any build system.
   Fast backend. Lowers the barrier to entry: a project without `compile_commands.json`
   gets at least a verbatim check.
2. **#052:** AST `CloneDetector` Type-2/3, precise but expensive (libclang backend).
3. **Future:** IR-canon for rewrites "in other words". Not now.

The frame is the same as the AST layer's: **a cross-component duplicated block = a missing
reuse edge** in the sense of Lakos physical design (replication vs hierarchical reuse).

Attribution:

- **Lakos** — duplication between components means the absence of a reuse edge and
  degradation of physical design (*why* it is bad).
- **GitClear** — copy-paste as the #1 measured signal of AI drift and duplication
  ratio as the empirical motivation. **NB:** the AI narrative is honest ONLY in
  commit mode (per-commit, like GitClear), not on a snapshot — see "Two modes".

## Main lessons of the spike (do NOT rediscover)

A sweep over ~25 repos + a manual eyeball check of EACH top finding (Codex) yielded
five conclusions that change the product scope:

1. **Detection is reliable, but "duplicate" ≠ "problem".** False positives of *detection* —
   zero. But of the top findings the larger part are *intentional twins* (Catch2 two
   reporters, abseil map/set, folly `SpookyHashV1`/`V2`, IrredenEngine demo-twins,
   LibreSprite command-twins, gm `encoder`/`encoder2`). The problem is in the intent, not the
   detection.
2. **Intent cannot be classified by a machine.** There is no reliable automatic split of
   "hole vs intentional twin" — **do not pretend there is.** Therefore the gate
   does not rely on intent classification (see "Two modes": gate = baseline).
3. **Ranking by length is inverted.** The longest near-100% blocks in a
   mature codebase are more often intentional twins (they are kept synchronized on purpose).
   A random hole diverges and shortens over time. **Raw length pushes the WRONG things
   to the top.** The main signal is different (§"Ranking").
4. **Ratio is exclude-dependent and misleading.** Catch2 raw 75%; `spdlog/include` 4.5%
   (with vendored `fmt`) vs 8.26% (without). Never put ratio in the headline.
5. **Excludes are the operational center of gravity.** The pretty sweep numbers were obtained by
   manual curation (10 repos got personal excludes added). Nobody will do this
   for the user — **defaults must not embarrass themselves on the first run.**

## Two modes — two different tasks (the central decision)

### snapshot = explorer (NOT a gate)

- Shows hot spots, ranks, **a human decides**. Does not break CI.
  **No AI claim.**
- Intentional twins are honestly present in the output — that's ok, it's exploration mode.

### commit / baseline = GATE (the main product mode)

- `--git-commit <sha>`, `--git-diff A..B`. Builds baseline + current snapshots,
  reports only blocks that touch the changed set AND are new relative to
  baseline.
- **Baseline freezes historical duplicates (including twins) → CI breaks only on a
  NEW duplicate.** The problem of intentional twins (lessons §1-2) fully
  dissolves here — there is no need to guess intent.
- The only honest place for the GitClear/AI narrative.
- The 2× cost (two snapshots) — document it; acceptable.

> Implementation of commit mode — P1-D (after the first product merge). But the design decision
> "snapshot=advisory / commit=gate" is locked NOW as central, because
> what we do NOT rely on in snapshot (ranking/intent) depends on it.

## Current status: spike closed, gate before the move

The spike was run over ~25 OSS and local repos (2026-05-29). **Verdict: the layer is
viable, we go into the product — after closing P0.** Artifacts — the directory
`experiments/line_duplication/` has been removed from the tree, remaining only in git history
(commit `35085ca`):

- `experiments/line_duplication/main.cpp` — a working standalone C++20 port (~1160 lines), not in the main build.
- `experiments/line_duplication/PERF.md` — time + peak RSS on reference trees.
- `experiments/line_duplication/LOCAL_SWEEP_REPORT.md` — historical raw sweep over ~20 repos.
- `experiments/line_duplication/PROJECT_EXAMPLES_REPORT.md` — current app-only run + `examples/`.

---

## ✅ Done (spike — do NOT rediscover)

- [x] **Algorithm ported to standalone C++20** (`experiments/line_duplication/main.cpp`):
      stream of significant lines, normalization, sliding window, hash, grouping by
      hash, **merge into maximal blocks**, ignore mask, default excludes,
      duplication ratio. The prototype's logic reproduced 1:1.
- [x] **Perf measured** on ~25 OSS/local repos (`PERF.md`, `LOCAL_SWEEP_REPORT.md`,
      `PROJECT_EXAMPLES_REPORT.md`): heavy runs — seconds, not minutes (grpc
      773k LOC → 2.4 s; gm 2.77 s / 314 MB; cpparch 1.2M → 1.9 s / 231 MB). Acceptable for CI.
- [x] **References confirmed by signal:** `fmt/include` 2.07% / 0 cross-file —
      a hit; `spdlog/include` — characteristic pairs `daily`↔`hourly`,
      `syslog`↔`systemd` surface at the top of the cross-file list.
- [x] **Findings validated manually** on live trees: `gm_github` — a 78-line
      `PacketLogger` across two modules and a 90-line identical `p_light.cpp` between
      `BAT2`/`IMR` (real missing-reuse-edges).
- [x] **Manual eyeball intent check (Codex)** over 21 unique cases: 0 false
      *detections*; the larger part of the top are intentional twins. Hence lessons §1-3 above.
- [x] **Commit-aware mode implemented and verified** in the spike (`--git-commit`,
      `--git-diff A..B` / `--git-diff A`, introduced-detection via blob comparison
      without a full checkout). A real commit `Sync from gm` → 17 introduced,
      the top one confirmed.
- [x] **Exclude masks derived empirically** (`FILTER_CLASSIFICATION_REPORT.md`,
      ~25 repos): "hard" (always: `single_include/extras/amalgamate/dist/
      *_autogen/qrc_*/moc_*/build-*/CMakeFiles/third_party/3rdparty/bundled/vendor`)
      vs "soft" app-only (`test/examples/upgrade/legacy/hardware_regs/CMSIS/deps`).
      Before/after: Catch2 75.68% → 6.27%, pico-sdk 42.05% → 21.85%. **This is the research input
      for the default exclude list of P0-A** (the auto-detect mechanism is not yet in the code).
- [x] **Test files excluded by default** in the spike (`main.cpp`, uncommitted):
      directories `test/tests/testing/fixture/fixtures/selftest`, stems `test_*`/`*_test`/
      `*_tests`/`*_unittest`; opt-out `--include-tests`. *Decision locked:
      exclude-by-default, not a severity tier.*
- [x] **3-class taxonomy of findings** (`DUPLICATE_PATHS_REPORT.md`): generated/
      package/vendor | test twins / expected variants | real manual copy-paste.
      This is the designed input for P2-F (confidence tiers) and the file-twin rate.

---

## ⬜ Remaining

### Gate before `src/scan` — P0 (blocks the move)

Close **P0-A, P0-B, P0-C** before moving the code into the product. They can be iterated in
the spike (cheap), then moved.

- [ ] **P0-A. Default excludes that do not embarrass themselves on the first run.**
      🟡 *research closed; a minimal mechanism in the code remains.*
  - **Hard masks (always):** `single_include`, `extras`, `amalgamate*`,
    `dist`, `generated`, `*_generated*`, `*_autogen`, `qrc_*`, `moc_*`,
    `build-*`, `CMakeFiles`, `CompilerId*`, `third_party`, `3rdparty`,
    `thirdparty`, `bundled`, `vendor*`, `_deps`. (`bundled` is mandatory: `spdlog`
    pulls `fmt` into `fmt/bundled/`.)
  - **4 auto-rules (cover ~80% of the manual curation) — implemented in the spike:**
    1. [x] **Build trees:** skip a directory containing `CMakeCache.txt`
       (and the entire subtree below). `isBuildTreeDir`.
    2. [x] **Nested repositories:** skip a child directory with its own
       `.git` (or `.hg`) — a single rule covers sandbox/snapshot/submodule,
       vendored repos. `isNestedRepoDir`. The scan root is not excluded.
    3. [x] **Tests:** path segment ∈ {`test`,`tests`,`testing`,`fixture`,`fixtures`,
       `selftest`} OR stem ∈ {`test_*`,`*_test`,`*_tests`,`*_unittest`}.
       opt-out `--include-tests`.
    4. [x] **Demos/examples:** `examples`/`example`/`demos`/`demo` — into the same
       non-product bucket as tests (the same opt-out `--include-tests`).
    - *Implementation lesson:* range-based `for (e : it)` iterates a copy of
      `recursive_directory_iterator` → `disable_recursion_pending()` was a no-op;
      switched to an explicit loop, otherwise dir-level pruning does not work.
  - **Do not guess vendor-in-src.** `minilzo/qhull/glad/CMSIS/libigl` and the like —
    via `exclude` from `.archcheck.yml`; an optional shipped blocklist of common
    names — as a separate step, if needed.
  - [x] **Audit:** in the text report a line `excluded subtrees: N (files dropped by
    mask: M)` + a deterministically sorted list of pruned roots with the
    reason (`[build-tree]`/`[nested-repo]`/`[mask]`/`[test]`/`[demo]`), cap 12 +
    `(+N more)`. `printExclusionAudit`.
    - *Decision:* **we do NOT count the sig-LOC cut off** — that would require traversing
      exactly the subtrees we skip (it would kill the gain). The list of roots
      + reason + the count of file-level masks answers "what was cut" cheaply.
      Infra directories (`.git`/`.cache`/`.idea`/build names without `CMakeCache.txt`) do not get
      into the list as noise.
    - json/sarif audit — at the stage of porting into the product (the reporters are there).
  - *Done when:* the first run on a typical OSS tree does not drown in build/autogen/
    tests/demos, and vendor-in-src can be muted purely by config. **Verified:**
    on cpparch the auto-rules cut out `sandbox/drift_repos/*` (nested-repo),
    `experiments/.../examples` (demo), build trees; 62 files, 0.02 s.
- [ ] **P0-B. Change the headline metric and report ranking.**
      The overall `%` cannot be the main signal (exclude-dependent, scary on
      packaging repos). And findings cannot be ranked by raw length (length
      pushes intentional twins up — lesson §3).
  - [ ] Headline text output = **concrete findings (blocks)**, not a percentage.
    For each block: `length`, `file:line <-> file:line`,
    `cross-file/cross-component`, where available — components.
  - [ ] **Rank by the heuristic "large partial transfer"**, NOT by raw
    length (details — §"Ranking"): a long block that is SMALLER than a whole
    file, between files with DIFFERENT names — top of the list.
  - [ ] Compute and output **two** versions of the ratio: `total_duplication_ratio` and
    `cross_component_ratio` (until integration with the component graph — an explicit
    `cross_file_proxy_ratio`). Gate/headline — on cross-component, not on total.
  - [ ] `--cross-only` affects **both the ratio computation and the output**, not just the output.
  - *Done when:* a glance at the report answers "where is the large partial transfer?",
    not "why is there a scary 75% here?".
- [ ] **P0-C. Regression anchors = known blocks, not %.** 🟡 *the data for
      the asserts is collected by the sweep; the tests themselves are not written in the product.*
  - [ ] Assert on the **presence** of characteristic blocks (`daily`↔`hourly`,
    `syslog`↔`systemd`) and the length/order of top-blocks — NOT on the exact ratio.
  - [ ] Explicitly document that the ratio is exclude-dependent and not a stable contract.
  - *Done when:* no product test fails due to a percentage drift on a
    new version of an upstream library.

### Integration into the product (after P0)

- [ ] **Port into `src/scan/line_dup_scanner.{h,cpp}`** as a neighbor of `include_scanner`:
      the same preprocessor-only path, **without libclang, without `compile_commands.json`**.
      Structures: `vector<SigLine{string normalized; unsigned lineno;}>` per file;
      `unordered_map<uint64_t, vector<pair<FileId, uint32_t>>>`; merge into
      maximal blocks by `(file_a, file_b, ia - ib, overlap)`. The window hash —
      a fast non-crypto one (`xxhash`/`wyhash` or analog; in the spike blake2b was taken
      for clarity, not speed).
- [ ] **One `IRule`** under `src/rules/duplication/`, static registration, without
      a universal rule engine. If #052 has already extracted a shared helper for component
      mapping / duplication reporting — reuse it; if not (for now
      #052 is in `future/`, not started) — do not drag in a refactor for the sake of symmetry.
- [ ] **Mapping blocks → components** via `graph/component_graph`. By
      default report only **cross-component** blocks. Wording:
      `"missing reuse edge: A and B share a <N>-line block"`. Intra-file/
      intra-component ones — suppress or hide behind verbosity.
- [ ] **Dedup cases by file pair, not by scope.** One duplicate that lands in two
      scopes (e.g. `spdlog` and `spdlog/include`) — is ONE case, not two (in the sweep
      22 readings → 21 unique). Count unique file pairs.
- [ ] **Config and CLI:**
  - Enabling: `--duplication=line` (umbrella, room for `--duplication=ast` for
    #052) **and/or** `defaults.line_duplication`. **Off by default.**
  - `thresholds`: `min_lines` (5), `min_window_chars` (~60), `min_complexity`,
    `exclude` globs.
  - Default built-in excludes from P0-A (hard masks + 4 auto-rules).
  - `exclude` from `.archcheck.yml` — the mandatory path for vendor-in-src.
  - Respect `// archcheck: allow`.
  - `--changed-files <list>` — a filter on the **output**, the corpus is always = the whole tree.
  - `--baseline <file>` — freezing the snapshot (snapshot mode).
  - Headline = findings; ratio (if computed) — a secondary diagnostic field.
- [ ] **Output** in text/json/sarif: two versions of the ratio + per block
      `file:line <-> file:line` + length in significant lines. In text mode
      first the top findings (by the ranking heuristic), then the aggregate fields.
- [ ] **`PERF-fast.md`:** transfer the numbers from the spike + one large run
      (`opencv`/`llvm`). Do not make a hard CI gate of it — a sanity check.

### Ranking in the explorer (hints, not a classifier)

> The goal is to sort the review list, not to deliver a verdict. **All signals are knowingly
> imperfect — they are weights, not rules. The gate does NOT rely on ranking** (it
> relies on the baseline: new vs old).

Sweep empirics (21 unique cases): **0** strict whole-file copies, **2**
almost-file ones (both intentional: OreStudio core↔server fork, pico-sdk
board variants), **20** — a transfer of PART of the code. In the wild people copy not a file but a chunk.
Size classes:

- **large partial transfer** (100–800+ lines, but < a whole file): AetherSDR,
  BambuStudio, Kartend, gm, grpc, samastra_itain — **top of the list**;
- **medium fragment**: GWToolboxpp, IrredenEngine, LibreSprite, folly, spectre;
- **small helper / single function** (like `jsonEscape`, 21 lines): Catch2,
  abseil, cpparch, moqx, nlohmann, spdlog, vmecpp — **bottom** (real but
  local, cheaply fixed).

**The main ranking signal is NOT raw length** (it pushes intentional twins,
mirroring a whole file). Rather: **a long block that is SMALLER than a whole file, between
files with DIFFERENT names** — cuts exactly into "large partial transfer"
(the dominant and most dangerous class), without requiring guessing intent. Additional signals:
cross-module distance; a "duplicated data tables" mark.

- [ ] Implement top-list ranking by this heuristic (a weight, not a filter).

### File-twin rate (reference, NOT a gate) — was P2-G

> **Motivation — a diverged copy is more dangerous than verbatim.** A full duplicate is fixed
> trivially (extract into a function). Diverged copies are a mine: it's unclear whether they diverged
> intentionally or by accident (a bug was fixed in one, forgotten in the other). The irony:
> the line pass catches exactly the least dangerous class (verbatim), and the diverged ones
> it misses — a single changed line breaks the match. This is the limit of the approach.

The danger hierarchy is **inverse** to our capabilities:

| Class | Danger | Who catches |
|---|---|---|
| Verbatim copy | low (easy to fix) | line pass ✓ |
| Renamed identifiers | medium | Type-2 / `--normalize-identifiers` |
| **Diverged (gapped)** | **high** | Type-3 — **#056** (fast token-overlap + LCS); outside same-name files — only there |
| Semantic rewrite | high, rare | Type-4 — nobody |

**A cheap proxy to gapped via the file name.** If matching pairs with the SAME
basename in different directories (or basename + numeric suffix: `encoder`/`encoder2`),
the match threshold can be lowered (~40%) — the name already carries confidence.

- [ ] **Rate rule:** a pair of files with the same basename in different directories;
  sharing ≥1 significant block; lowered match threshold (~40%); sort by the share of
  shared significant lines; label "file twin — likely fork". **A separate
  reference rate, NOT a gate** (intentional device variants are also same-named).
- [ ] A format prototype already exists: `DUPLICATE_PATHS_REPORT.md` — a flat index
  "file A → file B + lines + class". Move it into a product reporter.

**A priority caveat (from the sweep):** there are almost no same-named twin files in the real
sample (2 of 21, both intentional). The dominant class is a block transfer
between files with DIFFERENT names (§"Ranking"), which the file-twin rate does NOT
catch. Therefore this is a **cheap bonus, not the main feature** — a hook cannot be built on it.

### Optional mode: identifier normalization (Type-2 lite)

> Catches *renamed* copies (an agent copied and renamed the variables) on
> the same text machine, without AST. Raises recall at the cost of precision → **off by
> default**, with raised thresholds. Details — §"Normalization mode" below.
>
> **Boundary with #056 (to avoid duplication):** token normalization for the sake of
> *diverged* copies — that is #056 (token overlap + token-LCS). Here normalization
> stays **line-based** (normalized the line → match the same lines in the line pass):
> a cheap Type-2 within this pass, not a replacement for #056. If #056 goes into the product
> earlier — this mode may be left unimplemented and delegated to it.

- [ ] Type-2 lite behind the flag `--normalize-identifiers` / `strictness: aggressive`
      (default — `verbatim`). Normalize **only identifiers**; do NOT touch
      keywords, fundamental types, operators, literals. Raised thresholds
      + cross-component-only. Tests H (E/F/G).

### P1 — before the public release, does not block the first move

- [ ] **P1-D. Commit-aware + baseline = the normal CI mode.** Port from the
      spike (`--git-commit`, `--git-diff`, introduced-detection) and connect
      with the existing baseline mechanism. Historical intentional twins and old
      vendor-like duplicates are **frozen**, only **new
      introduced cross-component blocks** should break CI. This is the gate mode (§"Two modes").
  - Document the 2× cost (~1 GB / 7 s on heavy repos).
  - The known edge case (a false negative if the block already existed as a different
    pair) — document as a v1 limitation.
- [ ] **P1-E. Perf guard against pathologies.** The cost is not linear in LOC (the driver —
      duplicate density and file shape). A cap on occurrences-per-hash (windows with >K
      occurrences — limit pairing, otherwise a quadratic blow-up on pairs).
      `--timeout` / `--max-files` as an insurance for CI.

### P2 — precision improvements, as separate iterations

- [ ] **P2-F. Confidence/severity tiers.** 🟡 *the taxonomy is designed
      (`DUPLICATE_PATHS_REPORT`: generated/package · test-twin · real manual);
      in the code only test-exclusion is implemented.* Lower the severity for
      structurally symmetric pairs (variant/implementation twins like
      `SpookyHashV1`↔`V2`, `F14Map`↔`F14Set`); at minimum — a "low value" mark
      instead of a flat list. **But: intent cannot be classified by a machine (lesson §2)** —
      this is a severity hint, not a verdict.

---

## Algorithm (locked by the spike — port 1:1)

- **Stream of significant lines** per file:
  - skip empty lines;
  - skip pure-comment lines;
  - exactly **skip**, not zero out — so that different empty lines and
    comments between copies do not break the match.
- **Normalization:** `strip`; collapse whitespace (`\s+` → ` `).
- **Sliding window** of `N` significant lines (`N=5` by default — the headline threshold
  of GitClear), hash each window with a fast non-crypto hash.
- Group windows by hash; `>= 2` occurrences = seed duplicated window.
- **Merge into maximal blocks — MANDATORY:** the collapse criterion is one
  file pair, one diagonal (`ia - ib`), overlap. Without it a single region
  inflates into a bunch of shifted windows (8 "blocks" = one region — verified).
- **Duplication ratio** = significant lines covered by at least one duplicated
  window / all significant lines. A useful **secondary** metric; raw block count
  is fragile, the headline for the user is about concrete findings.
- **Ignore mask:** windows entirely of trivial scaffolding lexemes (`{}`, `;`,
  `else`, `break;`, `return;`, `public:`, etc.) OR with a total text length
  below ~`60` characters — skip.

### Confirmed reference numbers (for a sanity check of the port, NOT pixel-perfect)

- `fmt/include` → ratio ~`2.07%`, `0` cross-file blocks (a clean library).
- `fmt` whole tree → ~`2.24%`, `8` cross-file (a 41-line duplicate between fuzz harnesses).
- `spdlog/include` → characteristic pairs at the top: `daily_file_sink.h` ↔
  `hourly_file_sink.h` (15/14/12 lines), `systemd_sink.h` ↔ `syslog_sink.h`.
  - **NB on ratio (P0-C):** the number is **exclude-dependent**. With vendored
    `bundled/fmt` → ~`4.50%`; with `bundled` excluded (correctly) → ~`8.26%`.
    Therefore assert on the **presence of characteristic blocks** and the **order** of the cross-file
    count, NOT on the exact %.

## Metric

- Compute and output **two**: `total_duplication_ratio` and `cross_component_ratio`.
- **The gate and any headline — on `cross_component_ratio`**, not on total.
  `--cross-only` affects the computation, not just the output.
- cross-**file** — is a proxy; true cross-**component** requires a module map from
  `component_graph` (at integration). Until then mark it as
  `cross_file_proxy_ratio`.
- **In the headline/README — findings (large partial transfer), not a percentage.**
- **Dedup cases by file pair, not by scope** (22 readings → 21 unique).

## Normalization mode (Type-2 lite details)

**The asymmetry principle (keep in mind).** This is a CI gate that breaks the build.
A missed duplicate (low recall) — silent, nobody suffers. A false duplicate
(low precision) — a red CI on an honest PR → the person gets angry → after 2-3 times
turns the pass off entirely. **One loud false positive costs more than ten silent
false negatives.** Recall can be under-delivered, precision cannot (risk #3 from the spec).

**What to normalize:** identifiers (names of variables, functions, user
types) → a single placeholder (e.g. `$`).

**What NOT to touch:** keywords (`if`/`for`/`return`...), **fundamental
types** (`int` vs `double` — a real difference), **operators** (`+` vs `-`
distinguishes `add`/`sub`), **literals** (different constants are often meaningful).
Preserved operators/keywords are the main defense against false collapses.

**Tokenization** (the maintainer's decision at implementation):

- **(recommended)** clang raw `Lexer` — correct, buffer-only, **without AST and without
  `compile_commands.json`** (the fast-backend promise is preserved; LLVM is linked into the project
  anyway for the AST pass — it adds no new dependency).
- a self-contained C++ tokenizer — keeps the fast backend free of LLVM, but
  regexp tokenization of C++ is a known footgun (raw literals, comments). Take it
  only if full independence from LLVM matters.
- The default verbatim mode stays plain text without a tokenizer — the two modes have a different
  footprint, this is deliberate.

**Thresholds in normalization mode are higher than the defaults:** `min_lines` ↑ (e.g. 8 instead of 5),
`min_window_chars` ↑; cross-component-only is mandatory (small boilerplate is usually
within one component — that's how we cut it off).

## Perf

Confirmed: seconds and tens–hundreds of MB on 1M+ LOC (grpc 773k → 2.4 s; gm 2.77 s /
314 MB; cpparch 1.2M → 1.9 s / 231 MB). Nuance: the cost is **not linear in LOC**
(the driver — duplicate density / file shape).

- **Guard (P1-E):** a cap on occurrences-per-hash (explosive generated code → a quadratic
  blow-up on pairs); `--timeout` / `--max-files` as CI insurance.

## Non-goals (YAGNI / out of scope)

- **Type-2 by default.** The default — Type-1/verbatim only. Cheap Type-2 via
  identifier normalization — strictly optional, behind a flag. *Consistent*
  Type-2 — behind the AST pass #052.
- **Full Type-3** (gapped copies OUTSIDE same-name files — a diverged block
  inline in the middle of someone else's function) — NOT in the line pass; this is a **separate task
  #056** (fast token-overlap + token-LCS, parser-free). The file-twin rate here catches
  only gapped in same-name files — a cheap special case of what #056 does
  in a general token way; at #056 integration check whether it absorbs the rate.
- **Type-4 / semantics / IR level.**
- **Auto-classification of intent** (hole vs intentional twin) — lesson §2: not done by a
  machine, do not pretend.
- **Per-commit / temporal** GitClear metric beyond commit mode. Snapshot —
  explorer. `--changed-files` — a report filter, not a narrowing of the corpus.
- **Near-duplicate-FILE report beyond the file-twin rate** — later.
- **A vendored blocklist of common names** — later, if needed.
- **No Python in the product.** A pure C++20 single-binary path.

## Tests

- [ ] **A / clean:** one file without duplicates → ratio `0`, `0` blocks.
- [ ] **B / cross-file:** two files with a shared block `>= 5` lines (the
      daily/hourly-sink style) → one cross-component block found, length correct, the block
      **merged into a maximal one** (not N shifted windows).
- [ ] **C / trivial-only:** only braces and keyword scaffolding match →
      `0` blocks (the ignore mask works).
- [ ] **D / excludes:** duplicates in `bundled/`, a nested `.git`, a tree with
      `CMakeCache.txt`, `*_autogen` — excluded by default.
- [ ] **E / tests:** `*_test.cpp` and the `tests/` directory are excluded by default;
      `--include-tests` brings them back.
- [ ] **F / commit-gate:** a synthetic repo (a commit adds a verbatim copy) →
      baseline `0` cross-file, introduced `1`. A historical twin in baseline → NOT
      reported; a new duplicate → reported.
- [ ] **G / regression (free):** `fmt/include` `0` cross-file;
      `spdlog/include` finds the `daily` ↔ `hourly` block. Assert on the **presence**
      of the block and the order of the cross-file count, NOT the exact % (see P0-C).
- [ ] **H / Type-2 mode:** a consistently renamed copy found ONLY with
      `--normalize-identifiers`; structurally identical different functions below the threshold
      — NOT reported; `add`/`sub` (different operator) do not collapse.
- [ ] **I / file-twin:** two `foo.cpp` in different directories, ~50% match
      (diverged) → land in the **file-twin rate** (lowered threshold), but NOT in the
      ordinary block gate; a pair with different names at 50% — does NOT land in the rate.

## Acceptance criteria

- [ ] Pure C++20, without libclang, without `compile_commands.json`, without Python.
- [ ] Merge of seeds into maximal blocks implemented; no inflation of shifted windows.
- [ ] Exactly cross-component blocks are reported as a missing reuse edge.
- [ ] Default excludes: hard masks + 4 auto-rules (CMakeCache, nested `.git`,
      tests, demos); audit of the excluded in the report.
- [ ] Headline = findings (large partial transfer), ranked NOT by raw
      length; ratio is secondary and not the main UX signal.
- [ ] Two metrics (total + cross-component / cross-file proxy); the gate on cross-component.
- [ ] Two modes: snapshot (explorer, no gate/AI claim) and commit/baseline (gate).
- [ ] `// archcheck: allow` is respected.
- [ ] The pass behind a flag, off by default.
- [ ] One `IRule`, reuses existing plumbing where it exists (OCP/DRY).
- [ ] Dedup cases by file pair, not by scope.
- [ ] Regression tests assert the presence of blocks, not the exact ratio (P0-C);
      the ratio is asserted nowhere by exact value.
- [ ] Tests A–G green; H — if the mode is implemented; I — if the file-twin rate is implemented.
- [ ] File-twin rate: same-named files in different directories, lowered threshold,
      a separate reference rate (not a gate).
- [ ] Mode `--normalize-identifiers`: off by default; normalizes only
      identifiers; raised thresholds + cross-component-only.

## Reference findings from the sweep (the review reference — what counts as success)

Real missing-reuse-edges (NOT the longest — length deceives; size
classes — §"Ranking"):

- **Large partial transfer (top of the review list):** AetherSDR
  `ClientCompEditor` ↔ `StripCompPanel`; Kartend `mainwindow_*`; gm — a 78-line
  `PacketLogger` across two modules.
- **Duplicated data:** GWToolboxpp diacritics table `TextUtils` ↔
  `PluginUtils` (duplicated data ≈ almost always a hole).
- **Medium fragment:** spdlog `daily_file_sink` ↔ `hourly_file_sink`.
- **Small helper (our repo!):** `cpparch` — `jsonEscape` (21 lines)
  copied between `json_reporter.cpp` ↔ `violation_baseline.cpp`. A helper-level
  duplicate, not a file copy → extract into a util. An honest dogfooding story → **task
  #055** (`dogfood_json_escape_dedup`); without exaggerating the scale.

Intentional twins (detection is correct, do NOT flag — in baseline): folly V1/V2,
abseil map/set, Catch2 reporters, IrredenEngine demo-twins. Almost-file copies
(2/21) — OreStudio core↔server fork, pico-sdk board variants — also intentional.

## Key decisions

| Decision | Reason |
|---------|---------|
| Fast-pass Type-1 only by default | a cheap zero-setup layer without libclang |
| Skip blanks/comment-only instead of blanking | different empty lines and comments must not break the match |
| Merge by file-pair + diagonal + overlap | without it a single region falls apart into many shifted windows |
| Headline = findings, NOT a percentage | the overall % is too exclude-dependent and explains poorly where the problem is |
| Rank NOT by raw length | length pushes intentional twins up; the signal is "large partial transfer" (block < file, different names) |
| The gate does not rely on ranking/intent | intent cannot be classified by a machine; the gate = baseline (new vs old) |
| Two modes: snapshot=explorer / commit=gate | snapshot honestly shows twins (no AI claim); baseline freezes them, breaks only on a new duplicate |
| Duplication ratio is secondary | useful as an aggregate, but not as the main UX signal |
| Two metrics: total + cross-component | one should gate/look at cross-component, not total/headline |
| Dedup cases by file pair, not by scope | one duplicate in two scopes = one case (22→21) |
| Regression on the *presence* of blocks, not on % | ratio is exclude-dependent (spdlog 4.5% with `bundled` vs 8.26% without) — P0-C |
| Auto-exclude nested git repos | a single rule covers sandbox/snapshot/subrepo noise (P0-A) |
| Build tree = a directory with `CMakeCache.txt` | a cheap and precise built-in exclude for the first run |
| Exclude Qt autogen + demos/examples | `*_autogen`/`moc_*`/`qrc_*` and demos — noise, not a reuse hole |
| Tests exclude-by-default, `--include-tests` opt-out | test twins are real, but by default this is not a missing reuse edge |
| Vendor-in-src via config, do not guess by names | `minilzo/qhull/glad/CMSIS` are domain-specific |
| File-twin rate — reference, not a gate | there are almost no same-named forks (2/21), device variants are also same-named |
| Flag `--duplication=line` (umbrella) | leaves room for `--duplication=ast` (#052). Decided 2026-05-30 |
| Off by default | a tuning stage for FP is needed before a possible promotion into fast-defaults |
| Normalization: precision > recall, off by default | one loud FP breaks trust in the gate (asymmetry, risk #3) |
| CI gate = baseline / introduced-only | old intentional twins are frozen; only a new duplicate breaks |

## Changed files

| File | Change |
|------|-----------|
| `experiments/line_duplication/*` | ✅ spike + PERF/SWEEP (`35085ca`); ✅ P0-A in `main.cpp` (uncommitted): test/demo-exclusion + `--include-tests`, auto-rules `isBuildTreeDir`/`isNestedRepoDir`, `directoryPruneReason`, expanded masks, exclusion audit (`ExclusionAudit`/`printExclusionAudit`); fix of `recursive_directory_iterator` iteration; README updated; ✅ FILTER/DUPLICATE_PATHS/PROJECT_EXAMPLES reports + `examples/` (untracked, ~25 repos) |
| `src/scan/line_dup_scanner.h/.cpp` | fast line-dup scanner |
| `src/rules/duplication/...` | one rule on top of the fast scanner |
| `src/report/text_reporter.cpp` | text output: findings (ranking) + two metrics |
| `src/report/json_reporter.cpp` | json output |
| `src/report/sarif_reporter.cpp` | sarif output |
| `src/main.cpp` / CLI plumbing | `--duplication=line`, `--normalize-identifiers`, `--changed-files`, `--cross-only`, `--baseline`, exclude audit |
| config schema/docs | `defaults.line_duplication`, `strictness`, thresholds, excludes |
| `fixtures/line_duplication/pass/` | clean / trivial / vendored pass-cases |
| `fixtures/line_duplication/fail_cross_component/` | a real cross-component clone case |
| `tests/...` | unit/integration coverage (A–I) |
| `PERF-fast.md` | time + peak RSS on reference trees |

## Fixtures

- [ ] `fixtures/line_duplication/pass/no_duplicates_single_file/` (A)
- [ ] `fixtures/line_duplication/pass/trivial_windows_only/` (C)
- [ ] `fixtures/line_duplication/pass/bundled_excluded/` (D)
- [ ] `fixtures/line_duplication/pass/nested_git_excluded/` (D)
- [ ] `fixtures/line_duplication/pass/cmakecache_tree_excluded/` (D)
- [ ] `fixtures/line_duplication/pass/tests_excluded/` (E)
- [ ] `fixtures/line_duplication/fail_cross_component/` (B)
- [ ] `fixtures/line_duplication/commit_gate/` (F — synthetic repo)
- [ ] `fixtures/line_duplication/file_twin/` (I)
- [ ] `fixtures/line_duplication/normalize/recall_renamed/` (H-E)
- [ ] `fixtures/line_duplication/normalize/precision_different_meaning/` (H-F)
- [ ] `fixtures/line_duplication/normalize/operators_differ/` (H-G)

## Pointers

- **Reference algorithm:** `experiments/line_duplication/main.cpp` (the proven
  spike, formerly `copypaste.py`). Port the logic 1:1: hash → fast non-crypto,
  Python set/dict → `unordered_map`/`vector`.
- **Neighboring task:** #052 (`cross_tu_duplication_detector`) — AST layer, precise,
  expensive. Share the mapping into components + reporting, if a helper already exists.
- **Commit/incremental CI model** — reference Ericsson CodeChecker.
- **Sweep reports (~25 repos, untracked):** `FILTER_CLASSIFICATION_REPORT.md`
  (masks: hard/soft + before/after), `DUPLICATE_PATHS_REPORT.md` (3-class
  taxonomy + flat index of file pairs — input to the file-twin rate),
  `PROJECT_EXAMPLES_REPORT.md` + `examples/` (app-only top-blocks).
- **Spike lessons:** merge into maximal blocks is mandatory; the **presence of characteristic
  blocks** — a reliable regression anchor; both raw block count and ratio are fragile
  (count is inflated by shifted windows; ratio depends on excludes — P0-C); without
  project-local excludes the report drowns (P0-A); **length pushes intentional
  twins up** — rank by "large partial transfer" (§"Ranking");
  **intent cannot be classified by a machine** — the gate = baseline, not an intent verdict.

## In progress

- **2026-05-30: spec consolidated** — the submitted final version (sweep over ~25
  repos + a manual eyeball intent check on 21 cases) was merged into this file. This
  document is now the authoritative version, superseding all previous drafts/addenda.
- **P0-A closed in the spike (2026-05-30):** all 4 auto-rules (build-tree, nested-repo,
  tests, demos) + expanded hard masks + audit of the excluded. Verified on
  synthetic data and on cpparch (62 files, 0.02 s; auto-cut `sandbox/drift_repos/*`,
  `examples`, build trees). Uncommitted in the working tree:
  `experiments/line_duplication/main.cpp` + README.
- The unclosed code gate: **P0-B** (findings + ranking by "large partial
  transfer" + two metrics), **P0-C** (regression on presence).

## Next steps

1. ~~**P0-A (mechanism)**~~ ✅ closed in the spike: 4 auto-rules + hard masks +
   audit. It remains to move it into the product together with the port (json/sarif audit — there).
2. **P0-B** — headline = findings; rank by "large partial transfer"
   (NOT by raw length); two metrics (`total` / `cross_component`), `--cross-only`
   affects the computation.
3. **P0-C** — regression tests on assert-on-presence (the data from the sweep is ready);
   stop asserting on the exact %.
4. After closing P0 — port into `src/scan/line_dup_scanner.{h,cpp}` and an `IRule`,
   dedup by file pair.
5. After the first product merge — **P1-D**: baseline + introduced-only
   commit-aware mode as the normal CI form of this signal (the gate).
6. Optionally, as separate iterations: file-twin rate (§), `--normalize-identifiers`
   (Type-2 lite), P2-F confidence tiers.

## Summary
**Status:** superseded by #056 (decision 2026-06-01, see docs/duplication_architecture.md, header)

The line-based detector was implemented and worked (Type-1/verbatim + Type-2-lite),
but by an architectural decision the subsystem was reduced to a single token detector #056:
it covers Type-1 as a special case of Type-3, and the canonical failure of the line approach
(a copy with renaming = ~12% line match against 100% by tokens) is eliminated by
normalization. The #053 code was removed from the tree (it remains in git history).

**What outlived the task (knowledge archive):**
- The FP logic was moved into the #056 lexer: collapsing raw-string blobs `R"(...)"` into
  a single `lit`, discarding dead `#if 0`/`#if false` blocks.
- The `line`/`normLines` metric in Pair — an illustrative legacy of the line approach
  (and to this day — a working leg of the P0.6 joint-floor).
- The sweep empirics and exotica masks — in the subsystem's research docs.

All the "Next steps" items above were written BEFORE the 2026-06-01 decision and are cancelled by it:
the port of line_dup_scanner into the product will not happen. Their living heirs: baseline/gate
for duplicates — plan §7 of duplication_architecture.md; precision — #083.
**Closed:** 2026-06-11
