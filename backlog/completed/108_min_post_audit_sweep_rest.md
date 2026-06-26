# [CLEANUP][QUALITY] Post-audit sweep: sections 2–4 (deferred from #104)

**Created:** 2026-06-11
**Started:** 2026-06-19
**Completed:** 2026-06-19
**Status:** done
**Module:** SCAN / QUALITY
**Priority:** minor
**Difficulty:** quick_win (in fact)
**Related:** #104 (section 1 closed, be56245), #107, #105 (security hardening — touched)

Deferred remainder of #104 (wording and grep evidence — in
`backlog/completed/104_min_post_audit_cleanup_sweep.md`).

## Review summary (2026-06-19) — #104 descriptions verified against the code before work

Most items turned out to be **stale** (the code had moved ahead). The skeptic rule
fired: don't trust the #104 aggregate, verify each case.

1. **Section 2 — duplicates >5 lines** — reviewed:
   - **fork/exec** (`git_exec.cpp` ↔ `git_object_file_source.cpp`): the fork/pipe/
     dup2/lifecycle skeleton is **genuinely different** (one-shot stdout+stderr vs a long-lived
     bidirectional stdin+stdout for `cat-file --batch`). Merging = leaky abstraction,
     NOT done (extractability test failed). BUT inside it a **real** duplicate was found:
     the hardening flags `-c core.hooksPath=… -c core.fsmonitor= -c core.pager=cat`
     are identical in both and security-relevant (#105) — a divergence = a hole. ✅ **Extracted
     into `include/archcheck/git/git_hardening.h` (single source of truth)**, both sites
     iterate the shared array. The fork/pipe plumbing was left separate.
   - **toLowerCopy**: ❌ **no longer a duplicate**. In `duplication_scanner.cpp` there is no
     more `tolower`; one occurrence remains in `area_of.h:52` + a separate shared
     `file_classification.h::toLowerAscii` (5 places). No duplication → a new
     string-util header is NOT needed (it would be an abstraction with no need).
   - **plainJaccard**: ❌ removed back on 2026-06-13 (not duplicated).
   - JSON serialization: ❌ already deduplicated in #055 (json_escape). The vendor/test filter
     was unified in #129 (`AuthoredScope`). Both items closed by other tasks.
2. **Section 3 — duplication_scanner.cpp hygiene** — ✅ done 2026-06-13.
3. **Section 4 — lizard debt** — ✅ **no debt**. The exact CI gate
   (`lizard --CCN 15 -T nloc=30 --arguments 5 src/ include/`, ci.yml:154) **passes
   clean**. The `--length 30` noise — physical length, which CI deliberately does NOT gate
   (ci.yml: "otherwise densely documented code gets penalized for comments"). Splitting
   NLOC-30 functions = churn against the project's decision. NOT doing it.

**Verification:** 540/540 tests (including 47 git), dogfood `src/ include/ tests/` = 0,
lizard CI gate clean, clang-format clean.

## Changed files

- `include/archcheck/git/git_hardening.h` — new: shared `kGitHardeningArgs` (commit `50871c2`)
- `src/git/git_exec.cpp` — uses the shared array, local one removed
- `src/git/git_object_file_source.cpp` — argv iterates the shared array

## Outcome

**Status:** completed. The only real change turned out to be extracting the hardening flags into a
single source of truth (security meaning). The rest (toLowerCopy, JSON, fork/exec
skeleton, lizard §4) — stale or a false duplicate; the #104 descriptions were verified against the code
and dismissed with evidence. The skeptic rule fired: don't act on the description.
