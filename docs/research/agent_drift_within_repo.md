# Per-commit architectural drift in C++ OSS — and what AI agents change

**Date:** 2026-06-18 · **Instrument:** `archcheck --diff` (per-commit, memory backend)
**Corpus:** 1 188 C++ repos · 484 500 analyzed commits (520 177 attempted) since 2024-06.

> One-paragraph version. We ran a Lakos / C++-Core-Guidelines–grounded checker over
> every commit of 1 188 C++ OSS repos and measured how often a single commit introduces
> each kind of architectural regression. We then split commits into AI-agent-authored vs
> human-authored and asked, **controlling for repository and commit size**, whether
> agents drift worse. They don't. In no category are AI-attributed commits architecturally
> worse than human commits of the same size in the same repo; on self-admitted debt (and,
> more softly, test co-evolution) they are measurably *cleaner*. The one real agent
> difference is **commit size** — agent commits are ~1.3× larger — not a different *kind*
> of drift. A hand audit of flagged commits (§5) further shows the everyday signals are
> noisier than their raw rates suggest: most complexity fires are *inherent* complexity a
> competent author would keep, and only ~⅓ of copy-paste fires are duplication worth
> factoring out. The honest upshot: copy-paste (after a vendor/generated filter) and
> genuinely-new cycles are the trustworthy per-PR gates; everything else is an advisory
> nudge — and the gate is author-agnostic, which is exactly right because the AI
> difference is throughput, not sloppiness.

---

## 1. What we measured

`archcheck --diff <sha>^..<sha>` runs the shipped product on each commit and bundles
every check in one pass. A commit "drifts" in category *k* if it introduces ≥1 finding:

| category | what fires | gate? |
|---|---|---|
| `grown_cycles` | a new/grown include cycle (SCC) | gating |
| `god_headers` | a header crossing fan-in ≥50 | gating |
| `added_edges` | a new include edge | advisory |
| `cross_area` | a new cross-directory dependency | advisory |
| `newclone` | copy-paste (token clone) introduced | advisory |
| `complexity` | a function crossing/growing past cognitive-complexity 25 | advisory |
| `satd` | a TODO/FIXME/HACK/XXX added | advisory |
| `test_coevo` | production code changed without touching tests | advisory |

**Denominators are category-aware.** Bulk-import commits (>10 000 added lines) skip the
graph + complexity + clone checks by design (#117/#124 — a one-shot vendored/generated
dump is not authored evolution), so those categories are scored over non-bulk commits
only. Bulk is just 0.4 % of commits here, so it barely moves anything. (But see §5: a
*sub-bulk* generated/vendored regime just under the threshold is a real precision hole.)

**Data hygiene (three corrections found by dogfooding the run itself):**
- *empty-blob desync* in the memory backend corrupted every graph category — fixed
  (commit f3ab6ac), run restarted clean; the low final cycle/god rates confirm it.
- *parentless / shallow-boundary commits* fake "whole tree added" — excluded at source.
- *bulk imports* no longer gate the graph (only slow incremental drift should).

