# [CORE][CLI][DOCS][BACKLOG] Full alignment of contracts, scope and research tails

**Created:** 2026-06-05
**Started:** 2026-06-05
**Completed:** 2026-06-05
**Status:** completed
**Module:** CORE / CLI / DOCS / BACKLOG / RESEARCH-HYGIENE
**Priority:** critical
**Complexity:** XL
**Blocks:** —
**Blocked by:** —
**Related:** #045 (docs_sync_roadmap_mvp_spec), #051 (config_loader_v1), #073 (tech_debt_alignment_cleanup), #075 (mvp_v1_trusted_diff_workflow)
**Follow-ups (split-out tails):** #083 (duplication precision → gate-grade), #084 (fp_corpus_eval relocate from the shipped lib), #085 (backlog status normalize — duplication/experiments history)
**Source of truth:** [docs/architecture-spec.md](../../docs/architecture-spec.md), [docs/ROADMAP.md](../../docs/ROADMAP.md), [docs/product_vision.md](../../docs/product_vision.md), [docs/config_format.md](../../docs/config_format.md)

## Goal

In one big task, align the **real behavior**, the **public CLI/doc contracts**,
the **sources of truth for agents**, the **backlog state** and the **boundary between product and research**.

This is not a feature task. This is an **alignment + cleanup umbrella** following an audit:
remove false promises, dead/stale traces, placeholder branches and the desync between
docs, code and backlog.

The work allows **partial commits**. We commit by slices, but keep one common
context and one acceptance contract.

## Why this is one task, not a scattering of small ones

The problems here are causally linked:

- the docs promise what isn't in the runtime;
- the runtime surface already includes preview/research areas;
- the backlog and research docs describe different states of the same branches;
- AGENTS/source-of-truth for the agent conflicts with the real tree;
- part of the cleanup decisions depend on a product decision, not a mechanical text edit.

If you split this into 5-10 micro-tasks, the main point is lost: we need not just to
"fix links" or "update help", but to **reduce the product to an honest, self-consistent state**.

## TL;DR

Right now the main risk for `archcheck` is not in the graph core, but in the fact that the system already
looks more mature than it actually is:

- `--config` looks like runtime enforcement, but in fact it's a loader + thresholds;
- some docs still promise `file:line:column`, even though the `Violation` model
  has no place for it;
- `AGENTS.md` lives in an alternate pre-implementation reality;
- the duplication/research layer partly leaked into the shipped tree and the user-facing surface;
- there are placeholder/no-op implementations included in the production build;
- the backlog and markdown links contain stale references to deleted artifacts.

Until this is reconciled, any new feature will raise the maintenance cost faster
than the product's real value.

## Audit context

The task was assembled following a full pass over:

- core docs (`architecture-spec`, `MVP`, `ROADMAP`, `product_vision`, `config_format`);
- agent instructions (`AGENTS.md`);
- shipped code (`include/`, `src/`, `tests/`, `scripts/`);
- backlog (`new/`, `wip/`, `completed/`);
- research documents that already affect the product narrative.

The criterion used:

> look not for "AI wrote / didn't write", but for verifiable signs of AI-assisted accretion:
> promises without implementation, placeholder logic, scope creep, stale branches, doc-code drift,
> local duplicate plumbing and a test harness masquerading as a product path.

## Main problems

### 1. `--config` promises more than it actually does

Current state:

- the CLI help says `validate .archcheck.yml v1 and run check`;
- the loader actually parses `modules`, `layers`, `independence`, `forbidden`;
- the runtime from `Config` uses only `thresholds` for the default intrinsic-rule limits.

Consequence:

- the user gets the appearance of a product contract without actual enforcement;
- this is more dangerous than an ordinary TODO, because the behavior looks complete;
- any integration/docs examples on `.archcheck.yml` are currently misleading.

What needs to be decided:

- either honestly downgrade `--config` to a validate-only surface;
- or bring the config rules up to a real runtime pipeline;
- there must be no intermediate "kind of works" state.

### 2. The diagnostics contract diverges from the data model

Current state:

- the spec and MVP promise `file:line:column`;
- `Violation` stores only `ruleId`, `file`, `line`, `message`;
- the text/json reporters physically cannot output `column`.

Consequence:

- the public contract is wider than the domain model;
- the baseline/json schema is already starting to cement the incomplete form;
- it will hurt more later to introduce exact suppression/location-based semantics.

What needs to be decided:

- either honestly reduce the contract to `file[:line]`;
- or extend `Violation` and the entire output path to a real `column`.

### 3. The shipped tree contains placeholder/no-op research code

Suspicious zones:

- `src/scan/duplication/fp_corpus_eval.cpp` — a placeholder metric, effectively
  "assume everything is TP";
