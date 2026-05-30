# partial_duplication spike

Standalone C++20 spike for issue **#056** — fast-backend **partial (Type-3)**
duplication: catch *diverged* copies (renamed identifiers, flipped operators,
inserted lines) that the line-based Type-1 pass (#053) cannot.

- token-granularity, not line-granularity;
- bag-of-tokens overlap (plain + rarity-weighted), inverted-index candidates;
- no `compile_commands.json`, no libclang, snapshot-only;
- **not** product plumbing, **not** in the main build.

This is step 3 of #056 (minimal `tokenize + overlap + fixtures` spike). It does
**not** implement the token-LCS precise re-rank, baseline/gate semantics, or
config wiring — those wait on the #053-shared duplication plumbing.

See [SPIKE_REPORT.md](SPIKE_REPORT.md) for findings — including the headline
result that the task's stated default (rarity-weighted bag) is the wrong default
for the operator-divergence class.

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
```

## Options

| flag | default | meaning |
|------|--------:|---------|
| `--min-tokens N` | 30 | fragment floor (function scale) |
| `--max-tokens N` | 400 | fragment ceiling (else descend) |
| `--threshold F` | 0.60 | report pairs with gating metric ≥ F |
| `--rare-df N` | 4 | a token is "rare" (indexable) if `df ≤ N` |
| `--min-shared N` | 2 | candidate needs ≥ N shared rare tokens |
| `--metric M` | weighted | `weighted` or `plain` — which score gates+ranks |
| `--top N` | 25 | max pairs printed |

## Layout

- `main.cpp` — the spike (~600 lines, standalone).
- `cases/` — contrast fixtures A–E (see SPIKE_REPORT.md).
- `SPIKE_REPORT.md` — method, results, and what it means for #056.
