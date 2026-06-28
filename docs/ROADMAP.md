# archcheck — ROADMAP

_2026-06-25 · phase: **v0.1.0 released — stabilising; v0.2 (dependency policy rules) next**_

## Product framing

`archcheck` is a **dependency-diff / drift guardrail for C++ pull requests**.

It exists to catch, before merge:

- new include dependencies;
- new cycles;
- growth of the dependency knot / SCC;
- new links between project areas;
- later — violations of explicitly specified module boundaries.

The main product thesis:

> This PR added a dependency you probably did not want.

It is **not**:

- an AI attribution tool;
- a duplication-first checker;
- a universal quality platform;
- a bug finder / formatter / include optimizer.

"What's in progress right now" — `backlog/wip/`.
The queue — `backlog/new/`.
"What has already shipped" — [CHANGELOG.md](../CHANGELOG.md).
Real-world runs and research findings — [milestones.md](milestones.md).
The product frame in more detail — [product_vision.md](product_vision.md).

---

## Current focus

**v0.1.0 released** (2026-06-25): the trusted graph/drift/diff core has shipped, and a
prebuilt Linux binary lives in GitHub Releases for pinned CI install. The phase has shifted
from "finish the core" to **stabilization + transition to v0.2**.

Priorities of the current phase:

- stabilize `--diff`-in-CI: exit 2 on an unresolvable ref (#144);
- public-readiness: an outward-facing GitHub demo repo for new-clone drift (#123);
- start v0.2 — enforcement of module rules from `.archcheck.yml` (ADR-001).

**Reprioritization note (2026-06-28, corpus-backed):** across 516 572 per-commit results the
structural/boundary class that v0.2 enforcement targets is a **0.3–0.5 % event** (new cross-area dep
0.49 %, gate 0.31 %), while the live advisories move **30–45× more often** (test co-evolution 22.6 %,
complexity 17.2 %, added edges 14.8 %, new-clone 9.9 %). Implication: the public wedge is the **diff
guardrail + advisories**, not module-boundary enforcement. The cycle/god gate stays a low-FP silent
guardian, not a headline. **v0.2 module-enforcement is demoted to an opt-in power feature and is NOT a
public-launch blocker.** (Evidence: [JOURNEY.md](JOURNEY.md) "0.3 % event" episode;
`experiments/trending_run/drift_trending56.md`.)

What has shipped and is no longer "in the finishing focus" (details in [CHANGELOG.md](../CHANGELOG.md)):

- graph/drift/diff — the product wedge, in the release;
- duplication — advisory reporting (`--duplication`, new-clone drift in `--diff`), not a blocking gate;
- cheap-drift advisories (SATD, test co-evolution, local complexity, flag-argument ARG.1,
  bool-field accretion) — in `--diff`, advisory-first;
- AI-vs-human drift — research (conclusion: there is no agentic signature of drift, it dies under repo FE),
  not a product promise.

---

## v0.1 — Trusted Dependency Diff for PRs (released 2026-06-25)

**Goal:** show the architectural dependency changes introduced by a PR with almost no setup.

**Cover-story:** the user runs `archcheck` in CI and gets a clear
answer to whether this diff made the dependency graph worse.

### Product core

- include graph extraction without a mandatory `compile_commands.json`;
- baseline save/load;
- graph baseline + drift comparison;
- PR/diff workflow;
- deterministic text/json output;
- documented exit codes `0 / 1 / 2 / 3`;
- advisory-first default;
- a strict gate only for the most reliable regressions;
- advisory output of copy-paste introduced by a commit, in `--diff` (decision 2026-06-13:
  show it already in v0.1; the blocking duplication gate stays v0.2).

### Trusted signals

- intrinsic include-graph checks (`SF.7`, `SF.8`, `SF.9`, Lakos-style defaults);
- `DRIFT.1`, `DRIFT.2`;
- `DRIFT.4.CYCLE` (a new lateral link between modules forming a cycle with a
  baseline back-edge);
- new unwanted dependency edges;
- new cycles;
- SCC / dependency knot growth;
- new cross-directory / cross-area dependencies.

### What we're finishing in v0.1

- remove false and unfinished user-facing contracts;
- make diff/drift output stable and explainable;
- bring roadmap/docs/CLI in line with the real state;
- keep the zero-config signal useful and safe for the first time it's turned on.

### What is not the center of v0.1

- duplication as a blocking gate;
- AI attribution;
- AI rule synthesis;
- a broad AST semantic platform;
- visualization / plugin API / broad C support.

Duplication in `v0.1` shipped as an **advisory report** (`--duplication`,
report-only, exit 0) — a supported feature, but not a blocking gate and not the center of v0.1.

---

## v0.2 — Dependency Policy Rules

**Goal:** turn `archcheck` from a zero-config dependency diff into a dependency diff
plus enforceable project policy.

### Scope

- `.archcheck.yml` as a real runtime feature;
- modules / path mapping;
- `forbidden` dependencies;
- `layers`;
- `independence`;
- clearer violation explanations;
- CI examples;
- optional SARIF where it helps adoption.

### Supporting expansion

- fan-out growth;
- blast-radius growth;
- coupling growth;
- a selective `libclang` backend only where it improves enforceable checks.

Important: the product story of `v0.2` is not "now we have libclang," but
"now I can check my architectural boundaries."

---

## v0.3+ — Selective Semantic Expansion

Semantic expansion happens only after the product core and the policy layer
are stable.

Potential scope:

- AST-backed refinement of existing checks;
- SF.2 / SF.5 / SF.10 / SF.11;
- SF.21 preview/default-ON when ready;
- additional intrinsic drift metrics, if they hold up to the trust bar.

Principle: semantic expansion is needed only where it delivers verifiable
user value, not for the sake of the backend itself.

---

## Preview / Research

These directions are important, but they **do not belong in the trusted CI gate**, until
they have proven their stability and product-fit.

- duplication-as-blocking-gate (`--duplication` itself is already shipped as an advisory report; gate-grade precision — not yet);
- AI-vs-human drift comparison;
- AI-assisted rule synthesis;
- semantic dependency extraction beyond the current product core;
- Martin metrics;
- visualization;
- plugin API;
- C support.

They can be developed in `research/`, `preview/`, and `future/`, but should not be sold
as the core of the nearest product.

---

## What We Are Not Doing

- not building a "general code quality platform";
- not making an AI detector / AI attribution tool;
- not moving duplication into a mandatory gate before the needed precision;
- not blurring the MVP with broad AST-scope;
- not making a GUI / web dashboard / IDE extension;
- not replacing linters, formatters, and bug finders.

---

Detailed product arguments — [product_vision.md](product_vision.md).
Architectural context and long-form spec — [architecture-spec.md](architecture-spec.md).