- `phase12HeaderImplGate()` in the duplication scanner — a stub;
- nearby there's changelog/doc wording that looks as if this is already a full-fledged feature.

Consequence:

- it's hard for a user and a future developer to tell what's production-grade and what isn't;
- review becomes harder: no-op logic masquerades as a feature;
- preview/research code gets dragged into the core build without an explicit maturity boundary.

What needs to be decided:

- either move such pieces out of the shipped path;
- or very strictly mark the experimental/placeholder status;
- or push them up to real behavior, if they're already considered shipped.

### 4. `AGENTS.md` and the code-style truth conflict with each other and with reality

Current state:

- `AGENTS.md` still says `pre-implementation`, "no src/CMake/tests";
- `AGENTS.md` requires `_name` and `I*`, while `docs/code_style.md` fixes `name_`
  and explicitly forbids the `I`-prefix as the target style of new code;
- the code itself already follows something other than what's written in `AGENTS.md`.

Consequence:

- an agent following `AGENTS.md` will systematically make the wrong decisions;
- the source of truth for coding agents is not just outdated, but **dangerously outdated**;
- this is a direct channel for new slop and repeated desync.

What needs to be decided:

- bring `AGENTS.md` to the actual state;
- remove the duplicating norms from there, if they already live in `docs/code_style.md`;
- leave one consistent agent-facing contract.

### 5. The documents describe several different products at once

Current state:

- `architecture-spec` describes the long-form product and the v0.2+/future surface;
- `ROADMAP` is already trying to bring the focus back to the trusted drift core;
- `product_vision` separately says that duplication is a noisy preview/research;
- `MVP` still looks like the old pre-#006 document;
- `README` and `CHANGELOG` have in places already caught up with the code, in places drag historical wording.

Consequence:

- a new person can't quickly understand what's really shipped and what's still an idea;
- different documents tell different product stories;
- some decisions are already made but haven't reached all the documentation layers.

What needs to be decided:

- fix one current product story;
- synchronize MVP/README/ROADMAP/spec;
- remove the v0.2/v0.3 promises as if they were already the v0.1 core.

### 6. The backlog contains a stale narrative and broken traces of deleted branches

Current state:

- there are WIP/new/completed documents referencing deleted `experiments/*`;
- the markdown check showed dozens of local broken links;
- part of the duplication narrative has already been cancelled by architectural decisions, but lives on in the backlog.

Consequence:

- the backlog stops being a reliable state system;
- historical notes look like live plans;
- a person spends time reading what's no longer physically in the tree.

What needs to be decided:

- clean up the links;
- move cancelled/historical branches to an honest state;
- separate "the history of a decision" and "current work".

### 7. The research/preview layer sits too deep in the shipped surface

Current state:

- `--duplication` is already in the help and the CLI;
- the duplication subsystem is built into `archcheck_core`;
- meanwhile the product's own docs say duplication is preview/research for now;
- part of the research tests is tied to hardcoded corpus paths and doesn't even go into the hermetic CI build.

Consequence:

- the boundary between the product core and the research is smeared;
- the user-facing surface is already wider than the trust bar;
- every change to the duplication code affects the shipped binary more than desired.

What needs to be decided:

- decide whether this is already a product preview feature or an internal research path;
- rebuild the CLI/help/docs/build/test hygiene around that decision;
- don't keep a "half-shipped" zone without an explicit status.

### 8. There's a local accretion smell: large orchestration files and plumbing duplicates

Current state:

- `src/main.cpp` is already an application god-file;
- the git subprocess/fork-exec plumbing is repeated across several translation units;
- the duplication scanner grew into a long phase-pipeline with a lot of local heuristics.

Consequence:

- further changes become more expensive and riskier;
- policy decisions spread across several places;
- it's easy to get mode discrepancies.

What needs to be decided:

- carve up the application orchestration;
- collapse the common plumbing duplicates, if it can be done without extra abstractions;
- leave, after cleanup, a clearer boundary between core logic and command wiring.

## What's in the task

### A. Product contract alignment

- `--config`: make an honest decision on the semantics.
- Diagnostics contract: choose and align `file[:line]` vs `file:line:column`.
- Check that help/README/spec don't promise the impossible.
- Check that the changelog doesn't call shipped what's still placeholder/preview.

### B. Runtime/code cleanup around these contracts

- Remove placeholder/no-op code from the shipped critical path or mark it honestly.
- Carve up `main.cpp`, if it's needed for an honest alignment of the CLI semantics.
- Remove the explicit policy duplicates and discrepancies.

### C. Docs alignment

- Update `AGENTS.md`.
- Update `README.md`, `docs/MVP.md`, `docs/ROADMAP.md`, `docs/product_vision.md`,
  `docs/architecture-spec.md`, `docs/config_format.md` only to the necessary degree.
