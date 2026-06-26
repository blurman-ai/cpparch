# [AI][CONFIG][V1] Rules for the agent that fills in `.archcheck.yml.draft`

**Created:** 2026-05-28
**Started:** —
**Status:** new
**Module:** DOCS, CONFIG
**Priority:** major
**Difficulty:** M (contract + examples + anti-slop guardrails)
**Target release:** v1 phase 1 (post-MVP)
**Blocks:** practical use of AI synthesis without hand-writing the config
**Blocked by:** future/v1_maj_config_format_minimal_contract.md
**Related:** future/v1_maj_ai_config_synthesis_eval_protocol.md, future/010_maj_ai_rule_synthesis_contract.md, future/v1_maj_ai_config_iterative_loop.md (iterative run on top of these authoring rules), #053 / #056 (dup spikes — empirical source of rules for noise files and exclude, see section below), #054 (run over 50 AI repos — source of rules for noise LINES, `experiments/ai_repo_run/`)

## Goal

Pin down the rules by which the agent fills in `.archcheck.yml.draft` — without inventing
architecture and without disguising guesses as facts. The agent writes only `.draft`, not the final config.

## Target agent output format

The agent fills in `.archcheck.yml.draft` using the schema from `v1_maj_config_format_minimal_contract.md`.
Priority of rule types:
1. **`layers`** — if there is a directional hierarchy, use it; do not expand it into N×(N-1) `forbidden` pairs.
2. **`independence`** — if modules at the same level should not know about each other.
3. **`forbidden`** — pointwise prohibitions that do not fit into layers.

Every rule and every module gets a YAML comment with its source (`observed` / `inferred` / `speculative`).

## Allowed sources of truth

- The real file structure (paths, directory names)
- The include graph (who actually includes whom)
- README / ARCHITECTURE.md if present
- Explicit naming conventions (prefixes, suffixes)

## Forbidden agent behavior

- Inventing layers that are not in the repo (no `domain/` → do not write a `domain` module)
- Carrying architectural patterns over from other projects without evidence
- Making a `forbidden` rule on weak grounds (`speculative` → only `# TODO`, not a rule)
- Hiding uncertainty: all `speculative` fields must be explicitly marked

## Confidence levels

| Level | When | In .draft |
|---------|-------|----------|
| `observed` | Directly visible in the file structure or include graph | Written as a rule |
| `inferred` | Follows from naming/README, not confirmed by the graph | Rule + `# inferred` |
| `speculative` | Common pattern, no evidence in the repo | `# TODO` comment, not a rule |

## What a human must verify before accept

- Every `modules.*.paths` — do the files actually exist
- Every `layers` / `independence` rule — does it not break existing code
- All `# inferred` and `# speculative` comments — confirm or delete

## Example of a good draft

```yaml
version: 1

modules:
  core:
    paths: ["include/spdlog/**"]            # observed: directory exists
  sinks:
    paths: ["include/spdlog/sinks/**"]      # observed: directory exists
  details:
    paths: ["include/spdlog/details/**"]    # observed: directory exists

rules:
  - type: layers
    name: spdlog-layering
    layers: [sinks, core, details]          # inferred: sinks include core, details is low-level
    # TODO: verify sinks do not include details directly

  - type: independence
    name: sinks-independent
    modules: [sinks]                        # speculative: typical pattern, not verified
```

## Example of a bad draft (forbidden)

```yaml
# DO NOT DO THIS — no such directories in the repo:
modules:
  presentation:
    paths: ["src/presentation/**"]    # BAD: does not exist
  business_logic:
    paths: ["src/business/**"]        # BAD: invented

rules:
  - type: forbidden
    name: no-circular                 # BAD: too broad, no evidence
    from: [business_logic]
    to: [presentation]
```

## Addendum from the dup-spike experience #053/#056 (noise files and exclude)

> Source: runs of the fast-backend duplication spikes on real trees
> (cpparch `src/`, and especially `ttcg` — 15388 files, of which 315 MB are vendored
> `Src/packages/boost…`), 2026-05-30. For empirical background on exclude see also
> `experiments/line_duplication/FILTER_CLASSIFICATION_REPORT.md`.

### Why this concerns the config agent

Without cutting off noise files, the first run **drowns in them**: on `ttcg` without
`--exclude /packages/` the entire top of the duplicates list is boost `mem_fn_template.hpp`
template twins, zero signal about the project's code. The agent faces the same risk: it will see
`Src/packages/`, `build/`, `*_autogen/` as existing directories and, by the
current rule `observed: directory exists`, will record them as modules. **This is
false architecture.** The existence of a directory is a necessary but NOT sufficient
condition: you also need to confirm that it is **first-party**, not vendored/generated.

