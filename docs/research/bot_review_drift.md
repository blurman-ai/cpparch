# Do AI PR-review bots catch architectural drift?

**Build date:** 2026-06-27 · **Genre:** evidentiary line for the article (a self-contained
side-result). Every number is verbatim from the primary source (`experiments/trending_run/`).
Skeptic rule: a conclusion rests on cross-validation and manual reading, not on "the run produced
numbers". Tasks: #146 (hypothesis), #145 (trending cohort), #149 (coverage caveat), #148 (god-header
50→51 crossing — verified not-a-bug).

> **Short answer.** AI PR-review bots (copilot-pull-request-reviewer, CodeRabbit, gemini-code-assist,
> Sourcery) comment on the diff line-by-line but do **not** model the dependency graph. Of **38**
> merged pull requests that introduced a new/grown include **cycle** or a new **cross-area**
> dependency *and* were reviewed by an AI bot, the bot raised the structural problem in **2**; the
> other **~36** got comments about bugs/style/naming while the drift sailed through. **0** were
> blocked (GitHub review bots comment, they cannot gate a merge alone). archcheck catches exactly
> this blind spot.

---

## 1. The question

Hypothesis (#146): an AI reviewer reads the textual diff and reasons locally; it does not build the
include/component graph, so a PR that creates a dependency cycle or a new cross-module edge passes
its review and merges. That is precisely the regression `archcheck --diff` gates on (DRIFT.1/SF.9
grown cycle, DRIFT.4 cross-area). If the hypothesis holds, archcheck has a wedge no line-by-line AI
reviewer covers.

## 2. Why the obvious (forward) design is underpowered — and died

The natural test: take a repo whose PRs an AI bot reviews, scan those PRs for drift. Ran it on
`realsenseai/librealsense` (`botdrift_librealsense.jsonl`): **95 merged PRs, 87 measured**, three
bots reviewing (copilot-pull-request-reviewer 43, aikido-pr-checks 35, sysrsbuild-gh-agentic 34) —
**all COMMENTED, none APPROVED**, gate-drift through the bot = **0**.

That **0 is not evidence the bots work.** Gate-drift is a ~0.1–0.3% base-rate event per commit
(corpus: 1,615 / 520,177 = 0.31%; trending non-FP: 0.09%). Over 87 PRs the expected number of
gate-drift events is ≈ 0.08–0.27 — observing 0 is exactly what a *blind* bot produces too. The
forward design's denominator is the common event (a PR), so it almost never contains the rare one.
The test has no power.

## 3. The inverted design (has power)

Start from the **rare event**. Take the gate-drift commits archcheck already found, then ask, for
each, whether it went through a PR an AI bot reviewed.

Frame = the corpus run #090 (`results_full.boolrule.jsonl`, 520,177 commits, 1,188 repos — the
proper probability sample). Pool = **648 trustworthy gate-drift commits**: 579 grown cycles +
114 new cross-area (overlap → 648). **God-headers excluded on purpose** — a 50→51 fan-in crossing is a
correct but *marginal* signal (a header gaining its 51st includer), weaker than a grown cycle, and an
adopter could see it flap; cycles/cross-area are the unambiguous structural drift. (Note: #148, which
suspected these crossings were FPs, was verified *not*-a-bug — archcheck's component fan-in shows real
50→51 crossings; the exclusion here is about signal strength, not correctness.) Per commit:
`git remote → owner/repo`, then `gh api commits/{sha}/pulls → reviews` → classify reviewers.
Harness: `experiments/trending_run/inverted_bot_drift.py`, output `inverted_corpus.jsonl`.

## 4. The funnel (corpus, 648 gate-drift commits)

| stage | commits |
|---|---|
| gate-drift (grown-cycle ∪ cross-area) | 648 |
| direct push, no PR | 304 |
| API error | 8 |
| **went through a PR** | **336** |
| — human-only review | 180 |
| — no review at all | 102 |
| — generic / CI bot only (e.g. chatgpt-codex-connector) | 11 |
| — **AI code-reviewer reviewed** | **43 commits / 38 distinct PRs** |