- Don't rewrite everything in a row; change only what participates in the product contracts.

### D. Backlog/research hygiene

- Clean up the stale and broken references.
- Normalize the status of deleted/cancelled branches.
- Clearly separate product/current/research/historical.

## What's NOT in the task

- New architectural features on top of the current core.
- A big semantic/libclang expansion.
- A new duplication detector, a new rule engine or a new preview platform.
- Rewriting half the project "while we're at it".
- Refactoring for aesthetics with no direct benefit to the contracts.

## Commit order

Below are not mandatory exact commit names, but **working slices** that you can move along.

### Slice 1 — product truth first

#### Why this slice is needed

This is the starting slice. Its goal isn't to immediately rewrite the code, but to
**stop the false promises** in the user surface. After it,
help, `README` and the key documents should describe exactly what really exists.

#### What to read before starting

- [docs/architecture-spec.md](../../docs/architecture-spec.md)
- [docs/MVP.md](../../docs/MVP.md)
- [README.md](../../README.md)
- [docs/config_format.md](../../docs/config_format.md)
- [src/main.cpp](../../src/main.cpp)
- [include/archcheck/rules/violation.h](../../include/archcheck/rules/violation.h)

#### The main questions of this slice

Two decisions need to be made explicitly:

1. What `--config` really means in the current version.
2. Which diagnostics contract is honest at the current stage:
   `file`, `file:line` or `file:line:column`.

Until these decisions are pinned down, we can't go further. Otherwise the subsequent commits
will be built on different assumptions.

#### What exactly we do

1. Compare the promises in help and the documents with the real code behavior.
2. Write out what already works, what's only parsed, and what's not executed yet.
3. Fix only the critical entry points:
   - CLI help;
   - `README`;
   - those product documents that directly mislead the user.
4. Rewrite the wording so it's short and unambiguous.

#### What NOT to do

- Don't finish the entire `--config` runtime right here, if that's separate work.
- Don't rewrite the whole `architecture-spec.md`.
- Don't clean up the backlog and research docs in this commit.
- Don't add optimistic "coming soon" wording, if it's not needed.

#### How to know the slice is complete

- The user surface no longer lies.
- There's no direct conflict between help, `README` and the key product documents.
- The task explicitly records which decision was made on `--config` and diagnostics.

#### What should land in the commit

- Minimal help and docs edits removing the false promises.
- Without a big code cleanup.

#### What to look at in review

- Whether any hidden ambiguous wording remains.
- Whether new promises beyond what really exists have appeared.
- Whether it's clear to a junior where it's fact and where it's future work.

### Slice 2 — runtime alignment

#### Why this slice is needed

After the first slice the documents are already honest. Now we need to make sure that
**the code exactly matches the pinned-down contract**.

#### What to read before starting

- [src/main.cpp](../../src/main.cpp)
- [src/rules/rule_set.cpp](../../src/rules/rule_set.cpp)
- [include/archcheck/rules/violation.h](../../include/archcheck/rules/violation.h)
- [src/report/text_reporter.cpp](../../src/report/text_reporter.cpp)
- [src/report/json_reporter.cpp](../../src/report/json_reporter.cpp)
- the tests around config/reporters/violations

#### What exactly we do

1. Bring the `--config` runtime to the decision from `Slice 1`.
2. Bring `Violation`, the reporters and the related schemas to the chosen diagnostics contract.
3. Remove or isolate the placeholder/no-op code that looks like a finished feature.

The practical logic is this:

- if `--config` currently only validates and applies `thresholds`,
  the code must not give the impression of a full-fledged rules runtime;
- if the diagnostics contract is now without `column`,
  the code, output and docs must live without `column`;
- if `column` is still considered mandatory,
  it must be carried through the whole chain, not just in one place.

#### What NOT to do

- Don't drag the backlog and AGENTS cleanup in here.
- Don't bloat the task into a new rule engine.
- Don't refactor the architecture for beauty.

#### How to know the slice is complete

- The code and help answer the `--config` question the same way.
- The `Violation` model corresponds to the public diagnostics contract.
- The placeholder/no-op areas no longer masquerade as shipped semantics.

#### What should land in the commit

- Runtime edits.
- If needed, pinpoint test edits under the honest contract.

#### What to look at in review

- Whether a second hidden behavior remains.
- Whether new temporary stubs were introduced.
- Whether a new unnecessary abstraction layer appeared.

### Slice 3 — source-of-truth cleanup

#### Why this slice is needed

Right now the agent instructions conflict both with the code and with the rest of the documents.
This slice is needed so that **the human and the agent start from one reality**.

#### What to read before starting

