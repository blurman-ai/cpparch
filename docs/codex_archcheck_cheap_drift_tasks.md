# Codex tasks: cheap drift checks for archcheck

_Date: 2026-06-10_

## Context

`archcheck` already has the following checks:

- include/module graph analysis;
- cycle / SCC detection;
- new module dependency detection;
- clone / copy-paste detection;
- boolean field growth in structs/classes.

This task file asks Codex to create new project tasks for the next low-cost drift checks.

The common product rule:

> Blocking checks must focus on **new drift against baseline / PR diff**, not on punishing the whole legacy codebase.

The common implementation rule:

> Prefer cheap deterministic analysis: `git diff`, `git log --numstat`, token scan, text scan. Do not introduce full C++ compilation or mandatory libclang dependency for these tasks.

The common output rule:

- every check must emit machine-readable output;
- every finding must include file, line/range where possible, rule id, severity, reason, and suggested next action;
- every check must support:
  - full scan mode;
  - baseline mode;
  - diff/PR mode.

---

# Stages in archcheck pipeline

Codex must classify every new check into one of these stages.

## Stage 0 — Source discovery

Input:

- repository root;
- include/exclude globs;
- language filters;
- project config.

Used by:

- all text/token checks.

No finding should be emitted here.

## Stage 1 — Text/token scan

Input:

- files discovered in Stage 0.

Used for:

- flag arguments;
- parameter count;
- config bag growth;
- SATD markers;
- indentation complexity.

This stage must not require compilation.

## Stage 2 — Git diff scan

Input:

- current tree;
- baseline ref;
- changed files;
- changed line ranges.

Used for:

- new-code gating;
- new SATD;
- new flag arguments;
- parameter accretion in changed functions;
- test co-evolution.

This is the main PR/CI stage.

## Stage 3 — Git history scan

Input:

- `git log --numstat`;
- configurable time window or commit count;
- optional grep patterns for fix commits.

Used for:

- god-file-in-the-making;
- defect-attractor;
- longer-window config/parameter growth.

This should be report/advisory by default.

## Stage 4 — Baseline comparison

Input:

- previous baseline JSON;
- current scan JSON.

Used for:

- new violations only;
- drift deltas;
- suppressing legacy noise.

Every gate-capable rule must pass through this stage.

## Stage 5 — Policy evaluation

Input:

- findings from previous stages;
- project config;
- default severity.

Decides:

- gate;
- advisory;
- report only;
- ignored by config.

## Stage 6 — Output

Required formats:

- human CLI output;
- JSON output;
- CI-friendly exit code.

Exit code must be non-zero only for enabled gate violations.

---

# Task 1 — `flag-argument`

## Goal

Detect C++ APIs and call sites that encode behavior through boolean flags instead of explicit types, enums, or separate functions.

This extends the existing boolean-field drift idea from fields to interfaces.

## Motivation

Boolean flags in APIs are cheap to add and expensive to understand later. They often indicate that implicit state or mode selection is leaking into function signatures.

Examples:

```cpp
void set_mode(bool safe, bool verbose);
void render(Scene& scene, bool wireframe, bool shadows);
foo(true, false);
foo(true, false, true);
```

## What to detect

### Declaration/signature findings

Detect function declarations/definitions with:

- `>= 2` parameters of type `bool`; or
- `>= 1` `bool` parameter whose name looks like a mode flag:
  - `enable*`;
  - `disable*`;
  - `use*`;
  - `is*`;
  - `has*`;
  - `with*`;
  - `without*`;
  - `force*`;
  - `skip*`;
  - `no*`;
  - `do*`.

Initial threshold:

```yaml
flag_argument:
  max_bool_params: 1
  suspicious_name_patterns:
    - "^enable"
    - "^disable"
    - "^use"
    - "^is"
    - "^has"
    - "^with"
    - "^without"
    - "^force"
    - "^skip"
    - "^no"
    - "^do"
```

