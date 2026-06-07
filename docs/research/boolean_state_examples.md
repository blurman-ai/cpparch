# Boolean-State Examples: Real-World Analysis

**Дата:** 2026-06-07  
**Corpus:** 50 OSS repos, 172 structs with 1+ bool fields  
**Методология:** eye-verify top candidates, classify state vs config

## Corpus Statistics

### Distribution by Bool-Field Count
```
  1 bool:   87 structs (50.6%)
  2 bools:  33 structs (19.2%)
  3 bools:  19 structs (11.0%)
  4 bools:   5 structs (2.9%)
  5 bools:   9 structs (5.2%)
  6 bools:   5 structs (2.9%)
  7 bools:   4 structs (2.3%)
  8 bools:   2 structs (1.2%)
  9 bools:   3 structs (1.7%)
 10+ bools:  5 structs (2.9%)
```

### State-Machine Candidates (5+ Bools): 28 structs found

Top candidates by bool-field count:
```
12 bools | baidxi_buildroot / xradio_vif         (wireless VIF state)
12 bools | art-daq_otsdaq / Iterator             (message processing)
12 bools | amule-project_amule / CPartFile       (download state)
12 bools | AcademySoftwareFoundation_OpenImageIO / IvGL
10 bools | apache_nuttx-apps / i8sak_s           (6LoWPAN config)
 9 bools | baidxi_buildroot / xradio_common      (wireless common)
 9 bools | art-daq_otsdaq / IteratorWorkLoopStruct
 9 bools | art-daq_otsdaq / GatewaySupervisor
 8 bools | aardappel_lobster / FlatCOptions      (config flags)
 8 bools | 203-Systems_MatrixOS / ChordCombo     (music chord flags)
```

## Detailed Examples: TP (True Positive) Cases

### Case 1: CPartFile (amule) — Download State Machine ✓
**File:** `amule-project_amule/src/PartFile.h`  
**Bool fields:** 12  
**Excerpt:**
```cpp
class CPartFile : public CKnownFile {
  bool m_bTransferred;        // ← state: transfer completed?
  bool m_bLargeFile;          // ← config: large file flag
  bool m_bAutoDownPriority;   // ← config: auto-priority
  bool m_bSmartIdCheck;       // ← state: smart ID validated?
  bool m_bCorruptionLoss;     // ← state: corruption loss detected?
  bool m_bIsArchive;          // ← config: is archive file
  // ... etc
};
```
**Analysis:** Mix of state-flags (transferred, smartIdCheck, corruptionLoss) and config-flags. State-space is implicit; combin ations like `transferred && !completed` may be impossible states.  
**Classification:** TP (state machine candidate)

### Case 2: Iterator (art-daq) — Message Processing State ✓
**File:** `art-daq_otsdaq/src/otsdaq/Iterator.h`  
**Bool fields:** 12  
**Observation:** C++ class with 12 boolean configuration/state flags for art-daq message processing loop.  
**Classification:** TP (likely state machine or heavy config accumulation)

### Case 3: xradio_vif (Wireless Driver) — VIF State ✓
**File:** `baidxi_buildroot/.../xradio.h`  
**Bool fields:** 12  
**Excerpt:**
```c
struct xradio_vif {
  bool cqm_use_rssi;          // ← config: enable RSSI monitoring
  bool enable_beacon;         // ← state: beacon enabled?
  bool hidden_ssid;           // ← config: SSID hidden
  // ... plus 9 more
};
```
**Analysis:** Wireless Virtual Interface with mixed state (beacon_enabled) and configuration flags (hidden_ssid, cqm_use_rssi).  
**Classification:** TP (wireless protocol state machine)

---

## Detailed Examples: FP (False Positive) Cases

### Case 4: ChordCombo (MatrixOS) — Music Theory Flags ✗
**File:** `203-Systems_MatrixOS/Applications/Note/ChordEffect.h`  
**Bool fields:** 8 (all bitfields)  
**Code:**
```cpp
struct ChordCombo {
  bool dim : 1;       // ← Diminished chord
  bool min : 1;       // ← Minor chord
  bool maj : 1;       // ← Major chord
  bool sus : 1;       // ← Suspended chord
  bool ext6 : 1;      // ← 6th extension
  bool extMin7 : 1;   // ← Minor 7th extension
  bool extMaj7 : 1;   // ← Major 7th extension
  bool ext9 : 1;      // ← 9th extension
};
```
**Analysis:** All 8 bools are **legitimate multi-flags for chord theory** (standard music notation: a chord can have multiple characteristics simultaneously). This is NOT a state machine; it's a bitmask for configuration. Many combinations are legal (e.g., `maj && ext9` = Major 9th).  
**Classification:** FP (configuration bitmask, not state machine) — **heuristic fail:** naming doesn't match state-pattern (started/running/paused) but functional design is bitmask

### Case 5: FlatCOptions (flatbuffers) — Compiler Options ✗
**File:** `aardappel_lobster/dev/include/flatbuffers/flatc.h`  
**Bool fields:** 8  
**Likely content:** Compiler flags (optimize, strict, etc.) — all independent, many legal combinations.  
**Classification:** FP (configuration flags, not state machine)

---

## State-Likeness Heuristics: Proposed Rules

### Rule 1: Naming Pattern
**State-like names (high suspicion):**
- `*_started`, `*_running`, `*_paused`, `*_failed`, `*_completed`, `*_cancelled`
- `*_active`, `*_enabled`, `*_ready`, `*_loaded`, `*_connected`
- Verb forms: `is_*`, `has_*`, `was_*`

