# [SCAN][DUPLICATION] Composition/API-choreography clone research

**Created:** 2026-07-01
**Started:** 2026-07-01
**Status:** wip
**Module:** SCAN / DUPLICATION
**Priority:** major
**Complexity:** M
**Blocks:** possible future precision guard for #158 residual idiom/composition noise
**Blocked by:** —
**Related:** #158 (clone-detector FP reduction), #131 (fresh corpus remeasure),
#103 (copy-paste precision baseline), #070 (FP guards), #056/#072 (duplication detector)

> **RESUME HERE.** This is a research task, not a pre-approved product fix. Work autonomously:
> write temporary probes, run them, inspect real code examples, adjust, rerun, and leave a
> reproducible analysis. Do not ship a generic suppression guard unless the corpus evidence is
> clean enough and fixtures prove it.

## Scope clarification 2026-07-01

User clarified the first pass: do not run per-commit verification yet. The OSS corpus is available
at `~/oss`; for now check snapshot presence of function-composition patterns in
projects: functions made mostly from the same top-level call choreography with different domain
arguments.

## Why this exists

During #158 residual-FP review, the user raised a specific hypothesis:

```cpp
void f1()
{
  helper1(1, 2, 3);
  helper2(4, 5);
  helper3(7, 8);
}

void f2()
{
  helper1(5, 6, 7);
  helper2(11, 12);
  helper3(5, 4);
}
```

The detector may report this as copy-paste because the call choreography is the same after
normalization, but it may be legitimate composition/API usage: same sequence of operations over
different domain values, not a duplicated algorithm body that should be merged.

This is distinct from ordinary copy-paste with renamed variables. The task is to find whether a
real, suppressible class exists in the corpus, and whether it can be separated from true copy-paste
without losing important findings.

## Current evidence from #158

Temporary probes already tried on `experiments/corpus_remeasure_131/group3_findings_INLINECASE.jsonl`:

- generic call/callee LCS + argument overlap:
  - strict-ish configs removed FP only with equal TP collateral (`FP 1 / TP 1`, `FP 2 / TP 2`,
    `FP 4 / TP 5`, etc.).
- top-level call-statement choreography (`callee(...);` / `obj.callee(...);`):
  - `strict`: `FP 0 / TP 0`
  - `base`: `FP 2 / TP 0`
  - `loose`: `FP 3 / TP 3`
  - `many`: `FP 2 / TP 0`

But the actual base/many hits were not clean suppressible composition:

- `viperx1/Usagi-dono`: manager boilerplate around `QSqlDatabase`, `QSqlQuery`, `LOG`, `q.exec`.
  The user classified it as useful copy-paste/advisory.
- `xsscx/research`: heuristic-library diagnostic/check blocks. The user classified it as useful
  copy-paste/advisory.
- `FULL-FIRMWARE menu_led_control`: TFT slider drawing chain. It is composition-like, but the user
  also classified it as copy-paste/repeated UI drawing and the loose guard had TP collateral.

Conclusion so far: the signal is real, but the first generic thresholds are not safe.

## Research question

Can we identify a narrow, defensible class of composition/API choreography clones where:

- the normalized structure matches mainly because arguments inside calls were ignored or generalized;
- the fragments are dominated by top-level calls into an API/builder/DSL rather than local algorithmic
  control/data transformation;
- the call sequence is intentionally repeated with different domain arguments;
- suppressing or down-ranking these findings improves precision without unacceptable TP loss.

If the answer is "no", document that clearly with examples and leave no product change.

## Required datasets

Use at least:

- Group-3 labelled corpus current state:
  `experiments/corpus_remeasure_131/group3_findings_INLINECASE.jsonl`
- The TSV labels:
  `experiments/verification/fp_corpus_r2.tsv`
- Current matcher:
  `experiments/corpus_remeasure_131/match.py`

Then broaden if needed:

- `experiments/ai_repo_run/*_dup.txt` for raw detector output examples.
- Current `--duplication` runs on selected local repos under `~/oss`.
- Real snippets from repositories, not summaries. When reporting examples in chat or task notes,
  show full fragments up to 10 lines; if longer, state total line count and show the most relevant
  call sequence.

## Suggested temporary probe

Create a temporary script under `experiments/corpus_remeasure_131/`, for example
`composition_argstrip_probe.py`. It may be gitignored or committed only if it becomes useful
documentation.

The probe should:

1. Pull witness findings via `match.py`.
2. Extract both code spans from local git repos.
3. Build at least two views:
   - **arg-stripped view:** replace call arguments with placeholders, e.g.
     `helper1(ARG); helper2(ARG); helper3(ARG);`
   - **argument view:** preserve the argument token sets/sequences for each matched call.
