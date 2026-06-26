# [PROCESS] Adopt OSS standards for the git process

**Created:** 2026-05-26
**Started:** 2026-05-26
**Completed:** 2026-05-26
**Status:** done
**Module:** PROCESS
**Priority:** critical
**Difficulty:** medium
**Blocks:** all future tasks (the sooner conventions are fixed, the less drift)
**Blocked by:** —
**Related:** #006 (spec_refactor — the stability contract lands in the spec)

## Goal

Lock down the commonly accepted OSS standards for the cpparch git process and apply them to the existing tooling: GitHub Flow + Conventional Commits + SemVer 2.0 + Keep a Changelog + annotated `vX.Y.Z` tags. After this, nobody can object to a "non-standard" process.

## Context

Decision made in the 2026-05-26 session after discussion. Brief overview:

- **GitHub Flow** (not Git Flow). Master is always green, everything goes through feature branches + PRs. Used by fmt, spdlog, Catch2, nlohmann/json, ArchUnit. Sam Driessen (the author of Git Flow) admitted in 2020 that his model is overkill for web/OSS.
- **Conventional Commits** ([conventionalcommits.org](https://www.conventionalcommits.org/)). `<type>(<scope>): <description>`. Unlocks automation: commitlint, release-please, auto-changelog.
- **SemVer 2.0** ([semver.org](https://semver.org/)). Pre-1.0 → `0.x.y`, MINOR may break. 1.0 — when the CLI/JSON/config format are stable.
- **Keep a Changelog** ([keepachangelog.com](https://keepachangelog.com/)). `CHANGELOG.md` with Added / Changed / Deprecated / Removed / Fixed / Security sections + Unreleased on top.
- **Annotated `vX.Y.Z` tags**. Linux, Rust, Go, fmt. The tag is annotated, not lightweight.

The default branch stays `master` (renaming to `main` is churn without benefit).

Branches are named `<type>/<NNN>-<short-slug>`, where type ∈ `feat/fix/docs/refactor/chore/test/build`, NNN — the task ID from the backlog. Example: `docs/006-spec-refactor`, `chore/007-workflow-setup`, `feat/004-project-skeleton`.

## Execution plan

- [x] **1. Write `docs/dev/git_workflow.md`** (≈30 lines). Contains:
   - Branching: GitHub Flow + naming convention.
   - Commits: Conventional Commits + a dictionary of scopes for cpparch (`config`, `graph`, `scan`, `rules/sf`, `rules/lakos`, `rules/martin`, `report`, `cli`, `fixtures`, `build`, `docs`, `tasks`, `process`).
   - Versioning: SemVer 2.0 + the pre-1.0 rule.
   - Tags: `vX.Y.Z` annotated, push with `--follow-tags`.
   - Changelog: link to Keep a Changelog.
   - Trailers (`AI-Assisted`, `Verified`, `Risk`, `Co-Authored-By`) — kept on top of Conventional Commits, ignored by parsers.

- [x] **2. Update the `/commit` skill** for Conventional Commits.
   - Replace `<type>: [TAG] subject` → `<type>(<scope>): subject`.
   - Mapping of old tags to scopes: `[CONFIG]→config`, `[GRAPH]→graph`, `[SCAN]→scan`, `[RULES][SF]→rules/sf`, `[RULES][LAKOS]→rules/lakos`, `[RULES][MARTIN]→rules/martin`, `[RULES][CUSTOM]→rules/custom`, `[REPORT]→report`, `[CLI]→cli`, `[FIXTURES]→fixtures`, `[BUILD]→build`, `[DOCS]→docs`, `[DOCS][CLAUDE]→docs/claude`, `[DOCS][TASKS]→tasks`, `[PROCESS]→process`.
   - Subject ≤72 characters on the first line. Body separated by a blank line.

- [x] **3. Update the `/create-task` skill.**
   - After creating the file, suggest the branch-creation command `<type>/<NNN>-<slug>` (guess the type from the module; the user confirms).

- [x] **4. Create `CHANGELOG.md` following Keep a Changelog.**
   - Header with links to keepachangelog.com and semver.org.
   - An `[Unreleased]` section with empty Added/Changed/Deprecated/Removed/Fixed/Security subsections.
   - Ready for the first `0.1.0` release.

- [x] **5. Add the stability contract** to `docs/architecture-spec.md`.
   - What counts as a breaking change (MAJOR bump): changing exit codes, removing a CLI flag, changing the JSON report schema, changing the YAML config schema, changing the baseline format.
   - What counts as non-breaking (MINOR): adding CLI flags with a default, adding fields to the JSON report, new default rules (with an opt-out).
   - Pre-1.0 — semver allows breaking changes in MINOR, but the project still strives to avoid them without an explicit need.

- [x] **6. Short edits to CLAUDE.md and backlog/README.md.**
   - Mention `docs/dev/git_workflow.md` as the canon.
   - In backlog/README.md, the "Task lifecycle" section — add a step about branch creation.

- [x] **7. (optional) Notify the user about self-approve.**
   - A note in git_workflow.md: for solo-dev mode, set `Required approvals = 0` in the GitHub Rulesets for master.

## Done

- **2026-05-26** — Steps 1, 4, 5, 6, 7: created [`docs/dev/git_workflow.md`](../../docs/dev/git_workflow.md) (the full canon: GitHub Flow + Conventional Commits + SemVer + KAC + tags + release process), [`CHANGELOG.md`](../../CHANGELOG.md) (Keep a Changelog 1.1, accumulating `[Unreleased]`), the "Stability contract" section in [`docs/architecture-spec.md`](../../docs/architecture-spec.md) (a breaking vs non-breaking table for exit codes / CLI / JSON / SARIF / YAML config / baseline / default rules). A link to the workflow was added to `CLAUDE.md` and a lifecycle step 2 to `backlog/README.md`. Self-approve / direct push for admin are documented in the workflow doc. Commit `930e323`.
- **2026-05-26** — Post-hoc edit: removed the trailing link to `docs/dev/git_workflow.md` from the "Stability contract" section — the spec is about *what*, the workflow about *how*; don't mix the layers. Commit `f19c130`.
- **2026-05-26** — Steps 2, 3: the `/commit` skill rewritten for Conventional Commits 1.0 (type/scope tables from git_workflow.md, the `<type>(<scope>): <subject>` format, scope-map from the old `[TAG]`s, subject ≤72, lowercase, imperative). `/create-task` extended with a "When you start work — choosing the format" section: direct push for routine vs a feature branch `<type>/<NNN>-<slug>` for significant work.

## In progress

- (empty)

## Next steps

1. Open `docs/dev/git_workflow.md` — write it from scratch.
2. In parallel, prepare the mapping of old tags to scopes (see plan item 2).
3. After that — update `/commit` and `/create-task`, double-check that they are consistent with each other.
4. Create `CHANGELOG.md` — the last step.

## Key decisions

| Decision | Reason |
|---------|---------|
| GitHub Flow, not Git Flow | Git Flow is outdated even by its author's opinion; overkill for CLI / OSS |
| Default branch stays `master` | Renaming to `main` is cosmetic, not worth the churn |
| Conventional Commits, not custom | Unlocks commitlint, release-please, parsable history. A standard with well-known tooling partners |
| SemVer pre-1.0 = `0.x.y` | SemVer 2.0 standard §4; legitimizes breaking changes in MINOR before v1.0 |
| `v`-prefix on tags | More common in OSS (Rust, Go, fmt), the Linux kernel being the exception |
| Keep a Changelog, not auto-generated | A curated changelog reads well for humans; auto-gen can always be added later |
| Keep the trailers | Ignored by Conventional-Commits parsers, valuable as AI audit markers |
| PROCESS as a new scope/tag | The existing BUILD/DOCS don't fit semantically |

## Changed files

| File | Change | Commit |
|------|-----------|--------|
| `docs/dev/git_workflow.md` | new — the conventions canon | `930e323` |
| `CHANGELOG.md` | new — Keep a Changelog `[Unreleased]` | `930e323` |
| `docs/architecture-spec.md` | "Stability contract" section added | `930e323` |
| `docs/architecture-spec.md` | removed the workflow link from the spec (post-hoc) | `f19c130` |
| `CLAUDE.md` | link to git_workflow.md in Code style & AI constraints | `930e323` |
| `backlog/README.md` | feature branch as an option in step 2 "Starting work" | `930e323` |
| `.claude/commands/commit.md` | rewritten for Conventional Commits + scope tables | `f1d2629` |
| `.claude/commands/create-task.md` | "When you start work — choosing the format" section | `f1d2629` |
| `backlog/wip → completed/007_*.md` | task closed | (current commit) |

## How it works

The process is described by a single canon — [`docs/dev/git_workflow.md`](../../docs/dev/git_workflow.md). All other documents (CLAUDE.md, backlog/README.md, the `/commit` and `/create-task` skills) refer to it and do not duplicate its content.

**Layers:**
1. **Branching (GitHub Flow):** master is always green. Feature branches `<type>/<NNN>-<slug>` — optional, for significant work. Repo admin is on the bypass list → direct push to master is allowed. Force-push is blocked.
2. **Commits (Conventional Commits 1.0):** `<type>(<scope>): <subject>`. The type and scope tables are in `git_workflow.md`, the scope covers all cpparch subsystems.
3. **Versioning (SemVer 2.0):** pre-1.0 `0.x.y` (breaking allowed in MINOR), after v1.0 — strict semver.
4. **Tags (annotated `vX.Y.Z`):** `git tag -a vX.Y.Z -m "Release X.Y.Z"`, push with `--follow-tags`.
5. **Changelog (Keep a Changelog 1.1):** [`CHANGELOG.md`](../../CHANGELOG.md) at the root, `[Unreleased]` on top, fixed into `[X.Y.Z] - YYYY-MM-DD` at release time.
6. **Stability contract:** what counts as breaking (MAJOR) — a table in [`docs/architecture-spec.md`](../../docs/architecture-spec.md) §Stability contract. Covers exit codes, CLI flags, the JSON schema, SARIF, YAML config, baseline, default rules.
7. **AI audit trailers:** `AI-Assisted`, `Verified`, `Risk`, `Co-Authored-By` — on top of Conventional Commits, ignored by parsers.

## Controlled by

- **`.claude/commands/commit.md`** — the commit format is enforced by the `/commit` skill.
- **`.claude/commands/create-task.md`** — the task-name format and the branch advice.
- **GitHub Rulesets** (Settings → Rules → Rulesets for master):
  - Require pull request: ON, required approvals = 0.
  - Block force pushes: ON.
  - Bypass list: admin (`blurman-ai`).
- **GitHub Settings → General → Pull Requests**: Automatically delete head branches — should be enabled (optional).

## Related to

- **#006 spec_refactor** — the stability contract landed in the spec as part of #007; step 5 of #006 (closing the two-backend question) is already partly done. Remaining steps of #006: 1, 2, 3, 4, 6, 7, 8.
- **All future tasks** — use the `<type>/<NNN>-<slug>` conventions for branches, Conventional Commits for commits, reference `(#NNN)` in the subject.
- **#002 github_actions_ci** — when we get to CI, commitlint will be added to pre-merge checks (optional, after the first real contributor).

## Diagnostics

How to tell the process is being followed:

- `git log --oneline | head` — all commits of the form `<type>(<scope>): <subject>` or `<type>: <subject>` (if the scope is omitted). There should be no legacy-format `<type>: [TAG] subject`.
- `git tag` — all tags have a `v`-prefix and an annotation (`git show <tag>` shows the message).
- `cat CHANGELOG.md | head -20` — an `[Unreleased]` section exists, links to keepachangelog.com and semver.org in the header.
- `find backlog -name '???_*.md' | head` — all tasks are named `NNN_<priority>_<name>.md`.
- `ls backlog/` — the `new/ wip/ completed/` structure, no tasks at the root.
- `git push origin master --force-with-lease` — should be rejected (force-push is blocked for everyone).
