# How a fragment is judged copy-paste — the decision path

**Purpose.** A precise, code-level trace of how the duplication detector decides
whether a pair of code fragments is reported as copy-paste — every measure, every
gate, every threshold, in the order they run. Use it to answer "why did *this*
fragment get reported / suppressed?".

This is the **operational** companion to
[docs/duplication_architecture.md](../duplication_architecture.md) (the design
rationale / layer map). When the two disagree, the code wins — every claim here
cites `file:line` at the time of writing (HEAD, 2026-06-29). All paths are under
`src/scan/duplication/` and `include/archcheck/scan/duplication/` unless noted.

The detector is a single **token-based** pass — no AST, no libclang
(decision 2026-06-01). It runs in two surfaces, sharing one core:
- `archcheck --duplication <path>` — snapshot of a whole tree (advisory).
- `archcheck --diff <rev>` — per-commit new-clone gate (`scan/new_clone_drift.cpp`),
  which calls the same core then adds three commit-level filters (§7).

---

## 1. The unit of comparison: a *fragment*

A fragment is one **function/control body** — the token span inside a `{ … }` whose
`{` is preceded by `)` (`fragmenter.cpp:146`). Bodies are emitted only when their
token count is in **[minTokens=30, maxTokens=600]** (`fragmenter.h:28,33`,
`fragmenter.cpp:147`); otherwise the fragmenter descends into nested blocks.

**First consequence — the silent floor:** a function body **< 30 tokens is never a
fragment**, so it can never become a clone candidate. A short copied stub (e.g. a
10-line `setTime()` pasted across files) is invisible *before any similarity is
measured*. This is a fragmentation decision, not a similarity threshold.

Each fragment (`fragmenter.h:13-24`, built in `makeFragment` `fragmenter.cpp:99`):

| field | what | used by |
|---|---|---|
| `seq` | ordered **normalized** tokens | LCS, clone-type, candidate fingerprints |
| `rawSeq` | raw spelling per token (aligned to `seq`) | clone-type (EXACT vs RENAMED) |
| `bag` | multiset of normalized tokens | weighted/plain Jaccard |
| `normLines` | **set** of whitespace-normalized **raw** source lines | `lineOverlap` |
| `diversity` | distinct-trigram ratio of `seq` (low = repetitive) | data-table guard |
| `tokenCount`, `startLine`, `endLine` | size + location | boilerplate guard, reporting |

### Token normalization (selective) — `token_normalizer.cpp:290-303`

The normalized symbol (`seq`/`bag`) is built so that *structure* survives but
*incidental names* collapse:
- **keywords** → kept verbatim (`if`, `while`, `return`, …).
- an identifier **followed by `(`** (a call) or **preceded by `case`** → **kept as
  its name**. This is the load-bearing "selective normalization": callee names and
  case labels carry signal, so they are not erased.
- every other identifier (locals, **type names**, members) → **`id`**.
- string / numeric literals → **`lit`**.