4. Compare:
   - top-level call sequence LCS;
   - proportion of statements that are calls;
   - proportion of matched calls whose callee is the same but argument tokens differ;
   - argument overlap, both raw and after dropping local variable names;
   - shared non-call substantive lines;
   - control-flow density (`if/for/while/switch/return` vs plain calls).
5. Print ranked candidates with:
   - repo/sha/files/ranges;
   - corpus label and class;
   - current detector score if available;
   - metrics;
   - full call-sequence snippet.

Important: do not only count. Every promising bucket must be backed by real code examples.

## Hypotheses to test

### H1 — Argument-stripped clones reveal composition candidates

Run a mode that finds pairs whose arg-stripped call sequence is highly similar while argument
payloads differ substantially. Check whether these are real composition or just copy-paste with
renamed constants.

Expected risk: many UI/SQL/diagnostic blocks are still useful copy-paste findings.

### H2 — API/DSL family restriction may separate better than generic calls

Composition may concentrate in fluent/builder/drawing/plotting/menu APIs:

- Qt/QString/style builders;
- ImGui/ImPlot;
- TFT/rendering/menu drawing;
- SQL/schema setup;
- command/menu registration tables;
- diagnostic/reporting pipelines.

Measure these as named families, but do not hard-code a library-specific product rule without
evidence. A future product rule must be phrased as a general structural signal, not "skip ImGui".

### H3 — Non-call substance is the discriminator

A suppressible composition candidate should have little shared non-call logic. If both fragments
also share assignments, loops, branches, data transformations, or magic thresholds, it is more likely
copy-paste.

Test thresholds like:

- high top-level call density;
- low shared non-call line count;
- low control-flow density;
- low local computation token overlap outside call arguments.

### H4 — Down-rank may be safer than drop

If composition candidates are useful but lower priority, evaluate down-ranking / tagging instead of
suppression. The public product may tolerate "composition-like" advisory findings if they are rare
and explainable.

## Mandatory output

Produce an analysis section in this task file with:

- commands run;
- scripts/probes used;
- corpus numbers for each tested config:
  - FP removed;
  - TP collateral;
  - recall;
  - precision;
  - suppression;
- at least 10 real candidate examples, grouped as:
  - clean suppressible composition;
  - copy-paste despite same call chain;
  - ambiguous / user-review-needed;
  - TP collateral controls.
- final decision:
  - **ship guard** with exact rule and fixtures; or
  - **ship tag/down-rank only**; or
  - **do not ship**, document as future research.

## Analysis 2026-07-01 — snapshot presence pass, no per-commit verification

User clarification: use the local OSS corpus if needed, but do not run per-commit verification yet.
This pass checks only whether function-composition patterns exist in current project snapshots.

Commands:

```bash
python3 experiments/corpus_remeasure_131/composition_probe.py group3_findings_INLINECASE.jsonl
python3 experiments/corpus_remeasure_131/composition_snapshot_probe.py \
  --max-files-per-repo 500 --max-pairs-per-repo 0 --top 100000 \
  --out-md experiments/corpus_remeasure_131/composition_snapshot_full.md \
  --out-jsonl experiments/corpus_remeasure_131/composition_snapshot_full.jsonl
```

Probes:

- `composition_probe.py` — existing ignored Group-3 witness probe; re-run as a control.
- `composition_snapshot_probe.py` — temporary ignored snapshot probe. It scans `~/oss`
  current trees, skips build/vendor/vendored/test directories and test-like filenames, extracts
  C/C++ functions heuristically, and keeps pairs where:
  - both functions have at least 3 top-level call statements;
  - call density is at least 0.75;
  - the normalized top-level callee sequence is identical;
  - argument-token Jaccard is at most 0.45.
- Full-run navigation artifacts:
  - `experiments/corpus_remeasure_131/composition_snapshot_full.jsonl` — all retained pair records.
  - `experiments/corpus_remeasure_131/composition_snapshot_full.md` — all retained pairs with snippets.
  - `experiments/corpus_remeasure_131/composition_snapshot_full_index.md` — compact index.
  - `experiments/corpus_remeasure_131/composition_snapshot_repo_summary.tsv` — repo-level summary.
  - `experiments/corpus_remeasure_131/composition_snapshot_sequence_summary.tsv` — sequence-level summary.

Control on `group3_findings_INLINECASE.jsonl` reproduced the earlier warning: generic call-sequence
thresholds still trade FP removals for TP collateral.

```text
minCalls=4 calleeLcs>=0.80 argOverlap<=0.35 family>=0.60: FP 1 / TP 1
minCalls=5 calleeLcs>=0.85 argOverlap<=0.65 family>=0.60: FP 2 / TP 2
minCalls=5 calleeLcs>=0.85 argOverlap<=0.75 family>=0.60: FP 4 / TP 5
minCalls=6 calleeLcs>=0.85 argOverlap<=0.75 family>=0.70: FP 3 / TP 3
```

