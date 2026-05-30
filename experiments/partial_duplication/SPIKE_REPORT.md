# partial_duplication spike — report (#056)

**Date:** 2026-05-30
**Scope:** algorithmic spike (step 3 of #056): `tokenize + normalize +
fragment-split + weighted/plain overlap + inverted-index candidates + contrast
fixtures`, **plus** the P3 token-LCS precise mode + diff-view (added once the bag
metrics proved insufficient — see "P3" below). **No** product plumbing (that waits
on #053), **no** baseline/gate semantics. Standalone C++20, no libclang, no
`compile_commands.json`.

The spike exists to answer one question before the task commits to a default
metric: **does rarity-weighted bag-of-tokens overlap actually separate diverged
copies (Type-3) from structurally-similar-but-unrelated code?**

Short answer: **the contrast set works, but it overturns the task's stated
default.** Weighting is the right move for one failure mode and the wrong move
for the headline failure mode. Details below.

---

## Pipeline (what the spike implements)

1. **Lex + normalize.** Identifiers → `id`, all literals → `lit`; keywords,
   operators and brackets kept verbatim. Comments/strings stripped.
2. **Fragment split (parser-free).** A `{` immediately preceded by `)` is treated
   as a function/control body. Blocks of function scale (`[min_tokens,
   max_tokens]`) are emitted as fragments; larger blocks are descended into;
   smaller ones are dropped. This is the brace-balance heuristic from the task.
3. **Corpus stats.** Document frequency per normalized token → `idf = log(N/df)`.
   Ubiquitous tokens (`id`, `;`, `(`, `)`, `{`) collapse to weight ≈ 0.
4. **Inverted index** on low-frequency tokens (`df ≤ rare_df`) → candidate pairs
   sharing `≥ min_shared` rare tokens. Avoids the O(n²) all-pairs scan.
5. **Score.** Weighted Jaccard (Ruzicka) and plain Jaccard, plus an illustrative
   normalized-line overlap to stand in for the #053 line-based view.
6. **Report** `file:line ↔ file:line`, both metrics, line overlap, shared-rare.

`--metric weighted|plain` chooses which score gates and ranks.

---

## Contrast fixtures (`cases/`)

Run: `partial_duplication cases --min-shared 1` (see candidate-gen note below for
why `--min-shared 1` is needed on this tiny corpus).

| Case | What it is | plain | weighted | line | Verdict |
|------|-----------|------:|---------:|-----:|---------|
| **A** | `computeScore` ↔ `computeDelta` — renamed + every `+`→`-` | **0.844** | 0.355 | 0.125 | true diverged copy |
| **B** | `computeScore` ↔ `computeScoreV2` — renamed + inserted lines | **0.798** | 0.777 | 0.111 | true diverged copy |
| **C** | `joinNonEmpty` ↔ `smallestPositive` — same loop+if shape, different work | 0.616 | 0.375 | 0.188 | not a copy |
| **D** | `handleOpcode` ↔ `routeEvent` — two big switches, different dispatch | 0.597 | 0.454 | 0.063 | not a copy |
| **E** | `clamp` / `clampPos` — short near-identical guards | — | — | — | below `min_tokens`, never a fragment ✔ |

Note the **line** column: A and B sit at ~0.11. A line-based pass (even with
Type-2 normalization) is effectively blind to both, because identifiers were
renamed *and* operators flipped. The token pass keeps them at the top. That is
the whole reason this layer exists — confirmed.

`E` confirms `min_tokens`: `clamp`/`clampPos` are near-identical but never become
fragments (≈28 tokens < 30), so they never enter the comparison. Trivial-snippet
noise is filtered at the fragmentation stage, not by the score.

---

## Central finding — weighting overturns the task's default

The task fixes the default as *"bag-of-tokens overlap **со взвешиванием по
редкости токенов**"*, justified by big-`switch` / init-list false positives. The
spike confirms weighting kills those FPs — **and kills the headline true positive
with them.**

Look at the ranking each metric produces on the fixture set:

| | A (TP) | B (TP) | C (FP) | D (FP) |
|--|------:|------:|------:|------:|
| **plain** | **0.844** | **0.798** | 0.616 | 0.597 |
| **weighted** | 0.355 | 0.777 | 0.375 | **0.454** |

- **plain** cleanly separates: TPs at 0.80–0.84, FP cluster at ~0.60. A threshold
  of ~0.70 reports A+B and rejects C+D.
- **weighted** *inverts* the ranking for case A: the FP `D` (0.454) scores
  **higher** than the true copy `A` (0.355). Why? Case A diverged exactly in its
  rarest tokens (`+`→`-`). Rarity weighting trusts those tokens most, so flipping
  them is maximally penalized. The metric punishes precisely the edit that
  defines Type-3 divergence.

So on the fixtures **plain is the better default**, not weighted.

## But plain is not free — the real-code counter-example

Ran on the repo's own `src/` (25 files, ~5000 LOC, 189 fragments) with
`--metric plain --threshold 0.7 --rare-df 25`. 8 pairs reported. A representative
slice:

| pair | plain | weighted | line | reality |
|------|------:|---------:|-----:|---------|
| `git_object_file_source.cpp:61-69` ↔ `git_state.cpp:58-66` | 0.986 | 0.992 | 0.78 | real cross-file twin |
| `git_object_file_source.cpp:74-85` ↔ `git_state.cpp:37-53` | 0.894 | 0.969 | 0.50 | real partial twin |
| `include_resolver.cpp:60-62` ↔ `:66-68` (`find_exact`/`find_suffix`) | 1.000 | 1.000 | 0.20 | real, benign parallel helpers |
| `sf7_using_namespace.cpp:51-78` ↔ `include_scanner.cpp:309-335` | 0.741 | 0.439 | 0.04 | borderline: two hand-rolled line-scanners w/ depth counter — arguable *missing reuse edge*, not a copy |
| `main.cpp:400-415` (`consumeDiffModeFlag`) ↔ `include_scanner.cpp:197-214` (`next_significant_line`) | 0.748 | 0.414 | 0.08 | **false positive** — both are `string_view` scan loops, different logic |

Here **weighted is right and plain is wrong**: the FP `consumeDiffModeFlag ↔
next_significant_line` is bag-similar only because both lean on the same
`string_view`/`.substr`/`++`/`while` idiom. Weighting drops it to 0.41; plain
keeps it at 0.75.

### The honest conclusion

Neither metric dominates:

- **plain** catches operator/rename divergence (A) but over-reports idiom-similar
  loops (the `string_view` FP).
- **weighted** suppresses idiom FPs but discards operator-divergence TPs (A).

The reason is structural: **a bag of tokens cannot distinguish "same procedure,
operators flipped" from "different procedure, same idioms"** — both are
bag-similar and differ only in token *order/arrangement*. The discriminating
signal is sequence, which a bag throws away.

**Implication for #056:** the token-LCS "precise" mode (P3) is not an optional
nicety — for the operator-divergence class it is the only thing that can separate
TP from FP. The realistic design is **bag overlap for cheap candidate generation
(recall) + an order-sensitive confirm (LCS) for precision**, not "weighted bag as
the single default." The task's "weighting обязателен / token-LCS только opt-in"
split should be revisited before integration.

---

## P3 — token-LCS precise mode (now implemented)

The conclusion above predicted token-LCS is load-bearing for the
operator-divergence class. Implemented it behind `--partial-precise`: per-fragment
**ordered** token stream → LCS length → Dice ratio `2·LCS/(|a|+|b|)`, scored on a
widened candidate net (LCS supplies the precision, so candidate recall can be
loose). Each reported pair carries a token-level diff with adjacent delete+insert
collapsed into a single `changed` op.

It confirms the thesis — **and adds a calibration caveat the bag metrics did not
have.**

| Case | plain | weighted | **lcs** | verdict |
|------|------:|---------:|--------:|---------|
| **A** `computeScore↔computeDelta` | 0.844 | 0.355 | **0.915** | TP — caught |
| **B** `computeScore↔computeScoreV2` | 0.798 | 0.777 | **0.888** | TP — caught |
| **C** `joinNonEmpty↔smallestPositive` | 0.616 | 0.375 | **0.734** | FP — rejected |
| **D** `handleOpcode↔routeEvent` | 0.597 | 0.454 | **0.736** | FP — rejected |

Case A — the one weighting inverts — comes back at the **top** under LCS, and its
diff is exactly the divergence and nothing else:

```
[token-LCS=0.915]  acct.cpp:14-23 <-> acct_copy.cpp:17-26   (over 71/71 tokens)
  diff: 6 changed, 0 removed, 0 added
    ~ + -> -   (×6)
```

**New finding — token-LCS has a high floor, so it needs its own (higher)
threshold.** The normalized alphabet (`id`/`lit`/keywords/operators/brackets) is
tiny, so *any* two C++ function bodies share a long subsequence: C and D sit at
**0.73–0.74**, not the ~0.42 a richer alphabet would give. At the bag threshold
`0.60` LCS reports **19** pairs — C, D and a dozen cross-noise pairs all leak
through. The real TP/FP boundary is ~**0.80**:

- TPs: A 0.915, B 0.888, and `computeDelta↔computeScoreV2` 0.812 (both copies of
  the same original — legitimately related).
- FPs: C 0.734, D 0.736, nearest non-copy (`filler`) pair 0.778.

So precise mode defaults `--threshold` to **0.80** (overridable). At 0.80 the spike
reports **exactly the 3 true pairs and nothing else** — the first metric on this
fixture set that gets A, B, C and D all right at once.

**Refined implication for #056:** the two-stage architecture holds — cheap
bag/rare-token recall + order-sensitive LCS precision — but LCS is *not* a drop-in
at the bag threshold. It lives on a higher scale and must carry its own calibrated
gate (roughly: bag ~0.6, LCS ~0.8). Load-bearing for precision: yes. Free: no.

---

## Secondary finding — `rare_df` must be relative, not absolute

The task says "inverted index on low-frequency tokens" without fixing the cutoff.
The spike used an absolute `rare_df` and it does **not** scale:

- 13 fragments (fixtures): `df ≤ 4` is fine.
- 189 fragments (`src/`): `df ≤ 4` prunes **every** candidate (0 of 17766). The
  cutoff has to grow with the corpus.
- `src/` needs `rare_df ≈ 25` (≈13% of N) and `min_shared ≥ 2` to surface the
  real twins.

Candidate generation also pruned the *fixture* case A at the default
`min_shared = 2`: after renaming + operator-flipping, A shares only one rare token
(`*`) with the original, so it needs `min_shared = 1` to even become a candidate.
That is the same bag-granularity problem biting the recall stage: when divergence
hits the distinctive tokens, there is little left to index on.

**Implication:** `rare_df` should be a percentile of `N` (or an absolute
frequency cap tuned per-corpus), and recall needs a cheaper-than-rare-token
fallback (e.g. winnowed k-gram fingerprints) so that diverged copies are not
dropped before scoring.

---

## Perf

`src/` (189 fragments, 17766 possible pairs): **< 0.01 s wall, 4.5 MB RSS.** The
inverted index keeps it well clear of the quadratic. No perf concern at this
scale; the open question is candidate-list blow-up on large monorepos, not raw
speed.

---

## Boundaries (documented, not bugs)

- **Segmentation.** The `)`→`{` heuristic emits function and control-block bodies.
  A class/namespace body that happens to land in `[min,max]` is emitted whole
  instead of per-method; lambdas and macro-built bodies are approximate. This is
  inherent to parser-free fragmentation and acceptable for a fast backend.
- **Type-4 out of scope.** `for` ↔ `while` rewrites and other semantic clones are
  not targeted and not detected. The fixtures do not test for them.
- **Line overlap** here is illustrative only — it is *not* the #053 algorithm,
  just a stand-in to show that line granularity is blind to A and B.

---

## What this means for the task plan

- ✅ P1 core mechanic (tokenize/normalize/fragment/overlap/index) is proven viable
  and fast.
- ✅ **P3 token-LCS + diff-view implemented** (`--partial-precise`). It is the only
  metric that gets A, B, C, D all right at once — but on its own *higher* gate
  (~0.80), not the bag threshold. Promote P3 from "opt-in" to **load-bearing for
  precision** in the metric decision below.
- 🔁 **Revise the metric decision.** Default should not be weighted-bag-alone. The
  spike's resolution: plain (or lightly-weighted) bag + relative `rare_df` for
  *recall*, token-LCS at its own ~0.80 gate for *precision*. The task's "weighting
  обязателен / token-LCS только opt-in" split should be revisited before integration.
- 🔁 **Recall is fragile.** Absolute `rare_df` and `min_shared ≥ 2` drop true
  diverged copies; make `rare_df` relative and add a fingerprint fallback.
- ⏸️ P2 (gate/baseline) intentionally untouched — it belongs with the #053-shared
  plumbing, which is still in flight.