- [AGENTS.md](../../AGENTS.md)
- [docs/code_style.md](../../docs/code_style.md)
- [docs/code_quality.md](../../docs/code_quality.md)
- [docs/architecture-spec.md](../../docs/architecture-spec.md)
- the current tree `src/`, `tests/`, `CMakeLists.txt`

#### What exactly we do

1. Update `AGENTS.md` to the actual state of the repository.
2. Remove from it the knowingly false statements:
   - about `pre-implementation`;
   - about the absence of `src/`, `tests`, `CMakeLists.txt`;
   - about outdated style rules, if they no longer match the project.
3. Reduce the style/naming guidance to one source of truth.

What's important here:

- if a rule is already pinned down in `docs/code_style.md`,
  in `AGENTS.md` it's better to give a short consistent summary or a link;
- you can't keep two different rule sets for new code side by side.

#### What NOT to do

- Don't turn `AGENTS.md` into a duplicate of all the documentation.
- Don't rewrite the whole style guide unnecessarily.
- Don't change the code just to make it conform to an outdated `AGENTS.md`.

#### How to know the slice is complete

- An agent reading `AGENTS.md` will no longer make mistakes automatically.
- There are no direct contradictions between `AGENTS.md` and `docs/code_style.md`.
- The document is short, practical and reflects the current project tree.

#### What should land in the commit

- Edits to `AGENTS.md`.
- If needed, pinpoint clarifications in one or two related docs.

#### What to look at in review

- Whether historical statements remain as if they're still current.
- Whether there's now a dangerous duplication of rules.
- Whether it's clear from the document how to actually work in the repository now.

### Slice 4 — backlog and link hygiene

#### Why this slice is needed

The backlog and docs should be navigation across the current project state. Right now part of the
links are broken, and part of the historical tails look like an active plan.

#### What to read before starting

- [backlog/README.md](../../backlog/README.md)
- the affected files in `backlog/new/`, `backlog/wip/`, `backlog/completed/`
- the documents where broken local links were already found

#### What exactly we do

1. Fix the local markdown links where it's part of the current topic.
2. Remove links to deleted `experiments/*`, if they're served as a current reference.
3. Separate the current state and the historical notes.

The working rule:

- if a link should lead to a live file, fix it;
- if a link leads into the past, but the past is important, leave a short historical note;
- if a link is no longer needed, delete it.

#### What NOT to do

- Don't rewrite all the backlog files in a row.
- Don't change the meaning of old tasks, if the problem is only in links and statuses.
- Don't open new product decisions within link hygiene.

#### How to know the slice is complete

- The affected files no longer link into the void.
- Historical notes don't look like a current roadmap.
- The backlog can again be read as a working state system.

#### What should land in the commit

- Link edits.
- Pinpoint wording and status edits in backlog/docs.

#### What to look at in review

- Whether useful historical context is lost.
- Whether new broken links appeared from a mechanical replacement.
- Whether `current`, `historical`, `cancelled`, `research` are clearly distinguished.

### Slice 5 — preview/research boundary

#### Why this slice is needed

Duplication is currently in an intermediate zone: it's already visible to the user,
but doesn't yet look like a fully trusted part of the core. This slice draws
a **clear maturity boundary**.

#### What to read before starting

- [docs/product_vision.md](../../docs/product_vision.md)
- [docs/ROADMAP.md](../../docs/ROADMAP.md)
- [src/main.cpp](../../src/main.cpp)
- [src/CMakeLists.txt](../../src/CMakeLists.txt)
- the duplication-related code and tests in `src/scan/duplication/` and `tests/`

#### The main question of this slice

One duplication status needs to be chosen:

- internal research;
- preview;
- shipped capability.

The same answer should read out of:

- help;
- docs;
- build;
- tests;
- changelog;
- backlog.

#### What exactly we do

1. Pin down the chosen duplication status.
2. Bring the user surface to that status.
3. Bring the build and test hygiene to the same maturity boundary.
4. If needed, remove research artifacts from those places where they look like
   an ordinary product path.

In practice this means:

- preview must be explicitly marked as preview;
- research must not masquerade as an ordinary shipped feature;
- a shipped capability can't rest on placeholders and non-hermetic tails.

#### What NOT to do

- Don't improve the duplication algorithm itself "while we're at it".
- Don't build a new preview platform.
- Don't drag unrelated cleanup in here.

#### How to know the slice is complete

- The same thing is said about duplication across all layers.
- The user doesn't take a noisy preview for the trusted core.
- The build/test/doc surface no longer argue with each other.

#### What should land in the commit

- Edits to help/docs/build/test hygiene.
- Only those code changes needed for an honest status boundary.

#### What to look at in review

