# Clone detection: archcheck vs the field — consolidated comparison

_Article-ready synthesis of how archcheck's `--duplication` signal compares to
established clone detectors. Pulls together three prior efforts and adds one new
data point:_

- _C++ corpus head-to-head vs NiCad 6.2 — [nicad_vs_archcheck.md](nicad_vs_archcheck.md),
  [nicad_review.md](nicad_review.md), [nicad_metrics.csv](nicad_metrics.csv) (8 repos, June 2026)._
- _**New: pure-C head-to-head vs NiCad 3.5 on `monit-4.2`** (June 2026) — NiCad's home
  turf, where its C grammar parses 100 % of the code. Raw data + join script:
  [../experiments/clone_oracle_validation/NICAD_QUANT.md](../experiments/clone_oracle_validation/NICAD_QUANT.md)._
- _Tool landscape (PMD CPD, jscpd, SonarQube, Simian, Duplo, CCFinderX, Deckard) —
  [../docs/research/clone_tools_landscape.md](../docs/research/clone_tools_landscape.md)._

---

## TL;DR — are we better or worse than NiCad?

**For the job archcheck actually does (a per-commit CI gate on C++), decisively
better — and the comparison is not even about recall.** Two independent runs:

1. **On a C++ corpus (8 repos), NiCad is ~1 % useful signal and cannot gate.** Its C
   grammar fails on 38–94 % of files, so it never sees the authored C++; what it does
   report is ~97 % duplication *inside vendored C dependencies* (it ships no default
   vendored exclusion). archcheck finds **2409 authored C++ clones NiCad misses
   entirely**, at **~40× less wall-clock**, as a single static binary.
2. **On pure C (`monit-4.2`), NiCad's home turf, the two AGREE on every real clone.**
   Where they diverge, it is NiCad over-reporting *benign* clones (platform-twin
   file cliques, read/write sibling functions) that archcheck suppresses **by
   design** — i.e. NiCad's false-positive surface, not an archcheck recall hole. And
   archcheck additionally catches small same-file copy-paste that is **below NiCad's
   10-line floor**.

