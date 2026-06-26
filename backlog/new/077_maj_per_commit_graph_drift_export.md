# [RESEARCH][DIFF] Per-commit export of include-graph changes for manual verification of drift cases

**Date created:** 2026-06-02
**Date started:** —
**Status:** new
**Module:** EXPERIMENTS / DIFF / RESEARCH
**Priority:** major
**Complexity:** M
**Blocks:** zero-config sibling-drift validation
**Blocked by:** —
**Related:** #018 (git_diff_analysis), #054 (ai_repo_duplication_run), #075 (mvp_v1_trusted_diff_workflow), #076 (new_cross_area_dependency_drift)

## Goal

Build a reproducible export **across all commits in a repository's history** that
captures include-graph changes at the commit-to-parent diff level. We need not a new
product surface, but research material to answer the question:

> "Are there commits in real AI-heavy repos that draw a new direct
> link between neighboring modules / areas?"

## Context

Right now `archcheck --diff` can compare **one** revision with another, but we have no
ready-made mode that:

- walks the history;
- saves a summary for each commit;
- separately highlights commits where new dependencies appeared;
- produces a compact file for manual eyeballing.

A similar pattern already exists in duplication research:

- `partial_history_drift.sh`
- `generate_per_commit.py`
- `per_commit_hits.{jsonl,md}`

For graph drift we need the same layer, but on top of `archcheck --diff`.

## What to do

### 1. Add a per-commit graph exporter

- [ ] A script in `experiments/ai_repo_run/` that walks `git rev-list --reverse --first-parent HEAD`.
- [ ] For each commit with a parent, runs `archcheck --diff <sha>~1..<sha> .`.
- [ ] Saves summary metrics:
      `added_edges`, `removed_edges`, `grown_cycles`, `new_area_deps`.

### 2. Save candidates for manual review

- [ ] For commits with any drift signal, save a separate record.
- [ ] Include in the record:
      `sha`, `date`, `subject`, summary metrics, list of `added` edges,
      the `new_cross_area_dependencies` section if present.
- [ ] Produce a human-readable `.md` next to the `.jsonl`.

### 3. Do not mix with the MVP CLI

- [ ] Do not add a new CLI mode to `src/main.cpp`.
- [ ] Do not change the product's exit contracts.
- [ ] Keep this in `experiments/` as a research and validation tool.

## Acceptance criterion

- [ ] There is a reproducible script that exports the per-commit graph drift series.
- [ ] There is machine-readable output (`jsonl`/`tsv`) and a short human-readable digest (`md`).
- [ ] One can quickly single out commits where new dependencies/cycles/area-pairs appeared.
- [ ] The script is verified on at least one local repo.

## How to use the result

1. Run the exporter on an AI-heavy local repo.
2. Filter commits with `new_area_deps > 0` or large `added_edges`.
3. Open only those candidates by hand via `git show`.
4. Based on live cases, decide which zero-config sibling/module drift signal makes sense.

## Do not do in this task

- do not try to immediately guess SOLID/Lakos violations automatically;
- do not pull in config rules;
- do not change the product `--diff` surface;
- do not mix with duplication export.