- Whether a half-shipped zone remains without an explicit status.
- Whether access to the internal research workflow was lost, if it's still needed.
- Whether the trusted core is sufficiently explicitly separated from preview/research.

### Slice 6 — final alignment pass

#### Why this slice is needed

This is the final pass. Its goal isn't to open a new work front, but to make sure
that the previous commits really came together into one system.

#### What to read before starting

- the diff of all the previous slice commits;
- the final versions of help, `README`, the key docs and the affected runtime files;
- this whole task

#### What exactly we do

1. Walk the chain "promise -> code -> output -> documentation -> backlog".
2. Finish off the residual contradictions in terms, statuses and wording.
3. Update the final sections of the task itself, so it records the result,
   not just the process.

The questions to check:

- does help lie?
- does the code do exactly this?
- do the reporters output exactly this contract?
- do the docs argue with each other?
- does the backlog reference an old reality?

#### What NOT to do

- Don't start a new big refactor.
- Don't add new functionality.
- Don't open new product directions.

#### How to know the slice is complete

- The same question, asked of help, docs and code, gets the same answer.
- After reading the repository there's no feeling that there are "three different products" inside.
- The task records the final decisions and the tails deliberately left out of scope.

#### What should land in the commit

- Small final alignments.
- An update of the task itself to the final diagnostic state.

#### What to look at in review

- Whether the last small exceptions appeared that again create drift.
- Whether it's clear what was deliberately left out of this umbrella task.
- Whether it's safe to return to feature work after this.

## Acceptance criteria

- [x] `--config` no longer creates a false impression of runtime enforcement. *(Slice 1: help + config_format "Enforcement status"; Slice 2: confirmed — runtime is only thresholds.)*
- [x] The diagnostics contract (`line`/`column`) is the same in code and documents. *(Slice 1: docs → `file:line`; Slice 2: the code is already `file:line`.)*
- [x] No placeholder/no-op logic passed off as a finished feature remains in the shipped core. *(Slice 5a: the no-op `phase12` was removed; `fp_corpus_eval` is internal QA outside the user-facing surface.)*
- [x] `AGENTS.md` matches the actual state of the repository and the current style contract. *(Slice 3: full rewrite.)*
- [x] `README`, `MVP`, `ROADMAP`, `product_vision`, `architecture-spec` don't contradict each other on the current shipped surface. *(Slice 1/5b/6: diagnostics, --config, duplication status, CLI shape aligned.)*
- [x] Broken local markdown links found within this topic are either fixed or moved to honest historical notes. *(Slice 4: 26 fixed, experiments de-linked into historical notes.)*
- [x] The research/preview branches no longer masquerade as the product core without explicit marking. *(Slice 5b: duplication → explicit shipped advisory; experiments — a local sandbox, marked.)*
- [x] After the task is complete, a new developer can understand in a single pass:
      what's shipped, what's preview, what's research, what's future. *(achieved by slices 1-6.)*

## Risks

### 1. We accidentally expand the scope

There's a risk of turning the cleanup into "let's rewrite half the project". Must not.

The rule:

- if an edit doesn't reduce a false contract, stale state or research drift,
  then it's not part of this task.

### 2. We break the already-working core

Especially dangerous around:

- baseline formats;
- the JSON/report contract;
- the CLI exits and help;
- the preview duplication wiring.

The rule:

- first pin down the decision on the contract, then change the code;
- without a parallel "let's improve the architecture a little more".

### 3. We start treating the documents before deciding the product decision

For example, on `--config` you can't first rewrite all the docs, if it's not decided:
is it validate-only or enforcement.

The rule:

- first the decision, then the mass synchronization.

### 4. We mix a historical note and a current plan

You can't just delete the traces of old decisions, if they're needed as history.

The rule:

- the outdated either archive as historical,
- or move to a completed/obsolete note,
- but don't keep it as if it's current work.

## Diagnostics

Signals that the task is closed well:

- help/README/spec no longer require verbal caveats;
- a grep over the key contracts doesn't produce mutually exclusive wording;
- a local check of the markdown links shows no broken links in the affected zone;
- in the core build no explicit placeholder comments remain on the shipped path, except
  the consciously left and explicitly marked research stubs;
- reading the backlog, it's clear which branches are live and which are historical.

Signals that the task is closed badly:

- `--config` still "kind of works";
- the docs still say `file:line:column`, but the code doesn't;
- `AGENTS.md` still conflicts with `docs/code_style.md`;
- duplication remains simultaneously "preview only" and "core shipped" without an explicit decision;
- broken links and references to deleted experiments remain in the current docs/WIP.

## Decisions to be made during the task

### Decision 1 — `--config`

Choose one:

1. **Validate-only for now**
   Pros: honest, fast, matches the v0.1/v1 trusted core.
   Cons: the user-facing surface is temporarily weaker than expectations.

