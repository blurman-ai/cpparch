# ADR-002: SF.21 (and semantic SF rules) deferred to v0.2+

**Status:** accepted (2026-05-28, task #028; refined by #042/#050)
**Origin:** [backlog/completed/028_maj_rules_engine_mvp.md](../../backlog/completed/028_maj_rules_engine_mvp.md),
[backlog/future/050_min_sf21_anonymous_namespace.md](../../backlog/future/050_min_sf21_anonymous_namespace.md)

## Context

SF.21 (no anonymous namespaces in headers) was listed in the original v0.1 rule set.
A reliable check needs semantic analysis: an approximate text-scan produces false
positives, and false positives in a *default* rule poison trust in the whole tool.

## Decision

SF.21 ships with the libclang backend (preview v0.2 via `--with-clang`, default-ON
considered v0.3). Same reasoning covers SF.2/SF.5/SF.10/SF.11: anything that cannot
be checked precisely on the preprocessor level waits for the semantic backend.

## Rationale

- Default rules carry the product's credibility; a noisy default is worse than an
  absent one (corpus experience: every shipped FP class required a fix + regression
  fixture before users would trust the signal).
- clang-tidy already covers SF.21 (`misc-anonymous-namespace-in-header`) — duplicating
  it imprecisely adds negative value; duplicating it precisely requires the same AST
  cost clang-tidy pays.

## Consequences

- The spec's rule table marks SF.21 as v0.2 preview / v0.3 default-ON.
- The fast backend's rule set stays include/text-only by design — see ADR-003.
