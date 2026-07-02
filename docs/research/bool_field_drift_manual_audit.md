# Bool-Field Drift Manual Audit

**Date:** 2026-07-02
**Task:** #161
**Binary:** `archcheck 0.1.5` (Debug, HEAD `4a38bde`)
**Verdict up front:** on a fresh 53-finding stratified sample, the scanner's factual claim
("this named bool field was added to this struct") held in **46/53 = 87%** of findings.
The remaining 13% FP mass is mostly scope filtering (vendored `fmt/`, `testlib/`), plus
three confirmed parser gaps (`const bool`, brace-init `bool x{false};`, and one
unexplained before-image extraction failure). Signal is public-ready as an advisory with
neutral wording; concrete parser fixes are filed as follow-up (#164).

## Commands

```bash
cmake --build build/debug && ./build/debug/tests/archcheck_tests   # 616/616
# Sampling: from experiments/per_commit/results_full.boolrule.jsonl (517,975 ok rows,
# 10,735 with n_bool_field>0 across 787 repos), pick 45 distinct repos (seed=159),
# 1 random finding-commit each, re-run the CURRENT binary:
./build/debug/src/archcheck --diff --diff-mode=memory <sha>^..<sha> <repo>
# -> 53 DRIFT.BOOL_FIELD_ACCRETION findings from 45 repos; each finding then inspected
#    by eye via git show <sha>(-|^:)<file> (before/after struct bodies).
```

Historical context (#134 remains the aggregation source):
`docs/research/boolean_state_perstruct_drift_fullcorpus.md` — 499 structs accreting
bools across ≥4 commits: 335 content-drift, 163 config-bag, 1 churn; top-15 eye-check
was ~11 clean TP.

## Verdict summary (53 findings, 45 repos, fresh 2026-07-02 sample)

| Verdict | Count | Share |
|---|---:|---:|
| TP (domain/lifecycle state genuinely accreted) | 26 | 49% |
| USEFUL_ADVISORY (real bool, config-bag / UI state) | 20 | 38% |
| FP (scanner wrong or out-of-scope file) | 7 | 13% |

Kind distribution: domain-state 25, UI-state 15, config-bag 9, vendored 2, test-support 1.

Two-reviewer control: 4 findings were independently classified twice (blind); verdicts
matched on all 4 (GamePolicy → USEFUL_ADVISORY/config-bag; osci-render → TP;
plus two LCX controls in #162).

## Full verdict table

### Slice 1 (27 findings)

| # | repo | sha | file:line | field(s) | verdict | kind |
|---|---|---|---|---|---|---|
| 1 | antgroup_vsag | 636d509f | src/index/hnsw.h:297 | is_init_memory_ | TP | domain-state |
| 2 | CatchChallenger | b451e37f | tileset-tagger/MainWindow.hpp:61 | derivedFromMaps_ | USEFUL_ADVISORY | UI-state |
| 3 | cpp-atlas | 02f6dea8 | include/mainwindow.h:225 | m_fileTreeOnLeft, m_outputFullHeight | USEFUL_ADVISORY | UI-state |
| 4 | cpp-atlas | 02f6dea8 | ui/FindReplaceDialog.h:52 | m_replaceVisible | USEFUL_ADVISORY | UI-state |
| 5 | btop | 21ca9093 | include/fmt/chrono.h:827 | has_timezone_ | FP | vendored |
| 6 | btop | 21ca9093 | include/fmt/chrono.h:1052 | is_classic_ | FP | vendored + parser |
| 7 | Open-CMSIS-Pack_devtools | 0a36fb93 | ProjMgrWorker.h:109 | missing | TP | domain-state |
| 8 | magnum-extras | 9cd672a4 | textLayerState.h:261 | textEditCallbackTimeOverload | TP | domain-state |
| 9 | nico | 65b1a2e7 | type_node.h:927 | is_variadic | TP | domain-state |
| 10 | Usagi-dono | c9a4c9d7 | usagi/src/window.h:575 | exitingFromTray | USEFUL_ADVISORY | UI-state |
| 11 | ModbusScope | 20387b89 | adapterpoll.h:59 | _bWaitingForAdapterReady | TP | domain-state |
| 12 | osci-render | f7f4f7b0 | CommonPluginProcessor.h:77 | hasSetSessionStartTime | TP | domain-state |
| 13 | eigen | 381cae56 | SparseQR.h:326 | m_lastPivotLookAheadSkipped | TP | domain-state |
| 14 | ESP32WildlifeCAM | 041c10a1 | ai_wildlife_system.h:107 | enable* ×5 | USEFUL_ADVISORY | config-bag |
| 15 | ShapeWorks | e73c00a2 | AnalysisTool.h:307 | group_dwd_job_running_, dwd_computed_ | TP | UI/job lifecycle |
| 16 | ShapeWorks | e73c00a2 | StatsGroupLDAJob.h:27 | succeeded_ | TP | domain-state |
| 17 | imx-smw | aff72a44 | libobj_types.h:26 | force_destroy | TP | domain-state |
| 18 | xiaozhi-esp32 | d5d8b34b | main/ota.h:41 | has_serial_number_, has_activation_challenge_ | TP | domain-state |
| 19 | Disflux | b24c6c82 | Compositor.h:785 | isPropagatingSizeFactor | USEFUL_ADVISORY | UI-state |
| 20 | pi-hole_FTL | 6093a154 | webserver.c:210 | is_bound (bitfield) | TP | domain-state |
| 21 | AtlasForge | ba384211 | AssetValidator.h:95 | m_schemaLocked | TP | domain-state |
| 22 | rollingraft | bb047e70 | raft_node.h:224 | transport_batching_enabled | USEFUL_ADVISORY | config-bag |
| 23 | rollingraft | bb047e70 | asio_network_transport.cpp:452 | batching_enabled_ | TP | domain-state |
| 24 | spice2x | 63a0acac | overlay/windows/config.h:113 | apply_*/save_* ×8, all_cleared | USEFUL_ADVISORY | UI-state |
| 25 | ja2-stracciatella | 5f6adf88 | GamePolicy.h:47 | fixed_cost_to_shoot | USEFUL_ADVISORY | config-bag |
| 26 | zera-classes | c2d05170 | testlib/sessionexportgenerator.h:34 | m_dataLoggerSystemInitialized | FP | test-support leak |
| 27 | QontrolPanel | 1d1961e5 | soundpanelbridge.h:131 | m_isMediaPlaying | USEFUL_ADVISORY | UI-state |

### Slice 2 (26 findings)

| # | repo | sha | file:line | field(s) | verdict | kind |
|---|---|---|---|---|---|---|
| 1 | mpc-qt | da4a7a56 | src/mpvwidget.h:224 | isInBottomArea | USEFUL_ADVISORY | UI-state |
| 2 | GameEngineDarkest | c657919a | Rendering/Core/Texture.h:89 | m_isCube | TP | domain-state |
| 3 | facebook_mvfst | 16abd44c | OutstandingPacket.h:176 | isDSRPacket, isAppLimited, declaredLost | FP | parser (brace-init) |
| 4 | MaaFramework | 722e33a8 | Input/MessageInput.h:160 | 7 fields | TP | domain-state |
| 5a | haxorg | 284a88b8 | base_token.hpp:78 | traceStructured | USEFUL_ADVISORY | config-bag |
| 5b | haxorg | 284a88b8 | TraceBase.hpp:10 | traceStructured | USEFUL_ADVISORY | config-bag |
| 6 | managarm_mlibc | 7e0d2e83 | rtld-config.hpp:8 | debug, debugVerbose | USEFUL_ADVISORY | config-bag |
| 7a | arduino_thermostat | 7342a5d1 | LedController.h:74 | isQuickMessage | USEFUL_ADVISORY | UI-state |
| 7b | arduino_thermostat | 7342a5d1 | Thermostat.h:151 | previousHeaterState | TP | domain-state |
| 8 | Smatchet | a06fbddf | TrackerFieldSchema.h:48 | IsRequired | TP | schema DTO |
| 9 | ukraine_alarm_map | 72593759 | firmware/src/Constants.h:72 | ignore | USEFUL_ADVISORY | config-bag |
| 10 | MaekawaTomonori_GameEngine | 11dd34db | CameraDirector.hpp:35 | isLoop_ | TP | domain-state |
| 11 | Battery-Emulator | d31fa7f1 | NISSAN-LEAF-BATTERY.h:79 | flip_3B8 | TP | domain-state |
| 12 | pascalscript | 69e2a282 | include/ps_runtime.h:25 | range_check | TP | domain-state |
| 13 | electron | bfa5c933 | gin_helper/callback.cc:56 | one_time, called | TP | domain-state |
| 14 | perf-prof | 922476ef | monitor.h:179 | string | USEFUL_ADVISORY | config-bag (CLI) |
| 15 | AI-on-the-edge-device | 90beab93 | ClassFlowSensors.h:61 | _configParsed | TP | domain-state |
| 16 | stochtree | 923ae54e | ensemble.h:376 | is_exponentiated_ | TP | domain-state |
| 17 | USearch | bebb5394 | cpp/bench.cpp:516 | quantize_e5m2, quantize_e4m3 | USEFUL_ADVISORY | config-bag (bench) |
| 18a | NanaBox | 6b3d1e04 | Configuration.Specification.h:270 | RemoveHyperVDevices | FP | parser (unexplained) |
| 18b | NanaBox | 6b3d1e04 | Configuration.Specification.h:235 | HideHypervisorBit | FP | rename-as-accretion |
| 18c | NanaBox | 6b3d1e04 | Configuration.Specification.h:309 | Enabled | USEFUL_ADVISORY | config-bag |
| 19 | Workshop_Computer | 36b95935 | 80_breaky/src/main.cpp:73 | clock_seen_once_, external_clock_active_ | TP | domain-state |
| 20 | ispc | 8a29d04b | src/ispc.h:922 | disableTargetValidation | USEFUL_ADVISORY | config-bag |
| 21 | FreeRDP | ec9e74f5 | client/X11/xfreerdp.h:321 | isActionScriptAllowed | TP | domain-state |
| 22 | watchman | fc2407d1 | telemetry/LogEvent.h:59 | case_sensitive | FP | struct-name reuse |

## Twelve instructive examples (before/after verified by eye)

### 1. xiaozhi-esp32 `Ota` — the textbook TP

```cpp
// before                          // after
bool has_new_version_ = false;     bool has_new_version_ = false;
bool has_mqtt_config_ = false;     bool has_mqtt_config_ = false;
bool has_server_time_ = false;     bool has_server_time_ = false;
bool has_activation_code_ = false; bool has_activation_code_ = false;
                                   bool has_serial_number_ = false;
                                   bool has_activation_challenge_ = false;
```
A server-response object accumulating one presence flag per protocol feature, 4 → 6.

### 2. ShapeWorks `AnalysisTool` — per-feature flag-pair duplication

```cpp
// before                                  // after
bool group_lda_job_running_ = false;       bool group_lda_job_running_ = false;
bool lda_computed_ = false;                bool lda_computed_ = false;
                                           bool group_dwd_job_running_ = false;
                                           bool dwd_computed_ = false;
```
Adding a second analysis method copy-pastes the `{running, computed}` bool pair; the same
commit fixes a stale-state bug caused by exactly this bookkeeping style. Strongest
architectural example in the sample.

### 3. MaaFramework `MessageInput` — 7-bool hand-rolled state machine (TP)

```cpp
bool tracking_thread_init_done_ = false;
bool tracking_thread_init_ok_ = false;
bool rawinput_ensure_requested_ = false;
bool rawinput_ensure_done_ = false;
bool rawinput_ensure_ok_ = false;
bool mouse_lock_follow_active_ = false;
bool tracking_thread_started_for_lock_follow_ = false;
```
All 7 new in one commit; `done_=false, ok_=true` is representable but illegal — the
illegal-states risk the rule exists to surface.

### 4. GamePolicy (ja2-stracciatella) — config-bag, USEFUL_ADVISORY

```cpp
 bool multiple_interrupts;             // can interrupt more than once per turn
+bool fixed_cost_to_shoot;    // Changes the formula for APs to shoot
 int8_t enemy_weapon_minimal_status;
 bool gui_extras;                      /* graphical user interface cosmetic mod */
```
A JSON-backed game-options class; the bool is real but growth is expected here.
Config-bags are a separate public category, not parser FP.

### 5. btop `is_classic_` — FP: `const bool` blindness (parser, confirmed in source)

```cpp
// before (fmt 11)                 // after (fmt 12)
const bool is_classic_;            bool is_classic_;
```
The field existed all along; upstream dropped `const`. `kBoolRe`
(`src/scan/bool_field_drift.cpp:24`) accepts `mutable` but not `const`, so the
before-count missed it and the after "gained" it. The only factually false *field* claim
in slice 1. Same commit also shows the **vendored leak**: `include/fmt/` is a bundled
dependency bump, and `fmt` is not in `kVendoredLibDirs` (`file_classification.h:149`).

### 6. mvfst `OutstandingPacket` — FP: brace-init blindness (confirmed in source)

```cpp
// before                          // after
bool isDSRPacket{false};           bool isDSRPacket : 1;
bool isAppLimited{false};          bool isAppLimited : 1;
bool declaredLost{false};          bool declaredLost : 1;
```
A size-packing refactor. `kBoolRe` only accepts `= init` (not `{init}`), so all three
pre-existing fields were invisible in the before-image → 3 phantom "new" fields.

### 7. NanaBox `RemoveHyperVDevices` — FP: unexplained extraction failure (reproducer pinned)

Before and after lines are byte-identical apart from a trailing comment:
```cpp
bool RemoveHyperVDevices = false;            // TODO(Phase4): Remove Hyper-V ACPI devices
```
Re-verified during this audit: `kBoolRe` DOES match the before-line (a Python port
matches all six candidate lines of the before-image), so the initial "comment content"
hypothesis is refuted; the failure is in before-image struct-body extraction, mechanism
not yet identified. Reproducer: `NanaBox/NanaBox.Configuration.Specification.h` pair at
`6b3d1e04^` / `6b3d1e04`. Filed in #164.

### 8. watchman `WatchmanEvent` — FP: struct-name reuse after restructure

`MetadataEventData` (holding `case_sensitive`) was deleted and its fields merged into a
struct that reuses the name `WatchmanEvent` (previously field-less). Name-based struct
matching reads this as accretion. Inherent limit of the name-diff design; rare.

### 9. NanaBox `HideHypervisor → HideHypervisorBit` — FP: rename-as-accretion

The struct actually LOST a bool net (3→2). Name-set diffing cannot see renames.
Related cosmetic bug: message arithmetic reports the NET count with the full added-name
list ("accreted 1 bool field(s): A, B" — cpp-atlas #3).

### 10. electron `TranslatorHolder` — TP with nuance: state migrated from JS to C++

```cpp
 struct TranslatorHolder {
   v8::Global<v8::External> handle;
   Translator translator;
+  bool one_time = false;
+  bool called = false;
 };
```
The bools previously lived as V8 object properties; at struct level this is genuine
accretion with a real `one_time`/`called` dependency.

### 11. zera-classes — FP: test-support leak

`modulemanager/testlib/sessionexportgenerator.h` — real new bool, but test-support code;
`testlib` is not in `kTestDirNames` (`file_classification.h:442`).

### 12. pi-hole FTL — parser robustness positive control

```cpp
bool is_bound :1;   // new, joins is_secure/is_redirect/is_optional bitfields
```
Bitfield syntax parsed correctly; scanner also correctly ignored `std::vector<bool>`,
int tri-states, bool-returning methods, and bools in brand-new structs across the sample.

## FP taxonomy (fresh sample, root causes verified against scanner source)

| Class | Count | Root cause | Status |
|---|---:|---|---|
| Vendored dependency bump | 2 | `fmt` missing from `kVendoredLibDirs` | fix in #164 |
| Parser: brace-init `bool x{false};` | 1 (3 fields) | `kBoolRe` accepts only `= init` | fix in #164 |
| Parser: `const bool` | 1 | `kBoolRe` lacks `const` | fix in #164 |
| Rename-as-accretion | 1 | name-set diff, inherent | accept, document |
| Struct-name reuse after restructure | 1 | name-based matching, inherent | accept, document |
| Test-support dir leak | 1 | `testlib` not in `kTestDirNames` | fix in #164 |
| Unexplained before-image extraction | 1 | reproducer pinned (NanaBox) | investigate in #164 |

Historical FP classes from the old file-level prototype (bool locals, bool parameters,
generated registries) did NOT reproduce — the native per-struct path is stricter.

## Live cross-signal reproduction

`EricLeeFriedman_CopilotChess@d19b8246`: one diff emits bool-field accretion
(`struct 'Move' accreted 1 bool field(s): is_castling`) + 3 LCX + 2 new-clone findings —
a good public demo commit (a chess move model gains a mode flag while move-generation
logic grows and duplicates).

## Post-fix re-run 2026-07-02 — the fixable FP classes are GONE

On the user's "clean results" instruction, items A.1-A.5/B.1-B.3 of #164 were fixed the
same night (kBoolRe const + brace-init; paren-guard now ignores trailing comments —
the CONFIRMED NanaBox root cause, found by unit-test reproduction: the method-decl paren
filter rejected lines whose `// TODO(Phase4)` comment contains parentheses; gross-adds
message; qualified nested names; `fmt` vendor dir; `testlib`/`testlibs` test dirs; bare
`test`/`tests` stems). 622/622 tests (6 new), dogfood 0, lizard clean.

The identical 45-commit sample re-run with the fixed binary:

| | pre-fix | post-fix |
|---|---:|---:|
| Findings | 53 | 48 |
| FP | 7 (13%) | **1 (2%)** |
| Factually correct claims | 87% | **98%** |

- Removed, verified: btop ×2 (fmt vendor), mvfst phantom ×3-in-1 (brace-init), NanaBox
  ×3 (paren-comment phantoms ×2; `TimingConfiguration.Enabled` now correctly silenced by
  the net gate — and the `HideHypervisorBit` rename FP disappeared too, because the
  baseline now sees the commented old fields), zera-classes (testlib).
- Message/name corrections: cpp-atlas now says "accreted 2 ... : m_fileTreeOnLeft,
  m_outputFullHeight" (count matches list); nico reports `Type::Function`.
- **Recall recovered, eyeball-verified:** 2 NEW genuine TPs appeared — stochtree
  `ForestContainer.is_exponentiated_{false}` and `Tree.is_log_scale_{false}` (brace-init
  fields the old regex could not see). The CopilotChess control also gained a previously
  MISSED TP: `GameState` +4 castling bools (hidden by parenthesized comments).
- Remaining FP: exactly one — watchman `case_sensitive` (struct-name reuse after an
  inheritance-flattening restructure; inherent to name-based matching, documented).

## Recommendation

1. **Keep `DRIFT.BOOL_FIELD_ACCRETION` advisory, ship it.** 87% factual precision on a
   fresh stratified sample; every FP class is either fixable scope/parser work (#164) or
   rare and inherent (renames, struct-name reuse).
2. **Wording stays neutral** — "struct X accreted N bool field(s)" states a fact, not a
   defect. Config-bag growth is a legitimate finding class; do not call it FP, present it
   as expected-growth advisory.
3. **Fix pack #164** (parser + scope): `const`/brace-init in `kBoolRe`; `fmt` in
   `kVendoredLibDirs`; `testlib` in `kTestDirNames`; message count/list mismatch; NanaBox
   extraction reproducer as a unit test.
4. Public examples to use: xiaozhi-esp32 `Ota`, ShapeWorks flag pair, MaaFramework state
   machine, CopilotChess cross-signal commit.