Full snapshot numbers over the local OSS corpus:

```text
repos_scanned=2042
repos_with_pairs=991
pairs_retained=90020
unique_call_sequences=2975
unique_repo_sequence_combinations=4019
files_scanned=504412
functions_extracted=4939360
```

The run used `--max-files-per-repo 500` as a runtime cap, but disabled the pair cap
(`--max-pairs-per-repo 0`). Therefore this is "all retained examples found under the scan cap",
not a mathematical exhaust of every file in very large repos.

Heuristic category sketch from the full index:

| Category | Pairs |
|---|---:|
| critical-section / generated-like | 35765 |
| other | 28433 |
| assert / test-like | 9863 |
| status/error-chain | 5727 |
| registration/table | 4879 |
| hardware/io | 2156 |
| print/log | 2149 |
| ui-binding | 797 |
| serialization/walk | 209 |
| sql/binding | 42 |

Representative composition-like examples found in real projects:

| Class | Example |
|---|---|
| registration table | `BearWare_TeamTalk5` `SoundEventsModel` / `StatusBarEventsModel`: 31 `push_back(...)` calls, args overlap 0.0 |
| MFC/UI data binding | `CUBRID_cubrid` `VAS.CPP` / `WAS.CPP` `DoDataExchange`: `CDialog::DoDataExchange` + 15 `DDX_Control(...)`, args 0.016 |
| AST visitor field walk | `CUBRID_cubrid` `pt_apply_delete` / `pt_apply_spec`: 15 `PT_APPLY_WALK(...)`, args 0.143 |
| hardware register setup | `78_xiaozhi-esp32` board `Pmic` constructors: 13 `WriteReg(...)`, args 0.0 |
| UI toggle composition | `AlchemyViewer_Alchemy` `selectAllTypes` / `selectNoTypes`: 13 `setValue(...)`, args 0.0 |
| Qt translation/UI text setup | `Cockatrice_Cockatrice` `retranslateUi` / `retranslateUi`: `setTitle` + 11 `setText`, args 0.2 |
| telemetry/fact registration | `CubePilot_qgroundcontrol-herelink` fact groups: `_addFact(...)` + `setRawValue(...)`, args 0.0 |
| binding/init mirrors | `203-Systems_MatrixOS` PikaObj populate/init: `obj_setInt/obj_setStr` field sequence, args 0.1 |
| JSON serialization | `AB-lab113_hidering` `toJsonValue` overloads: `StartObject` + `INSERT_INTO_JSON_OBJECT`, args 0.045 |
| explicit serialization chains | `Arsia-Mons_Silencer` `Serialize` overloads: 10 `Serialize(...)`, args 0.045 |
| SQL statement binding | `0xV4h3_cpp-atlas` `completeSession` / `writeJournalEntry`: `prepare` + `bindValue`, args 0.0 |
| status/error propagation idiom | `AOMediaCodec_iamf-tools` `ReadStreamInfo` / `ReadAndValidate`: 8 `RETURN_IF_NOT_OK(...)`, args 0.097 |

Interpretation:

- The class exists. Repeated function-composition/API-choreography bodies are common enough to show
  up in 991/2042 snapshot repos under the current scan cap.
- The class is mixed. The same signature covers clean composition, data/registration tables,
  serializer/visitor boilerplate, status-macro chains, and likely real copy-paste with edited
  payloads.
- The full count is dominated by generated/SDK-like macro ceremony (`*_CRITICAL_SECTION_ENTER`,
  GLEW info dumps, generated layout printers) and by assert/error macro chains. That reinforces
  the need for manual class labels before any product rule.
- A generic "same callees, different args" suppression would be unsafe. Group-3 still shows TP
  collateral; the snapshot examples also include extractable/review-worthy cases such as settings
  save chains and SQL binding chains.

Current decision: **do not ship a guard**. This pass proves presence, not suppressibility. If this
continues, the safer product direction is a `composition-like` tag/down-rank candidate, then only
after a labelled sample separates UI/registration/binding composition from real copy-paste.

## Analysis 2026-07-01 — composition-percent gate recalculation

User corrected the classifier target: the interesting class is not ordinary copied blocks with a
few changed literals, but blocks made mostly of repeated API calls where all or almost all calls
have different arguments. Manual classifications from the discussion:

- `FLOX-Foundation_flox` `o.Set(...)` object population: **not copy-paste**, clean composition.
- `MUME_MMapper` `connect(...)` setup: **not copy-paste**, composition, even if accidental.
- `CUBRID_cubrid` `DDX_Control(...)`: copy-paste-ish, but structurally hard to fix; do not treat as
  a clear "bad duplicate" control.

Recalculation command:

