# [RESEARCH][DRIFT] LCX corpus validation: top-100 suspicious repos, ≥10 commits with complexity growth

**Date created:** 2026-06-11
**Date started:** 2026-06-11
**Status:** wip — script running
**Module:** EXPERIMENTS / SCAN / RESEARCH
**Priority:** major
**Complexity:** M
**Related:** #101 (LCX signal — closed), #102 (corpus prototype), #099 (awaiting the verdict on fallback)

## Goal

Validate the product LCX signal (#101) on the corpus: top-100 "suspicious" repos
(ai_pct ↓, prior findings ↓, commits ↓ from corpus_summary.tsv), for each find ≥10
commits with growth in local complexity via the PRODUCT binary `archcheck --diff`
(dogfood, not the Python prototype #102). Then an eyeball review of all findings (FP — a separate
list for manual verification by the user). Do not touch the code, except for obvious bugs (then
immediately with a test).

## Mechanics

- `experiments/lcx_corpus_run/run_lcx_scan.py`: selection from corpus_summary.tsv
  (LOC≥1000, commits≥8, clone in ~/oss), per-repo walk of non-merge commits with C/C++
  changes (new→old, cap 250), `archcheck --diff sha^..sha`, stop at 12
  finding-commits. Output: findings.jsonl + selection.txt + progress.log.
- The decision on #099 is tied to this run: if function-discovery covers real
  repos — #099 is closed as absorbed (into dropped/).

## Journal

- 2026-06-11: the smoke test revealed a bug in the script's PARSER (it looked for "LCX.", the product prints
  `DRIFT.LOCAL_COMPLEXITY`) — fixed; the signal itself worked immediately on the smoke test
  (fakelua: growth 11→16, 90→96 on real refactor commits).
- 2026-06-11: full run launched in the background.
- 2026-06-11/12: the background run stopped at 82/100 repos (decision: do not run all 100
  until quality is confirmed). 651 finding-commits, eyeball triage of 16 repos / 189
  commits → `triage.tsv` (reference): 167 TP, 15 MIXED, 7 FP. Precision 96%.
- 2026-06-12: all FPs reproduced on real commits, 4 root causes found
  in the scanner (not in the eyes). Per the user's command "fix all FPs and compare with
  the references", fixes A–D were made (see below) + pointwise verification of each case +
  a full re-scan of 188 reference commits (`rescan_triaged.py` → `rescan.jsonl`).

## FP classes and root causes (all reproduced)

1. **overload-crossmatch** (parseText ×10 Serial-Studio, handleEvent ×309 Alchemy,
   matchAndRewrite ×48 NVIDIA): inline methods inside classes got a bare name —
   discovery did not know the enclosing class/struct/namespace → namesakes of different classes
   matched against each other by line proximity.
2. **deletion-shift** (Cytnx Trace_ "0→53"): overloads with the same arity
   (string,string) vs (int64,int64) differed only by the nearest-line tie-break;
   a shift of ±45 lines flipped the match to a stub with score 0.
3. **arity-change-new-func** (setManualImage +has_mips): a signature change → no
   (name,arity) match → the whole inherited body surfaced as "new function N".
4. **unparseable parent** (ArduPilot 924a3e3860^): the parent commit genuinely did not
   compile (two unclosed `for` in FuseVelPosNED) → the function was not found in the
   parent → a phantom "new function 257". Aggravating: `#if/#else` with both
   branches in the token stream gave a false brace imbalance (Mudlet +2, Cytnx −1).

## Fixes (product code, each with unit tests)

- **A. Scope qualification** (`function_body_scan.cpp`): ScopeTracker — a stack of
  namespace/class/struct/union by brace-depth; inline methods get the full name
  (`Foo::parse`). Forward-decl/elaborated-type/template parameters are not pushed.
- **B. Param fingerprint** (`function_body_scan.{h,cpp}`, `local_complexity_drift.cpp`):
  FunctionSpan/FunctionComplexity carry a concatenation of the parameter-list tokens;
  findOldMatch prefers an exact fingerprint, nearest-line stays only a
  fallback in case of real ambiguity.
- **C. Arity-change fallback + brace-guard** (`local_complexity_drift.cpp`):
  in the absence of a (name,arity) match, same-name-any-arity is searched (lowConfidence →
  only a soft Δ≥5); a file with an UNCLOSED brace (depth>0) is silenced entirely
  (mega-spans like ROCm "1048"), an extra `}` (depth<0) is tolerated — forward
  discovery self-recovers.
