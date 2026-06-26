# [CI][RELEASE] Prebuilt archcheck binary for downstream CI consumers

**Created:** 2026-06-25
**Start date:** 2026-06-25
**Status:** completed
**Module:** CI / RELEASE / DOCS
**Priority:** major
**Complexity:** M
**Blocks:** fast downstream integrations of `archcheck` in third-party CI without building from source
**Blocked by:** —
**Related:** #136 (CLI/docs contract), #138 (release readiness), docs/ci_usage.md

## Goal

Give `archcheck` users a proper install path for CI: download a pinned
prebuilt binary from a GitHub Release, verify the version/checksum, and run
`archcheck` immediately, without spending 3-4 minutes of every workflow run on `git clone cpparch` +
`cmake` + `FetchContent` + build.

## Context

In `leadline`, `archcheck` was wired in per the current `docs/ci_usage.md` guide via
a build from source inside the downstream workflow. This works, but the first real
run showed the cost: the `Archcheck` job took about 4 minutes, almost entirely on building the
tool. The binary itself is small: a local Linux x86_64 release `archcheck` is about
1.6 MB and depends only on standard system libraries (`libstdc++`, `libm`,
`libgcc_s`, `libc`).

For a CLI tool used as a gate in other repositories, building from
source should be a fallback/developer path, not the main recommended CI path.
The main path: a versioned release asset.

## What to do

- Add a release workflow to `cpparch` that, on tag `vX.Y.Z`, builds
  `archcheck` for `ubuntu-24.04` / Linux x86_64.
- Package the binary into an asset with a predictable name, e.g.:
  `archcheck-${version}-linux-x86_64.tar.gz`.
- Publish a checksum next to the binary (`.sha256`) and document the verification.
- Include in the asset at minimum:
  - `archcheck`;
  - `LICENSE`;
  - a short `README`/install note, if useful.
- Verify that the downloaded asset runs on a clean GitHub-hosted
  `ubuntu-24.04` runner and prints a correct `archcheck --version`.
- Update `docs/ci_usage.md`:
  - recommended path: download the pinned release binary;
  - fallback path: build from source;
  - forbid `latest` as a CI default, recommend a pin by tag + checksum.
- Update `.github/workflows/example_archcheck_pr.yml` so that the install step
  uses the release binary, and build-from-source remains a comment/fallback.

## Acceptance

