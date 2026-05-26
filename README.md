# cpparch

Architecture testing and dependency rules for C++ projects.

## Why

C++ projects degrade over time:

- layers stop being layers
- modules start depending on everything
- include graph turns into spaghetti

Code review doesn’t catch this.  
Linters don’t check architecture.  
AI-generated code makes it worse.

**cpparch enforces architectural rules in CI.**

---

## What it does

- Parses `compile_commands.json`
- Builds dependency graph (includes, modules)
- Applies declarative rules (YAML)
- Reports violations with exact `file:line:column`
- Fails CI on violations

---

## Example

### Config (`arch.yaml`)

```yaml
modules:
  domain:
    match:
      path: src/domain/**

  infrastructure:
    match:
      path: src/infrastructure/**

rules:
  - id: domain-no-infra
    from: domain
    forbid:
      - infrastructure

  - id: no-cycles
    check: no_cycles
```

---

### Run

```bash
archcheck check --config arch.yaml
```

---

### Output

```text
error[domain-no-infra]: forbidden dependency domain -> infrastructure
  src/domain/order.cpp:12:10
  include: "../infrastructure/sql_repository.hpp"
```

---

## Key Features

- CI-first — non-zero exit on violations
- Deterministic — same input → same result
- No magic — explicit config
- Baseline support — freeze existing violations
- Zero-config mode — useful defaults

---

## CLI

```bash
archcheck check --config arch.yaml

archcheck init > arch.yaml

archcheck baseline create \
  --config arch.yaml \
  --output archcheck-baseline.json

archcheck check \
  --config arch.yaml \
  --baseline archcheck-baseline.json
```

---

## Output formats

- text
- json
- sarif

---

## What it is NOT

- Not a linter
- Not a bug finder
- Not a formatter
- Not include optimizer

cpparch checks architecture only.

---

## Status

Early development (MVP).

---

## License

Apache License 2.0

---

## Principle

If it cannot be validated with tests — it should not exist.
