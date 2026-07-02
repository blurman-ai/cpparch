# archcheck — Architecture rules for C++

> Document version: 2.4. Changes from 2.3 (2026-06-23):
>
> - Current state separated from roadmap: v0.1 described as a zero-config physical-design gate
>   + advisory regression layer; YAML module-rule enforcement, libclang, SARIF,
>   suppressions and rule toggles explicitly marked future.
> - Gate/advisory semantics synchronized with the shipped CLI: check gates SF.9 only;
>   drift gates DRIFT.1/2/4.CYCLE; `--diff` gates new/grown cycles and new god-headers.
> - Tech stack aligned to the repository: `ryml`, Catch2, CMake/Ninja,
>   fast preprocessor backend by default.
>
> Document version: 2.3. Changes from 2.2 (2026-06-12):
>
> - ID `DRIFT.4` taken by the shipped rule *lateral_module_dependency* (#118, CYCLE-grade gating);
>   the wave became `DRIFT.5`–`DRIFT.10`.
>
> Changes 2.2 from 2.1 (2026-06-11):
>
> - Graph-baseline flag names aligned to actual: `--drift-baseline` / `--save-graph-baseline`.
> - Drift wave renumbered: ID `DRIFT.3` taken by shipped bidirectional module coupling (#087), the wave became `DRIFT.4`–`DRIFT.9`.
> - Roadmap: DRIFT.1/2/3 marked as shipped in v0.1 (#086/#087); enforcement of YAML module rules moved to v0.2 (#045/#073); `--output`/`--metrics` marked v0.2+.
> - §"High-level code structure" redrawn to match the actual `src/` (flat `rules/`, `diff/`, `git/`, `scan/duplication/`); the "static table" registration replaced with the actual factory `rule_set.cpp`.
> - Audience: removed "without compile_commands.json it is impossible to parse C++" (contradicted its own two-backend scheme).
>
> Changes 2.1 from 2.0:
>
> - Headline reoriented: "module boundaries and dependency cycles in CI" — the core; Lakos / Core Guidelines / Martin downgraded to "in addition, cited sources" (#006).
> - AI-guardrail (EURECOM constraint decay paper) raised into `## TL;DR` as the third paragraph (#006).
> - Roadmap fully relaid out: v0.1 — fast backend, without `compile_commands.json` and libclang; libclang only in v0.2; Martin metrics — v0.4 optional; `--suggest-config` dropped (#006).
> - Two-backend question closed as an accepted decision, section rewritten (#006).
> - Added a "Stability contract" section — what counts as a breaking change after v1.0 (#007).
> - Product name fixed as `archcheck` (verified on GitHub / PyPI / crates.io / Homebrew / npm) (#003).
> - The "Default analysis" section extended with a "Phase" column, SF.4 removed from defaults with justification, Martin metrics marked as v0.4 (#006).
> - Config example reoriented: the core — `modules` + `forbidden_deps`, defaults — a secondary section; a minimal config for legacy added (#006).
> - Risks updated: licensing risk removed (resolved, Apache 2.0); libclang/compile_commands risks marked as v0.2+; Martin's A — v0.4 (#006).
>
> Version 2.0 → 1.0: demand sources confirmed, repositioning around Lakos + Core Guidelines, default-rules section reworked (3 levels), comparison table and MVP scope updated.

---

## TL;DR

Open source CLI for checking architectural invariants in C++ projects. **Shipped v0.1 core — zero-config physical design gate:** archcheck builds the include graph without `compile_commands.json`, runs conservative Core Guidelines SF.* / Lakos rules and keeps the CI gate narrow: a plain check fails only on SF.9 cycles, drift-baseline — on DRIFT.1/2/4.CYCLE, PR `--diff` — on new/grown cycles and new god-headers. The remaining shipped signals stay advisory and are explicitly marked in text/JSON.

YAML module rules (`modules:` + `rules:` with `layers` / `independence` / `forbidden`) are already parsed and validated, but **are not enforced in v0.1**. Runtime enforcement of module boundaries is the headline of v0.2 ([ADR-001](decisions/001-config-rules-deferred-to-v0.2.md)). For legacy projects there is `--baseline`: freeze the current findings, after which the report shows only new ones.

This closes a years-long gap in the ecosystem: for Java, C#, TypeScript, Python, Go, Rust, Dart such tools exist, in C++ — not a single live open source one.

**AI-guardrail.** A recent paper from EURECOM (Dente, Satriani, Papotti, May 2026) measured the *constraint decay* effect — a ~30 percentage-point drop in assertion pass rate in LLM coding when structural constraints are added. It is cured by moving architectural checks outside the prompt — into CI. The prompt degrades with context, CI does not.

The set of default rules has explicit attribution: cycles, god-headers and include-chains follow [Lakos](https://www.amazon.com/Large-Scale-Software-Design-John-Lakos/dp/0201633620), the rules from the SF section follow [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-source) (Stroustrup, Sutter). Future Martin metrics (I/A/D) remain an opt-in roadmap item, not the shipped CLI. Every default rule cites a recognized source — this removes the "that's just your opinion" argument.

---

## The problem

### Architectural drift

Any non-trivial C++ codebase erodes over time. Layers stop being layers. Modules conceived as independent start pulling on each other. Cycles appear in the include graph. God-headers grow. This is not a bug of a particular team — it is a statistical inevitability for a project older than two or three years.

Code review does not catch this — the reviewer sees the delta, not the structure of the whole project. Linters (clang-tidy, cppcheck) are about something else — style, baby's first bug. Bug-hunters (PVS-Studio, Coverity) — about correctness of operations, not structure. Between them remains a gap: nobody checks that the code matches the declared architecture.

### AI coding accelerates drift

Agents (Claude Code, Codex, Cursor) confidently generate working code, but poorly hold architectural constraints. A recent paper from EURECOM (Dente, Satriani, Papotti, May 2026) measured the effect: ~30 percentage points of drop in assertion pass rate when moving from a purely functional task to a fully structurally specified one. The authors called this **constraint decay**.

A second, independent measurement of the same effect is the GitClear "AI Copilot Code Quality" report (2024, updated 2025). On a corpus of ~211 million meaningful changed lines over five years (commercial repos + large OSS such as React and Chrome), a ~40% drop in the number of moved lines and a rise in duplication were recorded: 2024 became the first year in which copied lines outnumbered moved lines. Moving lines is a proxy for refactoring; its decline means that code stops consolidating into reusable modules and grows by copy-paste. EURECOM measures the structural assertion pass rate, GitClear — line operations in real commits; the methods differ, the conclusion is the same — AI code drifts structurally, not only functionally. Caveat: GitClear is vendor research (they sell a tool against exactly this problem), not peer-reviewed, and the causality "more AI → worse metrics" is disputed for them (Google DORA 2024 reported a small rise in quality). Therefore here it is treated as a corroborating measurement, not as proof.

It is cured not by the number of rules in CLAUDE.md, but by moving architectural checks outside the prompt — into CI. The prompt degrades with context. CI does not.

---

## Evidence of demand

### 1. A publicly recorded six-year-old gap

[TNG/ArchUnit issue #242](https://github.com/TNG/ArchUnit/issues/242) from October 2019: "Does an archunit exist for C/C++?". Closed by the ArchUnit maintainer (`codecholeric`) with the phrase: *"I'm really sorry, but I don't know any alternative for C/C++. There are some solutions for .NET/C#, but for C/C++ I don't know of anything."* This is six years of a publicly recorded gap from a person who monitors the ecosystem professionally.

### 2. The ecosystem of ArchUnit-style tools is actively growing, but not in C++

| Language | Tool | Status |
|---|---|---|
| Java | ArchUnit | v1.4.2 — April 2026, active |
| C# | ArchUnitNET | active |
| F# | FsArchUnit | active |
| TypeScript | ts-archunit | active |
| Python | ArchUnitPython | **appeared 3 weeks ago** (May 2026) |
| PHP | PHPat, Deptrac, PestPHP | active |
| Go | go-cleanarch, fresh-onion | active |
| Dart | lakos | active, implements exactly the Lakos metrics |
| Rust | dependency-constraints | active |
| **C++** | — | **none** |

The trend is unambiguous: C++ is the last large unoccupied niche. The appearance of ArchUnitPython in May 2026 is fresh evidence that the paradigm keeps expanding to new languages.

### 3. Existing "competitors" do not close the niche

| Tool | Class | OSS? | What's wrong |
|---|---|---|---|
| **CppDepend** | architecture + metrics | no (paid) | Paid, enterprise, GUI-first, LINQ-based, does not fit the CI culture of open source |
| **tomtom/cpp-dependencies** | include-graph | yes | **Dead** — last commit 2021, abandoned by Tomtom |
| **lukedodd/include-wrangler** | include-graph | yes | Abandoned, prototype |
| **jeremy-rifkin/cpp-dependency-analyzer** | include-graph | yes | Single-dev toy, only an adjacency matrix, no CI |
| **DeepEnds** | dependency viz | yes | Visual Studio plugin, not CLI |
| **IWYU** | includes optimization | yes | Minimizes includes, does **not** check architecture |
| **CppCheck / clang-tidy** | linter / bug-hunter | yes | A different task — bugs and style |
| **SonarQube Server 2025 R2** | "Design & Architecture" | no (paid serious) | Closed, expensive for opensource |
| **Sotograph** | enterprise | no | Closed, enterprise |
| **archcheck** | **architecture in CI** | **yes** | **This niche** |

### 4. The cultural risk and how to remove it

The C++ community is more conservative than the Java one. "Clean architecture" and "hexagonal architecture" in C++ are not a religion. But it has its own authoritative tradition: **Lakos** ("Large-Scale C++ Software Design") and **C++ Core Guidelines** (Stroustrup, Sutter).

Therefore the tool is positioned not as "ArchUnit for C++", but as **"Lakos physical design + Core Guidelines SF.* checks in CI"**. This speaks the native language of the C++ community and removes the "we don't need your Java concepts" argument.

---

## Why C++ specifically

In Java, C#, TypeScript, Python architectural tests are mainstream. In C++ there is a gap. Documented:

- TNG/ArchUnit issue #242 (see above), six years without a solution.
- Microsoft tried to close the niche through Visual Studio 2010 Layer Diagrams for C/C++. The feature died, in modern VS it does not exist.
- What exists in C++ is either paid (CppDepend), or dead (Tomtom/cpp-dependencies), or in an adjacent task (IWYU about minimizing includes).

---

## Target audience

In descending order of priority:

1. **C++ teams actively using AI coding.** They need an external guardrail that does not depend on whether the model remembers the rules.
2. **Open source projects of medium-large size (10k–500k LOC)**, where maintainers have no resource to manually check architecture in every PR.
3. **Teams with legacy C++ code** who want to stop further drift through `--baseline` (current violations are frozen, new ones break CI).
4. **Educational audience** — teachers, authors of courses on Lakos physical design, Core Guidelines, clean / hexagonal / onion in C++.

### Who is NOT the target audience

- Embedded teams under strict certifications (MISRA, CERT) — for them there are PVS-Studio, Coverity. They pay for a catalog of someone else's rules, not for a DSL for their own.
- Teams that need exactly semantic (AST) analysis, but have nowhere to get `compile_commands.json` — the libclang backend (v0.2) requires it. The include level does not have this limitation: the fast backend works on any project without a build system (see §Two-backend scheme).

---

## What it does (and what it does not)

### Does

- Works without a config with conservative defaults (see §"Default analysis").
- Scans sources with the fast preprocessor backend: without `compile_commands.json`, without libclang.
- Builds the include graph and checks the shipped default rules:
  - **SF.9** cycles — the core gate in a plain check.
  - **SF.7/SF.8** — source-file hygiene advisories.
  - **Lakos.GodHeader / Lakos.ChainLength** — physical-design advisories in check-mode.
- Supports `--baseline`: freeze the current findings, after which the report shows only new ones.
- Supports graph-baseline drift: `DRIFT.1`, `DRIFT.2`, `DRIFT.4.CYCLE` gate;
  `DRIFT.3`, `DRIFT.4.NEW`, `DRIFT.4.SDP` advisory.
- Supports PR `--diff`: gate on new/grown cycles and new god-headers; structural/hygiene
  drift (`added_edges`, chain/NCCD growth, SATD, test co-evolution, local complexity,
  flag arguments, new clones) report-only.
- Accepts `.archcheck.yml` v1, validates `modules:` / `rules:` and applies
  `thresholds:`; runtime enforcement of module rules — v0.2.
- Produces text and JSON (`--format=json`) for `check` and `--diff`.
- Returns a non-zero exit code only on a gated finding/regression (see §Exit codes).

### Does NOT do

- Does not look for bugs, UB, memory leaks — that's PVS-Studio, Coverity, CppCheck.
- Does not format code — that's clang-format.
- Does not check style and idioms — that's clang-tidy.
- Does not minimize includes — that's IWYU.
- Does not replace code review.
- Not a GUI, not a VS Code extension, not a web dashboard. **CLI and CI.**

The shipped advisory layer does not turn archcheck into a linter: these signals are diff-scoped,
regression-oriented and do not break CI. Their job is to show the risk of new code next to the
architectural delta, not to replace clang-tidy / bug finder / style gate.

---

## Default analysis (without a config)

When archcheck is run without `--config`, a set of universal rules with explicit attribution is applied. Every rule cites a recognized source — this removes the "that's just your opinion" argument. In v0.1 the default set is enabled in full; pointwise rule toggles and inline suppressions are a future surface, not the shipped CLI.

The structure of the defaults is by source of authority and by roadmap phase (v0.1 / v0.2 / optional).

### Level 1. C++ Core Guidelines section SF (Stroustrup, Sutter)

Rules for working with source files from the creator of the language and the chair of the committee — the official source. Only the statically checkable ones are implemented (part of the guide is about design, not about static analysis).

| Rule ID | What | Check type | Phase |
|---|---|---|---|
| **SF.7** | No `using namespace` in the global scope of a header | text-scan / AST | **v0.1** (approx), v0.2 (precise) |
| **SF.8** | Every `.h` has include guards or `#pragma once` | preprocessor | **v0.1** |
| **SF.9** | **No cyclic dependencies between source files** | graph | **v0.1** |
| **SF.21** | No anonymous namespaces in headers | AST | v0.2 (preview, `--with-clang`) / **v0.3** default-ON (see [`backlog/future/050`](../backlog/future/050_min_sf21_anonymous_namespace.md) — moved from v0.1: not related to physical-design / drift, clang-tidy already covers it) |
| **SF.2** | A `.h` does not contain definitions of objects / non-inline functions | AST | v0.2 |
| **SF.5** | A `.cpp` must include its `.h` | names + AST | v0.2 |
| **SF.10** | No dependencies on implicitly-included names | AST + includes | v0.2 |
| **SF.11** | Headers self-contained (can be included first in a TU without failing) | compilation | v0.2 |

**SF.4 (`#include`s come before other declarations)** — deliberately not a default: checked trivially, but with almost no product value — it is yet another style-check, not an architectural invariant. The shipped CLI has no `--enable=SF.4`. For the rule evaluation see [`docs/research/`](research/).

Marketing phrase: *"implements 8 statically-checkable rules from the C++ Core Guidelines SF section across v0.1, v0.2 and v0.3"* (SF.21 — a hygiene top-up in v0.3, see [`backlog/future/050`](../backlog/future/050_min_sf21_anonymous_namespace.md)).

### Level 2. Lakos physical design (Large-Scale C++ Software Design)

The Lakos book is the de facto Bible of C++ physical design since 1996. It is cited in the C++ community like Knuth in algorithms. The metrics are already implemented in the Dart package `lakos`, in Sw!ftalyzer, in XDepend — **but not in an open-source tool for C++**.

#### Metrics

- **CCD** (Cumulative Component Dependency) — the sum over all components of the number of components needed to incrementally test each of them. A direct indicator of the coupling of the whole system.
- **ACD** (Average Component Dependency) = `CCD / N`. The average number of components that can break when one is changed.
- **NCCD** (Normalized CCD) = `CCD / CCD_balanced_binary_tree(N)`. **>2.0 → almost guaranteed cycles**; ≈1.0 → a healthy hierarchy; <1.0 → a "horizontal" graph.
- **Component Dependency (CD)** for each component — the number of components it depends on transitively.
- **In-degree / Out-degree** — for each component.
- **Average connectivity `edges/nodes`** — the average number of direct dependencies per component. A cheap detector of "quiet drift": connectivity can triple without a single cycle and with low copy-paste (empirics — run #054 §7.3: stellar-core 1.9→6.3, FastLED 1.3→4.8). Planned for the report (#057). **We normalize specifically by nodes, not by KLOC**: per-KLOC mixes the volume of code into a structural metric (bloated LOC without new edges falsely "improves" the indicator), whereas `edges/nodes` is purely structural.
- **Max blast radius** — `max_n |reachableFrom(n)|`, the largest number of components affected by changing one. Already computed inside CCD; planned to be retained and printed (#057). The absolute version of the drift rule `DRIFT.6 blast_radius_growth`.

#### Rules

- **Acyclic physical dependencies** — the main rule of the book. The component dependency graph must be a DAG ("levelizable" in Lakos terminology).
- **Levelizability** — each graph node can be assigned a unique level (the length of the longest chain to the leaves).
- **God-headers** — components with an in-degree above a threshold (default 50) are reported as candidates for splitting. A direct consequence of Lakos's maximization of **fan-in/fan-out**.
- **God-component (fan-out)** — the mirror of god-header by out-degree: a component that itself depends on too many. An overload of *output* (an over-complicated consumer), whereas god-header is an overload of *input* (a bottleneck). The same authority (Lakos, "minimize fan-out"). Planned as the default rule `Lakos.GodComponentFanOut` (#057); the threshold is calibrated on the OSS corpus, fan-out has a different distribution than fan-in.
- **Include-chain length** — if the longest `#include` chain exceeds a threshold (default 10) — a warning. Breaks build time and testability.

CCD/ACD/NCCD by themselves are not threshold rules (not "pass/fail"), but they are published in the report and can be used in `--baseline` mode: "NCCD must not grow from commit to commit".

### Level 3. Martin's package metrics (R.C.Martin, 1994)

Classic metrics from "OO Design Quality Metrics" (Robert C. Martin). At the namespace or directory level.

| Metric | Formula | Meaning |
|---|---|---|
| **Ca** | number of modules depending on this one | afferent coupling |
| **Ce** | number of modules this one depends on | efferent coupling |
| **I** | `Ce / (Ca + Ce)` | instability, 0 = stable, 1 = unstable |
| **A** | `abstract types / all types` | abstractness |
| **D** | `\|A + I − 1\|` | distance from main sequence |

Used in PHP Depend, Lattix, NDepend, ArchUnit. A universal language for talking about architecture with people who came through the Java/C# school.

**Optional**, behind the `--martin-metrics` flag. Not part of the defaults for two reasons: (1) for C++ they require heuristics (what counts as an abstract type — a pure abstract class? anything with virtual? a template?), (2) they do not drive early adoption — the user buys "forbid domain → infrastructure", not NCCD. They appear in v0.4 (see Roadmap).

### Level 4. Indisputable practices without explicit authority

Obvious things that nobody will dispute, even without a citation:

- A `.cpp` includes another `.cpp` (almost always an error, except rare unity-build cases; marked via `// archcheck: allow`).
- Including from `src/` into `third_party/` (if the structure is recognizable by path).
- Forward declaration of types from `std::` (UB by the standard, governed by `[namespace.std]`).
- Cyclic dependencies between namespaces (à la "namespace A uses types from B, which uses types from A").

### Rule management

In v0.1 there are no shipped mechanisms for `--disable=...`, `--enable=...` and inline suppression comments.
The configurable surface today is `thresholds:` in `.archcheck.yml` and baseline files.
Pointwise enabling/disabling of rules and suppression comments belong to the future config
surface and should appear only with a dedicated CLI/JSON/test contract.

### Rule classes by source of architectural truth

For a product of the AI-coding era it is important to separate rules not only by topic
(Core Guidelines / Lakos / Martin / custom), but also by **where the
architectural truth** required for the check comes from.

#### Intrinsic rules

The rule is computed from the code, the dependency graph and the diff without knowledge of the domain.
It does not require manual parameterization by the user.

Properties:

- can be run on any repository immediately;
- well suited for zero-config CI;
- the false positive rate must be low;
- the rule must be explainable in a single phrase.

#### Repo-inferred rules

The rule requires an architectural hypothesis, but this hypothesis can be derived
automatically from the structure of the repository, history and the actual dependency
graph.

Later in the document this process — when an AI agent reads the repository's artifacts,
builds an architectural hypothesis and turns it into checkable rules — is
called **AI-assisted rule synthesis**.

**Operational shape (conceptually, the formal contract — see task #010):**
synthesis is a **separate shell flow** that the user invokes on their own
initiative (`archcheck synthesize > .archcheck.yml.draft`), not magic that
happens inside an ordinary `archcheck` run. archcheck itself **invokes no LLMs**:
synthesis is either local heuristics (path-structure /
common headers / facade detection), or a separate wrapper agent (Claude
Code / Cursor / your prompt) reading the same repository and producing YAML.
This preserves the privacy of the code (it does not leave without an explicit action) and
makes the archcheck core deterministic.

Properties:

- the AI agent can propose a parameterization after reading the repository;
- until the user confirms, the rule works as `report-only` or
  `warning`;
- after confirmation the inferred config becomes a normal part of the project.

Typical inference sources:

- directory structure (`include/`, `src/`, `public/`, `private/`, `detail/`);
- the actual entrypoints between subsystems;
- historically established facades;
- the baseline of the current graph.

#### User-declared rules

The rule expresses the intended architecture of the team, which cannot be reliably
guessed from the code.

Properties:

- without explicit configuration it is not enabled;
- it is exactly this class that gives the maximum business value;
- but it is exactly this one that has the highest adoption threshold.

Typical examples:

- `domain` does not depend on `infrastructure`;
- `ui` depends only on `application`;
- raw SQL is forbidden outside the data layer;
- specific third-party dependencies are forbidden in part of the system.

### Drift-regression rules

**Snapshot analysis (intrinsic + user-declared rules on the current state of the repository) remains the core of the product** — it underpins the first run without a config, the dog-fooding on the top-N OSS projects from the v0.5 roadmap, and the entire scope of user architectural contracts. **DRIFT is an additional layer** for the scenario when there is a graph baseline and it makes sense to compare "before" and "after". This rule class catches not "bad code in general", but **structural degradation relative to the baseline**.

DRIFT works in pair with `repo-inferred rules`: synthesis formalizes the architectural hypothesis from the repository, DRIFT then guards that this hypothesis does not degrade. Together they form a working loop of **AI-assisted rule synthesis + drift regression** — two half-cycles of one task (inferring the contract ↔ regression from the contract), both useful in the AI-coding era.

#### Relation to the ordinary baseline

DRIFT does not replace the ordinary baseline of violations — they answer different questions:

- `--baseline` freezes rule violations (`SF.*`, `Lakos`, custom): "have new violations of known rules appeared?"
- Graph-baseline (`--save-graph-baseline` / `--drift-baseline`) stores a snapshot of the dependency graph: "has the structure of the graph itself worsened, even if none of the other rules fired?"

#### Shipped v0.1 drift layer

Drift analysis in v0.1 is deliberately narrow:

- only the intrinsic file-level dependency graph;
- a separate graph baseline (`--save-graph-baseline` / `--drift-baseline`);
- gate only on `DRIFT.1`, `DRIFT.2`, `DRIFT.4.CYCLE`;
- `DRIFT.3`, `DRIFT.4.NEW`, `DRIFT.4.SDP` and pre-existing intrinsic findings — advisory;
- without repo inference and without automatic parameterization by an agent.

Its job is to catch objective structural degradation relative to the baseline, without turning
legacy debt into an endlessly red CI.

#### DRIFT.1 `no_new_shortcut_edge`

**Class:** `intrinsic`

**What it catches:** a new direct dependency edge that shortens an already
existing multi-step path.

**Formalization:**

- the baseline dependency graph of the project is built;
- for each new edge `u -> v` that appeared after the change,
  it is checked whether a path `u => v` of length `>= 2` existed before the change;
- if such a path existed, the new edge is marked as a `shortcut edge`.

**Shipped semantics:** in `--drift-baseline` this is a gating regression. In PR `--diff`
an ordinary added edge stays advisory if it does not create a new/grown cycle
and does not turn a file into a new god-header.

#### DRIFT.2 `no_new_cycle_or_cycle_growth`

**Class:** `intrinsic`

**What it catches:** the appearance of a new cycle or the growth of an existing strongly
connected component.

**Formalization:**

- the baseline and post-change graphs are decomposed into SCCs;
- the rule fires if a new SCC of size `> 1` appeared,
  a self-loop appeared, or an existing SCC grew by the number of nodes / edges.

**Note:** this is the strongest and least disputable candidate for the first
implementation.

#### Next-wave rules

The following drift rules are recognized as promising, but are not part of the first
prototype:

> **Numbering corrected twice:**
> - ID `DRIFT.3` taken by the shipped *bidirectional module coupling* (#087).
> - ID `DRIFT.4` taken by the shipped *lateral_module_dependency* (#118, CYCLE-grade gating).
> The wave of research rules became `DRIFT.5`–`DRIFT.10`; names did not change.

- `DRIFT.5 public_surface_growth`
- `DRIFT.6 blast_radius_growth`
- `DRIFT.7 hub_reinforcement`
- `DRIFT.8 boundary_bypass_of_existing_entrypoint`
- `DRIFT.9 scattered_new_boundary_access`
- `DRIFT.10 hotspot_inflow`

Reasons for deferral:

- `DRIFT.5`, `DRIFT.8`, `DRIFT.9` require repo inference;
- `DRIFT.6`, `DRIFT.7` require threshold tuning and should first live as
  `report-only`;
- `DRIFT.10` requires git history and should be opt-in
  (`--with-git-history` or equivalent).

#### Graph baseline

For `DRIFT.*` the core must rely on a separate graph baseline:

- `--drift-baseline <file>` — load the baseline graph and enable the DRIFT rules (shipped flag name);
- `--save-graph-baseline <file>` — write the baseline graph (shipped flag name).

A git-based mode is acceptable as a convenient adapter on top of this mechanism, but not as
the only semantics:

- `--graph-baseline-from-git <rev>` — optionally, later.

#### Minimal baseline contract

In the first implementation `graph-baseline` must store a canonical snapshot,
not derived metrics:

- format version;
- a normalized list of nodes;
- a normalized list of edges.

All derived quantities (`SCC`, `reachability`, `blast radius`, `hubness`)
must be computed when loading the baseline.

This makes the baseline simpler, more robust and does not tie the storage format to the
current set of drift rules.

---

## Config-based analysis

**The core of the config is `modules` + `rules` (`layers` / `independence` / `forbidden`).** This is a headline feature of **v0.2** ([ADR-001](decisions/001-config-rules-deferred-to-v0.2.md)): in v0.1 the user installs archcheck for its zero-config value (cycles, god-headers, chains, drift gate) — without a single line of YAML; the config contract adds to it the declared module boundaries when the team is ready for it. The default rules (SF.*, Lakos metrics) are always enabled; they will be disablable via a `defaults` block (phase 2).

### Format

The full specification of the `.archcheck.yml` v1 format is [`docs/config_format.md`](config_format.md). There the three rule types (`layers`, `independence`, `forbidden`), their semantics, four reference examples (tiny / layered / legacy / mixed), the list of what is in scope for phase 1, and the SemVer contract of the schema are fixed.

A minimal prudent example for context:

```yaml
version: 1

modules:
  domain: { paths: ["src/domain/**"] }
  app:    { paths: ["src/app/**"] }
  infra:  { paths: ["src/infra/**"] }

rules:
  - type: layers
    name: main-layering
    layers: [app, domain, infra]
```

Extensions at the `defaults`, `thresholds`, `baseline`, `ignore`, pattern-rules level — phase 2, not the current contract. Details and rationale — in `docs/config_format.md`.

### Commands

```bash
# Analysis with defaults, without a config and without compile_commands.json.
# Fast backend — preprocessor scan, works on any project.
archcheck [path]

# JSON output for CI. Check-mode JSON contains a top-level gate and per-finding disposition.
archcheck --format json [path]

# With a custom config: validate .archcheck.yml v1 + apply thresholds.
# Module rules parsed/validated only; enforcement — v0.2.
archcheck --config .archcheck.yml [path]

# Freeze legacy findings; later report only new findings.
archcheck --save-baseline archcheck-baseline.json [path]
archcheck --baseline archcheck-baseline.json [path]

# Graph-baseline drift gate.
archcheck --save-graph-baseline graph.json [path]
archcheck --drift-baseline graph.json [path]

# Canonical PR check.
archcheck --diff [--diff-mode=disk|memory] [--format=text|json] <revspec> [path]

# Advisory-only modes.
archcheck --duplication <path>
archcheck --history <path>
archcheck --scan <path>
archcheck --graph <path>
```

Future-only surfaces: `--with-clang`, `--compile-commands`, `--format sarif`,
`--output`, `--metrics`, rule toggles and suppressions.

### Exit codes

- `0` — no gated finding/regression. Advisory findings may be in the report.
- `1` — a gated finding/regression found.
- `2` — configuration or parsing error.
- `3` — internal tool error.

---

## Stability contract

After the `v1.0` release the following interfaces are public and subject to [SemVer 2.0](https://semver.org/spec/v2.0.0.html). Any breaking change → MAJOR bump.

| Interface | Breaking change (MAJOR) | Non-breaking (MINOR) |
|---|---|---|
| Exit codes | removing a code, changing the semantics of an existing one | adding a new code |
| CLI flags | removing a flag, changing behavior, renaming | adding a flag with a default value |
| Report JSON schema | removing a field, changing a type | adding a field |
| SARIF output | changing the schema after SARIF appears | adding fields (within the SARIF spec) |
| YAML config format | removing a key, changing type/semantics | adding a key with a default |
| Baseline file format | changing the schema (breaks existing baselines) | extending the schema with backward-compatible reading |
| Set of default rules | adding a rule that breaks previously green projects | adding an advisory-only rule or a rule with separate opt-in |

**Before v1.0** (`0.x.y`) — SemVer §4 permits breaking changes in MINOR. We strive to avoid them, but they are not forbidden. Any breaking change is recorded in the [`CHANGELOG.md`](../CHANGELOG.md) `Changed` section with the marker `**Breaking:**`.

---

## Tool architecture

### Tech stack

- **Language:** C++20.
- **Parsing:** two backends (see below). Fast — preprocessor scanning (its own parser of `#include` directives and comments), without AST dependencies. Slow — libclang / libtooling (LLVM) for AST semantics. tree-sitter was considered — it gives only a syntax tree without semantics, insufficient for semantic rules.
- **Config:** YAML via `ryml` (FetchContent, no system install).
- **Build:** CMake + Ninja.
- **CI:** GitHub Actions.
- **Tests:** Catch2 v3.

### Two-backend scheme (the decision for v0.1)

For include-only rules (SF.7, SF.8, SF.9, cycles, god-headers, chain length) libclang is excessive — these rules are solved by preprocessor scanning. libclang is needed only for semantic rules (SF.2, SF.5, SF.10, SF.21, Martin abstractness, rules from the C/I sections of Core Guidelines).

**Decided: two backends, fast by default.** Confirmed by measurement (#043, see [`docs/dev/spike_clang_perf.md`](dev/spike_clang_perf.md)): on spdlog (141 TU) libclang-19 single-thread = ~15 s, regex baseline = ~11 ms — a difference of ×1350. On a monorepo of ~5000 TU libclang-only single-thread = ~9 minutes, which is incompatible with a CI guard job of "seconds per PR".

- **Fast backend (preprocessor-only)** — the default and the only shipped backend in v0.1. Works without `compile_commands.json`, instantly, on any build system. Covers the critical-path rules of v0.1: SF.7/SF.8/SF.9, Lakos.GodHeader, Lakos.ChainLength, graph-baseline drift and `--diff` graph signals.
- **libclang backend** — v0.2+ opt-in. Needed for AST checks: SF.2 / SF.5 / SF.10 / SF.11, rules from sections C and I of Core Guidelines, Martin's *Abstractness* (A).

Advantages of the decision:
- **Low entry threshold.** The first run on a new project — without configuring CMake and `compile_commands.json`.
- **Performance.** Large projects get a fast check of the main rule set without the libclang overhead.
- **Reduces MVP scope.** v0.1 does not depend on libclang for its main value.

Trade-off: the semantic Level 1 rules (SF.2, SF.5, SF.10, SF.11) shift from v0.1 to v0.2. This is a conscious choice — better to defer semantics than to require `compile_commands.json` on the first run.

### Dependencies — minimum

Only libclang and a YAML parser. No boost, no heavy graph libraries (the graph is `unordered_map<NodeId, vector<NodeId>>`, nothing more is needed).

### Distribution

- A single static binary via GitHub Releases.
- A Docker image for CI.
- Pre-built for Linux x86_64, Linux arm64, macOS arm64, Windows x64.
- Optionally later: a Homebrew formula, a vcpkg port, an AUR package.

### High-level code structure

```
src/
  main.cpp                   # entry point, CLI parsing
  config/
    config_loader.cpp        # YAML -> internal Config (+ v1-schema validation)
  scan/
    include_scanner.cpp      # fast preprocessor-only backend (v0.1, default)
    include_resolver.cpp     # resolving directives against the project tree
    project_files.cpp        # tree walk, vendor/test filters
    duplication/             # token clone detector (advisory --duplication)
    clang_scanner            # libclang-based semantic backend — planned v0.2
  graph/
    dependency_graph.cpp     # graph, SCC/cycles (called component_graph in spec v2.0)
    algorithms.cpp           # levelization, CCD/ACD/NCCD (called metrics in the spec)
    baseline.cpp             # graph baseline (deterministic YAML snapshot)
    diff.cpp                 # added/removed edges, grown SCC
  rules/                     # flat directory, one file = one rule
                             # (grouping core_guidelines/lakos/martin/custom
                             #  did not materialize — see CLAUDE.md)
  report/
    text_reporter.cpp
    json_reporter.cpp
    sarif_reporter           # planned v0.2
  diff/
    regression_report.cpp    # --diff: structural regressions between git revisions
  git/
    git_state.cpp            # fork/exec git, worktree
    git_object_file_source.cpp
tests/
  integration/               # sample projects with known violations
  unit/
```

One rule = one class implementing the `IRule` interface. Registration — the factory `makeDefaultRuleSet`/`makeDriftRuleSet` in `rules/rule_set.cpp`: adding a rule = a new pair of files + one factory line; existing rule files are not edited.

---

## Dogfooding and marketing strategy

### Regression check on other people's repos

A job is added to CI that runs the tool once a week on the top-20 popular C++ open source projects and publishes a report. Candidates:

- `fmt` — small, clean, a baseline with few violations.
- `Catch2` — header-only.
- `nlohmann/json` — a heavy template hierarchy.
- `folly` — large, corporate.
- `abseil-cpp` — corporate, cleanly written.
- `spdlog` — medium, clean.
- `grpc`, `protobuf` — large, with codegen.
- `opencv` — huge, legacy.
- `llvm-project` — the analyzer analyzes itself.
- `boost` — a separate universe.

#### What this gives

- Regression testing. If the tool broke on a familiar project — that's a signal.
- A speed benchmark. The README will have real numbers on real projects.
- PR material. Found cycles / violations → an issue in their repos → "contributed to LLVM/folly/fmt" on the author's resume.
- **A ready HN/Reddit post.** A table of NCCD across the top-20 C++ projects is news in itself, carrying the tool along.

### Public launch plan

1. **MVP in private**, 2-4 weeks.
2. **Soft launch.** The repo is public, the README is ready, no marketing. Testing on 3-5 familiar projects.
3. **A comment in TNG/ArchUnit issue #242:** "FYI, started this, here's the repo". The issue is closed, but that is exactly why it is rarely commented on, and any user googling "ArchUnit C++" ends up there. SEO weight has been accumulating for six years — a free adoption channel.
4. **A Hacker News post.** Title: "Show HN: archcheck — Lakos metrics and Core Guidelines checks for C++". A screenshot with metrics on llvm-project / boost. Subtitle: the first open source ArchUnit-style tool for C++ in 6 years.
5. **Reddit `/r/cpp`** — in parallel.
6. **Twitter/X:** tag Anastasia Kazakova (CLion PM), TNG (creators of ArchUnit), Bartek Filipek (`bfilipek.com`, actively writes about Core Guidelines), John Lakos personally (he is active in the C++ community).
7. **CppCast / cpp.chat.** Prepare a 5-minute pitch.

---

## Roadmap

### v0.1 — MVP: zero-config physical design + trusted PR diff

**Cover story:** a run on someone else's project, without `compile_commands.json` and libclang, gives a useful
result immediately: a narrow gate on objective structural regressions and an advisory layer for
the rest of the risk.

- **Fast backend** (preprocessor-only) — the only one.
- YAML config: parsing and validation of the v1 schema (`layers`/`independence`/`forbidden`) shipped; **enforcement of module rules moved to v0.2** (#045/#073) — `--config` is so far validate-only + `thresholds:`.
- **Core rules:**
  - Dependency cycles (SF.9).
  - God-headers (Lakos): in-degree > threshold (default 50).
  - Include-chain length (Lakos): > threshold (default 10).
  - SF.7 (using namespace in `.h` — text-scan, approximate).
  - SF.8 (include guards / `#pragma once`).
- **`--baseline` from day one**: freeze the current findings; in check-mode the gate stays SF.9.
- **`--diff` PR workflow**: new/grown cycles and new god-headers gate; structural/hygiene drift advisory.
- A text report with colored output in a TTY + a JSON report (`check` and `--diff`).
- Exit codes (see §Exit codes).
- Basic CI on GitHub Actions for archcheck itself.

**Goal:** the user can clone a repo, run `archcheck`, get a meaningful short report,
freeze legacy debt with a baseline and embed `--diff` into CI. Without CMake dances, without libclang.

### v0.2 — libclang backend + the rest of SF (3–4 weeks)

- **Runtime enforcement of YAML module rules** (`layers` / `independence` / `forbidden`).
- **The libclang backend** becomes an opt-in mainline (via `--with-clang`).
- The rest of the SF rules: SF.2 (no defs in headers), SF.5 (`.cpp` includes its `.h`), SF.10 (no implicit includes), SF.11 (self-contained headers).
- A precise version of SF.7 (via AST instead of text-scan).
- **SARIF output** for GitHub Code Scanning.

### v0.3 — Rules from the C / I / NL sections of CCG + BDE + AI loop (3–4 weeks)

See [docs/research/rules/](research/rules/).

- **C:** C.121 (interface pure abstract), C.133 (no protected data), C.134 (uniform access level).
- **I:** I.2 (no mutable globals), I.3 (no singletons), I.22 (no complex global init).
- **NL:** NL.27 (file suffix).
- **SF.21** — anonymous namespace in `.h`, default-ON (preview in v0.2 via `--with-clang`; moved from v0.1 — not related to physical-design / drift, see [`backlog/future/050`](../backlog/future/050_min_sf21_anonymous_namespace.md)).
- **Bloomberg BDE:** no-inter-component-friendship, external-linkage-declared-in-header.
- Others: forward-decl-of-std, deep-nested-namespace.
- ~~**DRIFT.1 + DRIFT.2** — the first prototype of drift-regression rules~~ — **already shipped in v0.1** together with advisory DRIFT.3 (bidirectional coupling): `--drift-baseline` works as a regression gate, only DRIFT.1/2 fail a run (#086, #087). The item is left as a trace of the original plan.
- **AI-assisted rule synthesis contract** — `archcheck synthesize` subcommand, heuristic mode + wrapper-prompt mode, output `.archcheck.yml.draft` with provenance comments. The implementation is done in a separate task after the contract is fixed (#010).

### v0.4 — Martin metrics + distribution polish (3–4 weeks)

- **Martin metrics:** Ce, Ca, I, A, D at the namespace level — optional, behind a flag. We enable it **only if users ask**: for early archcheck adopters hard rules are more valuable than dashboard metrics.
- Custom pattern rules: regex over text (raw SQL outside data layer, etc.).
- A pre-commit hook out of the box.
- A Docker image.
- A static binary in release artifacts for Linux x86_64/arm64, macOS arm64, Windows x64.
- A GitHub Actions workflow example.

### v0.5 — Templates and community (1–2 months)

- **Templates** for clean / hexagonal / onion / layered architectures (ready-made `arch.yaml`).
- A regression check on top-N OSS projects (fmt, Catch2, spdlog, abseil, folly, etc.).
- Full documentation: getting started, configuration reference, all rules, comparison with alternatives.
- A migration guide from CppDepend and Tomtom/cpp-dependencies.

### Long-term (6–12 months, optional)

- A plugin API for custom rules.
- Optional graph visualization (graphviz output, not a GUI).
- C support (if there is demand).
- Lakos hierarchical reuse metrics.
- An optional bridge to the clangd index for speed on large projects.

### What we do not do

- **`--suggest-config`** — dropped from the roadmap. Auto-inference of module structure is either trivial (by directories — the user writes it in 5 minutes), or magic that won't be trusted. If a concrete request arises later — we will reconsider.

### Development progress

Real dogfood runs and achievements are logged in `milestones.md` in the private
[archcheck-journal](https://github.com/blurman-ai/archcheck-journal) companion repo (#167).

- **2026-05-26 — development start.** The scan + graph subsystem is complete (`scan_includes`, `discover_files`, `include_resolver`, `DependencyGraph`, iterative Tarjan SCC). The preview CLI `archcheck --graph <path>` works end-to-end. The first external run: the `gm` project (2192 files, 7.7 s), 2 real cycles found in Unigine SDK headers.

---

## Key risks and open questions

1. **Libclang performance (v0.2+).** On large projects the libclang backend may be slow — confirmed by measurement #043 ([`docs/dev/spike_clang_perf.md`](dev/spike_clang_perf.md)): ~73 ms median/TU, single-thread ~3.5 min on 2000 TU. Mitigation: the fast backend is the default, libclang only opt-in for semantic rules; parallelism over TU (a per-thread libclang index); AST caching; a clangd index bridge — a long-term option.
2. **Compile commands availability (v0.2+).** Some projects do not generate `compile_commands.json` out of the box (especially MSBuild). Mitigation: the fast backend works without it, so the risk concerns only users who enabled `--with-clang`. Documentation on generation for CMake / Bazel / Meson.
3. **Usability of the default rules.** Too strict → the user will see 5000 violations and close the tab. Mitigation: the default rules are conservative (only SF.7/8/9/21 + cycles + god-headers + chain length), plus `--baseline` from day one.
4. **C++ templates (v0.3+).** Template classes upon instantiation create non-obvious inter-module dependencies. Plan: in v0.1 the rules work over includes (this is enough for cycles / god-headers / SF.7/8/21); in v0.3+, when the AST rules arrive, analyze the template declaration; instantiations — long-term.
5. **"Abstract type" for Martin's A (v0.4).** Pure abstract? Anything with virtual? Template? Document the heuristics, provide settings. Not a blocker — Martin metrics are optional.
6. **AI-assisted rule synthesis: correctness and privacy (v0.3+).** Three risks of one process:
   - **False architectural hypothesis.** The agent reads `src/utils/` and decides it is the entry point layer, or declares `domain` dependent on `infrastructure` because it saw one transitive include. Mitigation: the synthesis output always starts in `report-only` / `warning`, requires an explicit `confirm` from the user; the output is marked with `# synthesized: <hypothesis>` comments so the origin of each rule is visible.
   - **Privacy.** Synthesis by definition means "something reads your code". Mitigation: the archcheck core does not invoke an LLM itself (see §"AI-assisted rule synthesis" / operational shape) — it is a separate shell flow, the user decides which agent/API to trust with the sources. By default synthesis is local heuristics, without the network.
   - **Volatility.** The same repo + the same agent → different synthesis results from run to run (due to LLM non-determinism). Mitigation: synthesis produces a diff against the previous `.archcheck.yml.draft`, does not rewrite it wholesale; the final config is always an explicit user action.

---

## Success metrics

### 3 months after the public release

- 50+ stars on GitHub.
- 5+ external issues or PRs (not from the author).
- 1+ mention in a blog post or podcast from a third party.
- Documentation covers all the basic scenarios.

### After 6 months

- 200+ stars.
- An active regression check on 10+ known C++ projects in CI.
- 2-3 regular contributors.

### After 12 months

- 500+ stars.
- A stable 1.0 release.
- Used in at least 5 public C++ projects for CI.

---

## Development principles

- **YAGNI.** We do not build a feature until a concrete user asks for it.
- **Single responsibility.** The tool checks architecture. It does not visualize, does not format, does not look for bugs.
- **CLI-first.** No GUI, no web dashboard. Run from a shell, output — text or JSON.
- **Zero-config first.** A run without arguments gives a meaningful result.
- **Single binary.** v0.1 ships as a standalone CLI; libclang is future/optional.
- **Authority over opinion.** All default rules have attribution (Core Guidelines / Lakos / Martin). No "in my opinion".
- **Boring technologies.** C++20, libclang, YAML, GitHub Actions.
- **Documentation as code.** Documentation in the repo, generated into gh-pages. No separate site.

---

## Next steps

The current release-readiness status lives in [`backlog/TASK_TRACKER.md`](../backlog/TASK_TRACKER.md).
This spec does not duplicate the manual list of blockers, so it does not go stale.

The nearest product line after v0.1:

1. Bring the public applicability of the zero-config first run on real repos with bundled deps
   (vendored/generated exclusion and corpus sign-off) to completion.
2. Release v0.1 with the shipped CLI contract: check/drift/diff gate/advisory semantics,
   text/JSON output and baseline workflows.
3. Start v0.2: runtime enforcement of YAML module rules, the libclang semantic backend,
   SARIF and a rule-management surface.

---

## Sources

- [C++ Core Guidelines, section SF](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-source) — Stroustrup, Sutter.
- John Lakos. *Large-Scale C++ Software Design.* Addison-Wesley, 1996.
- John Lakos. *Large-Scale C++ Volume I: Process and Architecture.* Addison-Wesley, 2019.
- Robert C. Martin. *OO Design Quality Metrics: An Analysis of Dependencies.* 1994. [PDF](https://linux.ime.usp.br/~joaomm/mac499/arquivos/referencias/oodmetrics.pdf)
- [ArchUnit issue #242 — "Does an archunit exist for c/c++"](https://github.com/TNG/ArchUnit/issues/242)
- Dente, Satriani, Papotti (EURECOM). *Constraint Decay in LLM-Assisted Code Generation.* May 2026.
- GitClear. *AI Copilot Code Quality.* 2024 (updated 2025). PDF.

---

*Version 2.4. The document is reworked as the discussion proceeds; for concrete changes see the list in the header.*
