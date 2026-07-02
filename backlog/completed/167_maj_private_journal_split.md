# [RELEASE] Split internal journal + research bulk into a private companion repo

**Created:** 2026-07-02
**Status:** done 2026-07-02 — executed same day: private repo
[blurman-ai/archcheck-journal](https://github.com/blurman-ai/archcheck-journal) created and
pushed (b43b68f); JOURNEY.md, milestones.md, dup_band_70_80.md, analysis/ moved
(byte-identity verified before removal); public references + CLAUDE.md + skills + memory
updated (public commit 9dfc9eb). Symlinks were already gitignored — left local. History
NOT scrubbed (explicit decision). Remaining smoke-test: next session's /checkpoint must
land in the companion repo.
**Module:** RELEASE / DOCS
**Priority:** major
**Complexity:** M
**Blocks:** clean public-repo appearance for the #163 announcement
**Blocked by:** —
**Related:** #163 (launch plan), #150 (EN translation / "0 Cyrillic" acceptance)

## Decision context

The public repo keeps: product code, tests, fixtures, user-facing docs, spec/ADRs,
`backlog/` (process transparency), and the manual-audit research docs (they back the
public precision claims). A private companion repo (e.g. `blurman-ai/archcheck-journal`)
receives the internal narrative and heavy research bulk:

- `docs/JOURNEY.md` (212 KB, contains deliberate verbatim Russian author quotes) — the
  raw narrative material; git-versioned because parallel agents append to it
  (2026-07-02 proved git is the collision safety net).
- `docs/milestones.md` (46 KB), root `dup_band_70_80.md` (135 KB).
- `analysis/`, `sandbox/` trees (experiments/ is already mostly gitignored — audit what
  IS tracked there and move the tracked leftovers).
- Broken/personal symlinks: `dup_scratch` (points to a personal Cyrillic path),
  `compile_commands.json` — delete from the public tree outright.

## Mechanics (order matters)

1. Create the private repo; move the files there WITH their git history if cheap
   (`git log --follow` export or a simple copy + "imported from cpparch@<sha>" note —
   decide; a plain copy is acceptable, the public history keeps the old versions anyway).
2. In the public repo: `git rm` the moved files + symlinks, add a one-line pointer in
   `docs/README` or CONTRIBUTING ("internal dev journal lives in a private companion
   repo; ask the maintainer").
3. Update every reference: CLAUDE.md (JOURNEY rule → new path/repo!), memory files
   that cite `docs/JOURNEY.md`, `/checkpoint`//`/findings` skill instructions if they
   mention it, README "Secondary goal" section stays (the story is still true).
4. The agent workflow must keep working: sessions append to JOURNEY in the companion
   repo checkout (decide the local path, e.g. `~/projects/archcheck-journal`), or the
   file stays in the working tree but gitignored-in-public + tracked-in-companion via
   a second remote — pick the SIMPLEST scheme that keeps one canonical file.
5. Note: removing files now does NOT scrub them from the public history. Decide
   explicitly whether that matters (option: squash-export at the #163 rename moment —
   NOT chosen for now; the repo was already public with this content).

## Acceptance

- [ ] Private repo exists; files moved; public tree clean of the listed items.
- [ ] CLAUDE.md + memory + skills updated so the JOURNEY-append rule still works.
- [ ] A session smoke-test: /checkpoint and a JOURNEY append land in the right place.
- [ ] No broken links in public docs (grep for the moved filenames).

## Do not do

- Do not rewrite public git history without a separate explicit decision.
- Do not delete JOURNEY content anywhere — this is a move, not a cleanup.
- Do not move the manual-audit docs (#160-#162 outputs) — they back public claims.
