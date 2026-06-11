# Codex task: add `local-complexity-drift` advisory rule to archcheck

## Context

`archcheck` already has or is expected to have tasks for:

- cycles / SCC drift;
- new dependency edges between modules;
- copy-paste / clone drift;
- boolean field growth in structs/classes;
- three recently created cheap-drift tasks.

This task is **only** for a new supporting signal:

> detect local control-flow complexity growth in changed C++ code.

Do not mix this task with flag arguments, parameter accretion, config-bag growth, ownership, hotspots, defect prediction, or full static analysis.

## Why this task exists

Recent AI-assistant adoption research reports a stronger persistent signal in local complexity / static warnings than in duplicated lines. For `archcheck`, this does **not** mean becoming a general code-quality linter.

The product interpretation is narrower:

> local complexity drift can indicate that architectural pressure is leaking into functions/classes as nested branches, special cases, and patch accumulation.

This rule must therefore be implemented as an **advisory drift signal**, not as a default blocking architecture gate.

## Product positioning

This check is not an architecture rule by itself.

Bad positioning:

```text
archcheck fails functions with high cognitive complexity
```

Correct positioning:

```text
archcheck reports local complexity drift around changed code.
It helps reviewers notice when architectural decisions are being encoded as local branching/special cases.
```

Default mode: **advisory only**.

Optional strict mode may later turn selected thresholds into gates, but that is not part of the first implementation unless the project already has a standard `--strict` mechanism.

---

## Rule name

Suggested rule id:

```text
DRIFT.LOCAL_COMPLEXITY
```

Alternative if existing naming conventions differ:

```text
ARCH.DRIFT.LOCAL_COMPLEXITY
```

Use the existing project convention if there is one.

---

## What to measure

Implement a lightweight cognitive-complexity proxy for C++.

Important: this is **not** required to match SonarQube cognitive complexity exactly.

The metric should be deterministic, cheap, stable across runs, and good enough for baseline/current comparison.

### Per-function metrics

For each detected function-like body:

```text
file
function_name_or_signature
start_line
end_line
meaningful_loc
branch_score
max_nesting_depth
deep_lines_count
indent_complexity_sum
local_complexity_score
```

### Per-file metrics

Aggregate:

```text
file
function_count
meaningful_loc
total_local_complexity_score
max_function_score
changed_function_count
changed_functions_complexity_delta
```

---

## Cheap metric definition

### 1. Strip noise first

Before scoring:

- ignore blank lines;
- ignore comments;
- ignore string and char literals when detecting keywords;
- ignore preprocessor-only lines for branch scoring;
- count only meaningful code lines for LOC-like metrics.

Do not require compilation, compile database, libclang, or preprocessing through a real compiler.

### 2. Approximate function boundaries

Use the project’s existing lightweight tokenizer/scanner if available.

If not available, implement a conservative scanner:

- detect likely function definitions by signature followed by `{`;
- exclude obvious control blocks: `if (...) {`, `for (...) {`, `while (...) {`, `switch (...) {`, `catch (...) {`;
- support namespaces/classes by brace tracking;
- support methods, free functions, constructors, destructors, lambdas if reasonably cheap;
- if a body cannot be classified as a function, allow file-level scoring fallback.

Do not try to perfectly parse C++.

### 3. Branch tokens

Within each function body, count branch/control-flow tokens:

```cpp
if
else if
for
while
do
switch
catch
?:
&&
||
break
continue
goto
co_await
```

Optional, if cheap and reliable:

```cpp
case
default
return inside nested branch
```

Caution: `case` can inflate score heavily. If included, keep its weight low or make it configurable.

### 4. Nesting

Track approximate nesting by braces.

For each branch token that starts a structured block:

```text
score += 1 + current_control_nesting_depth
```

For logical operators and jump tokens:

```text
score += 1
```

For lines deeper than a configured nesting threshold:

```text
deep_lines_count += 1
```

Recommended default:

```text
deep_line_threshold = 3
```

### 5. Indentation proxy

Also compute indentation-based proxy, because it is cheaper and robust enough for drift:

```text
indent_level = leading_spaces / indent_width
indent_complexity_sum += indent_level for each meaningful line
max_indent_level = max(indent_level)
deep_lines_count = count(indent_level >= deep_line_threshold)
```

