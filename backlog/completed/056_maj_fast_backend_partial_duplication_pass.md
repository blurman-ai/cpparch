# [RULES][SCAN] Fast-backend partial duplication pass (Type-3 / diverged copies)

**Date created:** 2026-05-30
**Date started:** 2026-05-30
**Status:** wip
**Module:** RULES / SCAN
**Priority:** major
**Difficulty:** L
**Target release:** v0.2
**Blocks:** —
**Blocked by:** #053 (fast_backend_line_duplication_pass — the shared duplication plumbing, excludes and modes are better stabilized there)
**Related:** #006 (spec_refactor — fast backend as a separate layer), #033 (ai_drift_dataset — duplication as an AI-drift signal), #052 (cross_tu_duplication_detector — AST layer), #053 (fast_backend_line_duplication_pass — exact Type-1 pass), #054 (ai_repo_duplication_run — future consumer of the signal)

## Goal

Add to the fast backend a separate parser-free pass for **partial / diverged duplication** (Type-3):
catch diverged copies at the token level that the exact line-based block match
from #053 misses.

## Context

`#053` closes the verbatim / Type-1 layer: a long exact block, a fast snapshot,
a headline via concrete findings. But the main painful class of copy-paste is different:
code was copied, then in both copies people started editing in place. An
operator changed, a token was added, a line insertion appeared, but the order of actions
stayed the same.

This breaks the line-granularity approach. If every line has a small
non-normalizable edit, line LCS sees almost zero matches, even though
the fragments essentially remain the same procedure. So the unit of comparison
must be the **token**, not the line.

A separate pass is needed, not a complication of `#053`:

- `#053` stays a fast and exact Type-1 layer.
- This task adds a cheap Type-3 layer without `compile_commands.json` and without AST.
- `#052` stays the exact AST layer for the more expensive cross-TU analysis.

The positioning is the same: duplication between components is a **missing reuse
edge** in the Lakos physical-design sense. The AI narrative is permissible only in
commit/baseline mode, not in snapshot.

## Decision made

> **Reconsidered following spike #056 (2026-05-30).** The initial hypothesis —
> *weighted bag-of-tokens as the only default, token-LCS optional* —
> is **refuted**. See `## Spike #056 results` below and
> `experiments/partial_duplication/SPIKE_REPORT.md`.

### Two phases: bag (recall) → order-sensitive confirm (precision)

- **Phase 1 — recall (bag).** Candidates via an inverted index on
  low-frequency tokens + bag-of-tokens overlap. Cheap, linear, scales
  without a square over pairs, requires no libclang and no compile DB. The goal is **completeness**, not
  precision: the bag deliberately discards order.
- **Phase 2 — confirm (order-sensitive), MANDATORY.** token-LCS over all
  the recall-phase candidates. This is not "re-rank the top" and not opt-in — token order
  is exactly the axis that separates "same procedure, operator flipped"
  (still a copy) from "different procedure, same idioms" (not a copy). The bag throws this axis away,
  so precision holds here.
- **Granularity:** function-scale segments, not whole files.

**Rationale (spike fixtures):**

| metric | A (TP, `+→-`) | D (FP, switch) | verdict |
|---------|--------------:|---------------:|---------|
| plain bag | **0.84** | 0.60 | separates |
| weighted (idf) bag | 0.36 | **0.45** | **inverts**: FP above TP |

### idf weights — optional and experimental, NOT FP protection

- Previously idf was the default score and was presented as protection against FP. The spike showed
  the opposite: on the headline TP (operator flipped in every line) it is exactly the
  rare tokens that diverge, idf gives them maximum weight → **penalizes for the very difference
  that is "still a copy" for us**, and inverts the ranking (FP 0.45 > TP 0.36).
- Therefore the recall-bag is by default **plain (unweighted or weakly-weighted)**.
  idf stays as `--experimental-idf` with a warning and the numbers above; precision
  protection moved to order (phase 2), not weights.

### Numeric data blocks (embedded byte code / tables) — here (from #054)

