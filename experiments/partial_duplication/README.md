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
```

## Options

| flag | default | meaning |
|------|--------:|---------|
| `--min-tokens N` | 30 | fragment floor (function scale) |
| `--max-tokens N` | 400 | fragment ceiling (else descend) |
| `--threshold F` | 0.60 (0.80 if `--partial-precise`) | report pairs with gating metric ≥ F |
| `--rare-df N` | 4 | a token is "rare" (indexable) if `df ≤ N` |
| `--min-shared N` | 2 | candidate needs ≥ N shared rare tokens |
| `--metric M` | weighted | `weighted` or `plain` — which score gates+ranks |
| `--partial-precise` | off | token-LCS re-rank + diff-view; widens recall, gates on LCS (own ~0.80 scale) |
| `--top N` | 25 | max pairs printed |

## Layout

- `main.cpp` — the spike (standalone: lex/normalize, fragments, bag overlap,
  inverted-index candidates, and the `--partial-precise` token-LCS + diff-view).
- `cases/` — contrast fixtures A–E (see SPIKE_REPORT.md).
- `SPIKE_REPORT.md` — method, results, and what it means for #056.
