# [SCAN][DUPLICATION] Raise `--duplication` precision to gate-grade

**Created:** 2026-06-05
**Started:** 2026-06-05
**Status:** future (target release: v0.2+ — gate-grade precision requires the semantic layer)
**Module:** SCAN/DUPLICATION
**Priority:** major
**Complexity:** L
**Blocks:** an optional blocking `--duplication` gate in CI
**Blocked by:** P2 semantic/LLM confirm layer (v0.2) — see validation below
**Related:** #082 (alignment umbrella — parent, Slice 5b), #070 (FP fix proposals + spec of the header-impl gate), #071 (FP/TP classification), #078 (co-change harm-signal / severity)

## Goal

Raise the precision of the duplicate detector to a level where `--duplication` can
safely be made a **blocking CI gate** (fail on a duplication regression), not just an
advisory report.

## Context (where the task comes from)

Within #082 (Slice 5b) duplication was deliberately shipped as an **advisory reporting
capability** (`--duplication` — report-only, always `exit 0`, does not block CI). This is
an honest framing: [docs/product_vision.md](../../docs/product_vision.md) directly states
that **precision is insufficient for a trusted CI gate** (idiom-floor ~40 FP, irremovable
without semantics). Raising precision is **algorithm work**, which #082 moved out of
scope ("Don't improve the duplication algorithm itself 'along the way'"). This task is about what #082
deferred.

## What's needed

