# [RESEARCH][CORPUS] bool_field_added — a per-commit column for correlations (sidecar + join)

**Created:** 2026-06-23
**Started:** 2026-06-23
**Completed:** 2026-06-25
**Status:** done (SUPERSEDED by #090)
**Module:** RESEARCH / experiments
**Priority:** major
**Blocks:** the correlation analysis of #119 (dependence of some findings on others)
**Blocked by:** —
**Related:** #134 (history per-struct drift — a different axis), #090 (drift metric into the product — ABSORBED this task), #124 (per-commit corpus run), #119 (unified per-commit drift correlation), #093 (flag_argument — a pattern exemplar)

## OUTCOME (2026-06-25) — SUPERSEDED by #090, the goal was achieved by a different path

This task chose **path B (a Python sidecar)** for cheapness. Along the way the user rejected
the Python duplicate of the filter ("why a Python script and not our filter?") and chose **path A — the native
rule #090** (`DRIFT.BOOL_FIELD_ACCRETION`). So the column was produced by the native corpus run of #090
(`results_full.boolrule.jsonl`, the `n_bool_field` column), and NOT the sidecar:
- **Section 1 (the sidecar script)** — written and smoke-validated 5/5 (`bool_field_added_per_commit.py`),
  but a full run over the worklist is **not needed** (the column was produced by #090).
- **Section 2 (the join)** — **dropped**: the native run immediately produced the full table with the column.
- **Section 4 (self-check)** — performed on the native column: an FP check 22/22 TP (each commit
  independently via `struct_fields` on sha^/sha), the distribution checked (median, outliers 105/183).

**The main contribution of #135 that survived the supersession:** a revision of the metric's definition
(added lines → **net count**, catches rename/replace/reformat → 0) and the catch of the parser bug #136 —
both went into #090. **Section 3 (correlations)** — moves to **#119** (config-bag magnitude
filter + control for commit size). This task is closed as achieved by a different path.

## Goal

Add to the per-commit corpus table (`experiments/per_commit/results_full.jsonl`) a column **`n_bool_field_added`** — how many boolean fields a given commit added to the body of a struct/class. This is a per-commit quantum (like `n_flag_arg`) that sits next to `n_complexity`, `n_newclone`, `n_satd`, `n_flag_arg` and allows computing **correlations** between the growth of booleans and copy-paste / complexity growth — the very thing the correlation analysis of #119 was started for.

## Why a sidecar and not the binary

**Granularity.** Per-struct drift (#134) is a history metric (a structure accreted booleans over ≥4 commits, axis = the structure). It CANNOT be stitched as a column onto the per-commit table (axis = the commit) — different units.

But "a commit added a bool field to a struct" is already a per-commit quantum, and it can go into a column. Two paths:
- **Path A (the binary):** a field detector in `src/` → `archcheck --diff` JSON → `categorize()` → a full re-run of the corpus (~12h). De facto a piece of #090. Expensive, touches the product.
- **Path B (a sidecar) — THIS TASK:** a separate Python pass over the worklist, counts added bool fields by the diff of each commit, writes them to its own JSONL keyed by `(repo, sha)`, then **joins** into `results_full.jsonl`. The binary isn't touched, no re-run needed. Cheap.

**B** is chosen: the goal is a research column for correlations, not a product feature. If the correlation turns out to be strong and useful — that is an argument for #090 (path A) already at the product level.

## What this is NOT

- NOT a rule implementation in `src/` (that's #090, deferred per YAGNI).
- NOT a re-run of the corpus binary.
- NOT history drift (that's #134).
- NOT a static counter of "lots of booleans right now" (78% noise per #089). We measure the **addition** of a field by a commit — a delta, not a level.

## Input data (all in place)

- Worklist: `experiments/per_commit/worklist_full.tsv` (repo<TAB>sha, 1685 repos).
- The table to join into: `experiments/per_commit/results_full.jsonl` (key — `repo`+`sha` in each record).
- Reuse the field parser from `experiments/boolean_state/perstruct_drift.py`: `STRUCT_RE`, `BOOL_RE`, `find_body`, `struct_fields` (depth-0, no signatures/locals — already validated in #089, 0% gross FP).
- Corpus: `~/oss/<repo>/`.

## Metric definition (REVISION 2026-06-23 after validation on live commits)

`n_bool_field_added` for a commit = the sum over structures of the **net increase** of the number of
depth-0 bool fields in structures **that existed in the parent** (`sha^`):

```
for each struct/class S whose definition exists in sha^ (by STRUCT_RE):
    delta(S) = |bool fields of S in sha|  −  |bool fields of S in sha^|    # depth-0, no signatures/locals
n_bool_field_added = Σ max(0, delta(S))
```

- **Existence of the structure** — by the presence of its *definition* in `sha^` (`STRUCT_RE`),
  NOT by "did it have booleans": a structure with 0 booleans that grew to N is drift.
- The structure isn't in `sha^` (new file / new struct) → **greenfield, not counted** (not drift).
- Field attribution — `struct_fields` (depth-0, no signatures/locals; fields nested in a
  sub-struct/union at depth>0 are NOT counted — the #089 boundary, a conservative undercount).

### Why a net count and NOT added lines (the first version was wrong)

The original formulation ("count `+bool` added lines filtered by the diff") gives
**false positives** on rename / typo-fix / replace: the line `+bool foo;` appears,
but the number of booleans in the structure doesn't grow (the old one left as a `-bool` line). Caught in validation:
- `intel/EventDescriptor`,`Event`: a typo-fix `kerneMapped→kernelMapped` — 2 false `+1`;
- `circt/FunctionLowering`: a replace `capturesFinalized→bodyConverted` — a false `+1`.

The **net delta** (`count_after − count_before` per structure) kills rename,
replace and reformatting at once: in those cases the count doesn't change → 0. We measure exactly the INCREASE in the number of
booleans, not the textual added lines. (Validation prototype: `experiments/boolean_state/_proto_bool_field_added.py`.)

### The metric is neutral — not a verdict

`n_bool_field_added` measures the accretion of boolean state as a **neutral phenomenon**, not
"bad code": some of the added booleans are legitimately orthogonal, some are boolean-blindness.
You can't tell them apart by eye one by one; the interpretation comes from the **correlation** (#119), not the raw count.
In detail, with an exemplar (FlashCpp `LambdaExpressionNode`, `constexpr ⊥ consteval`) —
[docs/research/boolean_state_accretion_good_vs_smell.md](../../docs/research/boolean_state_accretion_good_vs_smell.md).

## Execution plan

### 1. Sidecar script `experiments/boolean_state/bool_field_added_per_commit.py` ✅ DONE (2026-06-23)
The logic is validated in the prototype `_proto_bool_field_added.py` (`bool_fields_added`), built from it.
- [x] worklist (repo, sha); per commit `git diff --name-only <sha>^..<sha> -- '*.h'…` (we do NOT parse added lines — the metric is by net count).
- [x] Per header: `struct_fields` on `sha^` and `sha`; "existence" — `STRUCT_RE` in `sha^`; `delta = |bool sha| − |bool sha^|`, sum `max(0, delta)`.
- [x] Resumable (`load_done`), ThreadPool (≤8 workers), per-commit timeout, killpg of the group (`start_new_session`).
- [x] Output: `{"repo","sha","status","n_bool_field_added":N,"structs":[{"struct","delta","gained","file"}]}`.
- [x] Parentless/shallow — precompute `rev-list --max-parents=0` → `skipped_no_parent`.

**Validation (smoke):** 5/5 inspected commits = reference (FlashCpp=3, ovn=1, kwin=2, circt=0, intel=1); resume `done=5 todo=0`; parentless-skip OK.
**Remaining:** a run over the full `experiments/per_commit/worklist_full.tsv` (~1.05M) → `experiments/per_commit/bool_field_added.jsonl`.

### 2. Join into the correlation table
- [ ] Join script: read `bool_field_added.jsonl` into a dict by `(repo,sha)`, walk `results_full.jsonl`, append the field `n_bool_field_added` (0 if the commit isn't in the sidecar), write to `results_full_with_bool.jsonl` (do NOT overwrite the original — it is validated).

### 3. Correlations (under #119)
- [ ] **BEFORE correlating, clean the tail of 3 non-drift classes** (an on-the-fly eye check of the n=181 outlier showed that the top of the tail is exactly these; the data for a filter is sufficient: each record has a `file` and `struct`):
  - **vendored** — by path (`/vendor/`, `/third_party/`, `/extern/`, `/external/`, `/lib/`, `/examples?/`; exactly the `VEND` regex from `perstruct_drift.py`, which was NOT applied in the sidecar — we kept the run raw). Example: lobster `dev/external/SDL/` (+143).
  - **generated** — by path/name (`.pb.h`, `/generated/`, `*_generated.h`). Example: meshtastic `*.pb.h` (+51).
  - **config-bag** — **magnitude criterion: a threshold on the total number of bool fields in the structure** (e.g. >15–20 → flag-bag/config). Checked on real structures (see below): a config-bag gives itself away by the ABSOLUTE number of booleans, not by its form. The `CFG_NAME` regex (`config/settings/options/...`) is a weak secondary signal. Compute on the join: by `(repo,sha,file,struct)` re-read the structure at `sha`, count the total bools — ~9k non-zero hits, cheap, no restart. The semantics (methods branch on booleans = genuine boolean-blindness) is more precise, but requires libclang/.cpp → #090/v0.2.
    - **REJECTED (verified empirically): "few methods = config-bag".** PQCSettings (config) = 271 fields / **1343 methods** (getter/setter per setting) / 183 booleans; while the legit drift `physical_ctx` = 28 fields / **0 methods** / 2 booleans, `IpcEventPoolData` = 9 / 0 / 4. By method count, config looks the OPPOSITE of legit. bool% also doesn't separate (IpcEventPoolData 44% ≈ config). The ONLY thing that separates is the absolute number of booleans: PQCSettings 183 vs legit ≤4.
  - Compute correlations both on the full set and on the cleaned one — the divergence is itself a finding.
- [ ] Compute pairwise correlations `n_bool_field_added` × {`n_complexity`, `n_newclone`, `n_flag_arg`, `added_total`}.
- [ ] **Control for commit size** (`added_total`): the "everything grows together" correlation is an artifact of diff size (Simpson, see memory #115/#117). Compute partial corr / binning by added_total, not a bare Pearson.

### 4. Self-check (mandatory — CLAUDE.md "Self-check of conclusions")
- [ ] Manually enumerate 10+ commits with `n_bool_field_added>0`: open the diff, confirm that it is really an added bool **field in a struct**, not a local variable / parameter / reformatting.
- [ ] Check that a signature reformatting (as was the case in bpftrace `is_rawtracepoints` for ARG.1) does NOT give a false `+1`.
- [ ] Reconcile the distribution: how many commits with >0, the median — is it plausible? Not "too smooth"?

## Acceptance criterion

- `bool_field_added.jsonl` covers the entire worklist (resume-resilient).
- `results_full_with_bool.jsonl` = the original + the column, the original NOT touched.
- Correlations computed WITH a control for commit size, not a bare Pearson.
- Eye check of 10+ non-zero commits: a TP/FP verdict for each, the FP class recorded.
- Conclusion: does the growth of boolean fields correlate with copy-paste / complexity **after** controlling for size, or is it a size artifact.

## Pitfalls

- **Size confound** — the main risk. "A big commit → lots of everything" creates a false correlation. Without partial corr / binning the conclusion is invalid (a repeat of the Simpson rake from #115).
- **Rename / typo-fix / replace** (an FP class, caught in validation) → the line `+bool foo;` exists, but the number of booleans doesn't grow. **Removed by the net count** (count_after − count_before), NOT by a filter of added lines. This is exactly the reason for the revision of the metric definition (see above).
- **Reformatting** (constructor parameters, methods) → the depth-0 `struct_fields` parser doesn't catch them (there's a `(`/`)`), plus the net count is insensitive to line shifts. Verified on FlashCpp (parameters+methods next to fields).
- **Nested struct/union** (depth>0) — bool fields in them are NOT counted (the #089 boundary). A conservative undercount, not an FP. Confirmed on chiaki `chiaki_session_t` (a field in a nested `struct{}` skipped on purpose).
- **shallow clones**: parentless commits give "the whole file is new". Skip as in `run_worklist.py`.
- **git orphans**: blame/show/diff in threads → `start_new_session=True` + `killpg` on timeout, ≤8 workers (the user is at the machine).
- **Do not overwrite** `results_full.jsonl` and the #089/#134 artifacts — only new files.

## Notes for the implementer (Haiku-friendly)

Do NOT rewrite the field parser — import it from `perstruct_drift.py` (validated in #089).
The metric logic is already debugged and validated piece by piece in `experiments/boolean_state/_proto_bool_field_added.py`
(the `bool_fields_added` function = net count) — build the sidecar FROM IT, don't reinvent it.
The runner modeled on `experiments/per_commit/run_worklist.py` (resume, killpg, parentless-skip — copy the patterns).
The metric is the **net increase in the number of bool fields** of a structure (count_after − count_before, ≥0), NOT added lines and NOT a level.
Correlations without a control for commit size MUST NOT be shown. Do not touch `src/`.