### Call-site findings

Detect calls with:

- `>= 2` boolean literals in one call;
- adjacent boolean literals;
- boolean literals mixed with numeric magic values.

Examples:

```cpp
set_settings(true, false, 42);
render(scene, false, true);
create_user(name, true, false, false);
```

## How to implement cheaply

Use token scan, not full parser.

Approximate enough for first version:

1. Strip comments and string literals.
2. Tokenize identifiers, punctuation, parentheses, commas, templates approximately.
3. Detect likely function declarations:
   - identifier followed by `(` parameter list `)`;
   - parameter list contains `bool` tokens;
   - exclude obvious control flow keywords: `if`, `while`, `for`, `switch`, `catch`.
4. Detect likely call sites:
   - identifier followed by `(` argument list `)`;
   - argument list contains `true`/`false`.

False positives are acceptable in advisory mode, but gate mode must apply only to changed lines or new baseline deltas.

## Stage

- Stage 1: token scan.
- Stage 2: diff filtering for PR mode.
- Stage 4: baseline comparison.
- Stage 5: policy evaluation.

## Default severity

- Legacy/full scan: `advisory`.
- New declaration in changed lines: `gate_candidate`.
- New call with `>= 2` boolean literals in changed lines: `gate_candidate`.
- Actual default CI gate: off unless config enables it.

## Suggested rule ids

- `ARG.1.flag_argument_signature`
- `ARG.2.boolean_literal_call`

## Suggested message

```text
ARG.1 flag argument: function has 3 bool parameters. Consider enum, options type, or separate named functions.
```

```text
ARG.2 boolean literal call: call uses multiple true/false arguments. Consider named constants, enum, or typed options.
```

## Acceptance tests

Create fixtures:

```cpp
void ok(int a, bool enabled);                  // no finding by default
void bad(bool safe, bool verbose);             // finding
void bad2(bool enable_cache, bool force_sync); // finding
foo(true);                                     // no finding by default
foo(true, false);                              // finding
foo(x, true, false, 42);                       // finding
if (ok && bad) {}                              // must not be parsed as call finding
```

Required tests:

- full scan detects signature and call-site findings;
- diff mode reports only changed-line findings;
- baseline mode suppresses legacy findings;
- JSON output contains rule id, file, line, severity.

---

# Task 2 — `param-count` and `param-accretion`

## Goal

Detect functions whose signatures grow too wide or keep gaining parameters over time.

This is a cheap proxy for abstraction decay: instead of introducing a better object/model, code keeps extending function signatures.

## What to detect

### Absolute parameter count

Detect new or changed functions with parameter count above threshold.

Initial threshold:

```yaml
param_count:
  max_params: 4
```

Examples:

```cpp
void create_window(int x, int y, int w, int h, bool fullscreen, int monitor);
```

### Parameter accretion

Detect existing functions whose parameter count increases by `>= N` compared to baseline.

Initial threshold:

```yaml
param_accretion:
  max_added_params: 1
```

Example:

Baseline:

```cpp
void connect(Address address, Timeout timeout);
```

Current:

```cpp
void connect(Address address, Timeout timeout, bool retry, bool secure);
```

Finding:

```text
+2 parameters since baseline
```

## How to implement cheaply

Use the same approximate token scanner as `flag-argument`.

For each likely function declaration/definition:

- compute stable function key:
  - normalized file path;
  - function name;
  - optional enclosing class/namespace if cheap;
  - normalized parameter type count;
- store:
  - line;
  - name;
  - parameter count;
  - parameter types/names if available.

For baseline comparison:

- compare function key by name and approximate location;
- if exact key changed because params changed, match by file + function name + nearest old line.
- Keep implementation simple; mark ambiguous matches as advisory only.

## Stage

- Stage 1: token scan.
- Stage 2: diff filtering.
- Stage 4: baseline comparison.
- Stage 5: policy evaluation.

## Default severity