1. Implement an honest **P1.3 header-impl gate** (currently removed as a no-op; a ready
   spec — in [#070](../wip/070_maj_checker_fp_fix_proposals.md): matched-span ≥70%
   of decl tokens → suppress).
2. Measure precision on a labeled corpus (see also #084 — the evaluation harness).
3. Evaluate **P2 semantic/LLM confirm** (marked `⏳ v0.2` in the CHANGELOG) as a path to break through the
   idiom-floor.
4. Only upon reaching gate-grade precision — add a gating option
   (`--duplication --gate` or a threshold), with an explicit exit contract.

## Acceptance criteria

- [ ] Measurable precision on a corpus with a fixed methodology.
- [ ] P1.3 (or equivalent) implemented and covered by fixtures/tests.
- [ ] Decision made: is precision sufficient for an optional gate; if yes —
      the gate mode is implemented with an explicit exit code and documented in all layers.
- [ ] If precision is still insufficient — this is explicitly recorded, `--duplication`
      stays advisory, the task is closed with an honest conclusion.

## Notes

- Don't break the current advisory contract (`exit 0`) by default — gating is opt-in only.
- Fixtures are mandatory (project rule).

## Scoping / reconnaissance (2026-06-05, at start)

The scanner's data structures for implementing P1.3 were analyzed:

- `Fragment` ([include/archcheck/scan/duplication/fragmenter.h](../../include/archcheck/scan/duplication/fragmenter.h))
  provides `seq` (ordered normalized tokens), `rawSeq` (raw spellings,
  aligned with seq), `normLines`, `diversity`, `tokenCount`. This is enough to
  count declaration markers (`virtual`/`override`/`final`/access-specifier/`Q_OBJECT`/
  pure-`= 0`) and control flow (`if`/`for`/`while`/`return`/`switch`).
- Existing P1 classifiers (`phase10DataTableClassifier`,
  `phase11BoilerplateDensity`) — clean ~15-25-line functions: P1.1 down-weight
  (`p.weighted *= k`), P1.2 filter (drop). P1.3 will follow the same pattern.
- **Important:** `extractFragments` emits `)`-`{` bodies (function/control bodies), not
  clean declarations. So the header-impl FP arises in specific forms (inline bodies
  in .h, ctor init-lists, parallel definitions). The exact form needs to be seen
  on the 6 labeled header-impl examples from the corpus.

### Why the implementation wasn't done at start (honestly)

1. **The recall gate requires the corpus.** The #070 spec directly requires a run against 195 TP with
   a TP-retention metric and a rollback on recall drop. The corpus (`fp_corpus_r2.tsv`,
   6 header-impl FP) — in an **untracked `experiments/`**, not hermetic. Merging a precision
   suppressor without measuring recall is a violation of the #082 ethos (don't ship the unvalidatable).
2. **The decl-vs-executable heuristic is non-trivial** on tokens (distinguish `void foo();` from
   `foo();` without a parser). An error → either no benefit, or suppression of real TP.

### Implementation plan (when the corpus is available)

1. Look at the 6 labeled header-impl FP → understand the actual form of the fragments.
2. Implement `phase12HeaderImplGate` (name is free — the no-op was removed in #082) as a
   conservative classifier: declRatio ≥0.7 **AND** <2 executable statements → suppress.
3. Fixtures: `fixtures/duplication/header_impl/pass/` (a real logic clone — do NOT suppress),
   `fixtures/duplication/header_impl/fail_*/` (parallel declarations — suppress).
4. Run recall-retention against the corpus; roll back on a recall drop > threshold.
5. Only upon sufficient aggregate precision — decide on the opt-in gate (item 4 above).

**Status:** started, scoped.

### Clarification (2026-06-05): the corpus IS IN PLACE

The earlier "corpus unavailable" is **incorrect**: `experiments/` is a local untracked
sandbox, and on this machine it exists. The corpus is present:
`experiments/verification/fp_corpus_r2.tsv` (160 KB, 197 labeled lines, incl.
**6 header-impl FP**). Header-impl examples: platform-split stubs (os_macos vs os_linux,
`return accept-all`), Qt-test parallel decls (Q_OBJECT/initTestCase), NVI-lifecycle
skeletons (OnComponentCreated/...).

**Main difficulty (uncovered by the corpus):** a header-impl FP on tokens is **indistinguishable**
from a structural TP. Right next to it in the corpus is the TP "AKP05 cloned from AKP03" — structurally
identical (byte-identical methods), but it's a real copy-paste. A stub `return true;` vs
real logic `return computeX();` — without AST/validation they can't be separated. A crude
token heuristic gives noise for marginal gain (which is why the P1 classifiers are marked
"requires validation" in the CHANGELOG).

**Recall-safety is partly solved:** the pipeline drops by weighted only in
`phase8JointTokenOrderFloor` (BEFORE the P1 block); the P1 classifiers run last. So
P1.3 as a **down-weight** (not drop), inserted after `phase11`, is recall-neutral — worst
case: a structural TP is ranked slightly lower in the advisory output, but is still
reported.

**The right path (a focused session, not an autonomous burst):**
1. Run the detector on the labeled repos of the corpus (harness `duplication_all_projects_test`
   / `duplication_vmecpp_test`, they read `sandbox/`/`drift_repos/` — non-hermetic).
2. Loop: heuristic → measure precision/recall on the 197 labeled → tune.
3. Ship as a down-weight (recall-safe) once FP-vs-TP discrimination is proven.

Without this loop the implementation = guesswork. Awaiting a focused session with a run over the repos.

## VALIDATION RUN (2026-06-05) — the decisive result

The run was performed (corpus repos found in `~/oss/_aidev_dense/`).
For each of the 6 labeled header-impl FP: checkout the corpus SHA → `archcheck
--duplication` → check whether the *current* detector reports the labeled FP pair.

| Repo | corpus SHA | header-impl FP reported? |
|---|---|---|
| DataDog/java-profiler (os_macos↔os_linux) | b7cc386a | **NO** — not paired |
| fenrus75/powertop (test-stub↔prod) | 24956009 | **NO** |
| deltaeecs/CSR4MPI (.h↔.cpp decl-def) | top | **NO** (0 pairs at all) |
| ImagingTools/Acf (Qt-test .h/.cpp) | f8440296 | **NO** (27 pairs, none labeled) |
| b-macker/NAAb (sibling NVI headers) | c1d8208d | **NO** (12 pairs, none labeled) |
| **ImagingTools/AcfSln** (NVI `C*Comp.cpp`) | 28335c8c | **YES** — 5× **EXACT weighted=1** |

### Conclusions (by data, not guesswork)

1. **5 of the 6 header-impl FP of the corpus are ALREADY eliminated** by the current archcheck. The corpus
   baseline (6 header-impl, 42.1% precision, round 2) was measured with the **old/standalone** detector;
   the current archcheck with P0+P1 gates no longer produces them. That is, the P1.3 goal is largely
   **already closed** by existing mechanisms.
2. **The only remainder — AcfSln — is EXACT weighted=1** (NVI skeletons of component-`.cpp`).
   On tokens they are **indistinguishable** from a real copy-paste (the same weighted=1). Any
   token-based P1.3 (down-weight/drop) would suppress them **together with real clones** → recall
   suffers or precision doesn't rise. This confirms: the remainder requires **semantics** (understanding
   that these are different components with the same lifecycle skeleton), not a token heuristic.

### Final decision on #083

**P1.3 (token header-impl gate) is NOT to be implemented** — the goal is largely closed, the remainder is
counterproductive on tokens. More broadly: the dominant FP class — **idiom (108) + idiom-floor
(~40)** — the corpus and CHANGELOG directly say it's irremovable without semantics. So
**gate-grade precision is unreachable by token-level means**; the path is the **P2 semantic/LLM
confirm layer**, and that's **v0.2** (see CHANGELOG: "LLM confirmation planned v0.2").

→ `--duplication` stays **advisory** (as recorded in #082-Slice5b) until P2 semantics appear
in v0.2. Task status → **blocked** (on the v0.2 semantic backend), stays in
`wip/` (by the rule "blocked ≠ completed, keep in wip with status blocked"). Token-level
tuning here gives no ROI — reopen once the v0.2 semantic layer appears.

### What counts as "done" within this session

- The validation loop was **run** (what the user asked for): 6 header-impl repos,
  the before-state of the current detector measured.
- The P1.3 question is **closed by data**: not to be implemented (5/6 already eliminated, the remainder semantic).
- It's correctly recorded that gate-grade = a v0.2 semantic dependency, not token work.