```bash
python3 experiments/corpus_remeasure_131/composition_gate_recalc.py \
  --min-call-density 0.20 --min-diff-arg-ratio 0.50 --max-control-line-ratio 0.30 \
  --thresholds 0.10 0.15 0.20 0.25 0.30 0.35 \
  --out-md experiments/corpus_remeasure_131/composition_gate_recalc_full.md \
  --out-labelled-jsonl experiments/corpus_remeasure_131/composition_gate_labelled_full.jsonl \
  --out-snapshot-jsonl experiments/corpus_remeasure_131/composition_gate_snapshot_full.jsonl
```

Metric used:

```text
composition_percent =
  aligned same-callee calls with different argument tokens
  / average statement count of the two matched blocks
```

The sweep gate also required `matched_calls >= 3`, `callee_lcs >= 0.8`,
`call_density >= 0.2`, `diff_arg_ratio >= 0.5`, and `control_line_ratio <= 0.3`.

Labelled Group-3 baseline:

```text
labels=311 (143 TP, 168 FP)
fired_labelled_rows=76 (58 TP, 18 FP)
measurable_fired_witness_pairs=236
snapshot_pairs_with_recomputed_metrics=89382
```

Gate sweep on labelled Group-3 rows:

| Composition threshold | Suppressed rows | FP removed | TP collateral | Remaining precision | Remaining recall |
|---:|---:|---:|---:|---:|---:|
| 0.10 | 6 | 3 | 3 | 0.786 | 0.385 |
| 0.15 | 6 | 3 | 3 | 0.786 | 0.385 |
| 0.20 | 3 | 1 | 2 | 0.767 | 0.392 |
| 0.25 | 2 | 1 | 1 | 0.770 | 0.399 |
| 0.30 | 2 | 1 | 1 | 0.770 | 0.399 |
| 0.35 | 0 | 0 | 0 | 0.763 | 0.406 |

The top labelled examples explain why the percentage alone cannot be a suppress guard:

- `FULL-FIRMWARE-Coche-Marcos` TFT slider drawing is the highest FP
  (`composition_percent=0.333..0.345`), but it is exactly the kind of repeated UI drawing that a
  reviewer may still want to see as copy-paste/advisory.
- `esrrhs/fakelua` benchmark wrappers are TP at the same strength
  (`composition_percent=0.333`): repeated benchmark bodies with only backend/test names changed.
- `aethersdr/AetherSDR` JSON/theme serialization is TP collateral at
  `composition_percent=0.230`.
- `Usagi-dono` SQL-manager boilerplate and `NexusMiner` recovery/logging blocks appear in the low
  threshold bucket; the latter is simultaneously represented as a labelled TP control, so suppressing
  it would hide a user-classified real duplicate.

Snapshot top-10 by composition percent is dominated by GLEW info dumps and generated/table-like
registration/check functions (`glewInfoFunc`, `Add`, `CHECK_MEMBER`, `LIB_FUNCTION`) with
`composition_percent ~= 0.96..0.99`. These are good examples of "composition exists", but poor
evidence for a product suppressor because many are generated, SDK-like, or table declarations.

Additional sensitivity checks:

```text
strict: callDensity>=0.50, diffArgRatio>=0.75, control<=0.20
  -> 0 FP removed / 0 TP collateral at all tested thresholds

medium: callDensity>=0.50, diffArgRatio>=0.50, control<=0.30
  -> 1 FP removed / 1 TP collateral until threshold 0.30, then 0/0 at 0.35

diff-heavy: callDensity>=0.20, diffArgRatio>=0.75, control<=0.30
  -> 1 FP removed / 1 TP collateral at thresholds 0.10..0.15,
     then 0 FP / 1 TP at 0.20, then 0/0
```

Updated decision: **do not ship suppression**. A composition percentage is useful as a diagnostic
feature and maybe as a future `composition-like` tag/down-rank input, but by itself it does not
separate clean composition from true copy-paste. The first removals already include TP collateral,
and the highest-score snapshot examples are dominated by generated/table-like code.

## Acceptance if a product change is proposed

- [ ] The rule is narrow and structural, not library-name-specific.
- [ ] It has unit tests and duplication fixtures:
      `fixtures/duplication/composition/pass_*` and `fail_*`.
- [ ] It is measured on Group-3 with the corrected matcher.
- [ ] It does not regress #103 precision beyond an explicitly accepted threshold.
- [ ] It does not hide user-classified useful copy-paste examples from #158:
      `Usagi-dono`, `xsscx/research`, `FULL-FIRMWARE menu_led_control`, `NexusMiner`.
- [ ] Dogfood stays green.
- [ ] `lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/`
      has no new warnings.

## Do not do

- Do not add a broad "same callees, different args => suppress" guard.
- Do not tune thresholds only against labels; inspect the witness code.
- Do not trust cropped snippets. Show enough code to classify the example.
- Do not split this into another task until the first autonomous research pass has real results.
- Do not commit experimental product code unless it has fixtures and measured tradeoff.