- [x] There is a GitHub Release asset for Linux x86_64 with `archcheck` — **v0.1.0 released**
      (https://github.com/blurman-ai/cpparch/releases/tag/v0.1.0), tarball 1.3 MB.
- [x] There is a checksum asset (`.sha256`) and a checksum-verification example in the CI snippet
      (`ci_usage.md`, `example_archcheck_pr.yml`, release notes).
- [x] `docs/ci_usage.md` shows a fast install path via a pinned release.
- [x] `example_archcheck_pr.yml` no longer requires a CMake build in the happy path
      (release-download in the happy path, build-from-source — a commented-out fallback).
- [x] In a separate smoke job the downloaded release asset runs
      (`--version`/`--help`/`.`) — job `smoke` in `release.yml`, green on v0.1.0.
- [x] A downstream workflow can replace the build step with a download/install without
      changing the `archcheck --diff` contract (binary in `/usr/local/bin`, calling `archcheck` without a path).

## Progress (2026-06-25)

Done (all verified locally, not committed):
- `.github/workflows/release.yml` — new. Trigger on tag `v[0-9]+.[0-9]+.[0-9]+(-*)`,
  Release build on ubuntu-24.04 (`-static-libstdc++ -static-libgcc`), version-check
  of the binary against the tag, packaging `archcheck-X.Y.Z-linux-x86_64.tar.gz` (+LICENSE,
  README.install) + `.sha256`, `gh release create`, a separate smoke job on a clean runner.
  rc/alpha/beta tags → prerelease.
- `docs/ci_usage.md` — "Installing in CI" section: recommended pinned release + fallback
  build-from-source; both scenarios (full scan / PR diff) moved to release-install;
  an explicit ban on the mutable `latest`.
- `.github/workflows/example_archcheck_pr.yml` — happy path = release download+checksum,
  build-from-source commented out as a fallback.
- `docs/dev/git_workflow.md` — step 7 of the release process: the GitHub Release is now automatic.
- `.claude/commands/release.md` — a new command `/release X.Y.Z`: bump→changelog→build
  gate→commit→annotated tag→push (the tag triggers release.yml). Confirmation before push.

Local verification:
- Release build with `-static-libstdc++ -static-libgcc` OK; `ldd` = only libm/libc/ld
  (libstdc++/libgcc_s gone), `--version` = `archcheck 0.1.0`.
- End-to-end pack→sha256→verify(TARGET)→extract→`archcheck --version`→`archcheck empty`(exit 0) — OK.

Committed + pushed to master:
- 3bd3c07 — release pipeline + /release command.
- b79b96b — version-check vs tag numeric core (rc tags do not fail).
- 167bbf0 — fix checksum verify in release notes (the rename form failed for the consumer;
  reproduced both forms locally: rename → exit 1, original-names → OK).

Dry-run on real GitHub runners (rc1, then rc2 after the fix) — both fully
green, after the check both cleaned up (tag+release deleted). On a clean ubuntu-24.04:
checksum `...tar.gz: OK`, `archcheck 0.1.0`, `Usage:`, `No violations found.`.
The binary's version (static-link, ldd=libm/libc/ld) matched the tag. Asset ~1.3 MB gz.

Production release **v0.1.0 published** (fed39d9 → tag v0.1.0):
https://github.com/blurman-ai/cpparch/releases/tag/v0.1.0, prerelease=false, 4 assets.

### Astra 1.7 / old glibc (along the way)

Finding (skeptic-check): the dynamic ubuntu-24.04 asset requires **glibc ≥ 2.38**
and does NOT run on Astra 1.7 (glibc 2.28) — `version 'GLIBC_2.38' not found`.
Decision (user's choice — "two assets"): the second asset is built with `-static`
(libc baked in, no glibc dependency; archcheck forks git and does not touch
NSS/getpwnam → static glibc is safe). The release now publishes both:
- `archcheck-X.Y.Z-linux-x86_64.tar.gz` — dynamic, glibc ≥ 2.38, smaller;
- `archcheck-X.Y.Z-linux-x86_64-static.tar.gz` — static, any Linux ≥ 3.2 (Astra/Debian10/RHEL8).

Proven in CI: job `smoke-oldglibc` in a `debian:10` container (glibc 2.28 = Astra base)
downloads the static asset and runs it — `ldd ... 2.28`, `archcheck 0.1.0`,
`No violations found.`, exit 0. Locally on this machine (also Astra 1.7, glibc 2.28)
the static binary also works (`ldd` → "not a dynamic…").

Commits: c4350b9 (glibc note in the docs), fed39d9 (static asset + debian:10 smoke).
Dry-run rc3 — all 3 jobs green, cleaned up.

Remaining (external, on the user's side):
- Verify on `leadline`: replace build-from-source with release-download
  (static asset for the Astra runner, if it is there), full scan = `No violations found`.

## Outcome

**Status:** completed
**Completion date:** 2026-06-25

The release pipeline (`release.yml`) on tag `vX.Y.Z` builds and publishes two Linux
x86_64 assets (dynamic glibc≥2.38 + fully-static) with `.sha256` and smoke jobs
(clean runner + `debian:10` for old glibc). `v0.1.0` released and live.
`docs/ci_usage.md` + `example_archcheck_pr.yml` give a ready install path:
download → `sha256sum -c` → `install /usr/local/bin`, **static by default**
(works on any glibc, including Astra 1.7 / ubuntu-22.04). The `/release` command
automates bump→changelog→tag→push.

**Changed files (final pass, commit 4bc33aa):**
- `docs/ci_usage.md` — the install snippet default moved to the static asset.
- `.github/workflows/example_archcheck_pr.yml` — the example also on static.
- `README.md` — pointer to `docs/ci_usage.md`.

Earlier (this task): `release.yml`, `/release`, glibc note, static asset +
debian:10 smoke — commits 3bd3c07, b79b96b, 167bbf0, c4350b9, fed39d9, 3bd9b14, 8a9d810.

**External follow-up (outside cpparch):** verify on `leadline` the replacement of
build-from-source with release-download (static asset for the Astra runner).

## Do not do in this task

- Do not do package manager integrations (`apt`, Homebrew, pipx, npm) — that is a separate
  distribution layer after the first binary release.
- Do not make a Docker action the main path. Docker can be a follow-up, but for
  a single small CLI binary a release asset is simpler, faster, and more transparent.
- Do not use the mutable `latest` in the documentation as the recommended CI input.

## Possible CI snippet for the consumer

```yaml
- name: Install archcheck
  env:
    ARCHCHECK_VERSION: v0.1.0
  run: |
    gh release download "$ARCHCHECK_VERSION" \
      --repo blurman-ai/cpparch \
      --pattern "archcheck-${ARCHCHECK_VERSION#v}-linux-x86_64.tar.gz" \
      --output /tmp/archcheck.tar.gz
    gh release download "$ARCHCHECK_VERSION" \
      --repo blurman-ai/cpparch \
      --pattern "archcheck-${ARCHCHECK_VERSION#v}-linux-x86_64.tar.gz.sha256" \
      --output /tmp/archcheck.tar.gz.sha256
    cd /tmp
    sha256sum -c archcheck.tar.gz.sha256
    tar -xzf archcheck.tar.gz
    sudo install -m 0755 archcheck /usr/local/bin/archcheck
    archcheck --version
```

## Verification

- On `cpparch`: release workflow dry-run/manual test on a temporary tag.
- On `leadline`: locally replace the build-from-source install step with a release download
  and confirm the full scan stays `No violations found`.

## Risks

- ABI/glibc compatibility: building on `ubuntu-24.04` is convenient for current runners, but
  it is not the widest Linux baseline. Good enough for v0.1, but state the
  supported runner image explicitly in the docs.
- Supply chain: the binary must be pinned by tag/checksum; mutable latest forbidden
  as a default.
- The release process must not depend on a downstream repository.
