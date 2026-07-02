# [RELEASE] Public launch plan — packaging, hygiene, evidence, announcement

**Created:** 2026-07-02
**Status:** new (phase 0 hygiene items executed 2026-07-02, see below)
**Module:** RELEASE / DOCS / CI
**Priority:** major
**Complexity:** L (multi-phase, mostly S/M items)
**Blocks:** public announcement
**Blocked by:** #160/#161/#162 (evidence base — the "no nonsense goes public" gate) — done;
#127/#131 applicability sign-off (vendored/generated noise tail) — done, residual → #153
**Related:** #138 (release-readiness sync), #142 (prebuilt binary), #156 (demo repo),
#150 (EN translation), #003 (name check)

## Context — where the project actually stands

The repo is **already released**, not pre-release: tags `v0.1.0..v0.1.5`, a tag-triggered
release workflow publishing two checksummed Linux x86_64 binaries (dynamic + fully static)
with smoke jobs on clean runners and `debian:10`, a Keep-a-Changelog CHANGELOG, a live demo
repo ([blurman-ai/archcheck-demo](https://github.com/blurman-ai/archcheck-demo)), user-facing
CI/config/baseline docs, and a public-quality README. What is missing for a *public
announcement* is packaging breadth, community files, evidence write-ups, and the launch
sequence itself.

Sources for this plan: a full repo release-readiness sweep + external research on devtool
launches (opensource.guide, HN Show HN guidelines, Homebrew/Scoop/winget/AUR/GHCR/Actions
Marketplace docs, ripgrep/mold/fd launch case studies, PVS-Studio OSS-articles playbook).
Research details: ripgrep launched via a single evidence-heavy blog post with honest
anti-pitch; fd's own Show HN got 3 points and the tool still won via Reddit community posts;
mold's big HN thread was submitted by a third party. Conclusion: niche C++ venues + evidence
first, HN as a bonus.

## How maintainers decide "it's ready" (documented bar)

1. **Triable in one command** — prebuilt binary + install one-liner (HN guideline: "If your
   work isn't ready for users to try out, please don't do a Show HN").
2. **Differentiation stated** — the "why not clang-tidy/IWYU/CppDepend" table; the #1
   predictable comment in every venue.
3. **Honest evidence** — findings on real projects, hand-verified, with methodology
   (ripgrep/PVS-Studio pattern). This is exactly what #160/#161/#162 produce.
4. Repo hygiene: LICENSE, README answering what/why/quickstart/help, CONTRIBUTING,
   versioned release. (opensource.guide + GitHub community profile checklist.)

Project-specific readiness gate (from TASK_TRACKER + this session):
- [x] #160/#161/#162 manual evidence base complete — no signal ships un-audited.
      **Done 2026-07-02** (tasks in `completed/`; bugs found were fixed in #164).
- [x] #127/#131 vendored/generated sign-off (bundled-deps repos don't drown in noise).
      **Done 2026-07-02** (#131 closed → `completed/`): spdlog's bundled fmt, rocksdb
      third-party/, and terminal dep/ all excluded cleanly. Two classification bugs
      caught and fixed in the same pass (fmt self-exclusion, dep/ container gap) plus a
      case-sensitivity extension gap (`.C`). Honest residual documented, not blocking:
      #153 (fixtures, bpftrace/newsboat calibration, 4-rule path-trio gap) stays open.
      #131 Groups 2/4 (long-running corpus measurements) stay open in #123/#124 and
      #054/#066/#103 — tracked there, not gating this checklist item.
- [x] First-run sanity on 3–5 well-known OSS repos (fresh eyes, zero-config).
      **Done 2026-07-02**: fmt, spdlog, nlohmann/json, rocksdb, microsoft/terminal.
      Caught and fixed two classification bugs (fmt self-exclusion from #164 B.1;
      `dep/` container missing). All SF.9 gate cycles hand-verified as real mutual
      includes; fmt's single advisory (core.h has no include guard) is genuine.

## Phase 0 — repo hygiene (mostly done)

- [x] LICENSE (Apache-2.0) — already present.
- [x] CONTRIBUTING.md — **added 2026-07-02** (build, tests, fixture rule, one-rule-one-file,
      style pointers, commit conventions).
- [x] SECURITY.md — **added 2026-07-02** (private disclosure via GitHub security advisories).
- [x] CODE_OF_CONDUCT.md — **added 2026-07-02** (Contributor Covenant 2.1, standard).
- [x] Issue templates (bug / rule-fp / feature) + PR template — **added 2026-07-02**.
- [x] README: install section + release/license badges — **added 2026-07-02**.
- [x] README: demo screenshots or asciinema GIF (open item from #156). **Done 2026-07-02**:
      animated terminal GIF (`docs/assets/archcheck-diff-demo.gif`, 27 KB, 7 s) — real
      `--diff` output on the #156 demo repo replayed byte-for-byte (only the prompt is
      styled), embedded at the top of README. User-approved before commit. A browser
      screenshot of the live PR comment remains a nice-to-have, not a blocker.
- [x] **DECIDED 2026-07-02: rename now.** Repo renamed `blurman-ai/cpparch` →
      `blurman-ai/archcheck` before the announcement (GitHub redirects cover old links);
      README badges, docs/ci_usage.md snippets, and workflows updated in the same change.
      The local working directory stays `cpparch` (CLAUDE.md tool-path stability).
- [x] **DECIDED 2026-07-02: option (b) — journal + heavy internals go private.**
      Execution split into task **#167** (private companion repo for JOURNEY,
      milestones, dup_band, analysis/, sandbox/, symlink cleanup; backlog/ and the
      #160-#162 audit docs STAY public — they back the precision claims).
- [x] Scrub check: no secrets/sensitive material in git history (opensource.guide item).
      **Executed via #167** (2026-07-02): local paths and internal references scrubbed
      from public files and history; journal/research bulk moved to the private repo.

## Phase 1 — packaging & distribution

- [x] **Platform matrix — DECIDED 2026-07-02: NOT a launch blocker.** Launch with
      Linux x86_64; Windows x64 and macOS arm64 are planned post-launch as **#165** and
      **#166** (tasks created, ROADMAP/README note the plan). Linux arm64: on user demand.
- [ ] **GitHub Action wrapper** — `archcheck-action` repo with `action.yml` (composite:
      download pinned binary by version input, run `--diff`, emit step summary + optional
      sticky comment — the logic already exists in `example_archcheck_pr.yml`). Publish to
      Actions Marketplace (instant, no review). This is the #1 adoption funnel for a CI tool:
      `uses: blurman-ai/archcheck-action@v1`.
- [ ] **Docker image on GHCR** — scratch/distroless image with the static binary,
      `org.opencontainers.image.source` label, build in release workflow. S effort.
- [ ] **Homebrew tap** — `blurman-ai/homebrew-archcheck`; `brew install
      blurman-ai/archcheck/archcheck` works day one (homebrew-core needs ≥75 stars /
      ≥225 for self-submission — post-traction).
- [ ] Nice-to-have: scoop manifest (main bucket charter = OSS CLI devtools — perfect fit),
      AUR PKGBUILD, winget manifest. pip wheel (clang-format-wheel pattern) — v0.2.
- [ ] Skip at v0.1: npm wrapper, vcpkg/conan, apt/PPA, homebrew-core.

## Phase 2 — evidence building (soft launch)

- [ ] Finish #160/#161/#162 → publishable precision/FP numbers per signal.
- [ ] Run archcheck on 5–10 famous C++ OSS projects; hand-verify; write up 2–3 genuinely
      interesting findings with `file:line` (reuse `experiments/showcase/` gallery — dedup,
      eyeball, user sign-off per project rule).
- [ ] Launch blog post, ripgrep-shaped: problem (constraint decay) → pitch → **anti-pitch**
      (the What-it-is-NOT list) → real findings → methodology + reproducibility. The README
      already contains most of the pitch skeleton.
- [ ] Optionally file upstream issues for real findings (credibility + backlinks).
- [ ] 2–3 friendly C++ devs install from scratch on clean machines; fix friction.

## Phase 3 — announcement (staggered over days, not all at once)

- [ ] **r/cpp first** (niche beats general for this tool). ⚠ Read the sidebar
      self-promotion rules in a browser first — they were not machine-verifiable.
      Technical framing: "what it found on real projects", not "I made a tool".
- [ ] isocpp.org "Suggest an Article" + Meeting C++ blogroll submission (right audience,
      trivial effort).
- [ ] **Show HN**: plain title ("Show HN: Archcheck – architecture checks for C++ in CI
      (Lakos, Core Guidelines)"), author first-comment with backstory + differentiation,
      answer everything for 2–3 h, no friend-boosting (HN bans it). A flop is normal
      (fd: 3 points) and non-fatal; the second-chance pool exists.
- [ ] awesome-cpp PR (`[archcheck](link) - description. [Apache-2.0]`, alphabetical,
      one PR), lobste.rs (author checkbox), #include<C++> Discord, X/LinkedIn.

## Phase 4 — post-launch

- [ ] Answer every issue fast for the first weeks (the documented "someone committed to
      respond" bar).
- [ ] Serial "archcheck vs <project>" findings posts (PVS-Studio/scc compounding pattern).
- [ ] Pitch CppCast / ADSP once there are adoption stories.
- [ ] homebrew-core submission at the stars threshold.

## Top-3 leverage / top-3 fatal mistakes (from the research)

Leverage: (1) prebuilt binaries + Action wrapper BEFORE any announcement; (2) evidence
post from the corpus infrastructure; (3) C++-native venues before/alongside HN.
Fatal: (1) try-it friction (build-from-source-only); (2) no differentiation vs clang-tidy;
(3) marketing tone / astroturf.

## Acceptance

- [ ] All Phase-0 items closed or explicitly decided by the user.
- [ ] Phase-1 must-haves shipped: multi-platform binaries, Action wrapper, GHCR image, tap.
- [ ] Evidence gate green: #160/#161/#162 done, showcase examples user-approved.
- [ ] Launch post drafted and user-reviewed.
- [ ] Venue sequence executed by the user (announcements are outward-facing — the user
      posts, not the agent).
