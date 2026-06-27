# archcheck in CI — integration guide

Entry point for anyone integrating archcheck into their pipeline. Two scenarios:

1. **Whole-repo analysis** — a gate on every push to a protected branch.
2. **PR diff** — a gate only on the regression introduced by a specific PR.

They can be used together or separately. Below: what to choose, ready-made
snippets, and the output contract. The deep mechanics of `--diff` live in
[ci_integration.md](ci_integration.md), the baseline file format — in
[baseline_format.md](baseline_format.md).

## TL;DR — which mode to take

| I want…                                                      | Command                                  | Gates on         |
|--------------------------------------------------------------|------------------------------------------|-------------------|
| Catch architectural violations across the whole tree         | `archcheck <path>`                       | cycles (SF.9)      |
| Not fail on legacy debt, catch only the new stuff            | `archcheck --baseline <file> <path>`     | new violations   |
| Catch graph drift against a frozen snapshot                  | `archcheck --drift-baseline <file> <path>` | DRIFT.1/2/4.CYCLE |
| Catch the regression a PR introduced                         | `archcheck --diff <revspec> <path>`      | new/grown cycle, new god-header |

`<path>` defaults to the current directory. `compile_commands.json` is **not
needed** for shipped rules (SF.7/8/9, Lakos, DRIFT) — the fast include scanner
does the work.

## Contract: exit codes

Identical across all modes (see [CLAUDE.md](../CLAUDE.md) §"Exit codes"):

| Code | Meaning                                     | What CI does |
|------|---------------------------------------------|---------------|
| `0`  | gate clean (advisory findings may exist)    | green         |
| `1`  | gated violation                             | red           |
| `2`  | config / IO / git error, invalid input      | red           |
| `3`  | internal error                              | red           |

**Key principle — advisory-first.** Exit `1` means specifically a gated finding
(cycle / drift / regression), not "something was found at all". SF.7/8, chain
length, god-header, added edges, NCCD growth, SATD, etc. — are printed to the
report, but do **not** fail the job. This is deliberate: the gate is narrow, so
you don't get "5000 violations on the first run". Debt is frozen via
`--baseline`.

## Installation in CI

### Recommended path: pinned release binary

Download a ready-made binary from a GitHub Release — seconds instead of a 3-4
minute build. **Pin by tag and verify the checksum**; do not use the mutable
`latest` in CI (supply-chain risk + non-reproducibility).

```yaml
- name: Install archcheck
  env:
    ARCHCHECK_VERSION: v0.1.0        # pin by tag, not latest
    GH_TOKEN: ${{ github.token }}    # gh release download requires a token
  run: |
    asset="archcheck-${ARCHCHECK_VERSION#v}-linux-x86_64-static.tar.gz"
    cd /tmp
    gh release download "$ARCHCHECK_VERSION" --repo blurman-ai/cpparch --pattern "$asset"
    gh release download "$ARCHCHECK_VERSION" --repo blurman-ai/cpparch --pattern "$asset.sha256"
    sha256sum -c "$asset.sha256"     # fails if the binary was tampered with
    tar -xzf "$asset"
    sudo install -m 0755 archcheck /usr/local/bin/archcheck
    archcheck --version
```