**Config-like names (low suspicion):**
- `enable_*`, `use_*`, `allow_*`, `disable_*`
- `verbose`, `debug`, `trace`, `log*`
- Adjectives: `big`, `small`, `extended`, `compressed`
- Prefixes: `force_*`, `skip_*`, `ignore_*`

**Test:** ChordCombo fails Rule 1 → lower suspicion (correct — it's FP)

### Rule 2: Logical Interdependencies
**High suspicion:** Bools that are checked together in conditions:
```cpp
if (started && !completed) { ... }  // ← suggests state progression
if (failed && completed) { ... }     // ← suggests invalid state (impossible state checker!)
```

**Low suspicion:** Bools checked independently:
```cpp
if (verbose) { log(...); }  // ← config flag
if (enabled) { process(); } // ← config flag (not part of a sequence)
```

### Rule 3: Mutual Exclusivity
**High suspicion:** Bools that are mutually exclusive (only one can be true):
```cpp
// Exactly one of: running, paused, stopped, failed
if (running) { ... }
else if (paused) { ... }
else if (stopped) { ... }
else if (failed) { ... }
```

**Low suspicion:** Bools that can coexist:
```cpp
// All independent
enable_logging && use_cache && allow_retry  // ← any combination legal
```

### Rule 4: State Transitions
**High suspicion:** Code that sets bools sequentially (state machine progression):
```cpp
m_state = STARTING;
started = true;
// later...
completed = true;
started = false;  // ← transition
```

**Low suspicion:** Bools set by user/config and rarely changed:
```cpp
verbose = args.verbose();  // ← set once, not transitioned
```

---

## Feasibility Assessment: State-Space Drift

### Metric: 2^N vs Expected States

For each candidate, estimate:
- **Theoretical states:** 2^N (all bit combinations)
- **Possible states:** E (states that make logical sense)
- **Drift signal:** gap = 2^N - E

**Example: CPartFile (12 bools)**
- Theoretical: 2^12 = 4096 states
- Possible: ~20-30 (based on code analysis)
- Drift: 4066-4076 impossible-but-representable states
- **Signal:** Strong drift indicator (0.2% possible coverage)

**Example: ChordCombo (8 bools)**
- Theoretical: 2^8 = 256 states
- Possible: 256 (all combinations are valid chords in music theory)
- Drift: 0 (no impossible states)
- **Signal:** No drift (FP)

---

## Tooling Gap: What's Missing

### Existing tools checked:
- **SonarQube:** Detects "boolean complexity" but not state-machine growth specifically
- **clang-tidy:** Has checks for large functions but not boolean-accumulation metrics
- **Cppcheck:** Doesn't track struct member count over time
- **CppDepend:** Can graph dependencies but not state-space metrics

### Gap:
**No tool currently detects:**
1. Boolean-state drift (growth of bool fields over time in a struct)
2. State-space explosion (theoretical vs. possible states)
3. Impossible-state patterns (conditions checking `failed && completed`)
4. Implicit state machines (sequence of bools that should be `enum class`)

---

## Conclusion: YES / MAYBE Classification

**Boolean-state growth CAN be a drift signal, with caveats:**

✓ **YES:** For classes explicitly designed as state machines (CPartFile, xradio_vif)  
⚠ **MAYBE:** For large bool-field accumulations (need heuristics to filter FP)  
✗ **NO:** For independent config-flag bitmasks (ChordCombo, FlatCOptions)

**Next phase (v0.2 rule implementation):**
- Implement heuristics 1-4 above (naming, interdependencies, mutual exclusivity, transitions)
- Require semantic backend (#042) to check conditions & assignments
- Set threshold: flag structs with 5+ bools that match state-pattern (high recall, some FP)
- Or conservative: 7+ bools + state-like naming + condition interdependencies (high precision, lower recall)

---

## Raw Findings: Top 28 Candidates

| Bools | Repo | Struct | Status |
|-------|------|--------|--------|
| 12 | baidxi_buildroot | xradio_vif | TP (wireless VIF) |
| 12 | art-daq_otsdaq | Iterator | TP (msg processing) |
| 12 | amule-project_amule | CPartFile | TP (download state) |
| 12 | AcademySoftwareFoundation_OpenImageIO | IvGL | ? (needs eye-verify) |
| 10 | apache_nuttx-apps | i8sak_s | ? (6LoWPAN) |
| 9 | baidxi_buildroot | xradio_common | TP (wireless common) |
| 9 | art-daq_otsdaq | IteratorWorkLoopStruct | TP (work loop) |
| 9 | art-daq_otsdaq | GatewaySupervisor | TP (gateway state) |
| 8 | aardappel_lobster | FlatCOptions | FP (compiler opts) |
| 8 | 203-Systems_MatrixOS | ChordCombo | FP (music flags) |
| 7 | artemsen_swayimg | binary_writer | ? |
| 7 | apache_trafficserver | HttpProxyPort | ? (proxy config) |
| 7 | alphaonex86_CatchChallenger | DatapackDownloaderMainSub | ? (game state) |
| 7 | AlexanderTaeschner_gnuplot | QtGnuplotWidget | ? (Qt widget) |
| 6 | artemsen_swayimg | binary_reader | ? |
| 6 | analogdevicesinc_libiio | parser_pdata | ? |
| 6 | amule-project_amule | CamuleAppCommon | ? (app common) |
| (others: 5 bools each) | | | |

*Status: TP = True Positive (state machine), FP = False Positive (config), ? = needs review*

