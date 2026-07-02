# Duplicate Detection Architecture

_Current as of 2026-06-01. The source of truth for the design of the duplication
subsystem is this document; detailed measurements are in the spike reports (see "Artifacts")._

> **Decision 2026-06-01 — a single detector retained.** The subsystem has been
> reduced to the **fuzzy token-based #056** (Type-3, "edited copies"). The
> line-based #053 and AST cross-TU #052 have been **removed from the tree**
> (`experiments/line_duplication`, `experiments/clone_detector` deleted —
> they remain only in git history). We are **not coming back** to them: #053
> correctly caught only Type-1/verbatim, which #056 covers as a special case;
> #052 (libclang, expensive) never made it out of proof-of-concept. The FP
> logic accumulated in #053 (filtering out raw-string blobs and dead `#if 0`)
> is not lost — it was carried over into the #056 lexer. The sections below
> about #053/#052 are a historical reference about what existed and why we
> tried several layers.

## 1. Purpose and positioning

Duplication between components is a **missing reuse edge** in the sense of Lakos
physical design: code that should have been shared (a header, a base class, a
template) but was multiplied by copy-paste. archcheck looks for such places in
order to:

> Empirical confirmation of this framing is [research/dedup_techniques_corpus.md](research/dedup_techniques_corpus.md):
> an analysis of real commits in OSS where developers removed copy-paste (the
> dominant technique being extraction of shared code into a shared
> header/module, exactly the "missing reuse edge").


- **gate in CI** the growth of duplication (not a dashboard — exit≠0, forces action);
- give **concrete findings** (`file:line ↔ file:line`), not a general `%`;
- run in the fast backend: **without libclang and `compile_commands.json`**, cheaply,
  deterministically.

The key principle of the whole subsystem: **detection without a code change = zero.**
A report that nobody fixes contradicts the positioning (CI-gate, not GUI).
Therefore any expensive layer (especially semantic checking) is justified only
if its output **gates a PR or produces remediation** (extraction of shared code).

## 2. Layers — what we tried and what we kept

| Layer | Task | Granularity | Cost | Backend | Status |
|------|--------|---------------|------|--------|--------|
| **#056** token-based | **Type-3 / diverged** (edited copies) | **token** | cheap | preprocessor | **kept — the sole detector** |
| **#070** precision | filter coincidental/idiom-FP over #056 | token + meaning | medium | — | task (on top of #056) |
| ~~#053 line-based~~ | Type-1 / verbatim + Type-2-lite | line | cheap | preprocessor | **removed** (git history) — Type-1 covered by #056 |
| ~~#052 AST cross-TU~~ | exact cross-TU clones | AST | expensive | libclang | **removed** (git history) — never left PoC |