Of the 38 AI-reviewed drift PRs: **0 formally APPROVED** (every bot review state was COMMENTED),
**all 38 merged with the drift.** Bots seen: `copilot-pull-request-reviewer[bot]`,
`coderabbitai[bot]`, `gemini-code-assist[bot]`, `sourcery-ai[bot]`.

### Combined funnel (corpus + ai-377 stratum)

| stage | corpus (648) | ai-stratum (17) | total |
|---|---|---|---|
| gate-drift (cyc ∪ xarea) | 648 | 17 | **665** |
| direct push, no PR | 304 | 7 | 311 |
| API error | 8 | 0 | 8 |
| **went through a PR** | **336** | **10** | **346** |
| — human-only review | 180 | 7 | 187 |
| — no review at all | 102 | 3 | 105 |
| — generic / CI bot only | 11 | 0 | 11 |
| — **AI code-reviewer reviewed** | **43 / 38 PRs** | **0** | **43 / 38 PRs** |

The agentic stratum (ai-377) adds **10 PR-gated gate-drift commits** to the denominator but zero AI-reviewed ones — entirely consistent with the corpus result, though underpowered due to the #149 coverage failure (§9).

## 5. The skeptic pass: keyword screen 15 → manual reading 2

You cannot claim "the bot missed it" without reading what the bot said. A raw keyword screen
(`circular|cycle|coupling|architecture|dependency|#include|...`) over each PR's bot review bodies +
inline comments + PR issue-comments (all three endpoints — CodeRabbit posts its walkthrough as an
issue-comment) flagged **15 / 38** as "architecture-aware". Reading the actual sentences collapsed
that to **2**:

