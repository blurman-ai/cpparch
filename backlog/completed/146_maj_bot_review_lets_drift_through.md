# [RESEARCH][SHOWCASE] An AI review bot lets architectural drift through

**Created:** 2026-06-26
**Started:** 2026-06-26
**Status:** wip
**Module:** RESEARCH / SCAN / SHOWCASE
**Priority:** major
**Related:** #145 (trending cohort, the same scan), #123 (outward-facing demo — candidate), #119 (agentic drift)

## Hypothesis

AI PR-review bots (`copilot-pull-request-reviewer[bot]`, `sysrsbuild-gh-agentic`,
CodeRabbit, Sourcery, Qodo, …) read the diff line by line — style, bugs, naming — but
**do not model the architecture**. So a PR that introduces a new cycle / god-header / cross-area
edge **passes their review and gets merged**. archcheck catches exactly this.

This is potentially a **killer product demo** (a candidate for the outward-facing #123): "the AI reviewer
said LGTM — archcheck found a new dependency cycle in the same PR".

## Signal (objective, verifiable)

A PR with a review from a bot account. Cleaner than the author trailers of #119 (where the agentic label
turned out to be a labeling artifact): a review bot either participates in a PR or it does not — visible via `gh api`.
Reviewers found: `sysrsbuild-gh-agentic` (realsenseai, login literally "agentic"),
`copilot-pull-request-reviewer[bot]` (onnxruntime/openvino/arrow/librealsense),
`aikido-pr-checks[bot]`, `github-advanced-security[bot]`.

## Method (`experiments/trending_run/bot_review_drift.py`)

1. Merged PRs of a repo (`gh api pulls?state=closed`), keep those reviewed by a bot (+ review
   STATE: **APPROVED** = the strongest "the bot said ok").
2. `archcheck --diff merge^1..merge` (the PR change by first-parent) → drift signals.
3. Flag: a bot-reviewed PR with GATE drift (cyc/god) or cross-area = **drift the bot let through**.
4. Metric: share of bot-reviewed PRs with drift + individual examples (bot APPROVED → new cycle).

## Discipline

This is NOT about population representativeness — it is about the **mechanism**: does this
class of review bot miss this class of defect. In the limit — a case study with concrete PR examples.
Drift is rare (~0.1% gate), so PR scale is needed to fish out the ones that slipped through.

## Focused result librealsense (2026-06-26) — NULL, but instructive

144 merged PRs, 95 bot-reviewed (66%), 87 scanned (8 timeouts). **0 gate drift**
(0 cycles/god-headers/cross-area). 10 PRs added edges (1–13), the bots commented on them.
**All 112 bot reviews = `COMMENTED`** (zero APPROVED).

Why this is NOT "the bot did well", but simply a weak test:
1. **Statistics:** gate drift ~0.1% of commits → over 87 PRs the expectation is ≈0.09. Finding 0 is
   the base rate, not a signal. The sample is 1–2 orders of magnitude too small.
2. **Wrong target:** librealsense is a mature 10-year-old stable lib, little drift by
   nature. We need young/being-restructured ones (the AI stratum).
3. **The bots comment, they do not approve** → the claim = "the review did not mention the architecture", not "the bot
   approved something bad".

The method IS VALID: 10/87 showed added_edges>0 — the diffs are not empty, archcheck sees the PR changes.

## Design PIVOT: invert the search (instead of iterating over bot PRs)

Do not scan thousands of bot PRs for rare drift. **First find the drift** (it is already computed
per-commit in #145 trending + will be computed in the AI stratum), **then** for the drifting commits/PRs
check the bot review via `gh api`. We look where the signal is.
- Confirmation: the trending scan caught gate drift on the fly in `78/xiaozhi-esp32` (×6),
  `apache/arrow` (×1) — these are the candidates for "did a bot review it".

## Inverted result — corpus (2026-06-27) — POSITIVE, eye-checked

`experiments/trending_run/inverted_bot_drift.py` over **648 trustworthy gate-drift commits**
(579 grown-cycle + 114 cross-area from corpus #090; god-headers excluded as #148-FP-prone).
Funnel: 304 direct-push + 8 api-err → **336 PR-gated**; of those 180 human-only, 102 no-review,
11 generic/CI-bot, **43 commits / 38 distinct PRs reviewed by an AI code-reviewer**
(copilot-pull-request-reviewer, coderabbitai, gemini-code-assist, sourcery-ai). **0 APPROVED**
(all COMMENTED — bots cannot block alone), **all 38 merged with the drift**.

Skeptic pass (mandatory): raw keyword screen over review bodies + inline + PR issue-comments said
15/38 "arch-aware"; **reading the sentences collapsed it to 2** (`FeJS8888/FeEGELib#14` "Break the
circular include… reduce coupling"; `TinyMUSH#75` "tightly couples eval") — the other 13 were false
keywords (`architectures`=hardware ISA, `cycle`=in-game day/night, `separation of concerns ✅`=praise,
`architecture details`=.md blurb). **Net ~36/38 PRs: the bot commented on bugs/style, never the
structural drift.** Denominator verified clean — cycles re-confirmed real (jank#757 2→4, nvme-cli#3391,
hand-traced SPECFEMPP#943 0→7 assembly↔source). Concentration caveat: 38 PRs = 17 repos (SPECFEMPP 11,
FlashCpp 5, Xorion 4). Trending-13: 0 AI-reviewed (all human-only/no-review). Full write-up:
[docs/research/bot_review_drift.md](../../docs/research/bot_review_drift.md).

**Showcase candidate (#123):** `PrincetonUniversity/SPECFEMPP#943` "Single include header for assembly
module" — copilot's summary said the PR "breaks circular dependencies… to cut circular includes";
archcheck found a **new 7-member cycle**. The bot endorsed the PR's framing; the resulting graph has
a cycle. The thesis in one PR.

**Status:** completed 2026-06-28

## How it works

Inverted funnel design: start from the rare event (gate-drift commit), not the common one (a PR). Pool
= 665 gate-drift commits (648 corpus + 17 ai-stratum). Per commit: `gh api commits/{sha}/pulls` to
find the PR, then reviews to classify reviewer type. Of 346 PR-gated commits, 43 (38 distinct PRs)
had an AI code-reviewer; none were blocked (state=COMMENTED throughout); all merged with the drift.

Keyword screen (15/38 flagged) → manual reading collapsed to 2 genuine structural flags.
Net: ~36/38 PRs had zero structural engagement from the bot reviewer.

Key output: [docs/research/bot_review_drift.md](../../docs/research/bot_review_drift.md)  
Showcase: [experiments/showcase/006_cycle_specfempp_bot_blindspot.md](../../experiments/showcase/006_cycle_specfempp_bot_blindspot.md)

## Plan

- [x] PoC + focused librealsense → NULL (sample too small, mature lib). The method is valid.
- [x] **Invert:** corpus gate-drift commits → PRs → bot review → 38 AI-reviewed drift PRs, ~36 no
      structural engagement, 0 blocked, all merged. Eye-checked, denominator TP-verified.
- [x] Fold in the ai-377 agentic stratum → combined funnel table added to §4 of the research doc
      (corpus 648 + ai-stratum 17 = 665 total gate-drift; 0 AI-reviewed in stratum, same as corpus).
- [x] Bright case → showcase/demo (#123): `experiments/showcase/006_cycle_specfempp_bot_blindspot.md`
      — SPECFEMPP#943, AI reviewer said "cut circular includes" + 0 comments + merged, archcheck found
      a new 7-header cycle. Verbatim bot quote, hand-traced cycle, reproduce command.
