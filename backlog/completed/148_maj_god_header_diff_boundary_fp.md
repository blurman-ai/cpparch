# [NOT-A-BUG][RULES] `--diff` "new god-header" at the 50→51 boundary — verified correct

**Created:** 2026-06-26
**Started:** 2026-06-26
**Resolved:** 2026-06-27 — **not a bug; the original FP claim was a raw-grep artifact**
**Status:** resolved (not-a-bug)
**Module:** RULES / GRAPH / DIFF
**Related:** #126 (header/impl cycle FP — adjacent diff-precision class), #133 (god-header noise floor)

## Original symptom (trending run #145)

`78/xiaozhi-esp32` produced 6 gate-fails / 300 commits, all `new_god_headers`. A **raw `#include`
grep** suggested the headers were already god (≥50) in the parent (e.g. es8311 "51→52"), which would
make them false gate-fails ("the PR touched an already-god header", not "created" one).

## Resolution — verified with archcheck's own fan-in, not the grep

The task's own check-list item #1 ("print baseline/current fan-in as archcheck sees them, not the
grep") was executed via a temporary env-gated debug print in `detectNewGodHeaders`. archcheck's
component-resolved fan-in for all 6:

| commit | header | archcheck baseline → current (thr=50) |
|---|---|---|
| 97c0e75 | `es8311_audio_codec.h` | **50 → 51** |
| 022d984 | `system_reset.h` | **50 → 51** |
| d545f74 | `mcp_server.h` / `settings.h` | **50 → 54** / **50 → 52** |
| 7ad22d4 | `power_save_timer.h` | **50 → 51** |
| c9fa5fa | `i2c_device.h` | **50 → 51** |
| cfb635d | `single_led.h` | **50 → 51** |

Every header was at **exactly 50** in the parent — **not** god (god requires `> 50`) — and the commit
pushed it to ≥51. These are **genuine threshold crossings, not false positives.** The raw-grep "51"
over-counted versus archcheck's component resolution (which dedups headers→components and conditional
includes); archcheck sees baseline = 50, the maximal non-god value.

Confirmed the +1 is a **real new includer**, not resolver noise: commit 97c0e75 ("feat: add ATK
DNESP32S3B3 board", PR #1931) adds a new file `main/boards/atk-dnesp32s3-box3/atk_dnesp32s3_box3.cc`
containing `#include "codecs/es8311_audio_codec.h"` — the literal 51st includer. xiaozhi is an embedded
project where common board headers are included by ~50 board variants; each new board ticks one across
the line. The signal is real: the header *did* just become a god-header.

`detectNewGodHeaders` (`regression_report.cpp`) is **correct** — it already subtracts the baseline
god-set (`bFanIn[i] > threshold`). No code change. The debug print was reverted.

This is a textbook self-check catch (CLAUDE.md): a raw-grep result produced "FP" → the conclusion was
unfounded → archcheck's own numbers refuted it before it reached the user. Records that called these
"FP class #148" (JOURNEY, bot_review_drift.md) corrected accordingly.

## Residual (optional, NOT a bug) — boundary flapping sensitivity

A header sitting at exactly the threshold crosses 50↔51 on tiny changes, so an active project can see
repeated "new god-header" gate-fails as common headers proliferate (xiaozhi: 6 in 300 commits ≈ 2%).
Each is a *correct* report, but a marginal one. If adopter feedback shows this is noisy, a v0.2
sensitivity knob (require crossing by a margin, or `baseline ≤ threshold − N`) could damp it — a
**deliberate sensitivity trade-off**, not a correctness fix, and it would suppress real signals. Filed
as a thought, not a task.
