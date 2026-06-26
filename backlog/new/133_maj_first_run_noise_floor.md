# [RULES][CLI] First-run noise floor: ChainLength/GodHeader flood on header-heavy libs

**Created:** 2026-06-19
**Started:** 2026-06-19
**Status:** wip — the main fix (advisory in check-mode) is done + verified; the remainder = `--diff` mass-rename guard (narrow)
**Module:** RULES / GRAPH / CLI
**Priority:** major (go-public quality — first contact)

## Done (2026-06-19) — the "advisory in check-mode" decision

User's choice: **gating (exit 1) in check-mode only for a cycle (SF.9)**; ChainLength,
GodHeader, SF.7/SF.8 — reported but advisory (exit 0). This is recommendation §7 of the report
("gate = cycles; god-header and the rest advisory") and a mirror of the `--diff`/drift model.

- `src/cli/check_command.cpp`: added `reportCheckGate` (modeled on `reportDriftGate`)
  — exit 1 only if there's an `SF.9`; prints "N advisory finding(s) … not gated …
  use --baseline". Replaced `return all.empty() ? 0 : 1;`.
- Test `cli_smoke_e2e_test.cpp`: a new cycle fixture (gating exit 1) + the case "SF.7
  reported but exit 0"; json test → exit 0.
- **Verification (re-run sanity):** abseil 219 findings → **exit 0**, spdlog 40 → **0**,
  fmt → **0**; curl → **exit 1** (the cycle `curl.h↔multi.h` gates — TP, as it should).
  547/547 tests, dogfood 0, lizard/format clean. NOT committed (parallel sessions).
- ⚠️ **The exit-code contract shifted** (check-mode: 1 = cycles only, not "any
  violation") — needs to be recorded in CHANGELOG (v0.1, pre-tag, acceptable).

**Remainder:** only the `--diff` mass-rename guard (below) — narrow, doesn't block go-public.
**Blocks:** the announcement / public release (the first foreign repo must not drown in noise)
**Blocked by:** —
**Related:** #127 (vendored exclusion — removes PART of the noise, but abseil isn't vendored), #126 (SF.9 component collapse), #057 (cheap graph signals), MVP.md §"--baseline from day one"

## Evidence (first-run sanity, 2026-06-19)

`archcheck <repo>` (check-mode, no `--baseline`) on well-known C++ repos:

| Repo | Total | Breakdown | exit |
|------|-------|-----------|------|
| fmt | 1 | SF.8 ×1 | — |
| nlohmann_json | 2 | ChainLength ×2 | — |
| spdlog | 40 | **ChainLength ×39**, SF.8 ×1 | 1 |
| curl | 14 | ChainLength ×5, **GodHeader ×8**, SF.9 ×1 (TP) | 1 |
| **abseil** | **219** | **ChainLength ×211**, GodHeader ×8 | 1 |

**Conclusion:** first-run noise is **ChainLength** (chain threshold 10) and secondarily
**GodHeader** (fan-in 50). Cycles do NOT make noise (abseil/spdlog — 0 cycles; the curl cycle
`curl.h↔multi.h` is a genuine TP). This is exactly the "5000 violations on first run" that the
spec defends against with `--baseline` — but a **naive first `archcheck <repo>`** (which is
exactly how a person probes the tool) yields abseil = 219, exit 1, and it gets written off.

## Why this is a go-public blocker

The first thing a skeptic from HN does: `archcheck` on their own/a known repo **with no flags**.
If the answer is "219 violations, mostly include chain depth 11 > 10" — they close the
tab. The narrow, indisputable value (a cycle/copy-paste introduced by a PR) drowns in ChainLength noise.

## What to decide (this is design, not patched in haste)

1. **ChainLength threshold 10 — too aggressive for modern header-heavy C++.**
   abseil/spdlog/nlohmann deep chains are the norm, not debt. Options:
   - raise the default threshold (15? 20? — measure the distribution across the corpus);
   - make ChainLength **advisory** (not gating/exit-1) in check-mode — keep
     gating only for cycle/god-header (as in `--diff`);
   - keep the threshold, but in check-mode without `--baseline` print an explicit hint
     "N existing findings — run with --baseline to gate only new drift".
2. **GodHeader on config/logging hubs** (`curl_setup.h` fan-in 309) — structurally-but-
   legitimate (see the showcase history #003, removed as weak). A fan-in-only proxy.
   Option: allowlist of known wide headers / lower severity to advisory.
3. **First-run UX:** perhaps the default `archcheck <repo>` without `--baseline` should
   itself suggest `--baseline` ("this is a snapshot of all debt; for the CI gate use --diff").

## Cycle-gate mass-rename (narrow remainder, separate)

In check-mode cycles are clean. In `--diff` ~19% of cycle-fires are artifacts of mass
include-rewrite/move (coal 252 files, allwpilib 2477). This is a **separate** `--diff` guard:
suppress `grown_cycles` when the commit = a mass rename (>N rename edges). Not for
check-mode; move it to the #131 check or here as a sub-item. The `.tmpl/_impl` idiom is already
excluded (#088/#126) — do NOT touch it (the curl cycle and the JANA2 gem are genuine TP).

## Verification (fixtures mandatory)

- [ ] Decision on ChainLength fixed (threshold / advisory / baseline hint).
- [ ] Fixtures: a header-heavy chain of depth 12 → behavior per the chosen option.
- [ ] Re-run sanity: abseil/spdlog/curl → noise dropped to a defensible level; exit-1 only on
      real gating signals (or with an explicit first-run hint).
- [ ] Cycle/copy-paste TP (the curl cycle, JANA2) NOT suppressed.

## Self-check

Not "lower the thresholds so the numbers look quiet" — that's the inverse deception. The goal is for the
**remaining** firings to be defensible, while existing debt goes through `--baseline`,
as intended. Measure the chain distribution across the corpus before shifting the threshold.
