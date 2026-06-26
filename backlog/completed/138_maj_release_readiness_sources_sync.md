# [DOCS][PROCESS] Reconcile release-readiness status across README/MVP/TASK_TRACKER/backlog

**Created:** 2026-06-23
**Started:** 2026-06-23
**Status:** completed
**Module:** DOCS / PROCESS
**Priority:** major
**Difficulty:** S
**Blocks:** a clear answer to the question "what is really left before a public v0.1"
**Blocked by:** —

> **Order:** LAST in the #136–#141 wave. The release-readiness wording depends on the final positioning and CLI contract, so it sits after #139 → #137 → #136. Running it earlier risks synchronizing statuses onto a picture that has not yet settled.
**Related:** #045 (docs_sync_roadmap_mvp_spec), #103 (copypaste_per_commit_drift), #123 (diff_new_clone_gate), #127 (vendor_generated_exclusion), #131 (fresh_corpus_remeasure)

## Goal

Produce one consistent release-readiness status: identical in README, MVP, the task tracker and the backlog tasks themselves.

## Context

Right now different documents answer the same question differently.

- The README says the only open release item is `#105`, although it is already closed:
  [README.md#L149-L152](../../README.md#L149-L152).
- `docs/MVP.md` is internally inconsistent too: criterion 8 still calls `#105` a blocker,
  while further down the file already says that `#105` is closed and `#123/#103` are open:
  [docs/MVP.md#L78-L92](../../docs/MVP.md#L78-L92).
- `backlog/TASK_TRACKER.md` goes even further: there `#123` is already almost closed, and `#103`
  is described as effectively a completed research goal awaiting formal closure:
  [backlog/TASK_TRACKER.md#L52-L67](../TASK_TRACKER.md#L52-L67).
- Meanwhile in the changelog `#123` is already marked as a shipped capability, while the backlog files
  `#103/#123` themselves still sit in `wip/`.

This makes release talk unreliable: you cannot open a single document and understand where the home stretch actually is.

## Execution plan

- [ ] Designate one authoritative source of release-readiness for v0.1 (most likely `TASK_TRACKER.md`); the other documents should reference it rather than restate it by hand.
- [ ] Re-audit `#103`, `#123`, `#127`, `#131`: what is already shipped, what remains, what is merely awaiting a move/status cleanup.
- [ ] Update `README.md` and `docs/MVP.md` so they do not call closed tasks open.
- [ ] If a task is effectively done but the file hangs in `wip/`, record the move plan or the remainder in one line without fog.
- [ ] Add a short process rule: after landing a shipped feature, update not only the changelog but also the release-status SSOT.

## Done

- `backlog/TASK_TRACKER.md` designated as the SSOT for v0.1 release-readiness.
- README and MVP no longer call #105 an open release item.
- MVP records: #105 closed; #103 precision obtained; #123 product path shipped/advisory; the GitHub test repo remains outward-facing validation.
- The WIP tasks #103 and #123 received status notes separating product completion from board/demo cleanup.
- Release-readiness separates core MVP, board hygiene and public readiness (#127/#131 applicability sign-off).

## In progress

- (empty)

## Next steps

- Board hygiene: decide whether to move #103 into `completed/` as a separate cleanup commit, and whether to split out the #123 GitHub demo into a follow-up.

## Key decisions

| Decision | Reason |
|---------|---------|
| Have one SSOT for release-readiness | parallel manual statuses across 3-4 documents inevitably go stale |
| Audit the facts first, then edit the statuses | otherwise you can synchronously rewrite the documents onto the wrong picture |
| Separate `shipped` from `task file still open` | these are different states, currently mixed and confusing to read |

## Changed files

| File | Change |
|------|--------|
| `backlog/TASK_TRACKER.md` | clarify the SSOT on the current release tail if needed |
| `docs/MVP.md` | remove stale blocker wording |
| `README.md` | synchronize the status section |
| `backlog/wip/103_maj_copypaste_per_commit_drift.md` | clarify the actual remainder or closure |
| `backlog/wip/123_maj_diff_new_clone_gate.md` | clarify the actual remainder or closure |