So NiCad's apparent "higher recall" is its FP problem: it enumerates everything,
including the benign clones a developer should *not* refactor. archcheck is
precision-first and CI-capable; NiCad is a batch research tool with no CI
integration. **The honest framing (per [#107](../backlog/completed/107_maj_dup_external_oracle_cross_validation.md)):
NiCad is not ground truth.** Counting "NiCad clones we didn't reproduce" as misses
is the wrong unit — most of them are benign by Kapser & Godfrey, which our guards
intentionally drop.

---

## 1. The C++ corpus run (where NiCad can't parse)

Full data in [nicad_vs_archcheck.md](nicad_vs_archcheck.md). Headline:

| | archcheck | NiCad 6.2 |
|---|---:|---:|
| pairs / classes reported (8 repos) | 2414 pairs | 802 classes |
| found by **both** | 5 | 5 |
| **unique** authored C++ clones | **2409** | 19 (~6–9 useful) |
| unique vendored/generated noise | ~0 (excluded) | 780 |
| wall-clock, whole corpus | **≈44 s** | ≈1700 s |
| parse-failure rate | 0 | 38–94 % of files |
| packaging | single static binary | TXL + C-extension farm |

Overlap is essentially nil because the two tools see different code: archcheck =
authored C++, NiCad = the vendored C it can still parse. Every one of archcheck's
high-value finds (EXACT cross-file copy-paste: Kartend `mainwindow_dialogs ↔
mainwindow_setup`, BambuStudio `GLGizmoAdvancedCut ↔ GLGizmoColorCut`, AetherSDR
`ClientEqEditor ↔ StripEqPanel`) was invisible to NiCad — the file failed its C
grammar and never reached the detector.

**Caveat this run does NOT settle:** whether archcheck's selective-normalization
precision beats a *production C++-aware* tool (PMD-CPD / jscpd with
`--ignore-literals`). NiCad was not a viable C++ comparator. That bake-off is the
open item from [clone_tools_landscape §4](../docs/research/clone_tools_landscape.md).

## 2. The pure-C run on monit-4.2 (NiCad's home turf) — NEW

The corpus run invites the objection: *"you only win because NiCad can't parse C++;
on C it would crush you."* This run answers it. `monit-4.2` is pure C; NiCad parses
100 % of it. TXL was installed (the [#107](../backlog/completed/107_maj_dup_external_oracle_cross_validation.md)
"TXL missing" blocker was false — it was on disk, off `PATH`); NiCad 3.5 built after
two environment fixes (version-guard `10.[56]`→`10.[5-9]`, tools `-m32`→`-m64`).

NiCad: **27 clone pairs / 12 classes** (near-miss type-3, threshold 0.3, minsize 10).
archcheck: **21 pairs**. NiCad runs on line-preserving `.ifdefed` copies (verified: 0
line-count drift vs raw), so a `file:line` overlap-join is exact.

**Class-level coverage: archcheck reproduces 8 of NiCad's 12 clone classes.** Naive
edge-level recall (12/27 = 0.44) is an artifact of representation, not quality:
NiCad enumerates the **full clique** of a clone class (a 5-file twin set → 10 edges),
archcheck reports a **near-spanning star** (one hub file → others). Counting edges
penalizes the compact representation.

Triage of the 4 classes archcheck does not cover (framework from
[#071](../backlog/completed/071_fp_classification_rules.md): a = intentional benign
suppression, b = documented boundary, c = real recall bug):

| class | what | NiCad sim | verdict | category |
|---|---|---:|---|---|
| 12 | `device/sysdep_{DARWIN,FREEBSD,OPENBSD}.c` 65-80, byte-identical | 100 % | the 3 files are 95–98 % identical → **whole-file twins**; archcheck's P0.2 drops all pairs among them (this *is* the "3 whole-file groups suppressed" in its log) | **(a) by design** |
| 11 (edges) | same device twins, 90-111 block | 93 % | same whole-file suppression; the LINUX/HPUX edges (files only 81–82 % similar) survive and *are* reported | **(a) by design** |
| 1 | `process/` get_process_info, 51 lines | 70 % | ~60–70 % similar → below archcheck's 0.75 weighted floor + P0.6 line floor | **(b) threshold** |
| 5, 8 | `socket_read`/`socket_write`, `sock_read`/`sock_write` | 70–75 % | **different sibling functions** (read has timeout logic, write doesn't), not copy-paste — benign per Kapser & Godfrey | **(b) benign** |

**Zero category-(c) recall bugs.** Every divergence is either intentional benign
suppression or a deliberate precision/recall threshold choice.

Conversely, archcheck reports **9 pairs NiCad misses — all true clones, zero false
positives**: small same-file copy-paste below NiCad's `minsize=10` (gc.c
`_gcperm/_gcuid/_gcgid` at 6 lines; event.c `start`→`restart` at 6 lines) and a
renamed loop NiCad's near-miss config skipped (http/processor.c `HttpHeader`→
`HttpParameter`, 13 lines). On small same-file copy-paste, **archcheck's recall is
higher.**

**Net on pure C:** the two tools agree on every cross-file clone above ~80 %
similarity that isn't a whole-file twin. The divergence is fully explained by (1)
clique-vs-star edge enumeration, (2) archcheck's intentional whole-file suppression,
(3) archcheck's stricter floor (0.75 + P0.6) vs NiCad's 0.70, (4) NiCad's 10-line
floor hiding small copy-paste archcheck catches. No correctness gap surfaced — this
is independent cross-validation, not self-confirmation.

## 3. The broader field (other tools)

From [clone_tools_landscape.md](../docs/research/clone_tools_landscape.md). The key
insight: **the detector is essentially the same everywhere** (token-based
Karp-Rabin / Rabin-Karp class); comparing *detectors* is pointless. What differs is
the *product*.

| tool | technique | output | runtime / packaging | CI gate | what it is |
|---|---|---|---|---|---|
| **PMD CPD** | tokens, Karp-Rabin | block list | JVM | exit≠0 by `--minimum-tokens` | standalone clone detector |
| **jscpd** | tokens, Rabin-Karp | list + % | Node | exit≠0 by `--threshold %` | standalone clone detector |
| **SonarQube** | own `sonar-cpd` | density %, dashboard | server | quality gate by density % on new code | quality platform |
| **Simian** | line/text pattern match | block list | JVM | line threshold | proprietary clone detector |
| **Duplo** | line-based | block list | standalone C++ (abandoned) | min line length | clone detector |
| **CCFinderX** | suffix-tree tokens | clones + genealogies | research (100 MLOC) | — | research tool |
| **NiCad** | text + norm + LCS | clone classes | research, needs TXL | — | research tool (**precision gold-standard, 89–96 % type-3**) |
| **Deckard** | AST subtrees | clones | research (1 MLOC) | — | research tool |
| **archcheck** | tokens, rare-index + token-LCS | **`file:line ↔ file:line` pairs + clone type + "extract to shared"** | **single static binary** | exit≠0 on **new confirmed pair** + baseline | **architecture-invariant gate** (dup = one sensor beside cycles / SF.* / god-headers / levelization) |

What archcheck genuinely differs on (descending importance):

1. **It is not a clone tool — it is an architecture gate.** Duplication is one signal
   in one binary alongside cycles, levelization, SF.7/8/9/21, god-headers. None of
   the others have this; they only do clones (or, like Sonar, all-metrics-on-a-server).
2. **Single static binary, no JVM/Node/server.** "download and run" in CI. The only
   other standalone-C++ option (Duplo) is line-based and dead. This is also *why*
   archcheck ships its own detector — there is nothing to wrap under a "single
   binary, libclang-only runtime" contract.
3. **Output is concrete pairs + what to fix**, not a percentage. Sonar gates on
   density %; archcheck gates on a specific new pair with "extract to a shared
   header" (Lakos *missing reuse edge*).
4. **Selective normalization for C++ idioms** (keeps call names and case labels).
   PMD/Sonar normalize globally ("drop all identifiers"), which on C++ makes
   idiom-FP *worse* — every `switch` skeleton and `var = call(args)` collapses to one
   shape. This is the one real detector-level delta, and it only makes sense
   *because archcheck gates*: precision under a gate, not for a dashboard.
5. **Baseline from day one** — legacy frozen, gate catches only new duplication.

Where archcheck does **not** differ (kept honest): the detector itself is an
ordinary token Rabin-Karp class; Type-2 normalization is table stakes; and NiCad's
89–96 % Type-3 precision is the gold standard archcheck does not try to beat (Type-4
and AST cross-TU were dropped on purpose — archcheck is deliberately *less* than a
research tool).

## 4. One-line conclusion

If you strip cycles / SF.* / levelization out of archcheck and keep only
duplication, it is just another CPD and you should wrap jscpd instead. The
difference lives in duplication being **one sensor on an architecture dashboard**,
gated per-commit, in a single binary — which is exactly the thing NiCad (and every
research tool) cannot do, and exactly the thing the JVM/Node/server tools cannot do
as a drop-in CI binary. On detection quality, the two runs above show archcheck is
at parity with the precision gold-standard on its home turf (pure C) and is the only
tool that produces any actionable signal off it (C++).
