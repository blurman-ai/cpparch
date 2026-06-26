# [BUG][SCAN] Clone detector eats ~2GB on an embedded-data file (no size cap on tokenization)

**Created:** 2026-06-26
**Started:** 2026-06-26
**Status:** wip
**Module:** SCAN / DUPLICATION
**Priority:** major
**Related:** #127 (vendored/generated exclusion), #052/#056 (clone detector), #072 (detector port)

## Symptom (dogfooding on the corpus)

Corpus run `archcheck --diff` over `leejet/stable-diffusion.cpp`: **every** process
eats 1.0–1.9 GB RSS. 8 workers across commits of this repo at once → ~10 GB → **swap to zero**,
the machine on the edge of thrashing. Caught by the user's eyes: "archcheck is eating a ton of memory".

## Root cause

The repo has 6 files `src/tokenizers/vocab/*.hpp` of size **20–43 MB** — these are embedded
vocab/merges tokenizer data (BPE tables for gemma/umt5/gpt-oss), baked in as
`static const unsigned char ...[] = { 0x7b,0x22,... }`. They only pretend to be code.

1. They sit in `src/…/*.hpp` → the vendored/generated filter (#127) does NOT catch them (no
   `vendor/`/`third_party/` markers, the extension is "source-like").
2. 20–43 MB < **64 MiB** read-cap (`git_object_file_source.cpp:29`, `disk_file_source.cpp:12`)
   → the file is read in full.
3. `scanForDuplication` → `phase1TokenizeAndExtract` tokenizes it: 43MB → ~10M+ tokens
   → token-seq + rawSeq + k-gram fingerprint index (`fragmenter.cpp:107`, `clone_index.cpp:122`)
   = **~1.9 GB per process**.

This is both a bug (missing guard) and an optimization: the 64MB read-cap is fine for reading, but absurdly
high for tokenization under clone detection — a hand-written source is ~never >1MB.

## Fix (done, 2026-06-26)

The cap was placed at a SINGLE checkpoint — `lex()` (`token_normalizer.cpp:394`), and NOT in the duplication
scan: a skeptic check showed that complexity (`local_complexity_metrics.cpp:377`) and
flag-arg (`diff_command.cpp:173`) ALSO go through `duplication::lex(source)`. A cap only in
duplication gave 3.6GB (the other two were eating it). `lex` returns empty when `source.size() >
kMaxLexBytes` (1 MiB, `token_normalizer.h`) → all three lex scans skip huge files at once.
The graph scan does not go through lex (preprocessor include-scan) — not affected.

**RSS measurement (a single `--diff` on stable-diffusion.cpp):**

| | peak RSS | wall |
|---|---|---|
| before | ~3.6 GB | 21 s |
| after | **1.0 GB** | **4.4 s** |

3.6× on memory, 5× on time. For 8 workers: ~8GB instead of ~29GB → no swap death.

## Done
- [x] Cause localized (6 vocab `.hpp` 20–43MB, k-gram index + token vectors).
- [x] Skeptic: cap only in duplication = 3.6GB (complexity+flag-arg also go through lex). Moved into `lex()`.
- [x] Cap `kMaxLexBytes=1MiB` in `lex()` — single checkpoint for all lex scans.
- [x] Regression test (`duplication_token_normalizer_test.cpp`, #147): >1MB → empty, just under → tokenized.
- [x] RSS measurement before/after: 3.6GB→1.0GB, 21s→4.4s. All 572 tests green, dogfood 0.

## Architectural fix (2026-06-26) — generated-data exclusion stage (Linguist-style, #127)

After research (Linguist/SonarQube/PMD/clang-tidy: all EXCLUDE generated/data up-front, don't
process) a content heuristic `hasMinifiedContent` (`file_classification.h`) was added and wired
into `AuthoredScope` (excluded + fromFiles): **average line length > 110** (the Linguist threshold),
computed on a 64KB prefix (we don't walk the 43MB), floor 4KB. umt5.hpp = a single line of 2.5M characters
→ caught instantly and dropped from ALL scans (graph/bool/clone/complexity), not just from lex.

**Measurement (same commit): 3.6GB→731MB (4.9×), 21s→2.75s (7.6×).** Better than lex-cap-only (1.0GB),
because the file now doesn't even reach the graph/bool scans. Dogfood 0 (our code not affected), 573/573 tests.

- [x] generated-data detector (minified avg-line>110) → exclusion from all scans. Test + fixture.
- [ ] Residual ~730MB = the snapshot still READS the content before classification. Prefix/streaming-read
      of excluded files — a separate SourceSnapshot refactor (not a blocker).
- [ ] (opt.) `.gitattributes linguist-generated`/`-vendored` — a free signal from the repo, read it.
- [ ] bool_field (char-scan on changed-only) — lightweight, but now it too skips excluded files.