- New function with `> 4` params: `advisory`.
- Existing function gained `>= 2` params in PR: `gate_candidate`.
- Gate disabled by default until fixtures prove low noise.

## Suggested rule ids

- `ARG.3.long_parameter_list`
- `ARG.4.parameter_accretion`

## Suggested message

```text
ARG.3 long parameter list: function has 7 parameters, threshold is 4.
```

```text
ARG.4 parameter accretion: function gained 2 parameters since baseline.
```

## Acceptance tests

Fixtures:

```cpp
void ok1(int a, int b, int c, int d);
void bad1(int a, int b, int c, int d, int e);

class C {
public:
    void bad2(int a, int b, int c, int d, int e, int f);
};
```

Baseline/current pair:

```cpp
// baseline
void update(int id, int flags);

// current
void update(int id, int flags, bool force, int timeout);
```

Required tests:

- detects long parameter list;
- detects parameter growth against baseline;
- does not count `void` as one parameter;
- handles default arguments approximately;
- handles function pointer false positives reasonably or suppresses them.

---

# Task 3 — `config-bag-growth`

## Goal

Detect structs/classes that become dumping grounds for configuration, options, settings, or parameters.

This extends boolean field growth from `bool` only to broader config/state accumulation.

## What to detect

Candidate type names:

- `*Config`
- `*Configuration`
- `*Options`
- `*Settings`
- `*Params`
- `*Parameters`
- `*Context`
- `*State` — optional, disabled by default because it may be legitimate.

For each candidate struct/class, detect:

1. too many fields;
2. too many new fields since baseline;
3. consecutive growth without field removals over history.

Initial config:

```yaml
config_bag_growth:
  type_name_patterns:
    - ".*Config$"
    - ".*Configuration$"
    - ".*Options$"
    - ".*Settings$"
    - ".*Params$"
    - ".*Parameters$"
    - ".*Context$"
  max_fields: 20
  max_added_fields_since_baseline: 3
  enable_history_growth: false
  history_window_commits: 50
  max_consecutive_growth_commits: 4
```

## Examples

```cpp
struct RenderOptions {
    bool shadows;
    bool wireframe;
    int samples;
    float gamma;
    std::string profile;
    // keeps growing every PR
};
```

## How to implement cheaply

Use approximate struct/class scanner:

1. Find `struct|class <Name> { ... };`.
2. For candidate names, count likely fields:
   - lines ending with `;`;
   - exclude methods by skipping lines containing `(` before `;`;
   - exclude `using`, `typedef`, `static_assert`, `friend`.
3. Store:
   - type name;
   - file;
   - start/end lines;
   - field count;
   - field names if cheap.

Baseline comparison:

- match by file + type name;
- compare field count and field-name set if available.

History mode:

- use `git log -p` only if cheap enough, or skip in first implementation;
- first version can do only baseline/diff.

## Stage

- Stage 1: token/text scan.
- Stage 2: diff filtering.
- Stage 3: optional history growth, later.
- Stage 4: baseline comparison.
- Stage 5: policy evaluation.

## Default severity

- `max_fields` exceeded: `advisory`.
- `+3` fields since baseline: `advisory`.
- `+5` fields since baseline: `gate_candidate`, disabled by default.
- History growth: report only.

## Suggested rule ids

- `CFG.1.config_bag_size`
- `CFG.2.config_bag_accretion`

## Suggested message

```text
CFG.2 config bag growth: RenderOptions gained 5 fields since baseline. Consider splitting options or making state explicit.
```

## Acceptance tests

Fixtures:

```cpp
struct RenderOptions {
    bool shadows;
    bool wireframe;
    int samples;
    float gamma;
    std::string profile;
};
```

Baseline/current pair:

```cpp
// baseline
struct NetworkConfig {
    int port;
    int timeout;
};

// current
struct NetworkConfig {
    int port;
    int timeout;
    bool retry;
    bool tls;
    std::string cert;
    int pool_size;
};
```

