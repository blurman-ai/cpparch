# [DUPLICATION][RESEARCH] Quantitative validation against external oracles (deferred from #107)

**Created:** 2026-06-11
**Started:** 2026-06-19
**Completed:** 2026-06-19
**Status:** completed
**Related:** #107 (methodology + data), #106 (fixtures), #070/#059 (precision)

**Outcome:** the goal (quantitative validation against external oracles) was achieved through
TWO oracles. Step 1 (NiCad/monit) — numbers + disagreement triage, 0 recall bugs.
Step 3 (POJ-104) — Type-4 FP boundary confirmed (0 cross-label across 39582 candidates).
Step 4 (triage width) — 0-FP beyond LibreSprite (vmecpp 42/42 + corpus of 18 repos).
Step 2 (Bellon) — descoped: the canonical source of the 2002 sources is dead (not debt).
Consolidated report for the paper: `reports/clone_detection_comparison.md`.
Raw run data: `experiments/clone_oracle_validation/` (gitignored).

Deferred remainder of #107 — to obtain NUMBERS (precision/recall) against an external ground truth:

1. **NiCad/monit**: install TXL (txl.ca) + build NiCad → take an XML oracle on
   `examples/monit-4.2` → compare with our output by `file:line` overlap
   via disagreement triage (#071 extractability). Setup estimate: 0.5–1 day.
2. **Bellon**: blockers — no cook/weltab sources for the 2002 version (the ISO contains only
   results) and the oracle is in binary RCF (needs a Bauhaus reader or a CSV re-release).
   The textual per-tool CPF candidates parse trivially.
3. (opt.) POJ-104 as an FP boundary (Type-4 we must NOT flag).

Data already downloaded: `experiments/clone_oracle_validation/{downloads,bellon,nicad,pmd}`.
Methodology and the full history — in `backlog/completed/107_*.md` and
`experiments/clone_oracle_validation/FINDINGS.md`.

4. (from #071) Triage width: run `experiments/triage_dup_commits.py` on vmecpp and
   corpus repos — confirm 0-FP beyond LibreSprite.

---

## Progress (2026-06-19)

### Step 1 — NiCad/monit ✅ CLOSED (numbers + triage in place)

**The TXL blocker from #107 was false:** TXL is installed (`~/bin/txl` v10.8b),
it was simply not on PATH. NiCad was built after 2 environment fixes (in the unpacked NiCad, not in
archcheck): version-guard `10.[56]`→`10.[5-9]`; `tools/Makefile` `-m32`→`-m64`.

Results in `experiments/clone_oracle_validation/NICAD_QUANT.md` (+ `nicad_join.py`,
`results/archcheck_monit.txt`). Key points:
- NiCad: 27 pairs / 12 classes; archcheck: 21 pairs.
- Naive edge-recall 12/27=0.44 — **misleading** (NiCad enumerates full
  cliques, we report stars + intentional guards).
- **Class-level recall = 8/12 = 0.667.**
- Triage of the 4 uncovered classes: 1× whole-file suppression (category a, by design),
  3× below our floor 0.75+P0.6 / benign read-write siblings (category b).
  **Real recall bugs (c) — 0.**
- archcheck-only 9 pairs — **0 FP**: all real clones below NiCad `minsize=10`
  (small same-file copy-paste that we catch but NiCad does not).

### Step 4 — triage width ✅ CLOSED (0-FP confirmed beyond LibreSprite)

Results in `experiments/clone_oracle_validation/TRIAGE_WIDTH.md`
(+ `dup_triage_vmecpp.md`, `corpus_pairs.txt`, `corpus_sample.txt`).
- The 8→42 pair jump was explained: the June report **leaked test files**; tests
  are excluded from duplication on purpose since #070 (confirmed by `git show ec5988b^`).
  **Not a regression.** The v1 report was deleted, v2 is canonical.
- **vmecpp: all 42 pairs classified** (extractability #071): 12 TP + 14 TP-variant +
  16 benign, **0 FP**.
- **Corpus: 18 C++ repos at HEAD, 8094 pairs.** The risk zone was analyzed (22 pairs with
  the lowest weight 0.51–0.55) — all real duplicates, **0 FP**. The 0.6 floor cuts
  just before the zone of random coincidences.
- Observation: generated Rcpp bindings (tulpa) — real duplicates, candidates for
  a @generated exclusion (#127/#131), not FP.

### Step 2 — Bellon 🔴 BLOCKED PERMANENTLY (external source dead)

Re-verified: `.rcf` = textual XML schema + **binary tuples** (needs an RCF reader,
none available). ISO = only `results/` + `*-dummydeclarations.h`, **no cook/weltab
2002 sources** → the `file:line` join key cannot be reconstructed.
- Attempt to fetch the sources from the network (2026-06-19): the canonical Bellon site
  `bauhaus-stuttgart.de/clones/` **is dead** — a 303 redirect to domain parking
  (`ts.domainname.de`). No public source of the 2002 versions exists.
- Unlike TXL (found locally), there is nothing to finish Bellon with. **Descoped permanently**
  — not "debt", but the absence of an external artifact (cf. [[feedback_task_blocked_vs_completed]]).

### Step 3 — POJ-104 ✅ CLOSED (Type-4 FP boundary confirmed)

Results in `experiments/clone_oracle_validation/POJ_FP_BOUNDARY.md`
(+ `poj104/train.parquet`, `poj104/archcheck_poj.txt`). Dataset from HuggingFace
(`google/code_x_glue_cc_clone_detection_poj104`).
- 1950 programs / 65 problem classes / 39582 candidates → **15 pairs, all same-label,
  0 cross-label**. Type-4 (one problem, textually different implementations) we do NOT flag.
- The 15 same-label pairs — real textual clones (Type-1/2/3: shared code,
  rename), not semantic ones. Matches "Type-4 — no, deliberately".

### Consolidated report for the paper

`reports/clone_detection_comparison.md` — consolidates: corpus C++ head-to-head
(reports/nicad_vs_archcheck.md), the new monit-pure-C run (NICAD_QUANT.md) and
the landscape across other tools (docs/research/clone_tools_landscape.md). Main thesis:
NiCad ≠ ground truth; on pure C we are at parity with the precision reference, on C++ —
the only one that gives an actionable signal. Corrects the reading of "recall 0.667 = worse".
