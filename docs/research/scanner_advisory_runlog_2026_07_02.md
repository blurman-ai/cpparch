# Scanner Advisory Run Log — 2026-07-02

**Tasks:** #160, #161, #162
**Binary:** `archcheck 0.1.5`
**HEAD:** `4a38bde2af2c3fa9f1c1a023bd45a33374fb7674`
**Mode:** Debug build, local corpus artifacts + live `--diff` repros.

## Build And Tests

```text
$ cmake --build build/debug
ninja: no work to do.

$ ./build/debug/src/archcheck --version
archcheck 0.1.5

$ cd build/debug && ctest --output-on-failure
100% tests passed, 0 tests failed out of 616
Total Test time (real) = 5.61 sec
```

## Bool-Field Drift Run

Source artifact:

```text
experiments/per_commit/results_full.boolrule.jsonl
```

Summary:

```text
OK diff rows:                     517975
Rows with DRIFT.BOOL_FIELD:        10735
Repos with bool-field findings:      787
Added bool fields reported:        17510
Struct events reported:            13315
```

Live repro:

```bash
cd ~/oss/EricLeeFriedman_CopilotChess
~/projects/cpparch/build/debug/src/archcheck \
  --diff --format=json \
  d19b8246d1322e2cf0c89f6d62217371dc098f75^..d19b8246d1322e2cf0c89f6d62217371dc098f75 .
```

Relevant output:

```json
{"rule":"DRIFT.BOOL_FIELD_ACCRETION","file":"src/moves.h","line":15,
 "message":"struct 'Move' accreted 1 bool field(s): is_castling"}
```

Same diff also emitted:

```text
DRIFT.LOCAL_COMPLEXITY:
- GenerateKingMoves: new function, complexity 25
- GenerateCastlingMoves: new function, complexity 32
- ApplyMove: 7 -> 42

DRIFT.NEW_CLONE:
- src/moves.cpp:158-204 clone of src/moves.cpp:313-359
- src/moves.cpp:50-93 clone of src/moves.cpp:100-150
```

Verdict: useful advisory / cross-signal example. It should not be phrased as a bug.

## Local Complexity Run

Source artifact:

```text
experiments/lcx_corpus_run/findings.jsonl
```

Summary:

```text
Finding commits: 931
Repos:           100
LCX violations:  2623
```

Manual reference artifact:

```text
experiments/lcx_corpus_run/triage.tsv
```

Summary:

```text
Reviewed commits: 189
TP:               167
MIXED:             15
FP:                 7
```

Reference classes:

```text
genuine-growth:          144
new-file-complexity:      23
arity-change-new-func:    10
overload-crossmatch:       8
deletion-shift:            4
```

### Live TP

```bash
cd ~/oss/AlchemyViewer_Alchemy
~/projects/cpparch/build/debug/src/archcheck \
  --diff --format=json \
  fc56e0801fabc19d514d8c877050d0feb00b7de5^..fc56e0801fabc19d514d8c877050d0feb00b7de5 .
```

Relevant output:

```json
{"rule":"DRIFT.LOCAL_COMPLEXITY","file":"indra/newview/lllocalbitmaps.cpp","line":321,
 "message":"function 'LLLocalBitmap::decodeBitmap' grew local complexity from 23 to 26 (+3, crossed 25)"}
```

Other advisory output in the same diff:

```text
SATD.1:
- indra/llimage/llimageavif.cpp:222

TEST.1.prod_changed_tests_silent:
- prod +493/-11 across 16 file(s), tests +0/-0
```

Verdict: clean LCX TP.

### Historical FP Recheck

```bash
cd ~/oss/AlchemyViewer_Alchemy
~/projects/cpparch/build/debug/src/archcheck \
  --diff --format=json \
  9a6ace99ded620a0f30a549c2f68153cee89daf2^..9a6ace99ded620a0f30a549c2f68153cee89daf2 .
```

Relevant output:

```json
{"rule":"TEST.1.prod_changed_tests_silent","file":"<diff>","line":0,
 "message":"prod +11/-2581 across 27 file(s), tests +0/-0"}
```

Verdict: old LCX deletion-shift / collision-risk FP no longer reproduces in the current binary.

## New-Clone Evidence

Existing manual precision artifact:

```text
experiments/per_commit/PRECISION_103.md
```

Summary:

```text
Reviewed findings: 70
TP:                60
FP:                 6
Borderline:         4
Precision:       86-91%
```

Residual noise class: generated/vendor/subtree copies, not random unrelated code.

## CLI Note

During the live repro I first ran:

```bash
archcheck --diff <revspec> --format=json .
```

This exits with code 2 because the CLI parses `--format=json` after the revspec as a path.
Correct order:

```bash
archcheck --diff --format=json <revspec> .
```

## Bottom Line

- `DRIFT.NEW_CLONE`: strongest measured precision, suitable as the clearest public advisory/gate story.
- `DRIFT.LOCAL_COMPLEXITY`: current binary reproduces clean TPs and no longer reproduces at least one old FP class; keep advisory.
- `DRIFT.BOOL_FIELD_ACCRETION`: current binary reproduces the per-commit signal; useful when cross-signal or repeated per struct; keep advisory and neutral wording.
- Tasks #160-#162 are not complete yet because their manual-example quotas are stricter than this run log.
