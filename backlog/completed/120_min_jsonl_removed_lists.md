# [SCAN] generate_per_commit_graph_drift: save lists of removed edges into jsonl

**Created:** 2026-06-12
**Started:** 2026-06-12
**Completed:** 2026-06-12
**Status:** done
**Module:** SCAN
**Priority:** minor
**Difficulty:** small
**Blocks:** —
**Blocked by:** —
**Related:** #111 (discovered there — technical finding §5.1 of the report), #119 (file-split: removed lists would give precise rename detection)

> Anchors in `experiments/ai_repo_run/generate_per_commit_graph_drift.py` removed 2026-06-12.

## Goal

Append the full lists of removed edges to the per-commit jsonl so that the replay graph
(`lateral_drift_scan.py::IncrementalGraph`) does not accumulate phantoms (currently ≤5.6% of edges)
and the criterion's persistence condition starts working (currently satisfied trivially).

## Context

`generate_per_commit_graph_drift.py` parses the text output of `archcheck --diff
<sha>~1..<sha>` and puts into the record (lines 165-173):

```python
"added": collect_section(out, "added:"),                                  # present
"new_cross_area_dependencies": collect_section(out, "new_cross_area_dependencies:"),  # present
# NO removed LIST — only the removed_edges counter (line 69, from the summary)
```

Consequences (recorded in
[lateral_module_drift_corpus_run.md](../../docs/research/lateral_module_drift_corpus_run.md) §5.1):
forward-only replay of additions accumulates phantom edges (share by counters ≤5.6%);
the criterion's persistence condition (§3 item 5 of the criterion doc) is not checked.

## Execution plan

- [ ] **Step 0 — find the exact section header.** Run
      `./build/debug/src/archcheck --diff HEAD~1..HEAD .` on any repo with a removed
      include and see how the text reporter names the removed-edges section
      (expected `removed:` — cross-check against the `src/diff/` text output; `RegressionReport`
      has `removedEdges`). If there is NO list in the output at all — first add
      it to the text reporter of diff mode (mirror added, the same line format).
- [ ] **Step 1 — generator.** One line in the record (next to line 171):
      `"removed": collect_section(out, "removed:"),`
      `collect_section` (line 86) already knows how to parse sections.
- [ ] **Step 2 — consumer.** `lateral_drift_scan.py::IncrementalGraph`:
      method `remove(from_f, to_f)` — mirror of `add()` (line 333): remove the file
      edge; recompute the module-pair counter; the pair disappears when no file edges
      of the pair remain (so store a per-pair counter, not a bool). In
      `scan_repo()` apply `removed` BEFORE `added` of the same commit
      (git diff order). **Backward compatibility:** `record.get("removed", [])` —
      old jsonl without the field work as before.
- [ ] **Step 3 — persistence (optional, separate commit).** After a full
      pass over the repo: events whose pair (A,B) is empty again by the end of the window mark
      `persisted=False` (a new CSV column), do NOT delete — filtering on the
      analysis side. This enables condition 5 of the criterion.
- [ ] **Step 4 — validation.** On 2-3 repos with non-zero `removed_edges` (search for:
      `jq 'select(.removed_edges>0)' *_graph_drift.jsonl | head`): regenerate the
      jsonl, run the scan, cross-check: (a) fewer phantoms — the edge count of the replay graph
      at the end of the window is closer to the edge count of the real HEAD graph;
      (b) existing events did not disappear without reason.

## Important

- **Regeneration of all 481 jsonl is NOT required** — the field is optional; a full
  regeneration (~2 days for the regen of 43 repos in #111) is done only if someone
  actually enables persistence on the full corpus.
- Do not touch the format of existing fields — the jsonl already has three consumers
  (lateral_drift_scan, merge_summary, make_examples).

## Done

- Step 0: `archcheck --diff` outputs `removed (advisory):` — the section is present.
- Step 1: the field `"removed": collect_section(out, "removed (advisory):")` is already added
  to the generator (line 172) — both copies are identical (`experiments/` and `analysis/`).
- Step 2: `IncrementalGraph.remove()` is implemented (lines 349-366 of `lateral_drift_scan.py`);
  `removed` is applied before `added` (lines 472-479); backward compatibility
  `rec.get('removed', [])` on line 472.

- Step 4: validation on `TocinLang` (removed_edges=8 == len(removed)=8) and `Boxedwine64`
  (855 commits: 30 records with removed>0, in all of them counter == len(removed), 0 discrepancies).
  Regenerated jsonl: `/tmp/boxedwine_new.jsonl`.

## In progress

- (empty)

## Next steps

1. Step 3 (persisted column) — optional, a separate step later (out of scope for #120).

## Key decisions

| Decision | Reason |
|---------|---------|
| Optional field, no corpus regeneration | Regeneration takes about two days; phantoms ≤5.6% are tolerable until persistence is actually needed |
| removed before added in replay | Diff order: a move = remove+add, otherwise the pair would blink to "zero" |
| persisted column instead of filtering | The decision to discard is on the analysis side, do not lose data |

## Changed files

| File | Change |
|------|-----------|
| `experiments/ai_repo_run/generate_per_commit_graph_drift.py` | +1 line (removed field) |
| `src/diff/` text output | Only if the removed section is not in the output (step 0) |
| `experiments/ai_repo_run/lateral_drift_scan.py` | `IncrementalGraph.remove()` + replay order + persisted |

## How it works (summary)

**Principle.** The generator `generate_per_commit_graph_drift.py` parses the
`removed (advisory):` section from `archcheck --diff` and puts the list of removed edges into the jsonl record
(field `removed`, next to `added`). The consumer `lateral_drift_scan.py::IncrementalGraph.remove()`
applies them BEFORE `added` of the same commit (git diff order: a move = remove+add), so that
forward-replay does not accumulate phantom edges.

**Per-pair counter.** A module pair disappears from the graph only when no
file edge between the modules remains (`remove()` decrements FID/FOD and discards the pair at zero).

**Backward compatibility.** `rec.get('removed', [])` — old jsonl without the field work as
before (replay of additions only).

## What controls it

The field is optional — a full corpus regeneration is NOT required (phantoms ≤5.6% are tolerable).
The section header in the archcheck output: `removed (advisory):`. Step 3 (persisted column) is
deliberately out of scope.

## What it relates to

Discovered in #111 (§5.1 of the report). Feeds #119 (removed lists = precise rename signal) and
the general corpus scanner of lateral drift. The jsonl has three consumers: lateral_drift_scan,
merge_summary, make_examples — the format of existing fields was not touched.

## Diagnostics

Validation (2026-06-12): `TocinLang` (removed_edges=8 == len(removed)=8); `Boxedwine64`
(855 commits, 30 records with removed>0, in all of them `removed_edges == len(removed)`, 0 discrepancies).
Integrity check: `removed_edges` (counter from the summary) must equal `len(removed)`
(list) in every record.