Default:

```text
indent_width = 4
```

Tabs:

- either count tab as `indent_width`;
- or preserve existing project convention.

### 6. Combined score

Initial simple formula:

```text
local_complexity_score =
    branch_score
  + deep_lines_count
  + floor(indent_complexity_sum / 20)
```

This formula is intentionally simple. If the repo already has metric infrastructure, keep the formula separate/configurable.

---

## What to compare

This is a drift rule. It must compare **baseline vs current**, not just absolute complexity.

### Required comparisons

For changed files/functions:

```text
old_score
new_score
delta_score
delta_percent
meaningful_loc_delta
```

Report only functions/files touched by the diff unless the existing architecture supports whole-repo reports.

### Advisory findings

Emit a finding when any of these is true:

```text
new function local_complexity_score >= new_function_threshold

existing function local_complexity_score increased by >= delta_score_threshold

existing function local_complexity_score increased by >= delta_percent_threshold
AND meaningful_loc_delta >= min_loc_delta

file total_local_complexity_score increased by >= file_delta_score_threshold
AND file meaningful_loc_delta >= min_file_loc_delta
```

Recommended first defaults:

```yaml
local_complexity:
  enabled: true
  mode: advisory
  new_function_threshold: 20
  delta_score_threshold: 8
  delta_percent_threshold: 50
  min_loc_delta: 20
  file_delta_score_threshold: 25
  min_file_loc_delta: 50
  deep_line_threshold: 3
  indent_width: 4
```

These thresholds are guesses. Keep them configurable.

---

## When to run this check in the pipeline

This check should run **early**, because it does not need expensive project analysis.

Recommended pipeline stage:

```text
1. Collect baseline/current revisions
2. Extract changed files and diff hunks
3. Load text content for relevant C/C++ files
4. Strip comments/strings and tokenize lightly
5. Compute local-complexity metrics
6. Compare baseline/current
7. Emit advisory findings
8. Continue to include graph / SCC / module-edge checks
9. Continue to clone detection
```

So this check should run:

```text
after diff extraction
before include graph
before clone detection
before any libclang-based analysis
```

Reason:

- no compile database needed;
- fast feedback;
- useful even when include graph cannot be built;
- can be used as a supporting signal next to architectural findings.

---

## Output format

Use the existing report/finding format.

Each finding should include:

```text
rule_id
severity/advisory
file
function/signature if known
old_score
new_score
delta_score
delta_percent
start_line
end_line
short explanation
```

Example human-readable message:

```text
DRIFT.LOCAL_COMPLEXITY advisory:
VehicleController::update grew local complexity from 9 to 23 (+14, +155%).
The changed function added nested control-flow/special cases.
This is not an architecture violation by itself, but may indicate local patch accumulation.
```

Example JSON fields:

```json
{
  "rule_id": "DRIFT.LOCAL_COMPLEXITY",
  "level": "advisory",
  "file": "src/VehicleController.cpp",
  "symbol": "VehicleController::update",
  "start_line": 120,
  "end_line": 214,
  "old_score": 9,
  "new_score": 23,
  "delta_score": 14,
  "delta_percent": 155.6,
  "meaningful_loc_delta": 42,
  "reason": "local complexity drift in changed function"
}
```

---

## Integration with architecture findings

If possible, cross-reference this advisory with existing architecture drift findings.

Examples:

```text
same PR introduced new dependency edge + local complexity increased in touched adapter/controller file
same class had boolean-field growth + local complexity increased
same module participated in new cycle + local complexity increased
```

Do not make this mandatory for v1.

A simple future extension point is enough:

```text
finding.tags = ["local-complexity", "supporting-signal"]
```

Optional tags if correlated later:

```text
["near-new-edge"]
["near-state-accretion"]
["near-cycle-growth"]
```

---

## CLI / configuration

Follow existing CLI/config conventions.

Possible options:

```bash
archcheck ... --enable-local-complexity
archcheck ... --local-complexity-threshold 20
archcheck ... --local-complexity-delta 8
archcheck ... --local-complexity-mode advisory
```

But prefer config-file integration if the project already centralizes rules in YAML.

Suggested YAML:

```yaml
rules:
  local_complexity_drift:
    enabled: true
    mode: advisory
    new_function_threshold: 20
    delta_score_threshold: 8
    delta_percent_threshold: 50
    min_loc_delta: 20
    file_delta_score_threshold: 25
    min_file_loc_delta: 50
    deep_line_threshold: 3
    indent_width: 4
```

Default should be safe:

```text
enabled: false
```

unless the project already has an advisory-only default report section.

If enabled by default, it must not fail CI.

---

## Tests to add

### Unit tests for scoring

Create small C++ snippets and expected relative scores.

#### Flat branches should score lower than nested branches

Input A:

```cpp
void f() {
    if (a) doA();
    if (b) doB();
    if (c) doC();
}
```

Input B:

```cpp
void f() {
    if (a) {
        if (b) {
            if (c) {
                doC();
            }
        }
    }
}
```

Expected:

```text
score(B) > score(A)
```

#### Added branch increases score

Baseline:

```cpp
void f() {
    work();
}
```

Current:

```cpp
void f() {
    if (enabled) {
        work();
    }
}
```

Expected:

```text
delta_score > 0
```

#### Comments and strings must not create fake branches

Input:

```cpp
void f() {
    // if while for switch
    const char* s = "if while for";
    work();
}
```

Expected:

```text
branch_score == 0
```

#### Preprocessor conditionals

Input:

```cpp
void f() {
#ifdef X
    work();
#endif
}
```

Expected:

```text
preprocessor lines are not counted as branch tokens
```

This can be changed later, but first implementation should avoid noisy false positives.

---

### Drift tests

#### Existing function growth

Baseline:

```cpp
void update() {
    run();
}
```

Current:

```cpp
void update() {
    if (a) {
        for (;;) {
            if (b) {
                run();
            }
        }
    }
}
```

Expected:

```text
DRIFT.LOCAL_COMPLEXITY finding emitted
old_score < new_score
```

#### New complex function

Current-only:

```cpp
void newFunction() {
    if (a) {
        while (b) {
            if (c) {
                if (d) {
                    work();
                }
            }
        }
    }
}
```

Expected:

```text
finding emitted if score >= new_function_threshold
```

#### Small harmless change should not report

Baseline:

```cpp
void f() {
    run();
}
```

Current:

```cpp
void f() {
    run();
    log();
}
```

Expected:

```text
no finding
```

---

### Report tests

Verify report contains:

```text
rule id
file path
function name if detected
old score
new score
delta
advisory level
```

---

## Performance expectations

This rule must be cheap.

Target:

```text
O(total bytes of changed C/C++ files in baseline + current)
```

No recursive include expansion.

No AST.

No compiler invocation.

No dependency on SonarQube.

No dependency on libclang.

---

## Non-goals

Do not implement:

- exact Sonar cognitive complexity compatibility;
- full C++ parser;
- full function symbol resolver;
- global project complexity score as a gate;
- ownership/knowledge-risk analysis;
- defect prediction;
- generic “code quality” linting;
- automatic refactoring suggestions.

Do not fail CI by default.

---

## Acceptance criteria

Task is complete when:

1. `archcheck` can compute local-complexity proxy metrics for changed C/C++ files.
2. It can compare baseline/current scores.
3. It emits advisory findings for significant local complexity growth.
4. It does not require compile database, libclang, SonarQube, or include graph.
5. Findings appear in the normal report/JSON output.
6. Tests cover nested vs flat branches, comments/strings, drift detection, and no-finding harmless changes.
7. Documentation clearly states that this is a supporting signal, not a default architecture gate.

---

## Suggested implementation order

1. Add text/comment/string stripping helper or reuse existing one.
2. Add lightweight function boundary scanner or reuse existing tokenizer.
3. Implement per-function scoring.
4. Add baseline/current comparison.
5. Add rule config and default thresholds.
6. Emit advisory findings.
7. Add tests.
8. Add docs section: `local-complexity-drift`.

---

## Reviewer notes

Be skeptical of false precision.

This rule should not claim:

```text
This function has Sonar cognitive complexity 23
```

It should claim:

```text
archcheck local complexity proxy grew from 9 to 23
```

The useful property is stable drift detection, not exact theoretical correctness.
