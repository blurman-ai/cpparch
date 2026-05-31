# partial_duplication spike

Standalone C++20 spike for issue **#056** — fast-backend **partial (Type-3)**
duplication: catch *diverged* copies (renamed identifiers, flipped operators,
inserted lines) that the line-based Type-1 pass (#053) cannot.

- token-granularity, not line-granularity;
- bag-of-tokens overlap (plain + rarity-weighted), inverted-index candidates;
- no `compile_commands.json`, no libclang, snapshot-only;
- **not** product plumbing, **not** in the main build.

This is step 3 of #056 (`tokenize + overlap + fixtures` spike) plus the P3
token-LCS precise re-rank (`--partial-precise`). It does **not** implement
baseline/gate semantics or config wiring — those wait on the #053-shared
duplication plumbing.

See [SPIKE_REPORT.md](SPIKE_REPORT.md) for findings — including the headline
result that the task's stated default (rarity-weighted bag) is the wrong default
for the operator-divergence class, and that token-LCS (`--partial-precise`) is the
only metric that separates all of A/B from C/D — at its own higher gate (~0.80).

## Build

```bash
cmake -S experiments/partial_duplication -B /tmp/partial_dup_build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/partial_dup_build
```

## Run

```bash
# contrast fixtures (use --min-shared 1: tiny corpus, see report)
/tmp/partial_dup_build/partial_duplication experiments/partial_duplication/cases --min-shared 1

# plain metric, separates the contrast set cleanly
/tmp/partial_dup_build/partial_duplication experiments/partial_duplication/cases \
  --min-shared 1 --metric plain --threshold 0.7

# real code (rare_df must grow with corpus size)
/tmp/partial_dup_build/partial_duplication src --metric plain --threshold 0.7 --rare-df 25

# P3 precise: token-LCS re-rank + diff-view (own default gate 0.80, widened recall)
/tmp/partial_dup_build/partial_duplication experiments/partial_duplication/cases --partial-precise

# diff mode (#054): per-commit "did this commit ADD a Type-3 near-dup of code
# that already existed at the parent?"  --diff <sha>|<A>..<B> --repo <path>
/tmp/partial_dup_build/partial_duplication --diff <sha> --repo /path/to/repo \
  --subpath src --partial-precise --min-tokens 45 --rare-df-pct 8 --threshold 0.85
```

## Diff mode (#054)

Snapshot mode scans a tree and reports near-dup pairs — a *static* whole-tree
view. Diff mode attributes copy-paste to the **commit that introduced it**: for
commit `C` (parent `P`) it asks *did `C` add code that is a Type-3 near-duplicate
of code that already existed at `P`?* — a missing-reuse edge born in that commit.

- `--diff <sha>` (parent = `sha^`) or `--diff <A>..<B>`; requires `--repo <path>`.
- `--subpath <rel>` restricts both the diff and the baseline to a subtree.
- baseline = the whole parent tree (`git archive P | tar -x`);
  added fragments = function-scale fragments overlapping a line added in `C`
  (parsed from `git diff -U0 P..C` hunk headers, content from `git show C:<f>`).
- each added fragment is scored against the baseline with the same bag-recall +
  LCS-confirm; only the best baseline match per added fragment is reported.
- a self-edit guard drops same-file matches whose line range overlaps the added
  block (an in-place edit of a function is not copy-paste).
- output is machine-parseable: `commit=<sha> added_frags=N partial_hits=M
  max_sim=X` plus the hit list. Shells out to `git` (no libtooling).

Self-contained C++20, no libclang. All common flags above apply. Snapshot mode
is unchanged when `--diff` is absent.

## Options

| flag | default | meaning |
|------|--------:|---------|
| `--min-tokens N` | 30 | fragment floor (function scale) |
| `--max-tokens N` | 400 | fragment ceiling (else descend) |
| `--threshold F` | 0.60 (0.80 if `--partial-precise`) | report pairs with gating metric ≥ F |
| `--rare-df N` | 4 | a token is "rare" (indexable) if `df ≤ N` |
| `--rare-df-pct P` | 0 (off) | if >0, rare cutoff = `max(rare-df, N*P/100)` — relative, scales with corpus |
| `--min-shared N` | 2 | candidate needs ≥ N shared rare tokens |
| `--metric M` | weighted | `weighted` or `plain` — which score gates+ranks |
| `--partial-precise` | off | token-LCS re-rank + diff-view; widens recall, gates on LCS (own ~0.80 scale) |
| `--exclude S` | — | skip files whose path contains substring `S` (repeatable: vendor/build/generated) |
| `--top N` | 25 | max pairs printed |

On large trees use `--rare-df-pct` (an absolute `--rare-df` prunes every
candidate once N is in the tens of thousands) and an `--exclude` set for
vendored / generated / build paths — see `OSS_SWEEP_REPORT.md`.

## Layout

- `main.cpp` — the spike (standalone: lex/normalize, fragments, bag overlap,
  inverted-index candidates, the `--partial-precise` token-LCS + diff-view,
  `--exclude` and `--rare-df-pct`).
- `cases/` — contrast fixtures A–E (see SPIKE_REPORT.md).
- `SPIKE_REPORT.md` — method, results, and what it means for #056.
- `OSS_SWEEP_REPORT.md` — run across 19 OSS repos: cross-component findings +
  the relative-`rare_df` scaling result.
