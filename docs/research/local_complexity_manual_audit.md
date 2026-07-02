# Local Complexity Manual Audit

**Date:** 2026-07-02
**Task:** #162
**Binary:** `archcheck 0.1.5` (Debug, HEAD `4a38bde`)
**Verdict up front:** on a fresh 92-finding stratified sample (55 repos), the score
arithmetic was correct in every hand-checked case and the finding was factually right in
**90%** (TP 54% + USEFUL_ADVISORY 36%); FP 10%, concentrated in scope/move classes, not
in the metric. All previously-fixed #109 FP classes (overload cross-match, arity-change
fallback, deletion shift, lambda parsing) did **not** reproduce. Signal is public-ready
as an advisory; new scope/move gaps filed in #164.

## Commands

```bash
cmake --build build/debug && ./build/debug/tests/archcheck_tests   # 616/616
# Sampling: from experiments/per_commit/results_full.boolrule.jsonl rows with
# n_complexity>0, pick 55 distinct repos (seed=159), 1 random finding-commit each,
# re-run the CURRENT binary:
./build/debug/src/archcheck --diff --diff-mode=memory <sha>^..<sha> <repo>
# -> 92 DRIFT.LOCAL_COMPLEXITY findings; every diff opened by eye
#    (git show <sha>^:<file> vs <sha>:<file>, function bodies read, moves checked
#    via parent-tree greps / body md5).
```

Historical reference: #109 pinned run 7 (84 repos, 945 finding commits, 1813
violations) with FP classes A–K fixed; old `triage.tsv` = 189 reviewed commits
(167 TP / 15 MIXED / 7 FP).

## Verdict summary (92 findings, 55 repos, fresh 2026-07-02 sample)

| Verdict | Count | Share |
|---|---:|---:|
| TP (genuine new branching/nesting in the changed function) | 50 | 54% |
| USEFUL_ADVISORY (mechanically true, lower review value) | 33 | 36% |
| FP (scope leak, move/extraction misframe, mirror repo) | 9 | 10% |

Caveat: USEFUL_ADVISORY is inflated by one commit — NumKong's deliberate 3354-line SIMD
backend drop produced 14 kernel findings in one diff (flood, not signal). Per-commit
view: of 55 sampled commits, ~40 are TP-dominant.

Two-reviewer control: 2 findings independently classified twice, blind (carbon-lang
`IsSimpleAbiType` → TP; FastJsonDL `renderDeflatedJson` → TP); verdicts matched.

## Metric spot-checks (arithmetic verified by hand)

- `connectRooms` (Creation-Engine): hand-computed cognitive complexity = **32**, matches
  the reported score exactly (nesting-weighted, boolean-operator counting).
- `Unit::GetScheduledChangeAI` (EG-Source) 2→7: nesting weights 1+1+2+3 check out.
- `Menu::ProcessInputEventQueue` (skyrim) +5 from ONE `if` at nesting depth 4 —
  depth-consistent; the new sibling helper's complexity was correctly NOT bled in.

## Previously-fixed FP classes — regression check (the key result)

| #109 class | Reproduced? | Evidence |
|---|---|---|
| Overload cross-match | **NO** | umdk `enqueue_plus` vs `dequeue_plus` siblings correctly separated; gfxreconstruct `BlockParser::` vs `FileProcessor::ReadBlockBuffer` class-qualified correctly. |
| Arity-change fallback (false "new") | **NO** | carbon `IsSimpleAbiType` (+1 param), plasma `showInhibitionSummary` (0→3 params), Volt (+1 param), AdvancedVulkanDemos (+1 param) — all tracked as "grew". |
| Deletion shift | **NO** | not observed in 92 findings. |
| Lambda parsing | **NO** | lambdas in skyrim/Volt/tasksmack correctly attributed to enclosing functions. |
| Rename/intra-file move as "new" | **NO** | CollaboraOnline's moved `arrangePresentationWindows` not flagged; NumKong verified as genuinely new file. |
| Test leak | **YES (new sub-form)** | bare stem `tests.c` (secp256k1) — see FP taxonomy. |
| Cross-file move | **YES (two new sub-forms)** | copy-as-new (Cal3D), merge-as-grew (jscPrime) — see below. |
| Preprocessor artifacts | **Partial** | `#if DEBUG` loops inflate deltas (~6 of +17, steganosaurus) but never fabricated a finding alone. |

## FP taxonomy (9 findings, root causes verified)