During the #054 run (`experiments/ai_repo_run`) on spike #053 a class of noise surfaced:
**numeric data arrays** of the form `const BYTE g_main[] = { 69, 70, 88, 4, 0, ... }`
(output of the `fxc`/HLSL compiler in the Effekseer ShaderHeader DX backends). This is not code
but data; they yield huge false cross-file blocks (196 lines of numbers).

In #053 it was decided NOT to catch (it's line-based there, numbers are indistinguishable from code without tokenization;
the `#if 0` wrapper #053 already skips, but the arrays themselves — not). By the
user's decision (2026-05-30), detecting numeric/data blocks is the job of this pass:
at the token level a "line/block consisting almost entirely of numeric literals
and punctuation" is trivially detected and should be excluded (or not counted as
significant code). Account for this when implementing P1 (token overlap pass).

### What we deliberately do NOT do

- don't use character Levenshtein as the main metric;
- don't go into AST / CFG / PDG;
- don't collapse this into `#053`;
- don't try to catch Type-4 / semantic clones (`for` ↔ `while`, a different algorithm);
- don't chase zero FP: **idiom-FP** (different procedures with a shared idiom —
  e.g. two `string_view` loops) will be reduced by order, but not removed; we accept it as a
  residual precision floor.

## Classifying the nature of the duplicate (user request 2026-05-31)

> Idea: when the detector found a duplicate — **dig further and say whether it's a FULL
> match or whether something was changed** (a line inserted, a rename, an
> operator/constant edit). The spike already partly does this; below — what's confirmed on
> real data and what to add.

### What the spike ALREADY can do (snapshot `--partial-precise`)

For each pair it prints `lcs` (over normalized tokens), `line` (over raw
lines) and `diff: N changed / M removed / K added` with tags `~`/`-`/`+`. That is, the
data for classification is **already computed** — only the explicit verdict label is missing.

### Classification signal: `lcs` × `line` × `diff`

- **`lcs`** — match after normalization (`id`/`lit` collapsed, operators/
  keywords/brackets preserved). `lcs=1` = "the same up to names and literals".
- **`line`** — match of raw lines. A divergence of `lcs` and `line` shows that
  what was changed did NOT touch the structure (it went into the normalized tokens).
- **`diff`** — what exactly changed in what survived normalization.

### ⚠️ Finding (10 manual checks, 2026-05-31) — the naive rule is WRONG

A precise run on real duplicates (opentelemetry-cpp, opendht, bitcoin, FastLED,
sentry-unreal) + a manual `git`/`diff` cross-check. Summary of the checked pairs:

| Project / pair | lcs | line | What actually changed | Class |
|---|---|---|---|---|
| opendht network_engine 182↔253 | 1.0 | 1.0 | nothing (diff empty) | **VERBATIM** |
| FastLED k66↔k20 clockless | 1.0 | 1.0 | nothing (driver copied between platforms) | **VERBATIM** |
| opendht crypto req↔resp | 1.0 | 0.45 | `gnutls_ocsp_req_*`→`resp_*` (id) | **RENAMED** |
| opendht op_cache vals↔vids | 0.99 | — | loop variable name | **RENAMED** |
| bitcoin ripemd↔sha1 init | 1.0 | 0.82 | `ripemd160::Transform`→`sha1::` (id) | **RENAMED** |
| otel otlp_env 1203↔1259 | 1.0 | 0.45 | string literals `TRACES`→`METRICS`, `1.0f`→`5.0f` | **EDITED-lit** |
| bitcoin hmac256↔hmac512 | 1.0 | 0.24 | numbers `64`→`128`, `CSHA256`→`512` | **EDITED-const** |
| bitcoin ctaes 264↔276 | 1.0 | 0.64 | numeric macro args of a bit-range | **EDITED-const** |
| FastLED fastspi 332↔341 | 1.0 | 0.75 | constants `CTAS(1)`,`0xFFFF`→`0`,`0xFF` | **EDITED-const** |
| opendht securedht 126↔147 | 0.99 | — | `getId()`→`getLongId()` (1 changed) | **EDITED** |

