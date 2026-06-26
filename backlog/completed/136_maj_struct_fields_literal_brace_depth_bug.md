# [RESEARCH][BUG] struct_fields: a curly brace in a string literal breaks depth → false bool-delta

**Created:** 2026-06-24
**Started:** 2026-06-24
**Closed:** 2026-06-25
**Status:** done
**Module:** RESEARCH / experiments (perstruct_drift.py)
**Priority:** major
**Related:** #135 (bool_field_added — where the bug was caught by an eye-check), #089/#134 (the same parser)

## Symptom

In the eye-check of the 100 findings of #135, one commit (Tencent_KsanaLLM `af9c7e7789`, struct `PrintStepHook`)
produced a false `n_bool_field_added=+1`: the field `print_all_blocks_` existed in BOTH the parent AND the child
(only the logging line changed in the diff), but the sidecar counted it as an "addition".

## Root cause

`struct_fields` (in `experiments/boolean_state/perstruct_drift.py`) counts nesting depth
naively — `depth += line.count('{') - line.count('}')` — and **swallows curly braces inside
string/char literals and comments**. In the parent, the method body contained:

```cpp
ss << ", blocks={ ";   // '{' inside a string literal
```

→ depth got a phantom +1 for the entire rest of the struct → `bool print_all_blocks_;` (a depth-0 field)
was seen at depth 1 → **skipped**. In the child this block-with-the-string was removed → depth correct →
the field was counted. Difference between versions → false `+1`.

**FP class:** a literal/comment with a curly brace in a method body that APPEARS/DISAPPEARS between
versions (for the snapshot analysis of #089 this is just a silent undercount; for the DELTA of #135 — a false finding).

## Fix

Before counting braces, strip string/char literals and `//`-comments from the line:

```python
_LIT = re.compile(r'//.*$|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
clean = _LIT.sub('', line)
depth += clean.count('{') - clean.count('}')
```

The field match (`BOOL_RE`) stays on the ORIGINAL line — the strip is only for counting braces.

**Validation (7 cases):** KsanaLLM `PrintStepHook` 1→**0** (bug killed); 6 references unchanged
(FlashCpp 3, ovn 1, kwin 2, circt 0, intel 1, PQCSettings 181). ✓

**Residual limitation:** block comments `/* { */` spanning several lines are not caught by the fix (rare);
string literals and line comments — the main class — are covered.

## ADDENDUM 2026-06-24 — the first fix was INCOMPLETE (`find_body` also swallowed braces)

While porting it to the C++ rule (#090) it surfaced: the first fix only fixed line-depth, while **`find_body`
(finding the struct body) is also char-level and also swallowed the `{` from the literal**. On BALANCED literals
(KsanaLLM: `"blocks={ "` + `"} "`) `find_body` stayed correct — which is why the first fix "worked".
But an UNBALANCED `{` in a string (`"x={ "` without a matching `}`) breaks `find_body` too → the struct body is
wrong → fields are lost.

**Full fix:** `neutralize_braces(text)` — a state machine that blanks `{`/`}` inside string/char literals
AND comments (incl. block `/* */`), preserving length/lines. Applied to ALL brace operations
(`find_body` + depth) in **both** parsers: C++ `src/scan/bool_field_drift.cpp` and Python
`perstruct_drift.py`. Covers the former residual (block comments) too.

**Quantification on the corpus:** old (line-depth-only) vs full fix diverge on **0.33%** of structs
(~55 findings out of 16.7k). The current corpus run of #135 (PID 1264675) ran on the incomplete fix → carries these 0.33%.
The C++ and Python parsers are now aligned (unbalanced-literal → 0; KsanaLLM 0; FlashCpp 3; PQCSettings 181).

## Impact

- **#135** — the `bool_field_added.jsonl` run is defective (≈1% of findings: 1 of 100 in the eye-check). **Clean restart.**
- **#089/#134** — the same parser; their artifacts were computed before the fix (silent undercount, not false deltas).
  They will become more accurate on a re-run; I am not touching the artifacts on disk, marking them to be recomputed if needed.

## Done
- [x] Cause localized (KsanaLLM PrintStepHook, literal `"blocks={ "`).
- [x] Fix prototyped and validated on 7 cases.
- [x] Fix applied in `perstruct_drift.py` (`_LIT` strip before counting braces). Via the sidecar: PrintStepHook delta 0, commit 44→43, 6 references intact.
- [x] Clean restart of the #135 run (PID 1264675, 5 workers, defective output → `bool_field_added.jsonl.prebugfix_20260624`).
- [x] After the run — the eye-check was taken off by the native run of #090: `results_full.boolrule.jsonl`
      was produced by the already-fixed C++ rule (FP check 22/22 TP, the literal-brace class did not surface).

## OUTCOME (2026-06-25)

The bug is closed in the C++ rule code (#090) and in the Python prototype; it holds on regression tests,
not on manual validation:

- **Fix in C++** (`src/scan/bool_field_drift.cpp`): a brace-neutralized copy of the text (lines 76-94) —
  each `{`/`}` inside a string/char literal or line/block comment is blanked before ALL
  brace operations (`findBody` + depth-tracking), offsets 1:1 with the original, field matching is done
  on the original. Equivalent to the Python `neutralize_braces`.
- **Regression locked in by tests** (green, 25 assertions / 11 cases `[bool_drift]`):
  - unit `tests/unit/scan/bool_field_drift_test.cpp:82` — UNBALANCED `"x={ "` (the very
    residual case from the "ADDENDUM" that broke `findBody`) → no finding;
  - integration `tests/integration/scan/bool_field_drift_fixtures_test.cpp:37` + fixtures
    `fixtures/bool_field_drift/pass/literal_brace_{baseline,current}.h`.
- **Data clean:** the native corpus run of #090 (on which #119 and #134 stand) was done with the already-
  fixed rule → the 0.33% divergence of the incomplete fix did not get into the conclusions.
- The defective Python output of #135 (`bool_field_added.jsonl`) is superseded together with #135.