- **D. Largest-branch for #if/#elif/#else** (`local_complexity_metrics.cpp`):
  stripDirectiveTokens keeps the largest branch of the chain (buffering with a
  stack of chains). Covers both `#ifdef X #else <whole file> #endif` (Cytnx) and
  the stub branch `#ifdef Q_OS_WASM` over the real code (wiRedPanda) and
  unbalanced double `if (..) {` (Mudlet).

## Comparison with the reference (after fixes)

- FP commits: 5/6 went silent; the sixth (Alchemy 6e77969ee2) now reports a DIFFERENT,
  real growth (alloc_tex_image +10: an if+while+2if added to the body) — the growth was previously
  masked by the false "new function 54".
- TP commits: 163/167 retained the signal. All 4 "losses" turned out, on review, to be
  reference errors (the eyeball triage took a cross-match for growth, because the commits contained
  real changes elsewhere): FastLED fefca7b8c1 (rfind: scores identical
  6/21/1/0 in both versions), NVIDIA 23308b493d (matchAndRewrite: all scores
  identical, one pattern score 5<25 added), ROCm 572fbe20f6 (findLinkInfo:
  stub 0 + real 26 in BOTH versions), SenaxInc 977ca709f1 (get*Description: scores
  13/16 identical, the functions simply shifted by ~48 lines).
- MIXED commits dropped the artifact part, the real one stayed (NVIDIA bdae036928
  5→1, Serial-Studio 448a4cfe0e 6→3, Cytnx dc0b6b3dd0 4→1).
- Total over the 188 reference commits: 429 → 372 signals.
- Gates: 1602 assertions / 480 cases green; clang-format, cppcheck, lizard,
  dogfood (src/include/tests: 0 violations) clean.

## Open

- The reference triage.tsv contains 4 documented errors (above) — mark the lines
  on the next pass; recompute reference precision: 7 real FPs from the manual
  triage + 4 hidden FPs that passed as TP.
- The run of the remaining corpus repos (83–100 + eyeball review of 13–58) — after the
  user's decision.

## Nightly validation journal (2026-06-12, autonomous)

Full run #2 (100 repos, fixed binary A–D): 930 commits, 2071 violations,
85 repos with findings. A selective eyeball check by the user's protocol
("every ~10 findings") revealed three more sources of noise → fixes E–G:

- **E. lambdaPending via `;`** (`local_complexity_metrics.cpp`): `char buf[N];`
  armed lambdaPending, and the next bare block `{...}` was treated as a lambda body →
  a false +nesting on all its contents. Found on sbbs 2a0ee8dd1a ("hoist
  declarations" gave +25 with identical decision-points; after the fix 76→76,
  the signal correctly disappeared). Fix: `;` at parenDepth 0 resets the flag.
- **F. Versioned vendored directories** (`file_classification.h`): `src/zlib-1.3.2/`
  did not match the `zlib` entry (normalization gives `zlib1.3.2`). texstudio "update
  to zlib 1.3.2" gave 32 noise findings. Fix: the pointwise version suffix
  (`digits.digits`) is trimmed and the stem is checked against kVendoredLibDirs. 32→0.
- **G. Rescope match** (`local_complexity_drift.cpp`): gromacs "Move into gmx
  namespace" gave 29 phantom "new function" (after fix A the names changed
  `foo`→`gmx::foo`). The fingerprint does not work here — inside the namespace the
  parameter qualifiers were rewritten (`gmx::MDLogger&`→`MDLogger&`). Criterion: same short
  name + arity AND the old function DISAPPEARED from the new file → this is a move, not a new
  function (lowConfidence, only Δ≥5). 29→0.

Manually checked "suspicious" cases from run #2 that turned out to be honest TPs:
MaximumTrainer Account::Account 0→6 (the ctor got nested ifs of token migration),
Lightpad treeWidgetStyle 0→9 (ternaries isValid()?:), glasgow
buildConstraintNoOverlap 0→5 (the stub got an implementation, the fingerprint matched the exact
overload out of four), foundationdb LogSystemConsumer 0→92×5 (a move of an implementation
from LogSystem.cpp — mechanically honest, semantically a move; a known limitation
of per-file comparison), intechstudio f3477654f5 (bulk-port of 819 files — honest
new-file).

Reference after E–G: no regressions (163/167 TP, 5/6 FP, 4 known reference errors).
Run #3 launched (all fixes A–G).

## Addendum (2026-06-12, morning): fixes H–I on the user's question

The user doubted two "honest TPs" from the nightly summary. Eyeball check:
- glasgow buildConstraintNoOverlap 0→5 — confirmed a clean TP (the body was
  a single line report_unsupported, the commit implemented the overload).
- foundationdb "move of 861 lines" — confirmed a **hidden FP class**:

