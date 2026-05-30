# line_duplication spike

Standalone C++20 port of the verified `copypaste.py` idea:

- snapshot-only;
- line-based Type-1 duplication;
- no `compile_commands.json`;
- no libclang;
- intended for quick signal on real trees before any product integration.

Not part of the main build.

## Build

```bash
cmake -S experiments/line_duplication -B /tmp/line_dup_build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/line_dup_build
```

## Run

```bash
/tmp/line_dup_build/line_duplication /path/to/repo
/tmp/line_dup_build/line_duplication /path/to/repo --min-lines 5 --top 12
/tmp/line_dup_build/line_duplication /path/to/repo --cross-only
/tmp/line_dup_build/line_duplication /path/to/repo --include-tests
/tmp/line_dup_build/line_duplication /path/to/repo --git-commit <sha>
/tmp/line_dup_build/line_duplication /path/to/repo --git-diff <baseline>..<current>
/tmp/line_dup_build/line_duplication /path/to/repo --git-diff <baseline>
```

## Defaults

- source extensions:
  `.c .cc .cpp .cxx .h .hh .hpp .hxx .ipp .tpp .inl .inc`
- skipped dirs during discovery:
  `.git .cache .idea .vscode build out cmake-build-*`
- non-product files excluded by default (toggle back with `--include-tests`):
  - test-like: any path component equal to
    `test`, `tests`, `testing`, `fixture`, `fixtures`, `selftest`, `selftests`,
    or file stems like `test_*`, `*_test`, `*_tests`, `*_unittest`;
  - demo/example dirs: `examples`, `example`, `demos`, `demo`.
- auto-pruned subtrees (always, regardless of `--include-tests`):
  - **build trees:** any directory containing `CMakeCache.txt` (and below);
  - **nested repositories:** any child directory with its own `.git` / `.hg`
    (submodules, sandboxes, vendored snapshots) — the scan root's own repo is
    never excluded.
- duplication exclude masks (vendoring / packaging / generated / Qt autogen):
  `third_party`, `3rdparty`, `3rd_party`, `thirdparty`, `bundled`, `vendor`,
  `vendored`, `vendor*`, `extern`, `external`, `external_libs`, `deps`, `_deps`,
  `single_include`, `amalgamate*`, `generated`, `*_generated*`, `*.pb.*`,
  `CMakeFiles`, `build-*`, `*_autogen`, `moc_*`, `qrc_*`, `ui_*`
- trivial window filter:
  ignore windows made only of punctuation / scaffold keywords,
  or shorter than `60` normalized chars in total

Use `--include-tests` to keep test-like and demo/example files in the corpus.
Vendor-in-src (libraries dropped under `src/`, e.g. `minilzo`/`qhull`/`glad`)
is not guessed by name — add an explicit `exclude` for those.

## Output

Reports:

- discovered files;
- eligible files (`>= min_lines` significant lines);
- significant LOC;
- duplicated significant LOC;
- duplication ratio;
- total duplicated blocks;
- cross-file duplicated blocks;
- exclusion audit: count of pruned subtrees + a sorted list of pruned roots with
  reason (`build-tree` / `nested-repo` / `mask` / `test` / `demo`), plus the count
  of files dropped by masks. Excludes move the metric a lot, so the report shows
  what was cut. (LOC of excluded trees is intentionally not counted — that would
  mean walking the very subtrees we skip.)
- top N blocks by length.

`--cross-only` filters the top-list but does not change the ratio.

## Git-aware mode

The spike can compare two git snapshots via blobs, without materializing a full
checkout by itself:

- `--git-commit <sha>` means `<sha>^ .. <sha>`
- `--git-diff A..B` compares two refs
- `--git-diff A` compares ref `A` against the current working tree

Report semantics in git mode:

- baseline duplication ratio;
- current duplication ratio;
- changed C/C++ file count;
- duplicated blocks that are present in `current`,
  touch the changed file set,
  and are new relative to `baseline`.

This is an approximation of “duplicate introduced by this commit”.
It is intentionally diff-oriented, not temporal blame.

Note: historical commit analysis needs the parent commit to exist locally.
On shallow clones, `--git-commit <old-sha>` can fail if `<sha>^` is missing.
