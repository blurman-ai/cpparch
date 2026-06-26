# [RULES][SCAN][REPORT] Clone classification + explanation (LD.10 + LD.11)

**Creation date:** 2026-06-02
**Start date:** 2026-06-02
**Status:** completed
**Completion date:** 2026-06-05
**Module:** RULES / SCAN / REPORT
**Priority:** minor
**Difficulty:** S (one classifier over an already-computed match + passing raw tokens through)
**Target release:** v0.3 (research spike)
**Blocks:** —
**Blocked by:** —
**Related:** #071 (clone_detection_opportunities — the parent umbrella, LD.10+LD.11 carved out from it), #056 (token pass — the only detector), #070 (precision/FP fixes)

## Goal

Attach to each found duplicate pair (1) a label of the clone's nature —
`EXACT / RENAMED / LITERAL / MIXED / STRUCTURAL` (LD.10) — and (2) an explanation
of the match: matched tokens, similarity %, and a list of renames/literal changes
that normalization hid (LD.11). So that a developer immediately sees whether a clone is
dangerous or harmless, and *why* it matched at all — without reading the two fragments by eye.

## Context

Carved out of #071 as the cheapest and already design-blessed item: the pipeline in
[docs/duplication_architecture.md](../../docs/duplication_architecture.md) directly
draws a "nature classifier → VERBATIM/RENAMED/EDITED" stage, but it isn't in the code
yet.