2. **Bring up enforcement now**
   Pros: closes the debt entirely.
   Cons: this is already almost feature work, a risk of going out of scope.

Current recommendation:

- **validate-only for now**, enforcement as a separate phase after the cleanup.

**ACCEPTED (2026-06-05): validate-only for now.** `--config` validates `.archcheck.yml`
and applies `thresholds`; enforcement of `modules/layers/independence/forbidden` —
a separate phase after the cleanup. Help/docs are downgraded to validate-only.

### Decision 2 — diagnostics contract

Choose one:

1. **Lower the promise to `file[:line]`**
2. **Extend the model and output to `column`**

Current recommendation:

- don't take a "half-measure"; choose one explicitly and carry it through docs/code/schema.

**ACCEPTED (2026-06-05): lower to `file:line`.** `Violation` already stores only
`file`+`line`; `column` is removed from spec/MVP/AGENTS/README. Extending the model to
`column` is deferred (semantic expansion is out of scope of this umbrella).

### Decision 3 — duplication status

Choose one:

1. the preview surface stays user-facing, but is honestly marked;
2. duplication temporarily leaves the product-facing help/story;
3. duplication is promoted to a product and then stops living as half-research.

Current recommendation:

- keep it **preview**, but do it everywhere the same way and without a false product finality.

**ACCEPTED (2026-06-05): promote to a product** (diverges from the recommendation —
a conscious decision of the user). Duplication stops being preview/research and
becomes a shipped capability.

**An important consequence for the work order:** the `preview` label can't be removed while
placeholders/no-ops (`src/scan/duplication/fp_corpus_eval.cpp`,
`phase12HeaderImplGate()`) and non-hermetic tests on hardcoded corpus paths remain in the
shipped path — otherwise the promotion creates a *new* false promise. Therefore:

- **Slice 1** touches only `--config` and diagnostics (safe doc/help contracts);
- **Slice 2** removes/pushes up the placeholder duplication code;
- **Slice 5** carries the `preview → shipped` promotion across all layers
  (help/docs/build/tests/changelog/backlog) — only after the code became honest.

## Progress

### Slice 1 — product truth first ✅ (2026-06-05)

Three decisions pinned down (see the "Decisions" section): `--config` → validate-only;
diagnostics → `file:line`; duplication → promote to a product (after the code cleanup).

The user-facing surface aligned on the two safe contracts (`--config`, diagnostics):

- `src/main.cpp` — help for `--config`: "validate + apply thresholds; module rules not yet enforced".
- `docs/config_format.md` — added the section "Enforcement status (current release)":
  module rules are validated, but not enforced; the runtime applies only `thresholds`.
- `docs/MVP.md` — diagnostics `file:line:column` → `file:line` (+ a note about the `Violation` model).
- `docs/architecture-spec.md` — diagnostics `file:line:column` → `file:line`.

The archcheck build is green. README was already honest (`file:line`, config rules marked v0.2) — not touched.
`AGENTS.md:30` (`file:line:column`) was consciously left for Slice 3 (full AGENTS rewrite).

### Slice 2 — runtime alignment ✅ (2026-06-05, verification-only)

The check showed: **the runtime already matched both Slice 1 contracts** — only the docs lied.
No code edits were required.

- **`--config` validate-only:** `makeDefaultRuleSet` ([src/rules/rule_set.cpp](../../src/rules/rule_set.cpp))
  reads only `config.thresholds` (godHeaderFanIn, chainLength). `modules/layers/independence/forbidden`
  are parsed into `Config`, but the runtime doesn't touch them — there's no half-wired enforcement stub.
- **diagnostics `file:line`:** `Violation` = {ruleId, file, line, message};
  `text_reporter` prints `file:line: [rule] msg`; `json_reporter` — the fields rule/file/line/message;
  `violation_baseline` — the triple (ruleId, file, line). `column` is absent throughout the output chain.
- **placeholder/no-op:** a grep over `src/`/`include/` confirmed — **all** the placeholder code is
  duplication-scoped (`phase12HeaderImplGate`, `fp_corpus_eval`); outside duplication the shipped path is clean.

**Decision on the placeholder (important):** `phase12HeaderImplGate()` — a no-op in the live
pipeline `scanForDuplication`, BUT the docs (`duplication_fp_analysis.md`) and `CHANGELOG`
declare P1.3 as a working classifier ("-4 FP", "70%", "✅"). Removing the code while
leaving the docs lying → new drift. Therefore phase12 + `fp_corpus_eval` + the reconciliation
of the P1.3 narrative/CHANGELOG are moved to **Slice 5** (duplication→product), where the code and
narrative are aligned in one pass.