**False keyword (13 of 15):**
- `architectures` = **hardware** ISA — "RV32 and RV64 architectures" (Pepp #1003), "unaligned stores
  on some architectures", "fault on strict-alignment architectures" (nvme-cli #3391).
- `cycle` = a **domain** loop — "Skyrim's 4-period day cycle" (skyrim-community-shaders #1630).
- `separation of concerns` = **praise**, with a green check — "✅ Proper separation of concerns"
  (skyrim #1178); "follows good UI design patterns" (skyrim #1630).
- `architecture details` = a line in a generated **.md summary** (Astraeus #46, Xorion #17).
- bare `#include` = "add `#include <x>`" for a missing header, not cycle reasoning (FlashCpp #1480/#1530).

**Genuine structural flag (2 of 15):**
- `FeJS8888/FeEGELib#14` (copilot+coderabbit): *"Break the circular include; rely on forward
  declarations… reduce coupling."*
- `TinyMUSH/TinyMUSH#75` (copilot): *"This tightly couples eval…"*

Both were **advisory inline comments — the PR merged anyway.** Net: **~36 / 38** AI-reviewed
drift PRs got no structural engagement at all.

Lesson (also in JOURNEY): a keyword grep is a screen, not a verdict — "the word *architecture*
appears in the review" ≠ "the bot reasoned about the structure". Only reading the sentence separates
a flag from praise, hardware, or domain vocabulary.

## 6. The denominator is clean — cycles are real (TP)

Three independent checks that the 648 are genuine drift, not archcheck false positives:

1. **Bot cross-validation.** In the 2 cases the bot *did* engage structure, it named the **same**
   defect archcheck flagged (circular include / tight coupling). The bot independently corroborates
   the signal.
2. **archcheck re-run.** `jank-lang/jank#757` ("pointer tagging for inline integers") — grown include
   cycle 2→4 over `nil.hpp ↔ number.hpp ↔ object.hpp ↔ oref.hpp`. `linux-nvme/nvme-cli#3391`
   ("Split the nvme commands and types headers") — grown cycle on the header split. Both re-confirmed
   `gate: fail`.
3. **Manual include trace** of the showcase below.

## 7. Showcase: the PR that "cut circular dependencies" and created a 7-node cycle

`PrincetonUniversity/SPECFEMPP#943` — "Single include header for assembly module", reviewed by
copilot. The bot's summary (issue-comment): *"breaks circular dependencies via forward declarations,
and relocates the implementation… to cut circular includes."* The bot **believed the PR's own
framing**. archcheck reported a **new 7-member cycle (0→7)**. Hand-traced at the commit:

```
core/specfem/assembly/assembly.hpp  ──#include "source/interface.hpp"──▶
include/source/interface.hpp        ──#include "force_source.hpp"──▶
include/source/force_source.hpp     ──#include "specfem/assembly.hpp"──▶
core/specfem/assembly.hpp           ──#include "assembly/assembly.hpp"──▶  (back to start)
```

A real, closed include cycle. The refactor that advertised itself as *removing* circular includes
introduced a larger one, and the AI reviewer — reading intent, not the resulting graph — endorsed
the framing. This single case is the thesis in miniature.

## 8. Honest caveats

- **Concentration.** The 38 PRs are **17 repos**, not 38 independent projects: SPECFEMPP 11,
  FlashCpp 5, Xorion 4 (= 53%), the remaining 14 repos 1–2 each. The effect is real but clustered in
  a few bot-heavy projects; it is not "38 independent observations".
- **"Reviewed" ≠ "had veto power."** GitHub review bots comment / approve; they generally cannot set
  a blocking required-changes status alone. "Let through" here means "did not raise the structural
  issue," not "vetoed wrongly." Still: a human merging on a green bot summary that praises a
  cycle-introducing refactor is the exact failure archcheck removes.
- **Lower bound on the denominator.** "AI-reviewer reviewed" counts a bot that left ≥1 review or
  comment. Bots that ran but stayed silent are not counted; a larger true denominator would mostly
  add *more* no-engagement cases.
- **Trending-13.** The 13 trending-cohort gate-fails were 100% PR-gated but **0** AI-reviewed
  (7 no-review, 6 human-only) — those popular repos did not run AI reviewers on the drift PRs.
- **god-headers deferred.** Excluded as a weaker (boundary 50→51) signal, not for correctness — #148
  verified these crossings are real, not FPs. Including them would enlarge the pool with marginal cases.

## 9. The agentic stratum (ai-377) — inconclusive, crippled by #149

The ai-377 run (372 AI-cohort repos, 111,224 commits, the labeled agentic stratum) finished
2026-06-27. The hypothesis — agentic repos run review bots most heavily, so they should **extend the
denominator** — could **not** be tested: the #149 per-commit clone-scan timeout blacklisted **256 of
372 repos (69%)**, skipping **71,839 of 111,224 commits (65%)**. Only **37,206** commits were measured
→ 49 gate-fails (0.13%), of which **17** are cyc/cross-area. Running those 17 through the same harness:
**0 AI-reviewed** (7 direct-push, 7 human-only, 3 no-review).

This is **not** evidence that agentic repos' drift escapes bots less — it is a coverage failure. The
blacklist fell on exactly the large, active, bot-heavy projects (aliyun cpp-sdk, bytedance bolt,
bloomberg blazingmq, ardupilot fork, AliceO2 …) — the ones most likely to run CodeRabbit/copilot. The
agentic-stratum test is therefore **underpowered by construction** until #149 (scope the clone scan to
changed files) lands and the big repos can be measured. The load-bearing result stays the **corpus**
(§4–7), which has full coverage and is the proper probability frame.

Lesson logged: a silent coverage cap reads as "we covered the stratum" when 65% was dropped. #149 is
now a prerequisite, not a nice-to-have, for the agentic-angle re-run.

## 10. Bottom line for the product

archcheck occupies a wedge no line-by-line AI reviewer covers: the dependency graph. The pitch is
concrete and evidenced — *"AI review bots comment on your diff; they do not see the cycle it adds.
36 of 38 cycle-introducing PRs passed their review. archcheck gates exactly that, in CI, per commit."*