It lives in the spike [experiments/partial_duplication/main.cpp](../../experiments/partial_duplication/main.cpp)
(the only live detector — the token one, #056). The spike is marked `NOT product
plumbing` — it's a research foray; product integration into the binary comes later.

**Key design finding:** `Token` stores only the normalized `sym`
(identifiers → `"id"`, literals → `"lit"`), and the raw spelling is discarded during
lexing. Therefore EXACT vs RENAMED vs LITERAL can't be distinguished from the current data —
the raw token needs to be threaded through `Token.raw` → `Fragment.rawSeq` and compared
at the positions where the normalized streams matched.

**Classifier algorithm:**
- the normalized `seq` of the two fragments differ (in length or element-wise)
  → **STRUCTURAL** (Type-3 proper: insertions/deletions/reorderings/operator flips);
- the `seq` are identical (aligned 1:1) → look at what diverged in raw:
  - nothing → **EXACT**;
  - only `id` positions → **RENAMED**;
  - only `lit` positions → **LITERAL**;
  - both `id` and `lit` → **MIXED**.

Callee names and operators are stored in `sym` as is; if they differ —
`seq` will diverge → STRUCTURAL (a different call = structurally different code, defensible).

## Execution plan

- [x] `Token`: add a `raw` field (after `line`, so as not to break `{sym, line}` aggregate-init).
- [x] Lexer: fill `raw` in the `id` and literal branches (string/char/number/raw-string).
- [x] `Fragment`: add `rawSeq`, fill it in `makeFragment` alongside `seq`.
- [x] Function `cloneType(fa, fb)` — pure, over `seq`/`rawSeq`.
- [x] Output the label into both reporters (snapshot + diff-mode).
- [x] Fixture `fixtures_clone_type/` with pairs of known type + build + run.
- [x] **LD.11:** `DiffOp` carries the source indices `ai/bj` → `explainPair` reads `rawSeq` at `=` positions.
- [x] **LD.11:** output `explain: matched N tokens, similarity P%` + `ignored identifiers/literals` (precise mode).

## Done

- **Raw-token passthrough.** `Token.raw` ([main.cpp:93](../../experiments/partial_duplication/main.cpp#L93)) —
  filled only where normalization loses the spelling: `id` placeholders and
  literals (string/char/number/raw-string). Keyword/operator/callee — `raw` is empty,
  `rawSeq` falls back to `sym`.
- **`Fragment.rawSeq`** — aligned 1:1 with `seq`, filled in `makeFragment`
  (the only fragment factory → OOB excluded).
- **`cloneType(fa, fb)`** — pure function: `seq != seq` ⇒ STRUCTURAL; otherwise by the
  raw diff at `id`/`lit` positions ⇒ EXACT / RENAMED / LITERAL / MIXED. CCN < 15
  (lizard doesn't flag it).
- **The label in the output** of both reporters: `[weighted=1 RENAMED] …`.
- **Verification on the fixture** `fixtures_clone_type/sample.cpp` (4 families × 2
  functions). Debug build clean, the snapshot run gives exactly the expected:
  `A1↔A2 EXACT`, `B1↔B2 RENAMED`, `C1↔C2 LITERAL`, `D1↔D2 STRUCTURAL`.
  In `--partial-precise` cross-family pairs (a different callee) correctly go as
  STRUCTURAL, and the diff view shows `~ validateInput -> parseHeader` — the seed of LD.11.
- **LD.11 explanation** ([main.cpp:937](../../experiments/partial_duplication/main.cpp#L937)):
  `DiffOp` extended with source indices `ai/bj` — we reuse the existing
  `diffTokens` LCS backtrack (no DP duplication), and at `=` positions we read
  the `rawSeq` of both fragments. What normalization hid (`id↔id`, `lit↔lit`
  with different spelling) surfaces as `ignored identifiers/literals`. The printing
  is extracted into `printIgnored` (print dedup + function lengths under the threshold).
  Run on the fixture: `RENAMED` → `head -> alpha; conf -> beta; …`;
  `LITERAL` → `0.5 -> 0.9; 2 -> 3; …`; `STRUCTURAL` → matched/similarity + ignored
  in the aligned region + token-diff with callee changes. Exactly spec LD.11.
- **lizard** (`--CCN 15 --length 30 --arguments 5`): no new warnings;
  `cloneType` / `explainPair` / `printIgnored` under the thresholds. Pre-existing debt in
  `lex`/`main`/`diffTokens` not touched (`diffTokens` nudged +2 NLOC for passing the indices).

## In progress

- Nothing. LD.10 + LD.11 implemented and verified on the fixture in the spike.

## Next steps

1. (on request) close the task: `git mv wip → completed`, write the "How it works / Diagnostics" sections.
2. (later, separately) at the product integration of #056 into the binary — move the classifier+explain over and set up real `fixtures/clone_type/pass|fail_*`.
3. Further along #071: LD.14 (Clone Growth, sits on baseline/drift) or LD.16 (cross-module matrix, the most Lakos-native). explain is currently under `--partial-precise`; at integration — decide whether a separate `--explain` is needed.

## Key decisions

| Decision | Reason |
|---------|---------|
| Pass `raw` through `Token`/`Fragment`, not re-lex | EXACT/RENAMED/LITERAL are indistinguishable without raw; re-lexing a pair is expensive and duplicates logic |
| The `raw` field after `line` in `Token` | Preserves existing `{sym, line}` aggregate initializers (there are many) |
| `seq != seq` ⇒ STRUCTURAL before comparing raw | A normalized divergence = a structural edit by definition; compare raw only on a 1:1 alignment |
| Do it in the spike, not in the binary | #056 isn't in the product tree yet; integration — a separate step |

## Changed files

| File | Change |
|------|-----------|
| `experiments/partial_duplication/main.cpp` | LD.10: `Token.raw`, `Fragment.rawSeq`, `cloneType()`, label. LD.11: `DiffOp.ai/bj`, `explainPair()`, `printIgnored()`, explain block in the precise reporter |
| `experiments/partial_duplication/fixtures_clone_type/` | fixture with pairs of known type (4 families × 2) |

## Fixtures (if a rule)

- [ ] `fixtures_clone_type/` — EXACT / RENAMED / LITERAL / STRUCTURAL pairs in one file, label verification in the spike output (spike level; product `fixtures/clone_type/pass|fail_*` are set up at #056 integration)

## How it works

Each token carries `raw` (the source text) alongside the normalized `sym`; a pair of fragments
is aligned by the normalized sequence. **LD.10** assigns a label from the comparison of the
aligned tokens: both `seq` and `raw` matched → `EXACT`; only the `raw` identifier diverged
→ `RENAMED`; only the literal diverged → `LITERAL`; the normalized `seq` diverged →
`STRUCTURAL`; a mix → `MIXED`. **LD.11** (`explainPair`) shows the matched tokens, similarity %,
and the list of renames/literal changes hidden by normalization — the developer sees the nature of
the clone and *why* it matched, without reading both fragments by eye.

## Outcome

**Status:** completed (research spike). LD.10 + LD.11 implemented and verified on a fixture in
`experiments/partial_duplication/`. Product integration (porting into the binary + real
`fixtures/clone_type/`) deliberately deferred until the #056 port — as a separate step, not a blocker.