### What the agent MUST account for (add to the prompt)

- **Do not make modules out of vendored/generated/build dirs.** Hard noise classes
  (always outside the architecture, candidates for `exclude`, not for `modules`):
  `packages/`, `third_party/`, `3rdparty/`, `vendor/`, `bundled/`, `extern/`,
  `deps/`, `_deps/`, `single_include/`, `amalgamate*`, `dist/`,
  `*_autogen/`, `Generated/`, `moc_*`, `qrc_*`, `ui_*`, `*.g.cpp`,
  `build*/`, `cmake-build-*/`, `CMakeFiles/`.
- **Soft classes — by context, not blindly:** `test/`, `tests/`, `examples/`,
  `sandbox/`, `legacy/`, `upgrade/`. On `ttcg`, `Src/NTPROSurfaceSplitSandbox/`
  contained real code (`mesh_splitter.h` was copied there verbatim), so
  "sandbox" cannot be cleaned out automatically. Mark with `# inferred` / `# TODO`,
  do not drop silently.
- **Parallel near-duplicate trees are a duplication signal, NOT modules.**
  On `ttcg`: `Triangulation` vs `Triangulation_int`, `PolyPolygon` vs
  `PolyPolygon_int` — float/int copies (token-for-token identical, `plain=1.0`).
  The agent **must not** set up `*_int` as a separate layer/module: this is a missing
  reuse edge. The correct reaction is `# TODO: likely duplicated tree (see
  duplication pass), not a separate layer`, not two symmetric modules.
- **Refine the `observed` level:** "directory exists" is too weak for the
  vendored trap. `observed` = the directory exists **AND** is first-party **AND**
  participates in the include graph (has incoming/outgoing edges within the project).
  Otherwise — `inferred` at most.
- **`observed` from the include graph is reliable only on CLEANED lines.** A graph
  built from raw text catches false edges from license headers,
  commented-out includes, `R"(...)"` data, and `#if 0` blocks (see the section
  on noise lines below). Before promoting a finding to `observed`, the graph must
  walk the same cleaned lines as the dedup pass.

### Noise-LINE classes (not just files) — from run #054

> Source: a run of the fast-backend duplication spike over 50 real C++ AI repos
> (AIDev, `experiments/ai_repo_run/`, baseline `runs/v5_if0/`), 2026-05-30.
> All 6 classes confirmed on the corpus sources. This extends the section on
> noise FILES above down to the level of individual lines: even in a first-party file, some
> lines are not code and produce false cross-file "duplicates".

- **License headers `/* ... */`** (MIT/Apache text at the start of the file) produce
  a false cross-file "duplicate" block. Example: `mqtt_client` — a 19-line "duplicate"
  turned out to be MIT license text, not code. → dedup must **stateful**-strip
  block comments (like the production SF.7, `src/rules/sf7_using_namespace.cpp`), not
  only line `//`.
- **Case of vendor folders.** ALL variants occur in the corpus:
  `thirdParty / ThirdParty / 3rdParty / External / Submodules`. The
  exclude/skip matching must be **case-insensitive** (alpaka: 62%→20% after the fix).
- **Embedded raw-string data `R"( ... )"`** — shaders/SQL/scripts baked into a
  C++ string. Example: Effekseer, GL ShaderHeader. The contents are data, not code;
  strip the raw-string body.
- **`#if 0` / `#if false` dead blocks** — e.g. fxc disassembly listings under
  `#if 0` (Effekseer DX): 33%→28% after skipping. Skip with nesting taken into account.
- **Numeric data arrays** `const BYTE g_main[] = {69,70,88,...}` — shader byte-code
  as numbers, NOT under `#if 0`. A line-based strip does NOT fix this → handed off
  to **#056** (partial-duplication pass).
- **`#include` blocks and header↔impl headers** — identical `#include` lists between
  files and between `X.h`/`X.cpp` = shared dependencies, not a reuse edge. Skip
  `#include`. The header↔impl remainder (bodies/constructor parameters) is a
  cross-**component** signal, addressed by **#053 P0-B** (cross-component map),
  not line-level.

### Example of a bad draft — a vendored dir as a module (add to the doc)

```yaml
# DO NOT DO THIS — the directories exist, but this is not the project's architecture:
modules:
  boost:
    paths: ["Src/packages/boost.1.76.0.0/**"]   # BAD: vendored, belongs in exclude, not module
  autogen:
    paths: ["**/*_autogen/**"]                   # BAD: generated
  triangulation_int:
    paths: ["**/Triangulation_int/**"]           # BAD: duplicate copy of the float branch, not a layer
```