| Class | Count | Example | Root cause / status |
|---|---:|---|---|
| Vendor-mirror repo | 3 | canmeng12_packages (OpenWrt package dump, 100% bot commits) | corpus-gate issue, not scanner; document |
| Copy of pre-existing vendored code as "new" ×2 paths | 2 | Cal3D `calculateBoundingBox` (md5-identical to in-tree `cal3d-src/`) | `cal3d-src` not a recognized vendor dir + no same-body dedup; #164 |
| Move not suppressed on the GREW path | 1 | Windows-CalcEngine `jscPrime` 1→7 (deleted override's body absorbed by base stub) | movedPool guards only "new" reports; #164 |
| Extract-method reported as new over-threshold function | 2 | edk2-nvidia `TH500UpdatePcieNode` (commit says "Move..."), Creation-Engine `connectRooms` ("split long functions") | partial extraction can't match whole-function move pool; #164 (hard — may stay documented limitation) |
| Test-basename gap | 1 | secp256k1 `src/tests.c` | bare stems `test`/`tests` not in `isTestBasename`; #164 |

## Twelve instructive examples

### 1. hypertube `validateConfig` — ideal TP (new 48)

A genuinely new Copilot-authored validator with FOUR near-identical 3-deep
`contains/is_number/<0` blocks (download/upload × legacy/new schema). The score is
driven by extractable duplication — exactly what a reviewer should have pushed back on.

### 2. FluidDial `onEncoder` 15→31 (crossed) — TP via mechanical guard-wrapping

```cpp
 if (delta > 0 && myFro < 200) {
-    fnc_realtime(FeedOvrFinePlus);
+    if (transport) {
+        transport->sendRT(FeedOvrFinePlus);
+    }
 }
```
Six `if (transport)` wraps at depth 3–4 doubled the score. Substantively true — the fix
is a null-safe `sendRT` helper; the three sibling functions with the same pattern stayed
below threshold (correct thresholding behavior).

### 3. carbon-lang `IsSimpleAbiType` 6→11 — TP across a signature change

Gained a `bool for_parameter` parameter AND real branches (lvalue-ref, enum-underlying
type); tracked as "grew", not falsely "new" — the historical arity FP did not fire.

### 4. plasma-workspace `showInhibitionSummary` 5→21 — TP, 0→3 params matched correctly

Four genuinely new nested filter-guards (urgency, two blacklists) in the loop body.

### 5. LAPIS-SILO `getOutputSchema` — TP: `return {};` → 8 sequential `if (has(FIELD))` guards.

### 6. Windows-CalcEngine `jscPrime` 1→7 — FP: move on the grew path

The deleted `PhotovoltaicSpecularBSDFLayer::jscPrime` body (if+for+for, verbatim) was
absorbed by a base-class stub; only a 1-branch guard is new. 85% of the delta is
relocated code. The move pool exists but is consulted only for "new function" reports.

### 7. edk2-nvidia `TH500UpdatePcieNode` new 38 — FP: extraction punished

Commit message literally says "Move th500-specific PCIe node updates to separate
function". The parent got simpler; the report flags a complexity-REDUCING refactor.

### 8. secp256k1-zkp `tests.c` — FP: bare-stem test leak

A real new if/else, but in `src/tests.c`; the rule intends to drop test files and
`isTestBasename` misses the bare `tests` stem (needs `test_` prefix or `_test` suffix).

### 9. canmeng12_packages ×3 — FP: vendor-mirror repo

Real upstream growth (OpenAppFilter), but the repo is a bot-synced package aggregation
(100% `github-actions[bot]` "update <timestamp>" commits) — no author for the advisory
to reach. Corpus-gate class, not a scanner bug.

### 10. zlib-ng `crc32_copy_impl` 58→63 — ADVISORY: complexity relocation

`fold_16`'s `while` + two `if (COPY)` blocks were inlined into the caller while
`fold_16` became branch-free. Net function-set complexity conserved; the rule sees only
the receiving side. A commit-level net view would de-noise this class.

### 11. xgboost `Populate` new 25 — ADVISORY: `if constexpr` counts as branching

~6 of the branch points are compile-time `if constexpr` in a template-recursive tree
populate; template-heavy code systematically over-scores relative to runtime complexity.

### 12. ATenSpace `removeAtom` — TP with a smell: the added `if/if/for` nesting is real,
but the loop body is comment-only Copilot scaffolding (dead code a reviewer would delete).

## Post-fix re-run 2026-07-02

The #164 B.2 fix (bare `test`/`tests` stems in `isTestBasename`) shipped the same night
with the bool-parser fix pack; the identical 55-commit sample was re-run:

| | pre-fix | post-fix |
|---|---:|---:|
| Findings | 92 | 91 |
| FP | 9 (10%) | 8 (9%) |

- Removed, verified: secp256k1 `src/tests.c` (test-basename leak) — the only LCX FP with
  a shipped fix in this pass.
- Remaining 8 FP are exactly the OPEN #164 items, all inherent or deferred by design:
  canmeng ×3 (vendor-mirror repo — corpus provenance, not the scanner), Cal3D ×2
  (`cal3d-src` vendored snapshot + same-body dedup, B.4/C.2), jscPrime (grew-path
  movedPool, C.1), TH500 + connectRooms (extraction-as-new, C.3 — documented limitation).
- Group-3 duplication gate re-run after the classification changes: recall 41.4% /
  suppression 89.2% / precision 76.3% — identical to pre-fix, zero collateral on the
  clone corpus.

## Recommendation

1. **Keep `DRIFT.LOCAL_COMPLEXITY` advisory, ship it.** 90% factually-useful rate on a
   fresh sample; the previously-fixed FP classes stayed fixed; metric arithmetic verified.
2. **Fix pack #164 (scope/move):** bare-stem `tests`/`test` in `isTestBasename`;
   consult the movedPool on the grew path (jscPrime reproducer); consider same-body
   cross-path dedup for bulk imports. Extraction-as-new is harder — document as a known
   limitation unless a cheap partial-body match emerges.
3. **Wording:** keep "grew local complexity from A to B" — it states measurement, not
   judgment. For public docs, prefer clean TP examples (AlchemyViewer AVIF from the
   earlier live check, FluidDial onEncoder, plasma showInhibitionSummary) and avoid
   mega-import/one-shot-feature statistics unless labelled as low review value.
4. Design caveats to document (not bugs): `if constexpr` counted as branching;
   complexity relocation (inlining/decomposition) indistinguishable from creation
   without a commit-level net view; `#if` both-arms counting inflates magnitudes.