Required tests:

- detects candidate names;
- ignores non-candidate structs by default;
- detects added fields against baseline;
- handles public/private labels;
- does not count methods as fields.

---

# Task 4 — `satd-delta`

## Goal

Detect new self-admitted technical debt markers introduced by a PR.

This is not an architecture violation by itself. It is a cheap warning that the PR adds known unfinished work.

## What to detect

New changed lines containing:

- `TODO`
- `FIXME`
- `HACK`
- `XXX`
- `TEMP`
- `temporary`
- `workaround`
- `quick fix`
- `dirty`

Initial config:

```yaml
satd_delta:
  markers:
    - TODO
    - FIXME
    - HACK
    - XXX
    - TEMP
    - temporary
    - workaround
    - quick fix
    - dirty
  require_issue_for:
    - FIXME
    - HACK
  issue_pattern: "(#\\d+|[A-Z]+-\\d+)"
```

## How to implement cheaply

Use diff-only grep.

Important: this rule should operate on **added lines only** in PR mode.

Do not scan whole legacy code for gate.

## Stage

- Stage 2: git diff scan.
- Stage 5: policy evaluation.
- Stage 6: output.

No baseline needed for basic PR mode.

## Default severity

- New `TODO`: `advisory`.
- New `FIXME`/`HACK` without issue id: `gate_candidate`.
- Actual gate disabled by default.

## Suggested rule ids

- `SATD.1.new_satd_marker`
- `SATD.2.untracked_fixme_or_hack`

## Suggested message

```text
SATD.1 new technical-debt marker: TODO introduced in changed line.
```

```text
SATD.2 untracked FIXME/HACK: marker has no issue id.
```

## Acceptance tests

Diff fixture:

```diff
+ // TODO: clean this later
+ // FIXME: crashes sometimes
+ // HACK ABC-123: workaround for driver bug
```

Expected:

- TODO advisory;
- FIXME without issue: finding;
- HACK with issue id: no `SATD.2`, but may still produce `SATD.1`.

---

# Task 5 — `test-co-evolution`

## Goal

Warn when a PR changes production code significantly but does not change tests.

This is a review-prioritization check, not a hard architecture rule.

## What to detect

For a PR/diff:

- production code churn above threshold;
- test churn is zero or very low.

Initial config:

```yaml
test_co_evolution:
  production_paths:
    - "src/**"
    - "lib/**"
    - "include/**"
  test_paths:
    - "test/**"
    - "tests/**"
    - "*test*"
    - "*Test*"
  min_production_churn: 50
  min_test_churn_ratio: 0.05
```

Example finding:

```text
src churn: 820 lines
test churn: 0 lines
```

## How to implement cheaply

Use:

```bash
git diff --numstat <baseline>...HEAD
```

Compute:

- added + deleted production lines;
- added + deleted test lines;
- ratio.

Ignore:

- docs-only PRs;
- comment-only changes if available from scanner;
- generated files by config;
- file moves/renames if detectable.

## Stage

- Stage 2: git diff scan.
- Stage 5: policy evaluation.
- Stage 6: output.

## Default severity

- Always `advisory`.
- Gate only if user explicitly configures it.

## Suggested rule ids

- `TEST.1.production_change_without_tests`

## Suggested message

```text
TEST.1 production code changed without matching test changes: production churn 820, test churn 0.
```

## Acceptance tests

Create fake diff numstat fixtures:

```text
100 20 src/engine.cpp
50  10 include/engine.hpp
0   0  README.md
```

Expected:

- advisory finding.

With tests:

```text
100 20 src/engine.cpp
30  5  tests/engine_test.cpp
```

Expected:

- no finding if ratio passes threshold.

---

# Task 6 — `god-file-in-the-making`

## Goal

Detect files that are steadily growing over several commits and are likely becoming accumulation points.

This is a cheap early warning for future God files.

## What to detect

A file is suspicious if:

- it is already large, or above project percentile;
- it has grown in several consecutive touching commits;
- net growth exceeds threshold over a window;
- there were no meaningful reductions.

Initial config:

```yaml
god_file_growth:
  history_window_commits: 100
  min_current_loc: 500
  min_net_growth_lines: 200
  min_consecutive_growth_touches: 4
  ignore_paths:
    - "third_party/**"
    - "generated/**"
```

## How to implement cheaply

Use:

```bash
git log --numstat -- <path>
```

or one repository-wide `git log --numstat` pass.

Compute per file:

- current LOC;
- net added/deleted over window;
- number of touching commits;
- consecutive growth touches;
- growth ratio.

Do not require parsing C++.

## Stage

- Stage 3: git history scan.
- Stage 5: policy evaluation.
- Stage 6: output.

## Default severity

- `report_only` / `advisory`.
- Never gate by default.

## Suggested rule ids

- `SIZE.1.god_file_growth`

## Suggested message

```text
SIZE.1 god-file-in-the-making: file grew by 640 lines over 8 touches and is now 2200 LOC.
```

## Acceptance tests

Use synthetic git numstat fixture:

```text
commit A
100 0 src/big.cpp

commit B
80 0 src/big.cpp

commit C
120 0 src/big.cpp

commit D
90 0 src/big.cpp
```

Expected:

- finding if thresholds match.

Also test:

```text
commit A +100
commit B -120
commit C +20
```

Expected:

- no consecutive-growth finding.

---

# Task 7 — `indentation-complexity-drift`

## Goal

Detect cheap signs that code became more nested or branch-heavy without parsing C++.

This is a rough complexity proxy and must not be treated as a precise metric.

## What to detect

For each changed file or function-like region:

- max indentation increased;
- average indentation increased;
- indentation variance increased;
- count of deeply indented lines increased.

Initial config:

```yaml
indentation_complexity_drift:
  tab_width: 4
  deep_indent_columns: 16
  max_variance_increase_ratio: 0.30
  max_deep_line_increase: 20
```

## How to implement cheaply

Text scan:

1. For each non-empty, non-comment-only line, compute leading indentation columns.
2. Per file:
   - max indent;
   - average indent;
   - variance;
   - count of lines with indent >= threshold.
3. Compare baseline/current.

Optional later:

- compute per function-like region using brace ranges.

## Stage

- Stage 1: text scan.
- Stage 2: diff filtering optional.
- Stage 4: baseline comparison.
- Stage 5: policy evaluation.
- Stage 6: output.

## Default severity

- `advisory`.
- Never gate by default.

## Suggested rule ids

- `CMP.1.indentation_complexity_drift`

## Suggested message

```text
CMP.1 indentation complexity drift: deep-indented lines increased from 12 to 45 since baseline.
```

## Acceptance tests

Baseline/current fixture:

```cpp
// baseline
void f() {
    if (a) {
        work();
    }
}
```

```cpp
// current
void f() {
    if (a) {
        if (b) {
            for (;;) {
                if (c) {
                    work();
                }
            }
        }
    }
}
```

Expected:

- finding for increased deep indentation.

---

# Task 8 — `defect-attractor`

## Goal

Identify files that repeatedly receive bug-fix commits.

This is not an architecture violation. It is a review prioritization signal.

## What to detect

A file becomes a defect-attractor if it appears frequently in commits whose messages match fix patterns.

Initial config:

```yaml
defect_attractor:
  history_window_commits: 500
  fix_patterns:
    - "\\bfix\\b"
    - "\\bbug\\b"
    - "\\bcrash\\b"
    - "\\bregression\\b"
    - "\\bhotfix\\b"
    - "\\bsegfault\\b"
    - "\\bassert\\b"
  min_fix_commits_per_file: 3
  top_percentile: 90
```

## How to implement cheaply

Use:

```bash
git log --numstat --pretty=format:
```

For each commit:

- classify commit as fix-like by message regex;
- map changed files;
- increment fix count per file.

Output ranked list.

## Stage

- Stage 3: git history scan.
- Stage 5: policy evaluation.
- Stage 6: output.

## Default severity

- `report_only`.
- Never gate.

## Suggested rule ids

- `HIST.1.defect_attractor`

## Suggested message

```text
HIST.1 defect attractor: file was touched by 7 fix-like commits in the last 500 commits.
```

## Acceptance tests

Synthetic log fixture:

```text
fix crash in parser
src/parser.cpp

fix regression in parser
src/parser.cpp

bug in parser state
src/parser.cpp
```

Expected:

- `src/parser.cpp` reported.

---

# Priority order

Codex should create project tasks in this order.

## P0 — implement now

1. `flag-argument`
2. `param-count` / `param-accretion`
3. `config-bag-growth`

Reason:

These three are conceptually close to the existing boolean-field growth check. Together they form a coherent low-cost package:

> interface/config/state accretion

## P1 — implement after P0

4. `satd-delta`
5. `test-co-evolution`

Reason:

Very cheap, useful in PR review, but less specifically architectural.

## P2 — report-only analytics

6. `god-file-in-the-making`
7. `indentation-complexity-drift`
8. `defect-attractor`

Reason:

Useful as advisory reports, but too probabilistic for blocking gates.

---

# Required config shape

Add all rules under a new `cheap_drift` or equivalent section.

Example:

```yaml
cheap_drift:
  flag_argument:
    enabled: true
    mode: advisory
    max_bool_params: 1

  param_count:
    enabled: true
    mode: advisory
    max_params: 4

  param_accretion:
    enabled: true
    mode: advisory
    max_added_params: 1

  config_bag_growth:
    enabled: true
    mode: advisory
    max_fields: 20
    max_added_fields_since_baseline: 3

  satd_delta:
    enabled: true
    mode: advisory

  test_co_evolution:
    enabled: true
    mode: advisory

  god_file_growth:
    enabled: false
    mode: report

  indentation_complexity_drift:
    enabled: false
    mode: report

  defect_attractor:
    enabled: false
    mode: report
```

Modes:

- `off`
- `report`
- `advisory`
- `gate`

Definitions:

- `report`: appears only in report output, no warning status.
- `advisory`: appears in normal CLI/CI output, but exit code stays 0.
- `gate`: can fail CI if finding is in new/diff/baseline delta scope.

---

# Required output shape

JSON finding example:

```json
{
  "rule_id": "ARG.2.boolean_literal_call",
  "severity": "advisory",
  "mode": "advisory",
  "file": "src/render.cpp",
  "line": 42,
  "symbol": "render",
  "message": "Call uses multiple true/false arguments.",
  "details": {
    "bool_literals": 2,
    "threshold": 1,
    "stage": "diff"
  },
  "suggestion": "Consider enum, named constants, typed options, or separate functions."
}
```

CLI example:

```text
ARG.2 advisory src/render.cpp:42 render()
  Boolean literal call: 2 bool literals in one call.
  Suggestion: use enum, named constants, typed options, or separate functions.
```

---

# Required documentation

Codex must create or update documentation explaining:

1. These checks are cheap drift heuristics, not proof of bugs.
2. Only new/delta findings should be used as gates by default.
3. Legacy findings should be advisory/report only.
4. The checks are designed to catch erosion patterns early:
   - interface accretion;
   - config accretion;
   - implicit state accretion;
   - untested production churn;
   - growing complexity hotspots.

Suggested docs file:

```text
docs/cheap-drift-checks.md
```

---

# Non-goals

Do not implement now:

- full C++ parser;
- mandatory libclang;
- SZZ;
- ML defect prediction;
- AI-authorship detection;
- full CodeScene clone;
- full ownership/knowledge map;
- project-wide quality score.

The goal is a small set of deterministic, explainable, cheap checks that can run in CI and produce useful PR feedback.