- **H. Cross-file move detector** (`local_complexity_drift.cpp`):
  the body of a function that disappeared from one file of the diff (or was compressed to a delegate stub
  with score<5) silences the "grew 0→N" / "new function" of the same function (short name +
  arity + score ±max(2,10%)) in another file of the same diff. Pool consume-once.
  foundationdb db0ccfc99a: 9 phantoms → 0, net delta became an honest +414/−414.
- **I. Apache banner ≠ vendor for LCX** (`local_complexity_drift.cpp`):
  hasVendorLicenseHeader (the content heuristic of the duplicate scanner) silently threw out
  ALL files of Apache-licensed projects from LCX analysis — for foundationdb
  the larger part of its own code was not analyzed at all (which is why the absence of
  LogSystem.cpp produced no improvements). LCX now uses only the
  path/basename layers of the vendor filter. The silent loss of coverage is eliminated.

Effect on the reference: +3 "TP-LOST", all three checked by eye and correct:
FlashCpp a0acb5538c and 12ea8147ab — clean file-splits (a move without growth),
Cytnx 5e48a8c4a1 — the fifth reference error ("Rsvd grew 4→66" was a cross-match,
in both versions the Rsvd overloads have scores 45/4/4; the real move of
Rsvd_notruncate_BlockUT_internal score 69 is suppressed by the move pool correctly).

Run #4 (fixes A–I) launched.

## Checkpoint finding (2026-06-12, day): fix J

A selective check of run #4 across 40 repos: skyrim-community-shaders
2e57080e0a "rename weather editor to CS editor" gave 33 phantom "new function".

- **J. --no-renames in changedCppFiles** (`git_state.cpp`): git with rename detection
  gives in --name-only only the NEW path of a renamed file — the old path did
  not get into changedFiles, the move pool (fix H) did not see the disappearing side.
  Now a rename comes as a pair A+D. skyrim: 33→0. An integration test was added.

Effect on the reference: +2 reclassification (both — file-rename, the reference itself marked them
as "port"/"file rename"): Alchemy 56d8ece861 (llappviewerlinux→sdl),
21cmFAST 6017a20997 (PerturbField.c→PerturbedField.c). Total disabled reference lines: 9
(5 triage errors + 4 move/rename reclassification).

Run #5 (fixes A–J) launched.

## Run #5 checkpoints (selective eyeball check every ~20 repos)

- ~17/100: clean. gauge f2d18665c0 (uniform +13 ×3) — honest, dirty-tracking
  inserted into every display function (98 new branches). alien loadSave ×2 —
  different overloads with real legacy logic, the fingerprint distinguished them.
- ~38/100: clean. valentina InitScenes 0→21 (connect-lambda rubber-band),
  MAA navigate_to_stage 0→19 (retry navigation), webf 13 findings (+1638 lines
  of real selector matching) — all honest.
- ~57/100: no product bugs; two observations on corpus METHODOLOGY:
  (a) identical patches on different branches give duplicate findings (grid-fw SUKU ×2,
  the diffs literally coincide) — cured by dedup on git patch-id in the script;
  (b) snapshot commits of forks (nvdajp "Apply JP snapshot", 3.37M insertions,
  57 findings) — mechanically honest new functions, but not authored changes;
  for corpus statistics it is worth cutting off mega-commits by an insertions threshold.

## Selection incident (2026-06-12, day)