**Conclusion: `lcs=1, line<1` is NOT "renamed".** Underneath it hide TWO different
classes — **identifiers** changed (RENAMED) OR **literals/numbers** changed
(EDITED-const). Both collapse under normalization into `id`/`lit` → `lcs=1`. The naive
rule "low line → rename" would give a false label in 4 of 10 cases (where in fact
constants were edited — bitcoin hmac/ctaes, FastLED fastspi, otel).

### Classifier algorithm (what to add)

A label on each pair from the ALREADY-computed quantities + a breakdown of raw tokens by category:

1. `lcs == 1 && line ≈ 1` → **VERBATIM** (exact copy; diff empty).
2. `lcs == 1 && line < 1` → differences only in the normalized tokens; decompose:
   - predominantly `id` changed → **RENAMED** (a rename);
   - predominantly `lit`/numbers changed → **EDITED-CONST** (editing of
     constants/literals — the frequent "copy for a different size/type/platform").
   - (needs a comparison of raw tokens by category — the spike already distinguishes the
     normalized ones, add counting of class-of-changed-token.)
3. `lcs < 1` with `diff: changed` → **EDITED** (operators/structure were edited).
4. `lcs < 1` with `diff: added/removed` → **EXPANDED / SHRUNK** (inserted/deleted).

### Pitfalls (confirmed)

- **String/asm blocks.** Differences in string literals (SIMD asm in FastLED
  `vld3`→`vld4`, shaders) go into `lit` → a false `lcs=1, line~0.14`. Without a breakdown of
  the token category, edited-asm is passed off as verbatim. Numeric `lit` — the same.
- **"A family by size/type"** (hmac256/512, ripemd/sha1) — a frequent legitimate
  pattern; the classifier must call it EDITED-CONST, not VERBATIM, otherwise
  "a common template is needed" will be said where the difference is meaningful.

### Why this matters for #054 (within-repo AI/human)

The classifier gives a new axis for the AI-drift test: **are AI duplicates more often VERBATIM or
EDITED?** Hypothesis — AI copies more exactly (more VERBATIM/RENAMED), a human more often
edits in place (EDITED). Verifiable once there's PR labeling.

### Classifier mechanics: how to tell RENAMED from EDITED-CONST (addendum 2026-05-31)

> The missing part of P3b. Above it's locked *what* to measure; here — *how*, because
> naively it isn't computable from the available quantities.

**The problem to name directly.** Currently `lcs`/`diff` are computed over
**normalized** tokens. For `lcs=1` the normalized diff is **empty** —
for both RENAMED (changed `id`) and EDITED-CONST (changed `lit`/number). So on the
current output these two classes are **indistinguishable**: both pairs look like `lcs=1,
line<1, diff: 0/0/0`. `line<1` only says "something under the structure changed",
but not *what*. So the label can't be derived from what's already printed — refinement is needed.

**What to add to the tool (minimum):**

1. **Store the raw lexeme on the token.** `Token` currently carries only the normalized
   `sym` (+ `line`). Add `raw` (the source text) — or at least for `id`/`lit`,
   for operators/keywords `raw==sym`.