### Slice 3 — source-of-truth cleanup ✅ (2026-06-05)

`AGENTS.md` rewritten entirely under the actual state (per the rule "rewrite an outdated
.md entirely, don't stick on outdated banners"):

- removed the false `pre-implementation` / "no src/CMake/tests" → the real v0.1 state
  with a pointer to `CHANGELOG` as authoritative on the shipped set;
- diagnostics `file:line:column` → `file:line` (the Slice 1/2 tail in AGENTS closed);
- naming brought to `docs/code_style.md`: `name_` (not `_name`), methods/functions
  `lowerCamelCase` (the old "public PascalCase" was wrong), interfaces without the `I`-prefix
  for new code (the legacy `IRule` marked as non-target, rename out of scope);
- **the style is no longer duplicated** — AGENTS gives a short summary + a pointer to
  `code_style.md` as the single source of truth (the double rule set removed);
- subsystems moved to the present tense (they exist); deps pinned down (`ryml`,
  Catch2 v3, not "or"); `clang_scanner` marked v0.2;
- the outdated pre-impl bootstrap order removed;
- the rule "don't run the build" → "you can build freely, Debug by default"
  (the conflict with `CLAUDE.md` removed);
- the product name pinned down `archcheck` (the open branding question removed);
- all markdown links checked — they lead to live files.

### Slice 4 — backlog/link hygiene ✅ (2026-06-05)

The broken local markdown link checker was run: **27 broken in 7 files**.
Fixed **26**; 1 — a false positive (a C++ lambda syntax of the `cb`-capture form
in the prose of #070 — looks like a markdown link, but it's code; left as is).

**A clarification on `experiments/`:** the directory was NOT deleted entirely — it's an untracked
local sandbox (deliberately kept out of git). Only the subdirectory
`experiments/line_duplication/` was deleted (in the git history, commit `35085ca`).

Fixes:

- targets moved into `backlog/completed/` (018/022/023/025/047/048/v1_maj_config):
  paths updated in `ci_integration.md`, `config_format.md`, `research/ai_drift_cases.md`;
  along the way the outdated status "#018 wip" → "completed" was removed.
- 050 → `backlog/future/` (in `architecture-spec.md`, 3 links).
- a systemic missing `../` in `ci_integration.md` (links resolved into `docs/...`): added.
- `../research/` → `research/` in `architecture-spec.md`.
- the deleted `experiments/line_duplication/*` in 053/054: markdown links de-linked
  into backtick paths + a short historical note ("in the git history, commit 35085ca / see #053").

The checker after the fixes: 0 real broken links (only the false positive remained).

**Did NOT do** (out of scope of link hygiene, scope-creep risk): a mass reassessment
of the statuses of old tasks and the normalization of "cancelled branches" — the task explicitly
forbids rewriting backlog files in a row and changing the meaning of tasks.

### Slice 5a — duplication honesty cleanup ✅ (2026-06-05)

The safe part of Slice 5: remove the placeholder masquerade, fix the false claims.
Doesn't depend on the preview-vs-product decision.

- **code:** removed the no-op `phase12HeaderImplGate` (call + def) from
  `src/scan/duplication/duplication_scanner.cpp`. The behavior didn't change
  (it was `(void)candidates; (void)allFragments;`), 344/344 tests green.
- **docs/CHANGELOG:** P1.3 was declared as a working classifier ("-4 FP", "70%",
  "✅"), even though the code was a stub. Fixed in `duplication_fp_analysis.md` (3 places),
  `duplication_architecture.md`, `CHANGELOG.md` (10 filters → 9; P1 classifiers
  4 → 3; P1.3 marked as a no-op stub, removed, planned in #070).

Full gate: clang-format/cppcheck/lizard clean, build, 344/344 tests, smoke,
coverage 91.4/95.8/57.2 — PASS.

### Slice 5b — duplication → product ✅ (2026-06-05, variant 2 chosen)

**The user's decision:** variant 2 — consciously flip duplication into a product,
rewriting product_vision/ROADMAP, accepting the narrative change before the algorithm-trust work.

**The honest framing of the flip:** duplication is promoted to a **shipped advisory reporting
capability** (`--duplication` — report-only, always exit 0, doesn't block CI), and
NOT to a trusted blocking gate. This resolves the conflict with product_vision: the reasonable
argument "precision isn't enough for a trusted gate" is preserved (gate-grade is future
work), but duplication stops being "preview/experiment" and becomes a
supported feature. product_vision (L112) and ROADMAP (L101) already allowed
an "advisory mode" — the flip fit into that framing without a false promise.

Aligned across all layers (one answer everywhere):

- **help** (`src/main.cpp`): `--duplication` `(preview: ...)` → `(report duplicate
  code; advisory, does not gate CI)`.
- **product_vision.md**: duplication "a noisy research layer / preview only" →
  "a shipped advisory capability, but not a blocking gate"; "only then decide about
  the product feature" → "already shipped as advisory; the next step — precision up to gate-grade".
- **ROADMAP.md**: "duplication remains preview/research" → "shipped advisory, not a gate";
  the Preview/Research list: `duplication detection` → `duplication-as-blocking-gate`
  (the `--duplication` itself is already shipped advisory).
- **CHANGELOG.md**: added an entry about the `--duplication` advisory report (report-only,
  exit 0, not a gate); removed the "Preview signal" from the clone-type labels.

**Hermetic tests / fp_corpus_eval — checked, no work required:**

- `tests/CMakeLists.txt:26-28` already excludes the non-hermetic harnesses
  (`duplication_vmecpp_test`, `duplication_all_projects_test`, reading hardcoded
  corpus paths) from the CI build with an explicit comment. The rest of the duplication tests are
  synthetic/hermetic.
- `fp_corpus_eval` — internal QA tooling (the loader works; the placeholder `evaluate`
  is tested in a degenerate case), not user-facing, not on the `--duplication` runtime path.
  Doesn't masquerade as a product feature. Left as internal.

**README** was deliberately NOT touched: it's drift-focused, duplication isn't a headline
(ROADMAP explicitly: "not a duplication-first checker"). The advisory feature outside the main pitch — fine.

Full gate: clang-format/cppcheck/lizard clean, build, 344/344, smoke,
coverage 91.4/95.8/57.2 — PASS.

### Slice 6 — final alignment pass ✅ (2026-06-05)

An end-to-end check of the chain "promise → code → output → docs → backlog" across slices 1-5b.
All automatic checks clean:

- `file:line:column` — 0 occurrences (code + all docs);
- `phase12`/no-op gate — 0;
- `--config` enforcement over-promise in README/MVP/spec — none;
- duplication = one answer everywhere (advisory, not a gate) — no stray preview framing;
- broken local links — 0 real (1 false positive: a C++ lambda in #070);
- the `archcheck check`/`init` subcommand shape (the CLI has no subcommand) — fixed in
  `MVP.md` to the flag shape `archcheck --config`; `init` marked v0.2 (not shipped).
  The remaining `archcheck synthesize` — explicitly future (#010), doesn't contradict shipped.

**Acceptance criteria — status:** all 8 closed (see the checklist above; the placeholder criterion
is closed for the shipped runtime path — `phase12` removed, `fp_corpus_eval` is internal QA tooling
outside the user-facing surface).

**Umbrella #082 outcome:** the product is reduced to an honest self-consistent state.
Consciously left out of scope (with a rationale in the slices) and **split into separate
tasks**, so that the tails are tracked rather than hanging in a closed umbrella:

- **#083** — duplication precision up to gate-grade (algorithm work);
- **#084** — relocation of `fp_corpus_eval` from the shipped lib (build hygiene);
- **#085** — backlog status normalize (live/historical/cancelled, duplication/experiments).

The task is closed (`completed/`).

## Changed files

| File | Change |
|------|--------|
| `backlog/wip/082_crt_full_alignment_cleanup_umbrella.md` | umbrella task + 3 decisions pinned down, Slice 1 log |
| `src/main.cpp` | Slice 1: help `--config` downgraded to validate-only |
| `docs/config_format.md` | Slice 1: the section "Enforcement status (current release)" |
| `docs/MVP.md` | Slice 1: diagnostics `file:line:column` → `file:line` |
| `docs/architecture-spec.md` | Slice 1: diagnostics `file:line:column` → `file:line` |
| `AGENTS.md` | Slice 3: full rewrite under the actual state, naming → code_style.md, build rule → CLAUDE.md |
| `docs/ci_integration.md` | Slice 4: 11 broken links (missing `../`, 018/022/023/025 → completed/) |
| `docs/research/ai_drift_cases.md` | Slice 4: 047/048 → completed/ |
| `backlog/wip/053_*.md`, `backlog/wip/054_*.md` | Slice 4: de-link of the deleted `experiments/line_duplication` + a historical note |
| `src/scan/duplication/duplication_scanner.cpp` | Slice 5a: removed the no-op `phase12HeaderImplGate` (behavior-preserving) |
| `docs/duplication_fp_analysis.md`, `docs/duplication_architecture.md`, `CHANGELOG.md` | Slice 5a: P1.3 marked as a no-op stub (was "-4 FP ✅") |
| `src/main.cpp`, `docs/product_vision.md`, `docs/ROADMAP.md`, `CHANGELOG.md` | Slice 5b: duplication preview → shipped advisory reporting capability (not a gate) |