Run #6 suddenly scanned DIFFERENT repos (cpu6502 with 12 commits, etc.):
the grow job (#115) is cloning repos into ~/oss on the fly and updated corpus_summary.tsv
(04:39) — the top-100 by ai_pct shifted to freshly-cloned small AI repos.
The run was stopped. Decision: the selection is pinned by the file `selection_pinned.txt`
(run_lcx_scan.py reads it if present); the provable 84 repos of the original
selection were restored (the exact order from the progress log of run #1 + 1 by rank).
Ranks 85–100 of the original are unrecoverable (selection.txt was overwritten; among
the candidates it is impossible to distinguish "was on disk" from "re-cloned by #115 today"),
in run #2 they gave 0 findings — they do not affect the report. Run #7 (final,
fixes A–K, pin 84) launched.
- ~76/84 (v7): clean. A candidate for kVendoredLibDirs: `vsomeip` (EcuBus-Pro
  imported it as a tree +21917 lines → 19 noise new-functions; the path/basename
  layers do not catch it). siril 7a2ec4309f (12 findings) — a real feature spanning 21 files.

## Result of the final run #7 (2026-06-12, evening)

Pin of 84 repos, binary with fixes A–K, 11499s. Result:
- **945 finding-commits, 1813 violations, 82/84 repos with findings**
- 76 repos reached ≥10 commits, 73 — the target 12 (the rest — small repos
  that simply do not have that much growth)
- Composition: 1343 growth, 395 new function, 75 from-0 (stubs→implementations)
- Report with links: `experiments/lcx_corpus_run/REPORT.md` (3008 lines,
  per-repo heading links to GitHub + per-commit links + violations)
- Eyeball checkpoint checks during the run (every ~20 repos): no new classes of
  artifacts were found; all selectively checked "suspicious" cases turned out to be
  honest TP (gauge dirty-tracking, alien loadSave overloads, valentina
  connect-lambda, MAA retry navigation, autoware stub→implementation, minsky
  ICairoShim series, xLights merge-on-import).

Noise comparison: run #2 (broken scanner) — 2071 violations on 100 repos;
run #7 (fixes A–K) — 1813 on 84 repos with COMPLETE elimination of the known
FP classes and +coverage of Apache projects (fix I).

## Run #8 (2026-06-12, evening): fresh top-100 of the new corpus

Per the user's command: the entire wave of fixes was committed (ee16c12, 563e9e4,
c814998, 29a2339; file_classification and #116 went as commits of a parallel
session 362ca60/84ca991), then a run over the FRESH top-100 (without a pin — a new
corpus_summary + clones from #115). The v7 artifacts were saved as *_v7_pinned84.*.

Result v8: 931 finding-commits, 2623 violations, 100/100 repos (66 with ≥10).
1341s — the new repos are small. A structural feature of the new selection: AI-slop
repos with mega-drops ("extract sources" +419K lines → 241 findings; ohal 102;
SpecusGL 87) — for statistics, filtering mega-commits is mandatory
(an insertions threshold), see the methodological notes above. REPORT.md
was regenerated for v8.

## How it works (summary for the future reader)

The LCX corpus validation = a closed loop: a run of the product binary over the
top-suspicious corpus repos (`run_lcx_scan.py`, per-commit `--diff`) →
eyeball triage of findings into `triage.tsv` (reference: 189 commits, 16 repos) →
reproduction of each FP on a real commit → a fix in the scanner with a unit test →
re-scan of the reference (`rescan_triaged.py`) with the check "FPs gone, TPs stayed" →
a new full run → selective eyeball checkpoints every ~20 repos. The cycle
repeated until the checkpoints stopped finding new classes of artifacts.

## Key decisions

- **Reference before fixes**: triage.tsv was assembled BEFORE code edits — all fixes
  were checked against it; 5 errors of the reference itself were found by the detector and
  confirmed manually (the aggregate lies — verify case by case).
- **Pin the selection** (`selection_pinned.txt`): corpus_summary and clones are moving
  targets (#115), without a pin runs are incomparable.
- **Eyeball checkpoints during the run** (the user's protocol "every ~10
  findings"): they are exactly what caught the lambdaPending bug, the vendored versions, the Apache
  banner, the rename hole and the minsky counter-example.
- **Move ≠ growth**: a body move (between files/scopes/signatures) is silenced,
  duplication is reported; the boundary was fixed by a counterfactual experiment.

## Changed files

- `src/scan/function_body_scan.cpp`, `include/.../function_body_scan.h` —
  ScopeTracker, param fingerprint (ee16c12)
- `src/scan/local_complexity_drift.cpp` — fingerprint match, arity/rescope/move
  fallbacks, brace-guard, basename-only vendor (ee16c12, c814998)
- `src/scan/local_complexity_metrics.cpp` — largest-#if-branch, lambdaPending
  (563e9e4)
- `include/archcheck/scan/file_classification.h` — versioned vendor directories
  (362ca60, parallel session)
- `src/git/git_state.cpp` — --no-renames (c814998)
- `config/diff`: thresholds.diff_max_added_lines (#117, 6c0375a)
- tests: +9 files touched, 1784 assertions; journal/docs: 29a2339
- artifacts: experiments/lcx_corpus_run/{REPORT.md, REPORT_v7_pinned84.md,
  findings*.jsonl, triage.tsv, selection_v7_pinned84.txt}

## Outcome

**Status:** completed
**Date completed:** 2026-06-12

Goal achieved: the LCX signal was validated on the corpus with the product binary
(final runs: pin-84 → 945 finding-commits/1813 violations;
fresh top-100 → 931/2623). Along the way, 11 classes of scanner defects were found and closed
(fixes A–K) + the bulk-import threshold (#117). Reference: all confirmed
FPs eliminated, TPs preserved. #099 is closed as absorbed: function-discovery
provably covers real repos, the text-proxy fallback is not needed.