2. **Compare the raw on the aligned "equal" backbone.** The normalized LCS already
   gives an alignment (it's built by `diffTokens`). At positions marked `=`
   (normalized-equal), compare the **raw** lexemes and count by the category of the
   normalized symbol:
   - `sym=="id"`, `raw_a != raw_b` → `idSubst++` (a rename);
   - `sym=="lit"`, `raw_a != raw_b` → `litSubst++` (a literal/number edit).
   (For this `diffTokens` must return pairs of matched indices, not just
   length/ops — it currently returns ops, the raw alignment needs to be threaded through.)
3. **Structural edits we take from the normalized diff** as now: `~` on an
   operator/keyword/bracket → `opSubst`; `+`/`-` → `added`/`removed`.

**Decision rule (from the counters):**

| Condition | Label |
|---|---|
| norm-diff empty, `idSubst==0`, `litSubst==0` | **VERBATIM** |
| norm-diff empty, `idSubst>0`, `litSubst==0` | **RENAMED** |
| norm-diff empty, `litSubst>0` (± `idSubst`) | **EDITED-CONST** |
| norm-diff: there's a `~` op/keyword | **EDITED-LOGIC** |
| norm-diff: `+`/`-` dominate | **EXPANDED / SHRUNK** |

**Data/asm pitfall (mandatory).** If a fragment is almost entirely `lit` (strings/
numbers — shaders, SIMD-asm `vld3→vld4`, byte tables) and all edits are in `lit` —
do **not** call it VERBATIM; the label is **EDITED-DATA**, low confidence. Threshold: the share of
`lit` in the fragment above X ⇒ mark as data-heavy (echoes the "numeric
data blocks" above — there we exclude them from significant code entirely).

**Acceptance (reproduce the verified table).** The classifier must produce:
opendht network / FastLED clockless → **VERBATIM**; opendht crypto, bitcoin
ripemd↔sha1 → **RENAMED**; otel otlp_env (`TRACES→METRICS`, `1.0f→5.0f`), bitcoin
hmac256↔512, ctaes, FastLED fastspi → **EDITED-CONST**; opendht securedht
(`getId→getLongId`) → **EDITED-LOGIC**. The naive `line<1→RENAMED` will err on 4/10
(the EDITED-CONST cases) — and that's the regression test against the naive rule.

**Cost.** +1 field on the token and a pass over the "equal" backbone — linear, without
a new algorithm. Fits into the mandatory confirm phase (P3): the classifier is
its side output, not a separate pass.

**Why (#054).** It gives an axis for drift labeling: the AIDev run showed that duplication
grows in 39/85 projects (`experiments/ai_repo_run/SUMMARY.md`), but **attribution by
git doesn't work** (the `Co-Authored-By` labels are inconsistent). A
VERBATIM/RENAMED/EDITED label by PR labeling is a way independent of git labels to ask
"does AI copy more exactly (VERBATIM/RENAMED), does a human edit in place (EDITED)?".

## Algorithm v1 (revision after the spike)

1. Split files into function-scale fragments without a parser:
   a heuristic on `{...}` balance (in the spike — `)`→`{` as a marker of a function/block body).
2. Lex the fragments and normalize the tokens:
   identifiers → `id`, literals → `lit`, but keep keywords, types, operators
   and brackets.
3. Build an inverted index over low-frequency tokens. **`rare_df` —
   relative** (a percentile of the corpus df, ≈10–15% of N), not an absolute
   constant: in the spike an absolute `df≤4` over 189 fragments cut ALL 17766 pairs.
   A fallback to k-gram fingerprints is desirable so recall doesn't lose copies
   whose very distinguishing tokens diverged.
4. Generate candidates by the intersection of rare tokens (recall phase).
5. Compute **plain bag overlap**, filter by `similarity_threshold` and
   `min_tokens`. (idf — only under `--experimental-idf`.)
6. **Mandatorily** run ALL candidates through token-LCS (order-sensitive
   confirm) — this is the precision phase, not an option.
7. Report pairs `file:line ↔ file:line`, similarity, size and a diff-view
   of the divergences (`changed`, collapse adjacent delete+insert).

## Execution plan

### P0. Contract and shared plumbing

- [ ] Lock that this is a **separate pass**, not an edit of the line-based logic of #053.
- [ ] Reuse from #053 the discovery corpus, excludes, baseline model and
      the common duplication CLI umbrella (`--duplication=...`), don't copy this anew.
- [ ] Clarify the naming/config contract:
      `--duplication=partial`, `--partial-precise`,
      `.archcheck.yml` → `thresholds.partial_duplication.*`.

### P1. Token overlap pass

- [x] Add a scanner / matcher for tokenized fragments without libclang. *(spike
      `experiments/partial_duplication/main.cpp`)*
- [x] Implement token normalization:
      names/literals collapse, operators and keywords are preserved.
- [x] Implement weighted overlap by token rarity. *(idf = log(N/df))*
- [x] Implement an inverted index on low-frequency tokens. *(but `rare_df` must
      be relative — see the finding below)*
- [x] Introduce the thresholds `similarity_threshold` and `min_tokens`.

### P2. Explorer / gate semantics

- [ ] **Off by default.**
- [ ] Snapshot mode = explorer: ranks hot spots, doesn't gate.
- [ ] Commit / baseline mode = gate: the baseline freezes historical partial duplicates,
      CI fails only on new ones.
- [ ] text/json/sarif output in the vocabulary of the current duplication reports.

### P3. Order-sensitive confirm (MANDATORY phase, not opt-in)

> Reassessed by the spike: token-LCS is load-bearing for precision, not an "optional
> re-rank of the top". The bag gives recall, order gives precision.

- [x] token-LCS over the recall-phase candidates. *(spike: `--partial-precise`,
      Dice-ratio `2·LCS/(|a|+|b|)`, candidate-net widened — LCS gives precision)*
- [x] Show the alignment / diff-view and the place of divergence. *(diff with tags
      `~`/`-`/`+`; case A → `6 changed: + -> -`, exactly the divergence)*
- [x] Collapse adjacent delete+insert into `changed`. *(implemented in `diffTokens`)*
- [ ] Keep `--partial-precise` as a flag for the detail level of the diff output, but the
      confirm itself runs always. *(in the spike `--partial-precise` is still a mode toggle;
      "confirm always" — a product decision at integration time)*
- **Spike finding (do NOT reopen):** token-LCS lives on a DIFFERENT scale than
      the bag — the normalized alphabet (`id`/`lit`/keywords/ops) has a high floor:
      any two C++ bodies give LCS ~0.7 (C=0.734, D=0.736). At a bag threshold of 0.60 LCS
      reports 19 pairs (FP C/D leak). The TP/FP boundary ≈ **0.80** → precise mode
      defaults `--threshold` to 0.80 and gives **exactly 3 true pairs** (A 0.915,
      B 0.888, copy↔copy 0.812), C/D/noise cut. **LCS is load-bearing for precision —
      but with its own calibrated gate (bag ~0.6, LCS ~0.8), not drop-in.**

### P3b. Classifier of the duplicate's nature (VERBATIM/RENAMED/EDITED/EXPANDED)

> User request 2026-05-31. The breakdown and 10 verified pairs — in the section
> "Classifying the nature of the duplicate" above.

- [ ] Put a verdict label on each pair from the already-computed `lcs`/`line`/`diff`.
- [ ] Distinguish RENAMED (`id` changed) vs EDITED-CONST (`lit`/numbers changed) —
      needs counting the category of changed raw tokens (the naive "line<1→rename"
      is wrong in 4/10 verified pairs).
- [ ] Pitfall: string/asm/numeric blocks hide differences in `lit` → don't pass
      edited-asm off as VERBATIM (FastLED SIMD `vld3→vld4`, bitcoin hmac const).
- [ ] (for #054) compute the distribution of VERBATIM/EDITED by PR authorship.

### P4. Fixtures and regression

- [x] Add separate fixtures for partial duplication. *(spike-level:
      `experiments/partial_duplication/cases/` A–E; production fixtures under
      `fixtures/partial_duplication/` — at integration into `src/`)*
- [x] Cover the contrast case against the line-based approach:
      line-LCS would stay silent, token overlap must find it. *(A and B: line-overlap
      ≈0.11, token plain ≈0.80–0.84)*
- [x] Lock the boundaries: a semantic rewrite isn't caught and isn't counted as a bug.
      *(Type-4 explicitly out of scope; segmentation limits described in SPIKE_REPORT)*

## Acceptance criteria

- [ ] The new pass doesn't change the `#053` algorithm; it's a separate layer.
- [ ] C++20, no libclang, no `compile_commands.json`, no Python in runtime.
- [ ] The comparison granularity is **tokens**, not lines.
- [ ] The default is **plain bag-of-tokens overlap (recall) + mandatory
      order-sensitive token-LCS confirm (precision)** (weighted-bag-as-default
      refuted by the spike: it inverts the ranking of the headline TP).
- [ ] idf weights — only under `--experimental-idf`, with a warning; not the default.
- [ ] Candidates are built via an inverted index on low-frequency tokens with a
      **relative** `rare_df` (a df percentile, not an absolute constant).
- [ ] There are `similarity_threshold` and `min_tokens` in config/CLI.
- [ ] The token-LCS confirm runs **always**; `--partial-precise` controls only the
      detail level of the diff-view.
- [ ] Character Levenshtein is not used.
- [ ] The pass is **off by default**.
- [ ] Snapshot = explorer, commit/baseline = gate.
- [ ] Text/JSON/SARIF can output fragment pairs and similarity.
- [ ] Tests on the main cases A–F and H green; G locks the boundary, not a bug.
- [ ] The segmentation limits and Type-4 boundaries are explicitly described in the pass doc.

## Test cases

- [ ] **A. Operator in every line:** a copy of ~10 lines, in each `+`→`-`;
      token overlap must give a high similarity, the line-based match — no.
- [ ] **B. Insert/delete:** a copy with inserted lines is still caught.
- [ ] **C. Similar shape without a copy:** two unrelated fragments below the threshold.
- [ ] **D. Big switch FP:** two different big `switch`es aren't reported —
      **thanks to the order-sensitive confirm** (order/dispatch differ),
      NOT thanks to idf weights (spike: idf gives 0.45 here and drops the TP too).
- [ ] **E. Short guard:** a fragment shorter than `min_tokens` isn't considered.
- [ ] **F. Precise re-rank:** the diff-view shows the changed tokens.
- [ ] **G. Semantic rewrite:** `for` vs `while` isn't caught, this is a documented boundary.
- [ ] **H. Baseline:** a historical partial duplicate frozen, a new one reported.

## Done

- **Minimal algorithmic spike closed** (2026-05-30, "step 3"):
  `experiments/partial_duplication/` — standalone C++20, without libclang and
  `compile_commands.json`, **not in the main build**, without product plumbing.
  - `main.cpp`: lex+normalize (`id`/`lit`, keywords/operators verbatim) →
    parser-free segmentation by `)`→`{` → idf weights → inverted index over rare
    tokens → weighted + plain Jaccard + illustrative line-overlap.
  - `cases/`: contrast fixtures A–E + a filler corpus for realistic idf.
  - Run on the fixtures **and on the real `src/`** (25 files, ~5000 LOC,
    189 fragments; <0.01 s, 4.5 MB RSS).
  - Full breakdown: `experiments/partial_duplication/SPIKE_REPORT.md`.
- **The Type-3 thesis confirmed:** A (`+→-` in every line) and B (insert+rename)
  have line-overlap ≈0.11 (line-based blind), but token-plain ≈0.80–0.84.
- **E confirmed `min_tokens`:** short near-identical guards don't even
  become fragments.
- **P3 order-sensitive confirm implemented in the spike** (`--partial-precise`):
  an ordered token stream → token-LCS (Dice-ratio) → a diff-view with tags
  `~`/`-`/`+`, adjacent del+ins collapsed into `changed`. Case A, which idf
  inverted, returns to the **top** (lcs=0.915, diff = exactly `6× + -> -`).
  At an LCS gate of 0.80 precise gives **exactly 3 true pairs** (A 0.915 / B 0.888 /
  copy↔copy 0.812), C (0.734) / D (0.736) / noise cut. **Tests A/C/D/F on
  the spike green.** Breakdown — SPIKE_REPORT §"P3".

## Spike #056 results

**Hypothesis status:** *weighted bag-of-tokens as the default* — **refuted**.
The metric contract is revised in this document (sections "Decision made",
"Algorithm v1", acceptance criteria) **before** integration into `src/`. The full breakdown and
numbers — `experiments/partial_duplication/SPIKE_REPORT.md`.

Rationale (spike fixtures):

| metric | A (TP, `+→-`) | B (TP, insert+rename) | C (FP, shape) | D (FP, switch) |
|---------|--------------:|----------------------:|--------------:|---------------:|
| plain | **0.84** | **0.80** | 0.62 | 0.60 |
| weighted (idf) | 0.36 | 0.78 | 0.38 | **0.45** |

### Conclusions

1. **The default was wrong — idf inverts the ranking.** Weighting by
   rarity doesn't merely throttle the TP harder than the FP, it puts the FP (D=0.45) **above** the TP
   (A=0.36). The cause is ironclad: in "same procedure, operator flipped" it is exactly the
   rare tokens that diverge, idf gives them maximum weight — penalizing exactly the
   difference that for us is "still a copy".
2. **The problem is deeper than the weights — in the bag itself.** A bag of any kind doesn't distinguish "same
   procedure, operator flipped" from "different procedure, same idioms":
   the distinguishing signal is **token order**, and the bag throws it away. The weights are an attempt
   to rescue an approach that has discarded the necessary axis.
3. **Order is load-bearing for precision, not an option.** token-LCS stops being
   an "optional re-rank of the top" and becomes a **mandatory second phase over
   all candidates**. The design: bag = fast recall (candidates) →
   order-sensitive confirm = precision.
4. **`rare_df` must be relative.** An absolute `df≤4` over 189 fragments
   cut ALL 17766 pairs. The rarity threshold is a share of the corpus (a df percentile), not a
   constant; a fallback to k-gram fingerprints is desirable for recall.
5. **Idiom-FP — a residual floor.** `consumeDiffModeFlag` ↔
   `next_significant_line` (both `string_view` loops) — different procedures with a shared
   idiom. Order will reduce it, but not remove it. Accept it as a precision floor, don't
   chase zero.
6. **LCS — with its own gate, not drop-in (P3 finding).** token-LCS lives on a DIFFERENT
   scale than the bag: a tiny normalized alphabet has a high floor — any
   two C++ bodies give LCS ~0.7 (C=0.734, D=0.736). At a bag threshold of 0.60 precise
   reports 19 pairs (FP leak); the TP/FP boundary ≈ **0.80**. Therefore two different
   calibrated gates: bag ~0.6 (recall), LCS ~0.8 (precision). "Load-bearing" —
   yes, "free/drop-in at the bag threshold" — no.
7. **Rename = a FULL match, not partial (metric semantics).** A direct
   test: a 10-line function, a copy with only variables renamed
   (`values→items`, `total→acc`) → `lcs=plain=1.0`. A copy with operators edited
   (`+=`→`=…-`, inserted `if…continue`) → `lcs=0.84`. Normalization collapses all
   identifiers into `id`, literals into `lit` — so **rename is transparent** (this is exactly
   the Type-2 robustness, the pass exists for it). Partial (`lcs<1.0`)
   arises ONLY when the edit touched what survives normalization: operators,
   keywords, brackets, literals, structure. Consequences:
   - "Degree of match" = "how much of what survived normalization changed", NOT
     "how many lines were edited". Names are free, operators are not.
   - Most real diverged copies diverge by **renaming** (`_int`,
     `Client/Strip`, `V1/V2`) → they are in the bucket of **full** matches, not partials.
     So partial counters look small next to plain=1.0 — this is by design,
     not a shortfall.
   - Complementary with **#053**: line-based sees a rename as a difference (~12% line
     overlap), the token pass — as 100%. The two passes catch different axes of divergence.
   - **OSS validation (19 repos, `OSS_SWEEP_REPORT.md`):** there are in fact many partials,
     as soon as recall is relaxed (threshold 0.70, min-tokens 45, rare-df-pct 10) and
     large repos are included — folly 367, spectre 341, abseil 274, GWToolboxpp 227,
     grpc 215, AetherSDR 210, Bambu 183, ttcg 166. Type-3 exemplars with diffs
     (Catch2 junit↔sonarqube `&&→||`; ttcg float/int template→cast; folly
     AVX2↔SSE2) — in the report.

## In progress

- (spike done; integration into `src/` not started — awaits #053 plumbing)

## Next steps

1. Bring `#053` to a stable product contract on corpus/excludes/reporting
   (its P0-A/B/C + the move into `src/scan`).
2. ~~Revise the metric contract~~ — **done in this document** (bag-recall +
   mandatory order-sensitive confirm; idf lowered to `--experimental-idf`).
   Remaining: implement the token-LCS confirm phase (P3) at the move into `src/`.
3. Make `rare_df` relative (a df percentile); think through a
   fingerprint fallback for recall.
4. When the #053 plumbing is stable — single out what is actually reused
   (corpus/excludes/baseline/CLI umbrella), and move the spike algorithm into `src/`
   with product fixtures under `fixtures/partial_duplication/`.

## Key decisions

| Decision | Reason |
|---------|---------|
| A separate pass, not a complication of `#053` | line-based and token-based layers solve different classes of duplicates and have a different cost |
| Bag = recall phase (candidates), not the final score | the bag is cheap and scales with an index, but throws away token order |
| Order-sensitive token-LCS confirm — a mandatory precision phase *(revised #056)* | the bag doesn't distinguish "operator flipped" from "different logic, same idiom"; only order distinguishes |
| The default score — plain, idf only `--experimental-idf` *(revised #056)* | idf inverts the ranking of the headline TP (FP 0.45 > TP 0.36) — see "Spike results" |
| `rare_df` relative (a df percentile) *(revised #056)* | an absolute constant cuts all candidates as the corpus grows (0 of 17766 over 189 fragments) |
| Snapshot advisory, commit/baseline gate | no need to pretend the explorer can guess the intent of a duplicate |
| Type-4 explicitly out of scope; idiom-FP — a residual floor | semantic clones require AST/CFG; a shared idiom (`string_view` loop) gives an FP that order lowers but doesn't remove |

## Changed files

| File | Change |
|------|-----------|
| `backlog/wip/056_maj_fast_backend_partial_duplication_pass.md` | the statement + spike results, revision of the metric contract |
| `experiments/partial_duplication/main.cpp` | **spike**: token-overlap detector (standalone C++20) |
| `experiments/partial_duplication/cases/*.cpp` | **spike**: contrast fixtures A–E + filler |
| `experiments/partial_duplication/{SPIKE_REPORT,README}.md`, `CMakeLists.txt` | **spike**: report, instructions, build |
| `src/scan/partial_duplication_*` | the future token-based scanner / matcher |
| `src/rules/duplication/...` | future rule plumbing and registration |
| `src/main.cpp` / config loader / reporters | future CLI, config and output plumbing |
| `fixtures/partial_duplication/...` | future pass/fail fixtures |

## Fixtures

- [ ] `fixtures/partial_duplication/pass/`
- [ ] `fixtures/partial_duplication/fail_operator_per_line/`
- [ ] `fixtures/partial_duplication/fail_insert_delete/`
- [ ] `fixtures/partial_duplication/fail_cross_component/`
- [ ] `fixtures/partial_duplication/pass_semantic_rewrite_boundary/`

## Outcome
**Status:** completed — the spike (the task's goal) is done in full: a token Type-3
detector with selective normalization, fragmentation, a rare-token index, LCS-confirm
and a classifier was worked out in experiments/ and became the SOLE detector of the subsystem
(decision 2026-06-01, docs/duplication_architecture.md). Integration into src/ was done
by task #072 (port 854d53c/6b4507e + CLI --duplication). The "Next steps" above
(wait for #053 plumbing) are cancelled: #053 dropped as superseded by this same detector.
**Date completed:** 2026-06-11 (in fact — earlier; closed during a backlog pass)