Originally the layers were conceived as complementary: different axes of
divergence are caught differently. The canonical example is a copy with
**renamed** local names: #053 (line) saw it as ~12% match (names differ → lines
differ), while #056 (token) sees it as 100% (normalization makes names
transparent). This is exactly what caused the collapse to a single layer: #056
covers Type-1/verbatim as a special case of Type-3, and the separate cost of
libclang (#052) did not pay off. **Only #056** has been retained.

## 3. The token-pass pipeline (#056) — the main layer

```
discovery+excludes → lex+selective-normalize → fragmentation
   → inverted index (relative rare-df)         [RECALL: completeness]
   → bag overlap (plain)                        [RECALL: cheap scoring]
   → token-LCS confirm (order-sensitive)        [PRECISION: order]
   → character classifier                       [VERBATIM/RENAMED/EDITED/…]
   → precision filters (#070)                   [filter idiom-FP]
   → reporter / gate                            [action]
```

### 3.1. Lex + selective normalization

We lex without a parser. Normalization is **selective** (a key decision, see §6):

- identifiers of local variables/parameters → `id`;
- literals (numbers, strings, characters) → `lit`;
- **kept as-is:** keywords, operators, brackets, **names of called
  functions** (the identifier before `(`) and **case labels** (the identifier
  after `case`).

Why not put everything into `id`: otherwise any straightforward code `var = call(args);`
normalizes into the same skeleton `id = id ( id ) ;`, and unrelated fragments
falsely match. Keeping callee names and case labels leaves a **distinguishing
semantic signal** in the stream, stable under copying (local names are renamed
often, API calls rarely).

### 3.2. Fragmentation (parser-free)

Function-scale fragments by the `{…}` balance heuristic: a `{` preceded by `)`
is a function/control-block body. A block of size `[min_tokens, max_tokens]` is
emitted as a fragment; larger — we descend inside; smaller — we discard.
**Limitation:** blocks > `max_tokens` (≈100 lines) are split → large duplicates
are not reported as a single pair (a documented segmentation boundary).

### 3.3. Inverted index — recall, with a RELATIVE rare-df

Candidates — via an inverted index on low-frequency tokens (pairs sharing
≥ `min_shared` rare tokens). **`rare_df` must be relative** (a percentile of the
corpus df, ≈8–15% of N), not an absolute constant: on a large tree an absolute
`df≤50` cuts off ALL pairs (on 27k fragments — 0 of 371M). A fallback to k-gram
fingerprints is desirable — recall loses copies whose precisely distinguishing
tokens have diverged.

### 3.4. Bag overlap — cheap recall scoring

plain bag-of-tokens overlap (Jaccard). The goal of this phase is **recall**, not
precision: the bag deliberately ignores order. **idf-weighting is NOT the default**
(only optional): in the headline case "operator flipped in every line" the
rarest tokens diverge, idf gives them maximal weight and **inverts the ranking**
(a false switch 0.45 above a true copy 0.36). idf is appropriate in
precision-ranking, not on a recall-gate.

### 3.5. token-LCS confirm — precision, a MANDATORY phase

Token order is the axis that separates "the same procedure, operator flipped"
(a copy) from "a different procedure, the same idioms" (not a copy). The bag
throws this axis away, so LCS is not an "optional re-rank of the top", but a
load-bearing precision phase over all candidates. **LCS lives on its own scale:**
a tiny normalized alphabet has a high floor (any two C++ bodies give LCS ~0.7),
the TP/FP boundary ≈ **0.80** — therefore the precise mode has its own calibrated
gate ~0.80, not the bag threshold ~0.6.

### 3.6. Duplicate-character classifier (#070 P3b)

For each pair — a label from the already computed `lcs`/`line`/`diff` + an
analysis of raw tokens:

| Condition | Label |
|---|---|
| norm-diff empty, no subst | **VERBATIM** |
| norm-diff empty, `id` changed | **RENAMED** |
| norm-diff empty, `lit`/numbers changed | **EDITED-CONST** |
| norm-diff: `~` on operator/keyword | **EDITED-LOGIC** |
| norm-diff: `+`/`-` dominate | **EXPANDED / SHRUNK** |

A subtlety: at `lcs=1, line<1` **RENAMED and EDITED-CONST are indistinguishable
from the norm-diff** (both collapsed) — one must keep the raw lexeme and compare
on an "equal" LCS backbone (counting id-subst vs lit-subst). See #056 §"Classifier
mechanics".

### 3.7. Precision filters (#070) — three levels by cost

**Level 0: Selective normalization (the main lever, §6).** Hits FP at the root —
does not erase the distinguishing signal. Cheap, no threshold. ✅ Implemented in
the lexer.

**Level 1: P0 guards (mechanical, no semantics):**
- **P0.1** — diff-hunk+blame attribution (same-file overlap filter)
- **P0.2** — git rename/move suppress (whole files as a move, not drift)
- **P0.3** — coordinate revalidation (phantom-range filter)
- ~~**P0.4** — function-boundary anchor~~ (**removed 2026-06-11**, see history below)
- **P0.5** — symmetric-pair canonicalization (deduplication of (a,b)≡(b,a))
- **P0.6** — joint token∧order floor (require w ≥ 0.75 AND line ≥ 0.50 simultaneously)

Result: precision 42% → ~55–62% at TP-retention ≥ 98%. ✅ Implemented and
tested (46 tests PASS).

**Level 2: P1 classifiers (form, not meaning):**
- **P1.1** — data-table/literal-run classifier (low-diversity → down-weight)
- **P1.2** — boilerplate-density filter (very short → require w ≥ 0.90)
- **P1.3** — header-impl gate (**not implemented**: was a no-op stub, removed; planned — see #070)
- **P1.4** — file-local IDF down-weight (high-frequency tokens of the file)

Result: precision ~55–62% → ~65–75%. ✅ Implemented, requires validation.

**Level 3: Information-weight / entropy guard (fallback).** Trigram-diversity /
TF-IDF weight of a line for the most skeletal ones. **Demoted from main to
fallback**: measurement showed an **overlap of distributions** of TP/FP by
diversity (FP 0.25–0.28, a real TP 0.32) — **there is no clean threshold**
(idiom-FP floor ~40 cases).

**Level 4: LLM/semantic confirm (final, v0.2+).** Candidate → an agent reads
both fragments → verdict REAL/RENAMED/EDITED/IDIOM-FP/DATA-TABLE. Meaning
separates idiom-FP from a copy where form cannot. Expensive → only after
layers 1–3 and only if the verdict **gates/fixes** (otherwise not needed).
Planned in #059 L3.

## 4. Metrics and their meaning

- **`lcs`** — match by order of normalized tokens. `lcs=1` = "the same up to
  local names and literals".
- **`line`** — match of raw lines (illustrative, as in #053). A divergence
  `lcs`≫`line` shows that the edit went into normalized tokens (names/numbers).
- **`diff`** — what exactly changed of what survived normalization (`~`/`+`/`-`).

**Semantics that are important to keep in mind:** the metric measures **what**
was changed, not **how many lines**. Renaming local names → a **full** match (by
design — this is precisely the Type-2 robustness), not partial. A partial
(`lcs<1`) arises only when the edit touched operators/keywords/structure/literals.
Therefore most real diverged copies (`_int`, `Client/Strip`, `V1/V2`) lie in the
bucket of **full** matches, not partials.

## 5. Classes of false positives and how we treat them

| FP class | example | treatment |
|---|---|---|
| **call-skeleton** | `var=call(a); var=call(b);` from different domains | selective norm: keep callee names → different calls don't match |
| **switch/enum idiom** | three `switch{case X:return N}` from different domains | selective norm: keep case labels → fell under the gate 0.80 |
| **data-table** | tables of constants/colors, byte arrays of shaders | data-heavy guard: a fragment is almost entirely `lit` → EDITED-DATA / exclude |
| **residual idiom-FP** | a common idiom surviving everything above | accept as a floor; finished off by LLM-confirm |

The principle: **first do not erase the signal (normalization), then filter the
form (entropy, fallback), and at the end — meaning (LLM).** Form thresholds are
secondary, because their TP/FP overlap.

### 5.x FP Classification Rules (#071)

A systematized distinction between **TP (real copy-paste)** and **FP (legitimate
pattern)** for use in subsequent sessions. Based on the empirics from testing on
LibreSprite (all pairs — TP, 0 FP) and verification of semantics relying on
**Kapser & Godfrey, «"Cloning considered harmful" considered harmful»**
(Empirical Software Engineering, 2008, 13(6):645–692).

**Order of application: the extractability test BEFORE signal lists.** The
question: can the repeating fragments be extracted into a single function with
parameters? If **yes** — it is a **TP** (copy-paste, a refactor is needed). If
**no** (different semantics, different parameter types) — it is an **FP**, the
guards must filter it out.

#### TP signals (real copy-paste)

1. **Identical code in different files** — `weighted ≥ 0.95`, between different
   components. Example: an identical PNG-reading block in two places
   (`clip_win.cpp:521–529 ↔ 617–625`). Solution: a shared function.

2. **A fully copied code block** — `weighted = 1.0`, cross-file, often with
   minimal editing of variables. Example: a switch-statement in two commands.
   Solution: extract into a parameterized function.

3. **Repetition of the same code 3+ times within one file** — if it is not part
   of an if-elif chain handling different types. Example: XML parsing in four
   places in `skin_theme.cpp`. Solution: a helper loop or template function.

4. **Code extractable into a function with parameters** — ask the extractability
   test first (item above). If parameterization is possible → TP, regardless of
   the weighted value.

#### Active FP guards

Guards implemented in `src/scan/duplication/duplication_scanner.cpp`:

- **P0.2: whole-file clone suppression** (`phase9WholeFileSuppress`). Suppresses a
  file-pair only when ≥80% of the fragments of **both** files match (the smaller has
  ≥2) → a real move / copy / vendored twin (A ≈ B). The condition was once one-sided
  (≥80% of the *smaller* file), which conflated a twin with "extract a module, leave
  the original in place" (small ⊂ large): the latter is the detector's prime TP — the
  missing reuse edge — and was silently dropped. Two-sided coverage reports the
  extraction while still cutting the twin (#151). NOTE: a snapshot heuristic; the
  authoritative move-vs-extract discriminator lives in the `--diff` path's parent-guard
  (`new_clone_drift.cpp`), which has the baseline tree and disables this guard entirely.

- **P0.9: generated files** (`isGeneratedPath`, `file_classification.h`). Markers:
  `.pb.*`, `*.upb.*`, `*-upb.c/.h` (upb amalgamation), `moc_`, `ui_`, `qrc_`,
  `.tab.c/.h`, `lex.yy`, `lempar` (Lemon template), `_generated.`/`_generated_`,
  `/generated/`, SWIG `*_wrap.*`. Machine-generated files (protobuf/upb, Qt,
  flex/bison/lemon) are not refactored by a human — duplication in them is not
  actionable, and big amalgamations (all of upb concatenated into one file) otherwise
  read as a whole-file clone of every real source they bundle.

#### History of P0.4 (removed 2026-06-11)

**P0.4 function-boundary anchor** dropped same-file pairs with a gap ≤5 lines
between fragments — a defense against "the sliding window snagged the tail of one
function + the head of the next". But the fragmenter in `src/` is **brace-anchored**
(function bodies by `{…}` balance), a fragment physically cannot cross a function
boundary; the target FP class does not exist. Yet the guard suppressed the most
frequent copy-paste pattern: a function copied **right below the original**
(typical gap 1–2 lines) and slightly modified. Found during cross-validation
against external oracles (NiCad/Bellon, task #107): an identical clone with a gap
of 8 lines was reported, with a gap of 1 line — silently suppressed. The defect
lived unnoticed because both P0.4 "tests" were no-ops (`REQUIRE(true)` / without
calling the filter). Removed entirely; the regression is pinned with a real
fixture in `duplication_fp_guards_test.cpp` ("Same-file copy-paste pasted
directly below…"). Effect on corpora: monit 21→21 pairs (zero new FP), self-scan
archcheck 1→2 — the new pair turned out to be a **real TP** in our own lexer
(`tryConsumeString` ↔ `tryConsumeChar`). **Principle** (the same as for P0.7/P0.8):
a guard whose target FP class does not exist under the current fragmenter is a
pure tax on recall.

#### History of P0.7 and P0.8 (removed 2026-06-03)

They were path guards, similar to P0.9: **P0.7** filtered platform-twins
(`/(mac|win|linux|posix)/`), **P0.8** — perf-variants (suffixes `_nogamma`,
`_fast`, `_slow`). Removed for the reason: a path guard does not distinguish deep
platform-specificity (API, syscalls) from a random match in the implementation of
simple helper functions. A real example: `OS::sleep()` and `OS::truncateFile()`
are identical on POSIX platforms (os_macos.cpp ↔ os_linux.cpp) — this is
extractable copy-paste (TP), not intentional variation, but the path guard
suppressed them along with legitimate platform-specific branches. **Principle**:
`identical = report, regardless of path`.

#### Severity by co-change

Severity (the harm from a duplicate) is determined by context and co-evolution in
history, not by text similarity; this signal is separated from FP classification
and is accounted for in #078 (drift-gate on co-change). Here we classify only the
**form** (is-it-duplicate), not the **priority** (how-bad).

### 5.y Accidental clones (API-protocol / "composition") — researched and closed (#159)

The "same call choreography, different domain arguments" class the user spotted in #158
(`helper1(1,2,3); helper2(4,5);` vs `helper1(5,6,7); helper2(11,12);`) has an established
literature identity — record it so nobody re-derives the terminology:

- **"Accidental clones" / "clones by accident"** — *"code fragments that are similar due
  to the precise protocols they must use when interacting with a given API or set of
  libraries"* (Al-Ekram, Kapser, Holt, Godfrey, ISESE 2005; 69% of cross-system clone
  pairs in their study).
- An instance of the **Templating → API/Library Protocols** cloning pattern; manual
  raters classify it **"incidental"** — *"cannot be abstracted further... parameters
  changed in a non-systematic way"* (Kapser & Godfrey, EMSE 2008). Roy & Cordy's survey
  (TR 2007-541 §7.5.2) lists "consecutive method invocations" as a canonical
  frequent-FP class.
- Syntactically these are ordinary Type-2/Type-3 clones — the taxonomy encodes
  similarity, not origin, which is WHY token-shape heuristics cannot separate them.

**Empirical verdict (2026-07-02, two independent probe implementations):** no shape
signal separates this class from real copy-paste on the labelled corpus. Four
literature-backed signals all null: composition-percent (1:1 TP trade at every
threshold), callee ubiquity (wrong sign — real copy-paste uses ubiquitous calls),
argument-mapping systematicity (Baker's p-match; FP ≈ TP), cross-repo sequence 3-gram
frequency (both classes repo-local). Kapser & Godfrey's own data explains it:
parameterized clones ("same shape, different identifiers") were harmful 71–76% of the
time — "different args" *selects* true positives.

**Treatment:** no suppression, ever (measured collateral ≥1:1). The shipped gates
(token diversity, statement floor, switch-skeleton) already implement the practical
state of the art (CCFinderX's RNR/TKS thresholds) and keep most clean incidental cases
(connect-chains, `o.Set(...)` population, registration tables) out of the output.
Future v0.2 candidate: an advisory `incidental/api-protocol` **tag** (down-rank on high
call-density + high recomposed-ratio) — presentation, not precision. The real
discriminator is origin/evolution (copy events, inconsistent divergence in `--diff`),
not snapshot shape. Full analysis: `backlog/completed/159_maj_composition_clone_research.md`.

## 6. Key decision: selective normalization

Instead of post-hoc filtering of noise with thresholds — **do not erase the
distinguishing tokens at the input**. Measurement (datavis-mw, precise @0.70,
keep-calls+case):

- call-skeleton FP (WaterLevels↔EditorFrame) — **disappeared**;
- switch-idiom FP — 0.85 → 0.70–0.78 (**under the gate 0.80**);
- real copies (shared calls/labels) — **survived**.

The cost: more candidates (the match is now by shared API/labels — meaningful,
filtered by the threshold); we lose Type-2 copies where **the calls themselves**
were also renamed (a rare case). Decision: `--keep-calls` **by default**; the
entropy guard is a fallback.

## 7. Modes

- **snapshot = explorer.** Scans the tree, ranks hot spots, **does not gate**.
  This is an exploratory mode (search, overview), not a CI gate.
- **commit / baseline = gate.** baseline freezes historical duplicates; CI fails
  a PR on a **new** confirmed duplicate. This is where "a code change happens":
  the author hits the gate and extracts the common code. Confirm (#070) is needed
  here so the gate does not fail on idiom-FP (otherwise it will be disabled).
- **diff (#054).** Attribution of copy-paste to a commit: "did commit C add code
  that is a Type-3 duplicate of code that already existed in the parent P?".
  Through `git`, without checkout.

## 8. Excludes (vendored / generated)

Without cutting off the noise, the first run drowns in it (boost, upb-gen,
mcut/minilzo). Lessons:

- **matching is case-insensitive** — `ThirdParty` ≠ `third_party` ≠ `3rdParty`;
- **vendor is often without a marker** — it lies in ordinary subfolders
  (`src/mcut`), the name doesn't help; **generated code is project-specific**
  (`upb-gen/*.upb.h`, `*_autogen`);
- ⇒ a fixed list cannot be trusted; vendor/generated must be determined by
  **signals** (a license header, a `DO NOT EDIT`/`@generated` banner, the absence
  of incoming include edges from app code, an alien style), marked with the source.

See also the rules for the config-agent (`v1_maj_agent_config_authoring_rules`).

## 9. Boundaries (NOT bugs)

- **Type-4 / semantic clones** (`for`↔`while`, a different algorithm) — out of
  scope for the fast backend; this is AST/CFG (#052).
- **Segmentation** — `max_tokens` splits blocks >~100 lines; a class/namespace in
  `[min,max]` is emitted whole instead of its methods.
- **Recall** — a tight rare-token gate loses strongly diverged copies; a
  fingerprint-fallback is needed.
- **Idiom-FP floor** — cannot be reduced to zero by formal means (the overlap of
  distributions is proven); the remainder is removed by LLM-confirm.

### 9.1. Interface + thin delegating implementations — ✅ verified (#116)

The typical polymorphic pattern "an interface + N similar implementations, each
delegating to its backend with one or two lines" is NOT reported as copy-paste.
Confirmed by directed tests in `tests/duplication_thin_delegates_test.cpp`
(4 scenarios, 2026-06-13):

1. **Size threshold** (the main echelon): the body of a thin delegate
   `{ backend_.op(...); }` < `minTokens = 30` (fragmenter.h) — does not become a
   fragment at all. Verified: 3 implementations × 10 methods (arity 0–4) to
   DIFFERENT backends → 0 pairs; the same implementations to the SAME backend
   (differing only in the class name) → also 0 pairs.
2. **Selective normalization**: callee names are kept — implementations to
   different backends differ in the kept tokens. This echelon is secondary: in
   practice the bodies do not survive to scoring, they are cut by the size
   threshold.
3. **Extractability test**: identical SUBSTANTIVE bodies — an honest TP. Control:
   two identical `compute` bodies (>30 tokens, a shared callee) against a
   background of different functions (for a non-degenerate IDF) → the pair is
   reported.

**Boundary result (important):** the fragmenter measures the size of the BODY,
not the signature. A delegate with 10 parameters still gives a body
`backend_.op9(a..j);` ≈ 25 tokens < 30 → 0 fragments, 0 pairs. Inflating the
parameter list does not by itself push a delegate over the fragmentation
threshold; only a delegate with a multi-line body (> ~30 tokens) can show up, not
a single forwarding call.

**A note about a degenerate corpus:** with two identical fragments and no
background, the relative-df gives `idf = log(2/2) = 0` → weighted = 0, and even a
real TP does not reach the gate (see §6). Therefore the TP fixtures (scenarios
3–4) carry different background functions ≥30 tokens — this is a requirement of
the fixture, not a property of the detector.

## 10. The closed loop (otherwise it is all pointless)

```
detect (#053/#056 recall) → confirm (#070 precision) → ACTION
                                                         ├─ gate: new duplicate fails the PR → author dedups
                                                         └─ remediation: "extract into a shared header/base" (Lakos)
```

The success metric is **"did not grow / was fixed"**, not "how much was found".
#054 empirically confirmed: drift is **reversible** by extracting common code; the
typical technique is a shared header/base class.

## 11. Status: spike vs product

- **In `experiments/`** (standalone C++20, not in the main build): the token pass
  #056 in its entirety — normalization, fragmentation, index, bag, token-LCS,
  diff, selective normalization, excludes, diff-mode #054. Plus the data-blob
  filtering carried over from #053: raw-string literals (`R"(...)"`) are collapsed
  into `lit`, dead `#if 0`/`#if false` blocks are dropped in the lexer.
- **In `src/scan/duplication/`** — the token detector has been ported (#072) and
  connected to the CLI (`--duplication`, advisory, always exit 0): normalization,
  fragmentation, rare-token index + winnowing fingerprints (#092 closed
  fingerprint-recall), scoring (weighted/plain Jaccard, lineOverlap, token-LCS),
  P0/P1 precision filters (including the data-table guard P1.1), classifier.
- **The classifier scale in the code differs from §3.6:** what is shipped is
  EXACT/RENAMED/LITERAL/MIXED/STRUCTURAL (`clone_classifier.h`); the
  correspondence: VERBATIM≈EXACT, RENAMED≈RENAMED, EDITED-CONST≈LITERAL,
  EDITED-LOGIC/EXPANDED≈MIXED/STRUCTURAL.
- **Not implemented:** the LLM-confirm layer, the P1.3 header-impl gate, the
  baseline/gate mode for duplicates (§7 "commit/baseline = gate" — so far only a
  plan).

## 12. Map of tasks and artifacts

- **#056** — `fast_backend_partial_duplication_pass` (token Type-3) — **the core, the sole detector**.
- **#070** — `coincidental_clone_filtering` (precision: selective norm + confirm).
- **#054** — `ai_repo_duplication_run` (consumer of the signal: drift over history).
- **#033** — `ai_drift_dataset` (duplication as an AI-drift signal).
- ~~**#053** `fast_backend_line_duplication_pass` (line Type-1)~~ — removed, git history.
- ~~**#052** `cross_tu_duplication_detector` (AST layer)~~ — removed, git history.
- Literature (precision): essence-clones (info-theoretic, arXiv 2502.19219),
  Kapser & Godfrey (benign clones), SourcererCC (low-freq index).

> **`experiments/` is taken out of git** (a local sandbox, see the root
> `.gitignore`). Dumps and raw reports (`partial_duplication/*`,
> `ai_repo_run/SUMMARY.md`, corpus TSVs) — regenerable, they are no longer in git.
> The handwritten synthesis is preserved in tracked docs:
> - dedup techniques from OSS (ground truth TP) → [research/dedup_techniques_corpus.md](research/dedup_techniques_corpus.md);
> - FP classes + precision → [duplication_fp_analysis.md](duplication_fp_analysis.md);
> - corpus AI-correlations (#079/#080) → `backlog/wip/079…`, `backlog/wip/080…`.