After that the job just calls `archcheck …` (it's in `PATH`). There's only one
platform for now — Linux x86_64.

#### Which asset to take

Each release publishes **two** Linux x86_64 assets (each with its own
`.sha256`). The snippets above and below default to **static** — it works
everywhere:

| Asset | When | glibc |
|-------|-------|-------|
| `archcheck-<v>-linux-x86_64-static.tar.gz` ← **default** | any distribution, incl. **Astra 1.7**, Debian 10, RHEL 8, `ubuntu-22.04` | not needed (fully static) |
| `archcheck-<v>-linux-x86_64.tar.gz` | only recent runners (`ubuntu-24.04`/`latest`), if size matters (~1.3 vs 1.7 MB) | **needs ≥ 2.38** |

Static is built with `-static` — libc is baked in, there's no glibc dependency
at all, it runs on any Linux kernel ≥ 3.2. Compatibility with old glibc (2.28)
is verified in the CI smoke job on a `debian:10` container (= the base of Astra
1.7). archcheck forks `git` and works only with the filesystem (no
NSS/`getpwnam`/`getaddrinfo`), so the usual pitfalls of static glibc don't fire
here.

Dynamic is built on `ubuntu-24.04` (`-static-libstdc++ -static-libgcc`:
libstdc++/libgcc are baked in, but a glibc dependency remains). **Verified: on
glibc < 2.38 it fails with `version 'GLIBC_2.38' not found`** — i.e. on
`ubuntu-22.04` and any non-bleeding-edge distribution. Take it only if the
runner is guaranteed to be `ubuntu-24.04`/`latest` and the extra ~400 KB matter.
To switch — remove `-static` from the asset suffix in the snippet:
`asset="archcheck-${ARCHCHECK_VERSION#v}-linux-x86_64.tar.gz"`.

### Fallback / developer path: build from source

If your platform isn't among the release assets, or you want to build from your
own commit (the static asset works on any glibc, so the fallback is mostly
needed for non-x86_64 or building from a specific commit):

```yaml
- name: Build archcheck
  run: |
    cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    # binary: ./build/src/archcheck
```

Dependencies are pulled by FetchContent (internet needed for the first build;
cached via `actions/cache` on `build/_deps`).

---

## Scenario 1. Whole-repo analysis

The simplest gate: on every push, run the tree, fail on cycles.

```yaml
name: archcheck (full scan)
on:
  push:
    branches: [main, master]

jobs:
  archcheck:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4         # fetch-depth doesn't matter — no snapshot needed
      - name: Install archcheck           # see "Installation in CI" — pinned release
        env:
          ARCHCHECK_VERSION: v0.1.0
          GH_TOKEN: ${{ github.token }}
        run: |
          asset="archcheck-${ARCHCHECK_VERSION#v}-linux-x86_64-static.tar.gz"
          cd /tmp
          gh release download "$ARCHCHECK_VERSION" --repo blurman-ai/cpparch --pattern "$asset"
          gh release download "$ARCHCHECK_VERSION" --repo blurman-ai/cpparch --pattern "$asset.sha256"
          sha256sum -c "$asset.sha256"
          tar -xzf "$asset"
          sudo install -m 0755 archcheck /usr/local/bin/archcheck
      - name: Run archcheck
        run: archcheck src/               # path to the project's code
```

What you'll see in the log:

```
a.h: [SF.9] cycle: a.h → b.h → a.h
1 violation(s) (SF.9: 1)
```

or (advisory only — the job is still green, exit 0):

```
a1.h: [Lakos.ChainLength] include chain depth 11 exceeds threshold 10
1 violation(s) (Lakos.ChainLength: 1)
note: 1 advisory finding(s) reported, not gated ... Use --baseline to track existing debt.
```

### Legacy project: freeze debt via a baseline

If the first run spews existing debt — freeze it once, and from then on the gate
catches only **new** violations (analogous to ArchUnit's `FreezingArchRule`):

```bash
# once, locally or in a separate job — commit the file into the repo
archcheck --save-baseline archcheck-baseline.json src/
```

```yaml
- name: Run archcheck (new violations only)
  run: archcheck --baseline archcheck-baseline.json src/
```

The file format is [baseline_format.md](baseline_format.md). It is reviewed in
PRs like ordinary code.

### Graph drift against a snapshot

An alternative to a baseline, when you want to pin a *verified snapshot of the
graph* to the repo and catch its drift (new edges, grown cycles, SDP
violations):

```bash
archcheck --save-graph-baseline graph.json src/   # once, into the repo
```

```yaml
- run: archcheck --drift-baseline graph.json src/
```

Gated: `DRIFT.1` (new edge into an established cycle), `DRIFT.2`,
`DRIFT.4.CYCLE`; `DRIFT.3`, `DRIFT.4.NEW`, `DRIFT.4.SDP` — advisory.

---

## Scenario 2. PR diff (canonical)

Gates only on what **this PR introduced**: a new/grown cycle, a new god-header.
A PR that makes the architecture stricter passes green even on dirty legacy.

archcheck does not parse `.diff` — it compares **two dependency graphs**
(baseline vs current), materializing both tree states via `git worktree`. That's
why the input is a git revspec, not a patch.

```yaml
name: archcheck (PR diff)
on:
  pull_request:
    branches: [main, master]
  merge_group:                    # needed for GitHub Merge Queue; the job name must match

jobs:
  archcheck-pr-diff:
    runs-on: ubuntu-latest
    permissions:
      pull-requests: write        # for the sticky comment
    steps:
      - uses: actions/checkout@v4   # shallow (depth 1) — we do NOT fetch history
      - name: Fetch baseline snapshot   # one slice of base, depth 1 (not the whole history)
        run: |
          base="${{ github.base_ref }}"
          [ -z "$base" ] && base="${{ github.event.merge_group.base_ref }}"
          base="${base#refs/heads/}"
          git fetch --no-tags --depth=1 origin "+refs/heads/$base:refs/remotes/origin/$base"
          echo "ARCHCHECK_BASE=origin/$base" >> "$GITHUB_ENV"
      - name: Install archcheck   # pinned release — see "Installation in CI"
        env:
          ARCHCHECK_VERSION: v0.1.0
          GH_TOKEN: ${{ github.token }}
        run: |
          asset="archcheck-${ARCHCHECK_VERSION#v}-linux-x86_64-static.tar.gz"
          cd /tmp
          gh release download "$ARCHCHECK_VERSION" --repo blurman-ai/cpparch --pattern "$asset"
          gh release download "$ARCHCHECK_VERSION" --repo blurman-ai/cpparch --pattern "$asset.sha256"
          sha256sum -c "$asset.sha256"
          tar -xzf "$asset"
          sudo install -m 0755 archcheck /usr/local/bin/archcheck
      - name: Run archcheck --diff
        id: archcheck
        run: |
          archcheck --diff "$ARCHCHECK_BASE..HEAD" . \
            | tee archcheck-diff.txt
        continue-on-error: true    # let it reach the report-publishing step
      - name: Publish to Step Summary
        if: always()
        run: cat archcheck-diff.txt >> "$GITHUB_STEP_SUMMARY"
      - name: Post sticky PR comment
        if: always() && github.event_name == 'pull_request'
        uses: marocchino/sticky-pull-request-comment@v2
        with:
          header: archcheck-diff
          path: archcheck-diff.txt
      - name: Fail if regression
        if: steps.archcheck.outcome == 'failure'
        run: exit 1
```

A ready-made file — [.github/workflows/example_archcheck_pr.yml](../.github/workflows/example_archcheck_pr.yml).

**Canonical revspec for a PR:** `origin/<base>..HEAD`. The form
`<ref>` (without a right-hand side) means "baseline = ref, current = working
tree".

**Git history is not needed — only a snapshot of the base tree is.**
`archcheck --diff` compares two trees (`git diff` = tree-vs-tree) and
materializes one tree via `git worktree`; it calls neither `rev-list` nor
`merge-base`. That's why, instead of a heavy `fetch-depth: 0` (the whole history
— on big repos that's tens of seconds wasted), we take a **depth=1 fetch of a
single base ref**. `actions/checkout` gives a shallow HEAD by default; the
"Fetch baseline snapshot" step pulls the base as a single snapshot.
`fetch-depth: 0` remains a simple but heavy fallback — see
[ci_integration.md](ci_integration.md#why-a-shallow-base-fetch).

> ⚠️ The base **must** be fetched. If the base ref doesn't resolve (skipped fetch
> step, typo'd base), archcheck exits **2** with an error on stderr — it does **not**
> silently compare against an empty tree, so there is no phantom "everything added"
> gate. Don't skip the fetch step, and pin the right base.

PR publishing channels (Step Summary, sticky-comment, Check-run), merge queue,
submodules, edge cases — covered in detail in
[ci_integration.md](ci_integration.md).

---

## Machine-readable output

`--format=json` gives a stable schema (`version: 1`) with top-level `gate`,
`gating`, `advisory` — for your own integrations, instead of parsing text:

```bash
archcheck --format json src/
archcheck --diff --format=json "origin/main..HEAD" .
```

`stdout` — the report (text or JSON). `stderr` — diagnostics (git errors,
not-a-repo). `exit code` — the gate. SARIF / GitHub Code Scanning — an optional
v0.2 channel.

## Local check before push

```bash
git fetch origin main
archcheck --diff origin/main..HEAD .   # as in CI
archcheck --diff HEAD~1 .              # against the previous commit (current = worktree)
archcheck src/                          # full tree scan
```

## Related docs

- [ci_integration.md](ci_integration.md) — the deep mechanics of `--diff`:
  git-worktree, revspec, merge queue, submodules, publishing channels, edge
  cases.
- [baseline_format.md](baseline_format.md) — the format of baseline/graph-baseline files.
- [README.md](../README.md) — the pitch, the full list of rules and thresholds.
- `archcheck --help` — the authoritative list of modes and defaults.
</content>
</invoke>
