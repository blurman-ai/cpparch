# Scanner Findings — Manual Evidence Base (pre-public audit index)

**Date:** 2026-07-02
**Task:** #160 (cross-signal index; deep dives in #161/#162)
**Binary:** `archcheck 0.1.5` (Debug, HEAD `4a38bde`)

The final eyes-on audit layer before the public push: what each advisory signal actually
catches, with manually verified precision numbers and real code examples. Every fresh
number below comes from findings produced by the CURRENT product binary and classified
by reading the actual before/after code — not labels, not aggregates.

## Methodology

- Sampling frame: `experiments/per_commit/results_full.boolrule.jsonl` (517,975 ok
  per-commit rows over the local OSS corpus; 10,735 bool-finding commits in 787 repos)
  for bool/LCX; the #131 Group-3 labelled corpus + the #158 eyeball triage for
  duplication.
- Fresh samples (seed=159): 45 repos × 1 commit for bool (**53 findings**), 55 repos ×
  1 commit for LCX (**92 findings**) — 145 findings manually classified in this pass;
  each re-produced with `archcheck --diff --diff-mode=memory <sha>^..<sha> <repo>` and
  inspected via `git show` before/after.
- Independent double-classification control: 4 findings classified twice, blind — all
  verdicts matched.
- Full verdict tables and 24 before/after code examples:
  [bool_field_drift_manual_audit.md](bool_field_drift_manual_audit.md),
  [local_complexity_manual_audit.md](local_complexity_manual_audit.md).
- CLI note (kept from the first checkpoint): for `--diff`, `--format=json` must precede
  the revspec; `--diff <revspec> --format=json` is parsed as a path and exits 2.

## Per-signal verdict (the public-readiness table)

| Signal | Sample | TP | USEFUL_ADVISORY | FP | Public verdict |
|---|---|---:|---:|---:|---|
| `DRIFT.BOOL_FIELD_ACCRETION` | 53 findings / 45 repos, fresh; re-run post-#164-fixes: 48 findings | 49% | 38% | 13% → **2% post-fix** (1 inherent case) | **Ship as advisory.** After the same-night #164 fix pack (const/brace-init/paren-comment parser + fmt/testlib scope), the identical sample re-run shows 98% factually correct claims and two RECOVERED real findings; the single remaining FP is inherent struct-name reuse. |
| `DRIFT.LOCAL_COMPLEXITY` | 92 findings / 55 repos, fresh; re-run post-fix: 91 findings | 54% | 36% | 10% → **9% post-fix** | **Ship as advisory.** Metric arithmetic hand-verified; all previously-fixed #109 FP classes stayed fixed; test-leak FP fixed same night; remaining FPs are the open #164 B.4/C.1/C.3 classes + vendor-mirror corpus provenance. |
| `DRIFT.NEW_CLONE` | #158: 306-row labelled corpus + 82-fire eyeball triage (11 repos) + 26-FP hand-triage | labelled precision 76.3%; the user's eyes-on review of the residual 18 "FP" reclassified most as useful copy-paste → real precision ~85-90% (consistent with #103's 86-91%) | — | **Ship as advisory** (already the demo-repo showcase signal). Residual classes documented: incidental/API-protocol clones (#159), move/split, data tables. |
| SATD / test co-evolution / flag-argument | incidental observations only (live AlchemyViewer/CopilotChess runs) | — | — | — | Keep advisory; do NOT headline in launch materials until sampled the same way. |

## Live cross-signal examples (current binary)

### CopilotChess `d19b8246` — bool + LCX + clone in one diff

```text
src/moves.h:15:   DRIFT.BOOL_FIELD_ACCRETION — struct 'Move' accreted 1 bool field(s): is_castling
src/moves.cpp:311: DRIFT.LOCAL_COMPLEXITY — new function 'GenerateKingMoves' has local complexity 25
src/moves.cpp:417: DRIFT.LOCAL_COMPLEXITY — new function 'GenerateCastlingMoves' has local complexity 32
src/moves.cpp:510: DRIFT.LOCAL_COMPLEXITY — function 'ApplyMove' grew local complexity from 7 to 42
+ 2 same-file new-clone findings
```

A chess move model gains a mode flag while move-generation logic grows and duplicates —
the "drift, seen early" pitch in a single commit. Best public demo commit found so far.

### AlchemyViewer `fc56e080` — clean LCX TP

`LLLocalBitmap::decodeBitmap` 23 → 26 (crossed 25) from a real new AVIF branch;
SATD/test co-evolution reported as advisory context.

### AlchemyViewer `9a6ace99` — historical FP class stays dead

An old #109 deletion-shift reference FP: the current binary emits no LCX finding
(only `TEST.1.prod_changed_tests_silent`). Fixed classes stay fixed.

## Known noisy classes that stay OPEN (linked tasks)

- ~~Scope/parser gaps — bool: `const bool`, brace-init `{false}`, NanaBox paren-comment
  guard, vendored `fmt/`, `testlib/`; LCX: bare `tests.c` stem~~ — **FIXED 2026-07-02**
  (#164 A.1-A.5/B.1-B.3, unit-tested, reproducers silent, samples re-run). Still open in
  #164: `cal3d-src` vendored snapshots (B.4), grew-path move suppression (C.1),
  same-body dedup (C.2).
- Extraction-as-new (LCX flags complexity-reducing refactors) → #164 C.3, likely a
  documented limitation.
- Incidental/API-protocol clones ("composition") → #159 research complete: not
  separable at token level (4 signals tested, all null); v0.2 down-rank tag documented.
- Vendor-mirror repos (bot-synced package dumps, e.g. canmeng12_packages) →
  corpus-gate criterion; also a showcase-selection filter.
- Mega-import floods (one 3.3k-line SIMD-backend commit → 14 LCX findings) — honest but
  low-value; consider per-commit finding caps in PR-comment rendering (presentation,
  not detection).

## Public-facing conclusion

The three headline advisory signals — new-clone, bool accretion, local complexity — are
evidence-backed and safe to show publicly: their claims are factually correct in
~87-90% of fresh eyes-on samples, their FP classes are enumerated with reproducers and
open tasks (#164, #159, #153), and their wording states measurements rather than
judgments. The advisory layer ships as report-only context, never as a CI gate. SATD /
test co-evolution / flag-argument remain advisories but need the same sampled audit
before featuring in launch materials.
