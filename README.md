# archcheck

[![CI](https://github.com/blurman-ai/cpparch/actions/workflows/ci.yml/badge.svg)](https://github.com/blurman-ai/cpparch/actions/workflows/ci.yml)

Architecture testing and dependency rules for C++ projects.

## Why

C++ projects degrade over time:

- layers stop being layers
- modules start depending on everything
- include graph turns into spaghetti

Code review doesn’t catch this.  
Linters don’t check architecture.  
**AI-generated code accelerates the drift** — agents suffer *constraint decay* (EURECOM, 2026): ~30 pp drop in assertion pass rate when functional tasks gain structural constraints. The prompt degrades with context; CI doesn’t.

**archcheck enforces architectural rules in CI.**

---

## What it does

- Scans sources via a fast preprocessor pass (or libclang with `--with-clang` for semantic checks)
- Builds the dependency graph (includes → modules)
- Applies declarative rules from a YAML config
- Reports violations with exact `file:line:column`
- Fails CI on violations (non-zero exit)
- Supports `--baseline` from day one: freeze legacy violations, fail only on new ones

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

- **CI-first** — non-zero exit on violations
- **Baseline-friendly** — freeze legacy, fail on new
- **No setup required** — works without `compile_commands.json` (fast backend by default)
- **Deterministic** — same input → same result
- **Sourced** — every default rule cites a published authority (Core Guidelines, Lakos, Martin)

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

archcheck checks architecture only.

---

## Status

Early development (MVP).

---

## Secondary goal

Side experiment: test whether a useful, reliable, moderately complex CLI tool can be built end-to-end purely through agent conversation — no hand-written code.

---

## License

Apache License 2.0

---

## Principle

If it cannot be validated with tests — it should not exist.
