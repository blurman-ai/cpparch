# #071: FP Classification rules for duplicates

**Created:** 2026-06-03  
**Status:** wip  
**Started:** 2026-06-03  
**Priority:** high  

---

## Goal

Systematize and lock in **objective rules** for distinguishing between:
- **TP (True Positive)** — real copy-paste, needs a refactor
- **FP (False Positive)** — a legitimate pattern, the guards should filter it out

Goal: in future sessions, when analyzing duplicates, apply the rules immediately and not invent FPs where there are none.

---

## Done

- [x] **P0.2 whole-file guard** — file aggregation (≥80% of the smaller file's fragments matched → whole-file clone), counted in `ScanResult.wholeFileClones`, the pairs are removed. Tests green.
- [x] **P0.9 generated guard** — `isGeneratedPath` (`.pb.cc`, `moc_`, `ui_`, `.tab.c`, `lex.yy`, `/generated/`). The only surviving path guard.
- [x] **Duplicate output is structured** (`src/main.cpp`): `(TYPE, weighted, line)`, whole-file counter, re-sorting by weighted, the `Pair::type` field (EXACT/RENAMED/LITERAL/MIXED/STRUCTURAL) via `cloneType()`.
- ❌ **P0.7 (platform) and P0.8 (perf-variant) REMOVED** (2026-06-03): they suppressed real TP. Identical POSIX functions (`OS::sleep`/`truncateFile`, w=1.0) between os_macos/os_linux are extractable copy-paste, and the path guard didn't tell them apart from platform specifics. "identical = report, regardless of path".
- ⚠️ **NOT a guard:** status_bar `addText/Icon/ColorIndicator` — **high-priority TP** (worst-kind: copy + changed one type). Report, don't suppress.
- 📊 **Empirics:** LibreSprite, all 16 commits → 10 unique pairs, **all TP, 0 FP** (confirmed). The corpus "58% FP" is an artifact of the generic detector; our architecture doesn't produce them.
- [ ] Run the triage ([triage_dup_commits.py](../../experiments/triage_dup_commits.py)) on other repos (vmecpp, corpus repos) — check 0-FP at scale
- [ ] ⚠️ The "Rules (Draft)" and "External support" sections below still describe P0.7/P0.8 as FP guards — rewrite to match the facts (removed)
- [ ] Write the rules into `docs/duplication_architecture.md` § **FP Classification Rules**

---

## External support (authority, not opinion)

Our extractability test matches the empirical catalog of
**Kapser & Godfrey, «"Cloning considered harmful" considered harmful»** (Empirical
Software Engineering, 2008, 13(6):645–692) — this removes the "matter of taste" objection
(the project principle "authority over opinion"):

- their **parameterized code** (a copy reducible to a single parameterized function) is
  exactly our **worst-kind TP** (copied a block, changed one token);
- their **platform variations** / **replicate & specialize** were our guards
  P0.7-P0.8 (removed 2026-06-03; now only P0.9 generated and P0.2 whole-file remain);
- their thesis: harm is determined by **context and co-evolution**, not by text similarity —
  exactly why we move severity into a separate signal (see #078, co-change).

A summary of clone types, harm empirics, and detection methods with references:
[../../docs/research/code_clones.md](../../docs/research/code_clones.md).

## Rules (locked in docs/duplication_architecture.md §5.x)

The formalized TP/FP distinction criteria have been moved to the main architecture document: [docs/duplication_architecture.md](../../docs/duplication_architecture.md) §5.x **FP Classification Rules (#071)**.

There you'll also find: the extractability test (the order of application, before the signals), the 4 valid TP signals, the active guards (P0.2, P0.9), the history of removing P0.7/P0.8, and the severity context.

### Historical note: removed guards

**P0.7 (platform-twins)** and **P0.8 (perf-variants)** were path guards tied to the idea "different platforms → different implementations → not duplication". Removed 2026-06-03, because in practice identical code (e.g., `OS::sleep()` is the same on POSIX) can be either genuine platform specificity (different syscalls) or copy-paste of simple helper functions. A guard can't tell them apart — it suppresses both TP and FP. The decision: **identical = report, regardless of path**; then a human reviewer sees the pair and makes the call.

---

## Key decisions

- **"identical = report, regardless of path"** — path-based FP guards are wrong: identical code is extractable = TP, even across platforms. P0.7/P0.8 removed.
- **similarity-score ≠ refactor-priority** — a low weighted (different types dilute the bag-of-words) ≠ FP; such pairs risk being MISSED, not over-reported.
- **worst-kind TP** = copy a block + change one token (status_bar indicator triplet) — high priority, not "medium".
- **Our architecture ≠ a generic detector** — function fragments ≥30 tokens + P0.6 don't produce most of the corpus "FP" (decl headers, coincidental). The 42% precision doesn't apply to us.
- **Generated (P0.9)** — the only honest path-FP; but the markers catch little of the real stuff, a banner-scan of the raw file is needed.

## Changed files (committed — verified 2026-06-11)

- `src/scan/duplication/duplication_scanner.cpp` — P0.2 whole-file (`phase8WholeFileSuppress`, line 336), P0.9 generated (`isGeneratedPath`, line 173); P0.7/P0.8 removed. In master (history: `1329d06`, `be56245`).
- `include/archcheck/scan/duplication/duplication_scanner.h` — `ScanResult.wholeFileClones`, guard options.
- `src/main.cpp` — structured duplicate output (TYPE/weighted/line, counter, sorting).
- `tests/duplication_path_guards_test.cpp` — P0.9 + whole-file cases.
- `experiments/triage_dup_commits.py`, `experiments/dup_triage_libresprite.md` — triage tool + report.
- mem: `fp_classification_rules`.

## In progress / Next steps

1. Run the triage on vmecpp / corpus repos — check 0-FP at scale (awaits go; **research, outside the Haiku scope**).
2. Rewrite the stale "Rules (Draft)" / "External support" sections — plan for Haiku below.
3. (opt.) generated banner-scan; data-table guard (risk of killing the biquad TP) — **out of scope**, P1.3 closed by data in #083 (not implemented until v0.2 semantic).

## Plan for Haiku (2026-06-11) — documentation only

Before starting, MUST read in full: this task, [docs/dev/haiku_task_guide.md](../../docs/dev/haiku_task_guide.md) §2, [docs/duplication_architecture.md](../../docs/duplication_architecture.md) (the whole thing — it's the subsystem context).

**This is a docs-only task. Do NOT touch C++ code, tests, experiments.**

### The main trap (read twice)

The "Rules (Draft)" section of this task is **partially wrong**: items FP-2 (platform) and FP-3 (perf-variants) describe the guards P0.7/P0.8 as active, but they are **REMOVED 2026-06-03** (see "Done" and "Key decisions"). The authoritative semantics is the "Key decisions" section, the main principle: **"identical = report, regardless of path"**. Do NOT carry the Draft into the doc as is.

### Active guards (facts, verified 2026-06-11 from the code)

In `src/scan/duplication/duplication_scanner.cpp` these are alive: P0.2 whole-file (`phase8WholeFileSuppress:336`), P0.9 generated (`isGeneratedPath:173`). P0.7/P0.8 — removed. P1.3 header↔impl — closed by data (#083), not implemented. Before writing the doc, MUST re-verify with grep — if the set of guards has changed, stop and report.

### Step 1 — section in `docs/duplication_architecture.md`

In §5 "Classes of false positives and how we treat them" (line ~182) add a subsection `### 5.x FP Classification Rules (#071)`:

1. **Order of application: the extractability test FIRST** — "can it be extracted into a single function with parameters?" yes → TP, no (different semantics) → FP.
2. **TP signals** — 4 items from the "Rules (Draft) → TP" section (they are correct, can be carried over): identical code in different files (w≥0.95); a full copied block (w=1.0 cross-file); 3+ repetitions in one file; extractable.
3. **Active FP guards** — only those present in the code (see facts above): P0.2, P0.9.
4. **History of P0.7/P0.8** — a paragraph: they were path guards for platform/perf-variants, removed 2026-06-03, the reason — they suppressed real TP (identical POSIX functions between os_macos/os_linux — extractable copy-paste); the "identical = report" principle.
5. **Authority** — a reference to Kapser & Godfrey 2008 and [docs/research/code_clones.md](../../docs/research/code_clones.md) (the wording from the "External support" section, scrubbing the mention of P0.7/P0.8 as our guards).
6. Severity by co-change — a separate signal, see #078 (one line, don't elaborate).

### Step 2 — fix the sections of this task

- "Rules (Draft)" → rename to "Rules (locked in docs/duplication_architecture.md §5.x)", rewrite FP items 2-3 as the removal history, mark FP item 4 (the if-elif chain) honestly: not implemented as a guard, remains a manual triage criterion.
- "External support" — remove "our guards P0.7-P0.9" → "our P0.9 and the removed P0.7/P0.8".

### Definition of done

- `grep -n "FP Classification" docs/duplication_architecture.md` → ≥1 line.
- In the new subsection, the extractability test comes BEFORE the signal lists.
- `grep -n "P0.7\|P0.8" docs/duplication_architecture.md backlog/wip/071_fp_classification_rules.md` — every occurrence only in the context of "removed".
- `grep -cn "uncommitted\|No commits yet" backlog/wip/071_fp_classification_rules.md` → 0.
- `git status` — only `docs/duplication_architecture.md` and this file changed.
- No "outdated" banners — stale content is rewritten, not flagged.

### Escalation (when to stop and hand off to a senior model)

Stop, write here "Blocked: <what/why>" and report if: a grep over the code shows a set of guards different from the one declared above; you found a contradiction between "Key decisions" and docs/duplication_architecture.md that can't be resolved by the "identical = report" principle; it seems you need to change code. Next — Sonnet, then Opus.

---

## Related

- mem:`fp_classification_rules` — I'll update it as the work proceeds


## Result
**Status:** completed — the TP/FP rules are written into `docs/duplication_architecture.md`
§5.x (extractability test first, TP signals, the active guards P0.2/P0.9, the history of
removing P0.7/P0.8), the memory `fp_classification_rules` updated (commit 2217fbc).
The remaining item "triage at scale (vmecpp, corpus)" was moved to
#132 (`backlog/new/132_maj_oracle_quantitative_validation.md`).
Postscript 2026-06-11: the rules were immediately applied in #107 (disagreement triage) —
the extractability test confirmed a TP in our own lexer.
**Completed:** 2026-06-11