### Possible additional fields (for the deduplicator and beyond)

These fields go beyond the minimal contract (`version/modules/rules`) and belong
to the config format + passes #053/#056 — I record them here as input for their design:

- **Top-level `exclude:`** (glob list) — shared by the scanner, the duplication pass, and
  the rules. The single highest-impact field: without it the first run = solid noise.
  The agent fills it with the hard classes it discovers above (with a source
  comment); this is its direct output, on par with `modules`.
- **Deduplicator block** (draft, on #053/#056): `duplication.exclude`
  (on top of the shared one), `similarity_threshold`, `min_tokens`; marking intentional
  twins goes into the #053 baseline model, not the agent draft.
- The agent does NOT set dedup thresholds itself (`speculative`) — it only proposes
  a `# TODO` with justification; the decision is the human's.
- **Noise-line strip rules are deduplicator behavior BY DEFAULT, not agent config.**
  Stripping block-comment / raw-string / `#if 0` / `#include` and
  numeric-array (the last one — #056) is built into the pass; the agent does **not** set them
  and does not propose them: this is not an architectural decision but detector hygiene. They
  do not appear in the draft at all.
- **`exclude` matching must be case-insensitive** — fixed as a
  requirement of the format (corpus #054 contains `thirdParty/ThirdParty/3rdParty`
  intermixed). The agent writes the noise class as one variant, the engine matches
  case-insensitively.
- Cross-ref for specific classes: numeric data arrays → **#056**;
  header↔impl / cross-component remainder → **#053 P0-B**.

### OSS-sweep empirics: third-party is detected NOT by name

> Evidence base: `experiments/partial_duplication/OSS_SWEEP_REPORT.md` —
> a dup pass over 19 OSS repos. The main lesson is precisely about third-party / generated.

- **Vendored code is often without a `third_party/`/`vendor/` marker** — it sits in ordinary
  subfolders inside `src/`. BambuStudio: `src/mcut/`, `src/minilzo/`, embedded
  `src/nlohmann/`. A substring-exclude on `third_party`/`vendor`/`packages` does NOT
  catch them → they surfaced as "findings" (Bambu: 180 pairs, almost all inside these
  libraries) and would have surfaced for the agent as false `modules`.
- **Generated code is project-specific.** grpc: `src/core/ext/upb-gen/**/*.upb.h` produced
  **7712** "duplicates" (generated code is identical by construction). A `.pb.` mask
  misses it — upb has its own extension `.upb.h`. A token-LCS confirm does NOT help here:
  the code really is identical, the only cure is exclude.
- **Takeaway for the agent:** a fixed exclude list cannot be trusted. Detect vendored and
  generated code by **signals**, not by directory name: a foreign project's
  license header, banners `// DO NOT EDIT` / `@generated` / `generated by …`,
  the absence of incoming include edges from app code (isolated cluster / sink),
  a foreign style/naming. What is found → into `exclude` with a source comment;
  the uncertain → `# TODO`. A direct extension of `observed`: a directory exists
  ≠ it is yours ≠ it is hand-written.

## Execution plan

- [ ] Write `docs/ai_config_authoring_rules.md`: allowed sources, forbidden behavior, confidence levels
- [ ] Add to the doc a section on noise files: hard/soft exclude classes, the vendored trap, parallel duplicate trees (from #053/#056)
- [ ] Add to the doc a section on noise LINES (from #054): license headers `/* */`, case of vendor folders, raw-string data, `#if 0`, numeric arrays (→#056), `#include`/header↔impl (→#053 P0-B)
- [ ] Pin down the requirements: case-insensitive `exclude` matching; strip rules = engine default, not agent-draft; the graph for `observed` — on cleaned lines
- [ ] Refine the definition of `observed` (first-party + include graph, not just "directory exists")
- [ ] Reconcile with the config format the top-level `exclude:` and the draft `duplication` block (cross-ref #053/#056)
- [ ] Add 2 full examples: good draft and bad draft with annotations
- [ ] Pin down the minimal output contract: what must be in each `.draft` field
- [ ] Describe the checklist for the human before accept
- [ ] Tie in with the `synthesize` command design once it appears

## Done

- (empty)

## Changed files

| File | Change |
|------|-----------|
| docs/ai_config_authoring_rules.md | contract for the agent (incl. the section on noise files / exclude from #053/#056) |
| README.md | brief explanation of the `.draft` workflow after stabilization |
| (config format) | top-level `exclude:` + draft `duplication` block — reconcile with #053/#056 |
