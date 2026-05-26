# cpparch — MVP Scope

## Goal

Deliver a minimal but useful architecture checker for C++ projects.

MVP must:
- work on real projects
- be usable in CI
- provide immediate value without complex setup

---

## Non-Goals (MVP)

- No AST-based rules
- No plugin system
- No visualization
- No auto architecture inference
- No IDE integrations

---

## Core Features

### 1. compile_commands.json support

- Read compilation database
- Resolve include paths
- Process translation units

---

### 2. Include Graph

- Build directed graph of includes
- Node = file
- Edge = #include

---

### 3. Cycle Detection

- Detect cycles in include graph
- Report exact chain

Example:
A.h → B.h → C.h → A.h

---

### 4. Module Mapping

- Map files → modules via path patterns

```yaml
modules:
  domain:
    match:
      path: src/domain/**
```

---

### 5. Dependency Rules

Support only simple rules:

```yaml
- id: domain-no-infra
  from: domain
  forbid:
    - infrastructure
```

---

### 6. CLI

```bash
archcheck check --config arch.yaml
archcheck init
```

---

### 7. Output

#### Text

- Human readable
- file:line:column

#### JSON

- Stable schema
- Used for CI

---

### 8. Exit Codes

- 0 — OK
- 1 — violations
- 2 — config error
- 3 — internal error

---

### 9. Fixtures (mandatory)

Each rule must have:

- pass case
- fail case

Structure:

```
fixtures/
  no_cycles/
    pass/
    fail_simple/
```

---

## Minimal Architecture

```
parser → graph → rules → reporter
```

Modules:

- parser (compile_commands, includes)
- graph (dependency graph)
- rules (evaluation)
- reporter (output)

---

## Implementation Order

1. compile_commands reader
2. include graph
3. cycle detection
4. module mapping
5. dependency rules
6. CLI
7. text output
8. JSON output
9. fixtures

---

## Acceptance Criteria

MVP is done if:

- detects cycles in real project
- enforces 1 dependency rule
- outputs correct file locations
- runs in CI
- has fixtures for all rules

---

## Success Condition

User can:

```bash
archcheck check --config arch.yaml
```

and get actionable violations on real code.

---

## Constraint

If feature cannot be tested with fixtures — do not implement it.
