# [DOCS] Check availability of the name archcheck

**Date created:** 2026-05-26
**Date started:** 2026-05-26
**Date completed:** 2026-05-26
**Status:** done
**Module:** DOCS
**Priority:** blocker
**Difficulty:** S (an hour or two)
**Blocks:** #004 (project_skeleton), #002 (github_actions_ci)
**Blocked by:** —
**Related:** —

## Goal

Confirm or reject the name `archcheck` (binary) / `cpparch` (working repo name) before starting on code — renaming after the first release is expensive.

## Context

From `docs/architecture-spec.md` §"Key risks", item 4 (at the time of the task): name availability on GitHub/PyPI/crates.io/Homebrew was not checked. The README used `cpparch`, the spec — `archcheck`. We needed to converge on one.

## Execution plan

- [x] GitHub: check whether `github.com/*/archcheck` exists with a C++ context
- [x] PyPI: `pip index versions archcheck`
- [x] crates.io: check whether the `archcheck` crate is taken
- [x] Homebrew: check the formula
- [x] npm — for completeness (some dev tools are published there too)
- [x] Trademarks: a quick search
- [x] If `archcheck` is taken — replacement candidates (`archlint`, `cpparch`, `archdep`, `lakos`)
- [x] Lock the final name into README.md and architecture-spec.md

## Done

- **2026-05-26** — A check of both priority names went through WebFetch / WebSearch. Result:

  | Name | GitHub | PyPI | crates.io | Homebrew | npm | Trademark/brand | Verdict |
  |---|---|---|---|---|---|---|---|
  | **archcheck** | ✅ org/user free; ~10 same-named repos under private users, all 0–2 stars, various languages (Python/Java/JS/Go/HTML), no C++ | ✅ 404 | ✅ 404 | ✅ 404 | ✅ 404 | ✅ no conflicts found | ✅ **take it** |
  | **cpparch** | ✅ free; ~10 same-named repos, personal C++ archives; `willjoseph/cpparch` 2015 — abandoned, 0 stars | ✅ 404 | ✅ 404 | ✅ 404 | ✅ 404 | ⚠️ no direct conflict, but the similarity to `cppcheck` creates background noise in search and typing | ⚠️ risk |

  The backup candidates (`archlint`, `archguard-cpp`, `cpp-arch`, `archdep`, `lakos`) didn't need checking — `archcheck` is clean.

- **2026-05-26** — Decided: **`archcheck`** as the product name.

- **2026-05-26** — Bulk replacement `cpparch` → `archcheck` (with the variants `Cpparch` → `Archcheck`, `CPPARCH` → `ARCHCHECK`) in all `.md` files of the repo, except:
  - `.claude/commands/findings.md` — there the memory path `~/.claude/projects/-home-localadm-projects-cpparch/...` corresponds to the name of the local directory, not the product name.
  - `backlog/completed/007_*.md` — a historical record, not edited after the fact.

- **2026-05-26** — Cleaned up in `docs/architecture-spec.md`:
  - The preamble "Working name. Alternatives…" was removed.
  - "Key risks" item 4 (about the name) deleted; items 5/6/7 renumbered to 4/5/6.
  - "Next steps": item 1 (check the name) and item 3 (decide the two-backend scheme question) deleted — both closed; the rest renumbered.

## Key decisions

| Decision | Reason |
|---------|---------|
| `archcheck`, not `cpparch` | Clearer semantics; no collision with the well-known `cppcheck` (easy to confuse in search and typing); matches the authoritative spelling in the spec. `cpparch` is also technically free, but creates background noise. |
| Don't do defensive registration of squatting accounts on PyPI/crates/npm | Pre-1.0 OSS, with no publication — no point. If someone takes it while we build v0.1 — we'll reconsider. |
| Keep the local directory name `cpparch` | Renaming would break Claude's memory path and all hard-coded paths in the shell config. This is cosmetic, doesn't affect the product. |
| The GitHub repo is recommended to be renamed to `archcheck` | Settings → General → Repository name. GitHub automatically redirects the old URL. This is a manual user action — not done via git. |

## Changed files

| File | Change | Commit |
|------|-----------|--------|
| `README.md` | `# cpparch` → `# archcheck`, all mentions | (current) |
| `docs/architecture-spec.md` | preamble cleaned, risks item 4 removed, "Next steps" updated | (current) |
| `CHANGELOG.md` | `archcheck` in the header; the `[Unreleased]` URL points to the new repo URL | (current) |
| `CLAUDE.md` | all mentions replaced | (current) |
| `AGENTS.md` | all mentions replaced | (current) |
| `docs/MVP.md`, `docs/code_style.md`, `docs/code_quality.md`, `docs/dev/git_workflow.md` | all mentions replaced | (current) |
| `docs/research/README.md` and `docs/research/rules/*.md` | all mentions replaced | (current) |
| `backlog/README.md`, `backlog/new/001*`, `backlog/new/005*` | all mentions replaced | (current) |

## How it works

After #003 all internal documents use one name — `archcheck`. The principle is this:

1. **The product name (binary and project)** — `archcheck`. Used in the README, spec, CHANGELOG, documentation, commit messages.
2. **The local directory name** — stays `cpparch` for compatibility with already-configured tool paths (Claude memory, IDE config). This doesn't affect the user of the product.
3. **The GitHub repo** — still named `cpparch` for now. Recommendation: have the user rename it via Settings → General → Repository name → `archcheck`. GitHub will automatically set up a redirect from the old URL.
4. **Reserved namespaces** on PyPI/crates.io/npm/Homebrew — we don't take them. If someone beats us to it before publication — we'll reconsider (see the backup names in the plan).

## What governs it

- Name consistency in the repo — checked via `grep -rn cpparch --include="*.md"`. Should return only:
  - `.claude/commands/findings.md` (memory path, expected).
  - `backlog/completed/007_crt_workflow_setup.md` (historical references, expected).
  - `backlog/completed/003_blk_name_availability_check.md` — this file (process documentation).
- On a repeated sed pass in the future — add new exclusions via `! -path` in the `find` command.

## What it relates to

- **#004 (project_skeleton)** — now unblocked. When we create `CMakeLists.txt`, the binary name will be `archcheck`. Use it as the target name.
- **#002 (github_actions_ci)** — unblocked. The CI workflow will reference the `archcheck` binary.
- **#006 (spec_refactor)** — syncing the name with the README/spec falls into one of its subtasks (the general refactor); part of this work has already been done here.

## Diagnostics

Commands for verification:

```bash
# no remnants of the name in the codebase (except the expected exclusions):
grep -rn "cpparch" --include="*.md" | grep -v findings.md | grep -v completed/007 | grep -v completed/003

# the name in the README header — archcheck:
head -1 README.md

# the name in the spec header — archcheck:
head -1 docs/architecture-spec.md

# in CHANGELOG.md the URL points to the correct repo (after the manual rename on GitHub):
grep blurman-ai/ CHANGELOG.md
```