**Post-run classification refactor (#129, commits ec5988b…b01707a).** The run above
used a binary whose vendor/test/generated/banner filter was open-coded per rule with
divergent formulas (the FP audit in §5 surfaced this). It is now one shared gate
(`scan::AuthoredScope`) over a `SourceSnapshot` that reads+classifies each ref's tree
exactly once. We then **re-ran the corrected binary over the full agent+human corpus**
(128 393 commits — the 226 repos carrying ≥20 of each class) and compared old-vs-new on
the *same* commits with the *same* method: the agent-vs-human conclusion is unchanged in
6 of 7 categories, and the one that moved moved in agents' favor (§5). The numbers below
are the original-run numbers; the only material shift is copy-paste *recall rising* on
self-licensed projects, so the copy-paste rate is a **floor**.

---

## 2. Population baseline — the per-commit drift census

484 500 ok commits, 1 188 repos. Two aggregations are reported because they disagree
under skew: **pooled** (every commit one vote — big repos dominate) and **repo-macro**
(every repo one vote — the median project).

| category | pooled | repo-median | repo-p90 |
|---|---|---|---|
| test-coevolution (code w/o test) | 22.1 % | 23.3 % | 48.9 % |
| complexity-ratchet | 17.1 % | 16.1 % | 40.7 % |
| added-edges (new include) | 14.8 % | 13.2 % | 35.4 % |
| **copy-paste clone** | **8.4 %** | 8.0 % | 21.6 % |
| SATD (TODO/FIXME/HACK) | 4.4 % | 3.2 % | 11.4 % |
| cross-area coupling | 0.47 % | 0.14 % | 2.9 % |
| god-header | 0.21 % | 0 % | 0.57 % |
| dependency cycle | 0.11 % | 0 % | 0.20 % |

To our knowledge this is the first per-commit architectural-drift census of C++ OSS at
this granularity. Read it as: structural catastrophes (cycles, god-headers, cross-module
coupling) are **rare** per commit (tenths of a percent); the everyday signals are
**complexity-ratcheting** and **copy-paste**. Two of these rates do **not** mean what the
name suggests — `test_coevo` is a soft "no test touched" heuristic with ~50 % false
alarms, and a complexity-ratchet fire is usually *inherent* complexity, not avoidable
debt (both quantified in §5). The rates are **stable**: growing the run from 231 k →
337 k → 484 500 ok commits moved every figure ≤2 pp and changed no rank.

---

## 3. Agents vs humans — the core result

15.0 % of analyzed commits are AI-attributed (72 778 agent / 408 716 human / 3 006 non-AI
automation), classified by bot author, `Co-authored-by` trailer, or AI committer — never
by commit-body prose (that leaked: UI "cursor", domain "codexo.de"). 241 repos carry ≥20
of *both* classes; the paired tests below run over the 344 repos with ≥5 of each per
size-band.

**Why within-repo.** A naive pooled agent-vs-human comparison is confounded: agent-heavy
repos may simply be younger / less-reviewed, so *both* their agent and human commits drift
more. We have burned on this before (an "agentic velocity" effect died under repo fixed
effects, #115). So the only comparison we trust is **paired within the same repo and the
same commit-size band.**

### The complexity claim, in three levels of rigor

"Agents add more complexity" — true or not? It depends entirely on what you control for:

| level of control | complexity result | reading |
|---|---|---|
| **1. Pooled** (no control) | agents **1.76×** (27.0 % vs 15.4 %, non-bulk) | the seductive headline |
| **2. Within-repo, raw per commit** | Δ = **+1.55 pp**, 135 vs 102 repos, **p = 0.037** | agents *do* ratchet a bit more per commit |
| **3. Within-repo + size-matched** | Δ = **+0.06 pp**, 172 vs 157, **p = 0.44** | **null** — identical per equal change |

The level-2 effect is real but small, and it is **fully explained by commit size**:
agent commits are larger (within-repo median ratio **1.26×**, 140 vs 96 repos, p = 0.005;
corpus median 50 vs 21 LOC), and complexity-ratchet rises mechanically with commit size.
Hold size fixed (level 3) and the difference vanishes. So: *agents do not write more
complexity-prone code per change — they ship more change per commit.* (The level-2 result
is robust to the inclusion threshold — it holds at ≥5/band too, +2.29 pp, p = 0.0025 — but
see the multiple-testing note below: it is the one finding that would not survive
correction, which only strengthens the headline.)

### All categories, within-repo + size-matched (344 repos)

| category | median Δ (pp) | repos A>H / A<H | ties (Δ=0) | sign-test p | verdict |
|---|---|---|---|---|---|
| complexity | +0.06 | 172 / 157 | 15 | 0.44 | **null** |
| newclone (copy-paste) | −0.23 | 130 / 178 | 36 | 0.0073 | agents better |
| satd | −0.52 | 85 / 191 | 68 | 1.6·10⁻¹⁰ | agents better |
| test_coevo | −1.63 | 121 / 206 | 17 | 3·10⁻⁶ | agents better |
| cross_area | +0.00 | 60 / 104 | 180 | 0.0007 | no disadvantage |
| grown_cycles | +0.00 | 8 / 30 | 306 | 0.0005 | no disadvantage |
| god_headers | +0.00 | 28 / 53 | 263 | 0.0073 | no disadvantage |

**No category in which agent commits drift worse than human commits of equal size in the
same repo.** Two honest qualifiers:

- *The three structural rows are near-degenerate.* The median repo shows **zero**
  difference (180–306 of 344 repos are ties); the sign test reflects a small minority of
  repos in which, when there is any difference, slightly more favor agents. Read these as
  "no agent disadvantage," **not** a population-wide "agents are cleaner."
- *Multiple testing.* Seven categories are tested. The four "agents better/no-disadvantage"
  results all survive a 7-way Bonferroni cut (α = 0.0071); the one *worse-direction*
  signal — level-2 raw complexity, p = 0.037 — does **not**. The single result pointing
  against the thesis is the fragile one. That cuts in the thesis's favor, but we flag it
  rather than hide it.

The two robust, high-precision "agents cleaner" signals are **SATD** (real markers,
Δ = −0.52 pp, p = 1.6·10⁻¹⁰) and **copy-paste** (Δ = −0.23 pp, p = 0.0073). The largest
magnitude, **test_coevo** (−1.63 pp), rests on the lowest-precision category (§5) and is
reported with that caveat. This is **consistent with** an earlier before/after
observational study on this corpus (n = 61 repos, agent-adoption window) that found
structural debt did not rise relative to a human control while commit volume/size did —
two observational angles, same direction. Neither establishes causation (§8).

**The agent difference is commit size, not a different kind of drift.** That is the whole
finding, and it is what makes per-PR CI gating the right control: the gate measures the
regression per change and does not care who — or what — wrote it.

---

## 4. Large mature codebases — and why "mature" isn't the variable

Four heavily-reviewed flagship repos run with a generous timeout (curl, bitcoin, scylladb,
llama.cpp — 11 694 commits, 8 timeouts). The aggregate sits below the population, **but the
average hides two regimes:**

| repo | median LOC | clone % | complexity % | satd % | test-coevo % |
|---|---|---|---|---|---|
| bitcoin | 14 | 2.4 | **7.7** | 1.6 | 6.5 |
| curl | 12 | 5.1 | 15.6 | 0.7 | 15.7 |
| scylladb | 18 | 3.6 | 12.7 | 3.4 | 10.9 |
| **llama.cpp** | 34 | **16.8** | **31.4** | **12.2** | **29.4** |
| *(population)* | *24* | *8.4* | *17.1* | *4.4* | *22.1* |

bitcoin is the discipline exemplar — 14-line commits, complexity-ratchet 7.7 % (half the
population), zero cycles. curl and scylla also sit clearly below. But **llama.cpp drifts
above the population on every axis** — a young, hyper-active ML flagship behaving like the
corpus's messy tail. The variable that holds the line is **review culture + age, not size
or fame.** "Big mature repo" alone guarantees nothing.

### Copy-paste, seen side by side

The clone detector makes the regimes concrete (manually verified true positives):

- **bitcoin** — clones are ≤14 lines and confined to the periphery: a benchmark scaffold
  (`bench/peer_eviction.cpp`), a Qt-6.10 guard copied between two slots
  (`qt/transactionfilterproxy.cpp`), and 13 lines of IPC deserialization shared across two
  template headers (`mp/type-optional.h` ↔ `type-pointer.h`). None in consensus code.
- **llama.cpp** — clones are 96–131 lines in the **core engine**: every new model in
  `src/models/*.cpp` is ~100 lines copied from a sibling (arcee ↔ cohere2-iswa,
  pangu ↔ qwen2, paddleocr ↔ qwen2vl, eurobert ↔ olmo ↔ modern-bert); plus duplicated
  op-dispatch blocks in the Metal (131 L) and Vulkan (88 L) backends.

Honest note: llama's per-model duplication is partly a **deliberate** choice — each model
diverges, and the project prefers file-per-model over a mega-template. The detector flags
the 95 %-identical structure correctly; whether to deduplicate is a human judgment.

---

## 5. Precision — a hand audit from the author's chair

We re-examined flagged commits **from the perspective of the engineer who wrote the code**,
deliberately *not* assuming the checker is right. Each finding is graded **TP** (a real,
avoidable problem the author would fix), **JUSTIFIED** (the finding is technically correct —
it *is* complex / duplicated — but a competent author reasonably keeps it), or **FP** (the
checker is wrong or the finding is meaningless). Samples are ~12–18 per category, so these
are FP/signal *classes*, not precise rates.

### Complexity — almost never an "avoidable defect" signal

Of 12 audited fires: **0 TP · 9 JUSTIFIED · 3 FP.** Not one was complexity a competent
author would refactor. The JUSTIFIED fires are **inherent** complexity:

- protocol/format state machines & parsers (libspdm SPDM handshake CCN 95; an OvS
  flow-rule tokenizer; an ADIOS2 key-source precedence cascade),
- idiomatic resource-acquire/cleanup ladders (an iPXE PCI `probe` with mirrored
  `goto err_X` unwind),
- platform/async code (a SimpleBLE WinRT connect path),
- compile-time variant dispatch (a PETSc `if constexpr` 32/64-bit matrix builder),
- and **vendored third-party** code (rmm's bundled single-header `rapidcsv.h` CSV parser —
  not the project's to refactor at all).

The 3 FPs are **count artifacts**: a RISC-V decoder whose real CCN is ~6 but is inflated by
`#if` guards; flat Qt slots inflated by trivial `?:` flag-selection ternaries; and an
`encore` daemon loop reported at CCN 64 that a hand-count puts near ~22, where the
committed guard line carries an unbalanced-paren `[[unlikely]]` attribute the
preprocessor-only token scanner mishandles. (We tested whether encore was instead a
memory-backend baseline artifact and it is **not**: memory and disk backends agree exactly,
45→64 both, so the over-count is in the complexity *metric*, not the diff reconstruction —
same class as the other two.) **Takeaway: a complexity fire reliably means "this function
is genuinely complex," but rarely "you have avoidable debt here."**

### Copy-paste — the more actionable signal, but ~half is by design

Of 13 audited fires: **4 TP · 7 JUSTIFIED · 2 FP.** The TPs are gold — duplication the
author themselves would factor out:

- **GpgFrontend** pasted a verbatim 30-line `WaitForMultipleOperas` into a commit titled
  *"reduce code duplication"*;
- **nscp** created a whole new CLI file by copying the sibling module wholesale
  (PythonScript → LUAScript), diverging only by a path token;
- **tomu** copied UNIX-socket infrastructure into three client variants when a `shared/`
  dir already exists;
- **mori** deleted a shared RDMA helper and re-duplicated its ~70-line posting core into
  two atomic kernels.

The JUSTIFIED clones are intentional: per-dtype FP16/FP32 bodies under `#ifdef`
(nntrainer), per-syscall wrappers (nolibc), golden-test fixtures that must own their input
(mrdocs), Qt-6.10 deprecation guards (simdissdk), per-operator dispatch wrappers (warpx),
per-output-variable triads (SOILWAT2). The 2 FPs are **artifacts**: a generated single-file
amalgamation (`epiworld.hpp`) matched against its own source; and a staged code **move**
(`*__moved` corpses) counted as new duplication.

### New / under-reported FP & artifact classes (and what #129 fixed)

| class | category | status after #129 |
|---|---|---|
| **generated** (SWIG `*_wrap.cpp`, `*.pb.h`, moc/ui/qrc, lex/yacc) | clone, complexity, graph | **fixed** — `scan::isGeneratedPath`, one shared definition |
| **vendored third-party** (`third_party/`/`vendor/` dirs, single-header libs) | clone, complexity, graph | **fixed** for dir/basename via the unified gate; a foreign-licensed file inside a self-licensed repo (rmm `rapidcsv.h`) still flags → needs copyright-mismatch (#127) |
| **clone over-exclusion on self-licensed repos** | clone | **fixed** — dominant-banner guard (see below); was a silent recall hole |
| **sub-bulk generated/vendored dumps** (1k–10k lines, under the bulk cut) | clone, complexity | partly — now path/marker-filtered by #129; residual is foreign-licensed files (#127) |
| **staged code-moves** (`*__moved`, migrations) | clone | open — compare against deletions |
| **compile-time variants** (`#ifdef`, `if constexpr`) | clone | by-design idiom (won't-fix) |
| **token-scanner over-count** (`#if`/`?:`/`[[attr]]`) | complexity | open — needs parse-aware CCN |

The **sub-bulk** hole: in the 1k–10k added-line band (just under the bulk-skip threshold)
clone fired at **40–47 %** and complexity at **~50 %** (vs 8.4 % / 17.1 % population) in the
pre-#129 run, supplying **16.6 %** of all clone and **10.0 %** of all complexity fires — much
of it generated/vendored dumps. #129 now path/marker-filters generated and vendored code at
the source, removing most of that noise; the residual is foreign-licensed single files (#127).

**A recall hole the refactor exposed.** Before #129 the clone detector's license-banner
filter was *unguarded*: any project that puts a full license banner in every file — routine
for Apache/MIT/BSD codebases — had **all** its files excluded and reported **zero** clones.
So the pre-#129 copy-paste rate (8.4 %) *under-counts* — self-licensed repos contributed
nothing. The #129 dominant-banner guard (if >50 % of files carry a banner it is the project's
own license, not a vendor signal) closes this: gvsoc-core, for instance, went 0 → 43 real
authored clones in its trace subsystem. **8.4 % is therefore a floor**; the true population
copy-paste rate is modestly higher, concentrated in self-licensed projects.

### Does this bias the agent-vs-human result? (the corrected-scanner re-run)

We tested this directly instead of assuming it. The #129 binary was re-run over the full
agent+human corpus — **128 393 commits across the 226 repos with ≥20 of each class** — and
compared to the old scanner on the *same* commits, with the *same* within-repo size-matched
sign test (§3). The method was first validated by reproducing §3's table on the old data
exactly (complexity p = 0.44, SATD p = 1.6·10⁻¹⁰, … to the digit).

- **The scanner change is small and not class-biased.** It altered the count on ~1.5 % of
  commits: copy-paste **+1 424** (the recall fix — banners no longer hide self-licensed
  clones), complexity **−201** and include-edges **−55** (generated/vendored now excluded at
  source). The complexity/edge fixes hit agent and human commits about equally (0.26 % vs
  0.20 % of each class). The copy-paste recall fix surfaced **more *human* clones than agent**
  (1.41 % of human commits vs 0.84 % of agent) — it exposes human copy-paste that
  self-licensing was hiding, the *opposite* of inflating an agent disadvantage.
- **The within-repo verdict is unchanged in 6 of 7 categories** (complexity null on both;
  SATD and test-coevolution agents-cleaner on both, identical p; cross_area / grown_cycles /
  god_headers unchanged). The one move: copy-paste went from marginal (p = 0.098) to
  borderline (p = 0.049), **in the agents-cleaner direction** (median −0.22 → −0.64 pp). It
  is a fragile p that would not survive the §3 Bonferroni cut (α = 0.0071), so read it as
  "the recall fix firms up the pre-existing agents-better-on-copy-paste signal," not a new
  result.

So the headline survives the precision fix intact: **no scanner-FP class flips any category
toward an agent disadvantage; the only verdict that moved, moved in agents' favor.** The
residual untested-symmetry caveat is now narrow — `test_coevo`'s FP class (refactors/chores)
could still be differentially distributed by author class, the main caveat on that one row;
SATD and copy-paste do not depend on it.

---

## 6. Conclusions

1. **A converged per-commit drift baseline for C++ OSS exists** (§2) — copy-paste in ~8 % of
   commits, complexity-ratchet in ~17 %, structural breakage in tenths of a percent.
   Authority-grounded, reproducible, citable — *with* the §5 caveats on what each fire means.
2. **C++ structure absorbs the AI volume surge.** Repo- and size-controlled, AI-attributed
   commits are not architecturally worse than human commits anywhere; on SATD (robustly) and
   copy-paste they are cleaner, and on test co-evolution they look cleaner on a noisy signal.
   The agent difference is **throughput** (≈1.3× larger commits), not a new failure mode.
   This is the counter-narrative to "AI slop" — and it survives the controls (repo, size,
   multiple-testing) that kill the naive pooled story, plus a full re-run on the corrected
   (#129) scanner that left every verdict intact (§5).
3. **Maturity ≠ discipline; review culture is the variable** (§4) — bitcoin vs llama.cpp
   differ 4× in complexity-ratchet despite both being flagship C++.
4. **The tool's actionable core is narrow and honest** (§5): copy-paste flags real
   "factor-this-out" duplication ~⅓ of the time (and those cases are genuinely valuable);
   complexity reliably finds complex functions but those are mostly *inherent*, not
   avoidable; the graph categories are rare and carry export-header / reorg / template FP
   classes. Trust it where it is precise, and filter vendored/generated code first.

## 7. How to use it

- Run `archcheck --diff` **per PR in CI**. It is author-agnostic by construction — the right
  control precisely *because* the AI difference is volume.
- **Vendored/generated filtering is now built in** (#129): one shared `AuthoredScope` gate
  excludes `third_party/`/`vendor/` dirs, single-header libs, generated files (`*.pb.h`,
  `*_wrap.cpp`, moc/ui/qrc, lex/yacc), and license-banner files (dominant-banner-guarded so
  self-licensed projects are not over-excluded). Still recommended: treat sub-bulk
  (1k–10k-line) commits like bulk, and expect a foreign-licensed single file to slip through
  until copyright-mismatch (#127) lands.
- **Gate** on the precise signals: **copy-paste** (post-filter) and **genuinely-new
  dependency cycles** (exempt the `.h` ↔ `.tmpl.h` template-split idiom).
- **Advisory only:** complexity (mostly inherent — a heads-up, not a blocker), god-header
  (fan-in-only proxy; allowlist known wide headers), cross-area, test-coevolution.
- Adopt with `--baseline` so legacy debt doesn't block; the gate then catches only the
  *delta* a PR adds — the slow drift, which is what accumulates.

## 8. Limitations

- **"Human" = "no AI marker."** IDE autocomplete and stripped trailers are invisible in git,
  so the human class is contaminated with unmarked AI. This biases toward the null, making
  the "agents cleaner" findings conservative but potentially masking a pervasive effect on
  the null categories.
- **FP symmetry across author classes is assumed, not measured** (§5) — the main caveat on
  the test-coevolution result.
- **Coverage skews to faster repos.** 33 582 commits were skipped in repos flagged too slow
  (plus 790 per-commit timeouts); the baseline is the "fast/light" half of the corpus. The
  four mature exemplars (§4) partly offset this but are illustrative, not a population.
- **Complexity is a token-scanner metric, not a parse-aware one** — it over-counts on
  `#if` guards, `?:` chains, and `[[attr]]` syntax (§5). It is deterministic across
  backends (memory == disk, verified), so this is a metric-precision limit, not a bug.
- **Per-commit, not causal.** Observational; "agents" is an attribution label, not a
  randomized treatment.
- **Reproducibility.** `experiments/per_commit/{drift_rates,authorship_join}.py` +
  `results_full.jsonl` / `results_tasty.jsonl` / `authorship.tsv` regenerate every number here.