So `int64_t v = 0;` and `uint64_t v = 0;` both normalize to `id id = lit ;` — the
type rename **disappears** in `seq`/`bag` (the types aren't calls). But the *raw
line* `int64_t v = 0;` ≠ `uint64_t v = 0;`, which matters for `normLines` (§2).

---

## 2. The measures (what the numbers mean)

All in `similarity.cpp`. Two fragments `a`, `b`:

- **`weightedJaccard`** (`:11`) — IDF-weighted Jaccard over `bag`:
  `Σ w·min(ca,cb) / Σ w·max(ca,cb)`, `w = idf[token] = log(N/df)`
  (`clone_index.cpp:32`). **This is the default gate score** (`metric="weighted"`).
  Rare shared tokens count more; common ones (`if`, `id`) count less.
- **`plainJaccard`** (`:42`) — same, unweighted (`metric="plain"`, not default).
- **`lineOverlap`** (`:67`) — **Jaccard over the SET of verbatim lines**:
  `inter / (|a.normLines| + |b.normLines| − inter)`, where `inter` = count of
  whitespace-normalized **raw** lines present in both. Reported as `line`.
  - Because `normLines` is a **set** (`fragmenter.h:22`), repeated lines collapse
    (a 240-row `case` table contributes few distinct lines).
  - Because identifiers are **not** normalized here, a *renamed* copy shares few
    identical raw lines even when `seq` matches.
  - Because it is a **ratio over the union**, inserting/deleting lines in a copy
    **inflates the denominator** and drags `line` down — even though the absolute
    `inter` (the verbatim run) is large. **`inter` itself is computed and then
    discarded.**
- **`lcsRatio` / `lcsLength`** (`:104`,`:87`) — Dice over the **token `seq`** LCS
  (gap-tolerant, ordered). Used **only in `precise` mode** (`precise=false` by
  default), so the shipped gate does not use it. Note `lcsLength` operates on the
  token sequence; **it is never run over lines.**
- **`diversity`** (`fragmenter.cpp:15`) — distinct trigrams / (n−2) of `seq`; low ⇒
  skeletal/repetitive.
- **clone type** (`clone_classifier.cpp`) — `seq` differs ⇒ **STRUCTURAL** (Type-3);
  else by `rawSeq` diffs: none ⇒ **EXACT**, `id` only ⇒ **RENAMED**, `lit` only ⇒
  **LITERAL**, both ⇒ **MIXED**. **Informational only — never gates.** It requires
  equal-length `seq`, so a copy with any insert/delete is always STRUCTURAL.

---

## 3. Stage 0 — candidate generation (`clone_index.cpp:196`)

Scoring every fragment pair is O(n²); the index first proposes candidates. A pair
becomes a candidate if **either**:

1. **Shared rare tokens** (`buildRareTokenIndex` + `findCandidatePairs`): they share
   **≥ minSharedRare = 2** tokens whose document frequency **≤ rareDfCap = 4**
   fragments (`clone_index.h:16-18`). "Rare" is corpus-relative.
2. **Shared fingerprint run** (`addFingerprintCandidates`, on by default): a winnowed
   k-gram of the token `seq` matches — i.e. a **verbatim normalized run ≥
   fpK+fpWindow−1 = 12 tokens** (`clone_index.h:28-29`). A fingerprint shared by
   **> fpMaxPostings = 20** fragments is dropped as boilerplate (`:30`,
   `clone_index.cpp:179`).

A pair that is neither (no shared rare token *and* no 12-token verbatim run) is
**never scored** — invisible regardless of how similar it "looks".

---

## 4. Stage 1 — the score gate (`duplication_scanner.cpp:31-57`)

Each candidate gets `weighted`, `plain`, `line` computed. It is kept only if the
**gate score ≥ simThreshold = 0.60** (`duplication_scanner.h:31`). Default gate
score = `weighted`. (In `precise` mode the gate would be `lcsRatio`.)

This is the *recall* gate (bag similarity). The next stage is the *precision* gate.

---

## 5. The filter pipeline (`applyCandidateFilters` `duplication_scanner.cpp:369`)

Run in this exact order. ✂ = drops pairs, ⚖ = only re-weights.

| # | phase | rule | drops |
|---|---|---|---|
| sort | `phase4Sort` | by gate score desc | — |
| P0.5 | `phase5SymmetricPairCanon` | dedupe (a,b)≡(b,a) | ✂ dup reports |
| P0.3 | `phase6CoordinateRevalidation` | both spans must have `startLine ≥ 1`, `end ≥ start` | ✂ phantom ranges |
| P0.1 | `phase7SameFunctionFilter` | **same-file** pairs that **overlap or are immediately adjacent** (`end+1==start`) | ✂ intra-/adjacent idiom |
| **P0.6** | `phase8JointTokenOrderFloor` | keep iff **`weighted ≥ 0.75` AND `line ≥ 0.50`** | ✂ **the precision gate** |
| P0.2 | `phase9WholeFileSuppress` | drop a cross-file pair if **≥80 % of both files' fragments** match across the pair (smaller ≥2) | ✂ move/copy/vendored twins |
| P0.9 | `phasePathBasedFpSuppress` | either file is `isGeneratedPath` (.pb.cc, moc_, flex/bison, SWIG) | ✂ generated |
| P1.1 | `phase10DataTableClassifier` | both `diversity < 0.30` ⇒ `weighted ×= 0.85` | ⚖ only |
| P1.2 | `phase11BoilerplateDensity` | both `tokenCount < 20` ⇒ keep only if `weighted ≥ 0.90` | ✂ tiny low-conf |
| P1.4 | `phase13FileLocalIdfDownweight` | same-file & >½ tokens file-frequent ⇒ `weighted ×= 0.80` | ⚖ only |
| type | `phaseClassifyCloneType` | tag EXACT/…/STRUCTURAL | — |

**P0.6 is where most real edited copies die** (the precision gate), see §6.

**P1.1 and P1.4 only multiply `weighted`** — and they run **after** the joint floor
with **no subsequent threshold re-check**, so they do **not change which pairs
survive** (only the reported `weighted` value). Of the three P1 guards, only **P1.2**
(boilerplate density) actually removes anything. Treat P1.1/P1.4 as cosmetic in the
current ordering. *(P1.2's `tokenCount < 20` is mostly moot too, since fragments are
already ≥ 30 tokens; it can only bite a control-block fragment that slipped under.)*

---

## 6. Worked examples — why a pair passes or fails

### A. Real edited copy that is WRONGLY suppressed — `extract_json_uint64` ⟵ `int64`

Two adjacent functions in one file; the `uint64` body is the `int64` body with the
4-line negative-sign block deleted and `int64_t→uint64_t` / `INT64_MAX→UINT64_MAX`.

- **Stage 0:** share rare `find_json_value_pos` + a >12-token verbatim run ⇒ candidate. ✓
- **Stage 1:** types normalize to `id`, so `bag` is nearly identical ⇒ `weighted` high, ≥ 0.60. ✓
- **P0.1:** same file but spans neither overlap nor are adjacent (blank line between) ⇒ kept. ✓
- **P0.6:** they share ~8 verbatim lines (`inter≈8`), but the deleted block + edited
  lines push the **union** to ~18 ⇒ `line ≈ 0.44 < 0.50` ⇒ **DROPPED.** ✗

The verbatim signal (`inter≈8`) exists but is divided into a ratio and lost. This is
the dominant recall-loss mechanism (corpus #131 Group 3: recall ≈ 37 %).

### B. Look-alike idiom that is CORRECTLY suppressed — `updateStarGeometry` vs `updateMeteorGeometry`

Two functions sharing a framework preamble (`LLVertexBuffer`, `LLStrider`,
`allocateBuffer`, `LL_WARNS`) but every identifier renamed (`mStarsVerts`→`mMeteorVerts`,
`verticesp`→`positions`, `STAR_…`→`METEOR_…`).

- They share *token categories*, so `weighted` can be moderate, but they share almost
  **no identical raw lines** (`inter≈2`) ⇒ `line` low ⇒ dropped at P0.6 (or never a
  candidate if no rare token / 12-token run survives the renames).

**The discriminator between A and B is line-level verbatim identity, not token-bag
similarity** — A keeps names (long `inter`), B renames them (tiny `inter`). The
current gate reads this signal as a **ratio**, which conflates A's "edited copy"
(large `inter`, low ratio) with a short coincidence (small `inter`, low ratio).

### C. Short copy never even considered — a 10-line pasted stub

Body < 30 tokens ⇒ not a fragment (§1) ⇒ no candidate ⇒ no measurement. Suppressed
at the fragmentation floor, upstream of every similarity gate.

---

## 7. Per-commit gate — extra layers (`scan/new_clone_drift.cpp`)

`archcheck --diff` runs the core above on the **new tree**, then:

- **whole-file guard OFF** (`new_clone_drift.cpp:105`) — a commit that adds a file
  copy is exactly the signal we want, so P0.2 is disabled here.
- **added-line overlap** (`touchesAdded`, `:60`) — keep a pair only if one side
  touches a line the commit's diff **added**. A clone whose labelled lines are not in
  *this* commit's hunks is dropped.
- **parent-guard** (`parentPairKeys`, `:47`) — a pair whose normalized-token identity
  already existed in the parent tree is dropped (a pre-existing clone the diff merely
  touched does not re-fire). A genuinely new copy still fires (parent had one instance).
- **byte-cap** (`maxScanBytes`, default `thresholds.diff_max_clone_scan_bytes =
  40 MiB`, `config.h:55`) — if the new tree's authored bytes exceed it, the whole-tree
  clone scan is **skipped** (`skippedLargeTree`, advisory unaffected). Printed only in
  TEXT mode.

---

## 8. All defaults in one place

| option | default | file |
|---|---|---|
| `FragmentOptions.minTokens` / `maxTokens` | 30 / 600 | `fragmenter.h:28,33` |
| `IndexOptions.rareDfCap` / `rareDfPct` | 4 / 0 | `clone_index.h:16-17` |
| `IndexOptions.minSharedRare` / `minDiversity` | 2 / 0 | `clone_index.h:18-19` |
| `IndexOptions.fingerprints` / `fpK` / `fpWindow` / `fpMaxPostings` | true / 5 / 8 / 20 | `clone_index.h:27-30` |
| `ScannerOptions.metric` / `precise` | "weighted" / false | `duplication_scanner.h:29-30` |
| `ScannerOptions.simThreshold` | 0.60 | `:31` |
| `ScannerOptions.enableJointFloor` / `jointWeightedThreshold` / `jointLineThreshold` | true / 0.75 / 0.50 | `:32-34` |
| `ScannerOptions.enableP1Guards` / `enablePathGuards` / `enableWholeFileGuard` | true / true / true | `:35-37` |
| `diff_max_clone_scan_bytes` | 40 MiB | `config.h:55` |

None of these are exposed as CLI flags today; `--duplication` and `--diff` use the
defaults (`preview_commands.cpp:130`, `diff_command.cpp`).

---

## 9. Where signal is currently thrown away (known weak points)

For anyone tuning recall without re-introducing FPs:

1. **`lineOverlap` discards the absolute `inter`** (verbatim-line count), keeping only
   the union ratio — so P0.6 punishes edits (insert/delete) that don't reduce the
   actual copied run (example A).
2. **`lcsLength` is never run over lines** — the gap-tolerant ordered match exists but
   only applies to tokens in `precise` mode.
3. **P1.1 / P1.4 are post-floor re-weights with no re-gate** — currently no effect on
   which pairs survive.
4. **The 30-token fragment floor** is invisible and upstream of everything (example C).

A change targeting (1) — gate on the absolute verbatim-line run (or line-LCS) in
addition to the ratio — is the smallest lever on the example-A class. Validate any
such change with the corpus harness `experiments/corpus_remeasure_131/group3_*.py`,
which reports recall **and** FP-suppression together.
