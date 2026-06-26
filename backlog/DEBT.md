# Tech debt / known gaps

A living registry of gaps and debt found along the way. Rule: when you hit
"this doesn't exist / there's a hole here" → tell the user (what/consequences/how to fix/question),
and if we don't fix it right away — record it here. **Cleared on `/backlog-review`**:
stale → into a `backlog/new/` task, unneeded → strike out.

Entry format:
- **[YYYY-MM-DD] Short name** — what it is; *consequence*; how to fix; (related #NNN). Status.

---

## Open

_(empty)_

## Closed

- **[2026-06-14→15] archcheck silently diffs against an empty tree on an unresolvable
  baseline-ref** — `archcheck --diff X..Y`, where `X` (e.g. `sha^` of a root/shallow-boundary
  commit) does not resolve: the binary silently took an empty baseline → the whole repository was counted
  as "added" (on a shallow corpus ~50-62% of graph findings were this artifact). *Decision
  (user): variant (a)* — a warning to stderr, without changing exit/gate. **→ IMPLEMENTED
  2026-06-15:** `diff_command.cpp::warnIfBaselineUnresolved` (rev-parse --verify ref^{object},
  warning on failure) + an e2e test. Local commit, push waits for a PR. (related #124)

- **[2026-06-14→15] The graph gate and graph drift don't respect the bulk-import gate (#117)** — clone/
  complexity were skipped on bulk (added > 10000), but graph gating+drift were not → a mass
  import (vendor/generated, adds of 100k–5M lines) inflated the graph legitimately-but-uninterestingly
  (~23/28 of the top graph commits — bulk). *Decision (user): variant (b) + a principle* — the graph does NOT
  gate on bulk commits: bulk may be "as-is, fix later" or not the author's code →
  blocking a merge by the graph is not allowed; the tool catches SLOW drift, not a one-time dump.
  **→ IMPLEMENTED 2026-06-15:** `runDiffFullPath` computes advisory first, on bulk
  (complexitySkippedAddedLines>0) the graph (gating+drift) is not built — an empty report, gate ok;
  + an e2e test (cycle+bulk → not gated). Local commit, push waits for a PR. (related #124, #117)

- **[2026-06-14] empty-blob desync of `git cat-file --batch` in the graph memory mode** —
  `GitObjectFileSource::read()` on an empty (0-byte) blob returned by `size==0` without
  reading the trailing `\n` → the batch stream shifted, all subsequent files were read wrong
  → phantom include edges / grown cycles / god-headers in `--diff-mode=memory`.
  *Consequence:* systemic corruption of ALL graph categories of a corpus run (memory mode);
  copy-paste/complexity/SATD/test untouched (content-based, blob reading is correct).
  Found by dogfooding (running archcheck caught an archcheck bug): memory 102/118/3 vs
  disk 0/0/0 on a commit that didn't touch cyclic files. Fix: `parseBlobSize` →
  `optional<size_t>` (nullopt = not a blob, no trailer; a value incl. 0 = blob, read
  content(0)+trailer). Verified: memory==disk on 3 commits, 530/530 tests, +a unit test.
  (related #124). **→ fixed and committed 2026-06-15 (f3ab6ac), push waits for a PR.**

- **[2026-06-14] diff-JSON does not report the bulk-import-skip fact** — on `--diff --format=json`
  the skipping of advisory due to bulk-import (#117) is invisible: an empty violations list can't be
  distinguished from "checked, clean". *Consequence:* corpus JSON consumers count
  bulk commits as "clean", skewing the statistics. Fix: add a field to the advisory JSON
  (`complexity_skipped_added_lines`). (related #124, #117). **→ fixed in the code
  this session 2026-06-14.**
