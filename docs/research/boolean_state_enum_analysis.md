# Boolean-State → enum: анализ использования по 59 кандидатам

**Дата:** 2026-06-07
**Задача:** #089 / #090
**Метод:** 8 параллельных агентов прочитали реальный код каждого кандидата (определение структуры + grep использования полей по всему репозиторию) и вынесли вердикт по единой рубрике.
**Источник кандидатов:** [boolean_state_corpus_validation.md](boolean_state_corpus_validation.md) — экстрактор отобрал все struct/class с 5+ bool-полями в 50 OSS-репо (фильтр — только счётчик, без naming-эвристики).

## Рубрика вердиктов

- 🟢 **в enum** — все/почти все bool это взаимоисключающие фазы жизненного цикла, в каждый момент истинна ровно одна. Сворачивается целиком в `enum class State`.
- 🟡 **частично** — в основном независимые флаги, но есть узнаваемое подмножество-состояние → его вынести в enum, остальное оставить.
- 🔴 **оставить bool** — независимые тумблеры / `has_*`/`_can_*`/`_use_*`-флаги наличия / конфиг-опции / биты протокола. Перевод в enum был бы ошибкой (комбинации ортогональны и легальны).
- ⚪ **FP экстрактора** — это вообще не поля структуры (локальные `bool result`/`first` в методах, посчитанные многократно).

## Итог (главное)

| Вердикт | Кол-во | Доля |
|---------|-------:|-----:|
| 🟢 чистая машина состояний | **0** | 0% |
| 🟡 частично (есть state-подмножество) | **11** | 19% |
| 🔴 оставить bool | **46** | 78% |
| ⚪ FP экстрактора | **2** | 3% |
| **Всего** | **59** | 100% |

### Что это значит для правила `implicit_state_machine_growth`

**Счётчик «5+ bool» сам по себе — слабый сигнал: только 19% кандидатов имеют извлекаемое состояние, и ни один не является чистой машиной состояний.** Подавляющее большинство — это:

1. **Конфиг-мешки / CLI-опции** (Oiiotool 42, SettingsCache 76, Catch2 ConfigData 15, daemon 15, Arguments 11) — ортогональные тумблеры, любая комбинация легальна.
2. **`has_*`/`_can_*`/`_use_*`-флаги наличия/способности** (cgltf_material 15, VMStructs 8, IvGL `m_use_*`) — присутствие опциональной фичи/секции формата/возможности JVM.
3. **Биты протокола** (EV102, Charger109 — биты статуса/ошибок CHAdeMO/IEEE 2030.1.1, несколько fault одновременно).
4. **Lazy-cache «up-to-date» флаги** (vcs_VolPhase 8 — инвалидация кэша по независимым осям).
5. **Вендоренные библиотеки** (cgltf, Catch2, cpp-peglib, tclap, mosquitto) — трогать upstream нельзя в принципе.

### Конкретные правки правила (feedback в #090 / proposal)

1. **Исключать вендоренные пути** — `third_party/`, `vendor/`, `extern/`, single-header дропы (`catch.hpp`, `cgltf.h`, `peglib.h`, `tclap/`). В выборку попало ≥7 вендоренных структур — чистый шум.
2. **Сильнее давить конфиг-нейминг** — `*Settings`/`*Config`/`*Options`/`*Cache`/`*Arguments`/`*Preferences`/`*ConfigData`. `SettingsCache` (76 bools!) и `MessagePreferences` (11) — очевидные конфиги, проскочившие фильтр имени структуры.
3. **Префиксы-стопы для полей** — `has_*`, `_can_*`, `_use_*`, `m_use_*`, `enable_*`, `dev_*`, `*_provided`, `*_ok`, `*UpToDate*` → сильный сигнал «оставить bool». Добавить в config-pattern exclusions.
4. **Настоящий сигнал — не нейминг, а семантика:** взаимоисключение в цепочках `if … else if …` + групповое присваивание «один true, остальные false». Это видно только через AST/usage-анализ → подтверждает направление **v0.3 + semantic backend (#042)**. Regex-нейминг даёт максимум ~19% полезности на этой выборке.
5. **Отсеять FP экстрактора** — игнорировать `bool` в телах методов (локальные переменные), считать только поля класса. `bit_stream`, `CodeGenerator` — чистые FP (локальные `result`/`first`). ETL-таймеры — 8 структур, где из «5-6 bool» реальны только `enabled` + `repeating`.

---

## 🟡 Частично — есть извлекаемое подмножество-состояние (11)

> Самые ценные находки: тут перевод части полей в `enum class` реально уберёт невозможные состояния.

### bitstream_state (5 bools) — `AetherSDR` ⭐ сильнейший кейс
- **Файл:** [bitstream.h](file:///home/localadm/oss/AetherSDR/third_party/libmodem_core/bitstream.h)
- **Поля:** `searching`, `in_preamble`, `in_frame`, `complete` (фазы) + `aborted`, `enable_diagnostics`
- **Как используется:** Декодер обрабатывает 4 поля строго как взаимоисключающие фазы через `if(searching) … else if(in_preamble) … else if(in_frame) …`; при каждом переходе группа выставляется разом (`searching=false; in_preamble=true`). `reset()` возвращает в `searching=true`.
- **Что предлагается:** `enum class DecodeState { Searching, InPreamble, InFrame, Complete }` — групповые присваивания исчезают, инвариант «истинна ровно одна фаза» становится типобезопасным. `aborted`/`enable_diagnostics` оставить bool. *(NB: репо `third_party` — вендор, правка концептуальная.)*

### IvGL (12 bools) — `AcademySoftwareFoundation_OpenImageIO`
- **Файл:** [ivgl.h](file:///home/localadm/oss/AcademySoftwareFoundation_OpenImageIO/src/iv/ivgl.h)
- **Поля:** `m_dragging`, `m_selecting` (жест) + `m_use_*` (GL capabilities), `m_*_created` (латчи)
- **Как используется:** В `mousePressEvent` входим либо в drag, либо в select (ветки else), в release оба сбрасываются — де-факто взаимоисключающие фазы жеста. `m_use_*` — ортогональные аппаратные capability-флаги.
- **Что предлагается:** `enum class MouseGesture { None, Dragging, Selecting }`. Остальное оставить bool.

### ReactorNet (5 bools) — `Cantera_cantera`
- **Файл:** [ReactorNet.h](file:///home/localadm/oss/Cantera_cantera/include/cantera/zeroD/ReactorNet.h)
- **Поля:** `m_integratorInitialized` + `m_needIntegratorInit` (пара) + `m_verbose`, `m_atolUserSpecified`, `m_timeIsIndependent`
- **Как используется:** Пара кодирует ЖЦ интегратора: «не инициализирован» → «инициализирован, чистый» → «инициализирован, требует reinit» (initialize()/reinitialize()).
- **Что предлагается:** `enum class IntegratorState { Uninitialized, Initialized, NeedsReinit }`. Остальные четыре — bool.

### TransportPropertyData (6 bools) — `CoolProp_CoolProp`
- **Файл:** [CoolPropFluid.h](file:///home/localadm/oss/CoolProp_CoolProp/include/CoolProp/CoolPropFluid.h)
- **Поля:** `viscosity_using_ECS`, `viscosity_using_Chung`, `viscosity_using_rhosr` (метод) + `*_model_provided`, `conductivity_using_ECS`
- **Как используется:** Три viscosity_using_* проверяются цепочкой `if(...) {…; return;}` — выбор одного взаимоисключающего метода, рядом уже есть enum `hardcoded_viscosity`.
- **Что предлагается:** `enum class ViscosityModel { None, ECS, Chung, RhoSr, Hardcoded }`. `*_model_provided` — bool.

### VM (6 bools) — `DataDog_java-profiler`
- **Файл:** [vmEntry.h](file:///home/localadm/oss/DataDog_java-profiler/ddprof-lib/src/main/cpp/vmEntry.h)
- **Поля:** `_hotspot`, `_openj9`, `_zing` (вендор JVM, по построению взаимоисключающие) + `_can_sample_objects`, `_can_intercept_binding`, `_is_adaptive_gc_boundary_flag_set`
- **Как используется:** `_zing = !_hotspot && …`, `_openj9 = !_hotspot && …` — это «какой JVM», ровно один истинен.
- **Что предлагается:** `enum class JvmVendor { Unknown, Hotspot, OpenJ9, Zing }`. `_can_*` оставить bool.

### ImageBackingStore (5 bools) — `BlueSCSI_BlueSCSI-v2`
- **Файл:** [ImageBackingStore.h](file:///home/localadm/oss/BlueSCSI_BlueSCSI-v2/src/ImageBackingStore.h)
- **Поля:** `m_israw`, `m_isrom`, `m_isfolder` (тип бэкенда) + `m_iscontiguous`, `m_isreadonly_attr`
- **Как используется:** В конструкторе по префиксу (`RAW:`/`ROM:`/каталог/файл) выставляется ровно один — взаимоисключающий вид хранилища.
- **Что предлагается:** `enum class Backend { File, Raw, Rom, Folder }`. `m_iscontiguous` (динамический optimization-toggle) и `m_isreadonly_attr` (права) — bool.

### BedrockCommand (5 bools) — `Expensify_Bedrock`
- **Файл:** [BedrockCommand.h](file:///home/localadm/oss/Expensify_Bedrock/BedrockCommand.h)
- **Поля:** `_inDBReadOperation`, `_inDBWriteOperation` (взаимоисключающие) + `repeek`, `escalateImmediately`, `_commitEmptyTransactions`
- **Как используется:** Ставятся/гасятся попарно вокруг разных фаз (peek vs process), одновременно истинными не бывают.
- **Что предлагается:** `enum class DBOp { None, Read, Write }` (рядом уже есть `enum class STAGE`). Остальное — bool.

### db (5 bools) — `ElementsProject_lightning`
- **Файл:** [common.h](file:///home/localadm/oss/ElementsProject_lightning/db/common.h)
- **Поля:** `transaction_started`, `dirty` (фазы транзакции) + `developer`, `readonly`, `in_migration`
- **Как используется:** `db_begin`→`db_need_transaction`(lazy)→`db_commit` ветвится по `transaction_started`, затем по `dirty` — прогрессия фаз транзакции.
- **Что предлагается:** `enum class TxState { None, Open, Started }` для `transaction_started`. `dirty` можно оставить отдельным признаком; `developer`/`readonly`/`in_migration` — bool.

### MessageParseArgs (8 bools) — `Chatterino_chatterino2`
- **Файл:** [MessageBuilder.hpp](file:///home/localadm/oss/Chatterino_chatterino2/src/messages/MessageBuilder.hpp)
- **Поля:** `isReceivedWhisper`, `isSentWhisper` (взаимоисключающие) + 6 независимых признаков парсинга
- **Как используется:** Пара проверяется в одном if/else-if; сообщение не бывает одновременно входящим и исходящим шёпотом.
- **Что предлагается:** `enum class WhisperKind { None, Received, Sent }`. Низкий приоритет (косметика на POD-структуре аргументов).

### ObjectSampler (5 bools) — `DataDog_java-profiler`
- **Файл:** [objectSampler.h](file:///home/localadm/oss/DataDog_java-profiler/ddprof-lib/src/main/cpp/objectSampler.h)
- **Поля:** `_active` (ЖЦ start/stop) + `_record_allocations`, `_record_liveness`, `_gc_generations`, `_disable_rate_limiting` (конфиг)
- **Как используется:** `start()` атомарно ставит `_active=true`, `stop()`=false, callback гейтит на `_active`. Остальные — конфиг из Arguments.
- **Что предлагается:** Опционально `enum class SamplerState { Stopped, Active }` для `_active` (но это всего 2 состояния, bool/atomic тоже норм). 4 конфиг-флага — bool.

### Interpreter (5 bools) — `Emute-Lab-Instruments_uSEQ`
- **Файл:** [interpreter.h](file:///home/localadm/oss/Emute-Lab-Instruments_uSEQ/uSEQ/src/lisp/interpreter.h)
- **Поля:** `m_manual_evaluation`, `m_update_loop_evaluation` (концептуально взаимоисключающие режимы) + `m_attempt_expr_eval_first`, `m_eval_expr_if_def_not_found`, `m_builtindefs_init`
- **Как используется:** Пара переключается вокруг ручного ввода vs loop-апдейта, НО оба поля почти не читаются (write-only), `m_builtindefs_init` — мёртвый guard.
- **Что предлагается:** Концептуально `enum class EvalMode { Idle, Manual, UpdateLoop }`, но **сначала разобраться с мёртвым кодом**. `m_attempt_*`/`m_eval_*` — независимые тумблеры eval.

---

## 🔴 Оставить bool — конфиг / capability / протокол / вендор (46)

> Перевод в enum здесь был бы ошибкой: комбинации ортогональны и легальны одновременно.

### Конфиг-мешки и CLI-опции
- **Oiiotool** (42) — [oiiotool.h](file:///home/localadm/oss/AcademySoftwareFoundation_OpenImageIO/src/oiiotool/oiiotool.h) — CLI-тумблеры `ap.arg(...)`, легально комбинируются (`-v -n --no-clobber`).
- **SettingsCache** (76) — [cache_settings.h](file:///home/localadm/oss/Cockatrice_Cockatrice/cockatrice/src/client/settings/cache_settings.h) — синглтон QSettings, у каждого bool своя пара get/set + Qt-signal. Конфиг, не FSM.
- **Catch2 ConfigData** (15) — [catch_config.hpp](file:///home/localadm/oss/Catch2/src/catch2/catch_config.hpp) — **вендор**; `list*` подтверждённо не взаимоисключающие (аддитивные `if` в catch_list.cpp).
- **uSEQ ConfigData** (12) — [catch.hpp](file:///home/localadm/oss/Emute-Lab-Instruments_uSEQ/test/catch.hpp) — **вендор** (тот же Catch2).
- **daemon** (15) — [connectd.h](file:///home/localadm/oss/ElementsProject_lightning/connectd/connectd.h) — мешок `dev_*`/config-тумблеров; ЖЦ соединения уже в отдельных enum (`draining_state`).
- **Arguments** (11) — [arguments.h](file:///home/localadm/oss/DataDog_java-profiler/ddprof-lib/src/main/cpp/arguments.h) — agent-опции, проверяются комбинациями `_record_allocations || _record_liveness`.
- **MessagePreferences** (11) — [MessageLayoutContext.hpp](file:///home/localadm/oss/Chatterino_chatterino2/src/messages/layouts/MessageLayoutContext.hpp) — снимок `enable*`-чекбоксов, несколько highlight одновременно.
- **print_info_options** (10) — [imageio_pvt.h](file:///home/localadm/oss/AcademySoftwareFoundation_OpenImageIO/src/include/imageio_pvt.h) — options-bag, аддитивные `if`.
- **GLOBAL** (7) — [Uhr.h](file:///home/localadm/oss/ESPWortuhr_Multilayout-ESP-Wordclock/include/Uhr.h) — EEPROM-конфиг прошивки; смена формата сломала бы хранение.
- **GCodeChecker** (5) — [GCodeChecker.h](file:///home/localadm/oss/BambuStudio/bbs_test_tools/bbs_gcode_checker/GCodeChecker.h) — парсер-флаги фич G-кода, часто истинны вместе.
- **TextureSystemImpl** (5) — [texture_pvt.h](file:///home/localadm/oss/AcademySoftwareFoundation_OpenImageIO/src/libtexture/texture_pvt.h) — runtime-атрибуты через `attribute()`.
- **BedrockTester** (5) — [BedrockTester.h](file:///home/localadm/oss/Expensify_Bedrock/test/lib/BedrockTester.h) — тест-харнес CLI-тумблеры.

### GUI-настройки (gnuplot)
- **QtGnuplotWidget** (7) — [QtGnuplotWidget.h](file:///home/localadm/oss/AlexanderTaeschner_gnuplot/src/qtterminal/QtGnuplotWidget.h) — Q_PROPERTY + QSettings, ортогональны.
- **QtGnuplotWindow** (5) — [QtGnuplotWindow.h](file:///home/localadm/oss/AlexanderTaeschner_gnuplot/src/qtterminal/QtGnuplotWindow.h) — зеркало настроек виджета.
- **wxtConfigDialog** (5) — [wxt_gui.h](file:///home/localadm/oss/AlexanderTaeschner_gnuplot/src/wxterminal/wxt_gui.h) — чекбоксы диалога; многозначные опции уже сделаны `int`.

### `has_*`/`_can_*`/`_use_*`-флаги наличия и capability
- **cgltf_material** (15) — [cgltf.h](file:///home/localadm/oss/BlazingRenderer_BRender/core/fmt/cgltf.h) — **вендор** (cgltf); `has_*` секции/extension glTF, ортогональны.
- **cgltf_node** (5) — [cgltf.h](file:///home/localadm/oss/BlazingRenderer_BRender/core/fmt/cgltf.h) — **вендор**; `has_translation/rotation/scale/matrix` (частичный TRS допустим).
- **VMStructs** (8) — [vmStructs.h](file:///home/localadm/oss/DataDog_java-profiler/ddprof-lib/src/main/cpp/hotspot/vmStructs.h) — `_has_*`/`_can_*` capability JVM, кэш обнаружения.
- **AddElementNotificationClientCommand** (6) — [NotificationCommands.hpp](file:///home/localadm/oss/ENZYME-APD_tapir-archicad-automation/archicad-addon/Sources/NotificationCommands.hpp) — битовая маска подписок (3 оси × 2 уровня).
- **IteratorBase** (6) — [imagebuf.h](file:///home/localadm/oss/AcademySoftwareFoundation_OpenImageIO/src/include/OpenImageIO/imagebuf.h) — `m_valid`/`m_exists` ортогональные предикаты (все 4 комбинации осмысленны), горячий цикл.
- **api_connector** (5) — [api_connector.hpp](file:///home/localadm/oss/EVerest_EVerest/applications/pionix_chargebridge/include/charge_bridge/everest_api/api_connector.hpp) — три `*_enabled` под-API (битовая маска фич).

### Lazy-cache / dirty / validity флаги
- **vcs_VolPhase** (8) — [vcs_VolPhase.h](file:///home/localadm/oss/Cantera_cantera/include/cantera/equil/vcs_VolPhase.h) — 6 `m_UpToDate*` (инвалидация по независимым осям) + 2 property-флага.
- **InterfaceKinetics** (6) — [InterfaceKinetics.h](file:///home/localadm/oss/Cantera_cantera/include/cantera/kinetics/InterfaceKinetics.h) — cache (`m_ROP_ok`) + `m_has_*` + skip-опции; пары `if(!m_jac_skip_X && m_has_X)`.
- **value_information** (13) — [rapidjson.h](file:///home/localadm/oss/CoolProp_CoolProp/include/CoolProp/detail/rapidjson.h) — зеркало предикатов типа JSON (перекрываются: istrue+isbool); вдобавок мёртвый код.
- **GraphBuilder** (6) — [graph_builder.h](file:///home/localadm/oss/Emute-Lab-Instruments_uSEQ/uSEQ/src/signal_engine/graph_builder.h) — sticky-error + 2 static init-guard + режим-флаг.

### Конфиг-свойства / режимы (не взаимоисключающие)
- **Reaction** (7) — [Reaction.h](file:///home/localadm/oss/Cantera_cantera/include/cantera/kinetics/Reaction.h) — свойства реакции из YAML + флаги сериализации.
- **Reactor** (6) — [Reactor.h](file:///home/localadm/oss/Cantera_cantera/include/cantera/zeroD/Reactor.h) — 2 enable + 4 skip-опции якобиана (64 легальные комбинации).
- **ResidualHelmholtzGeneralizedExponential** (7) — [Helmholtz.h](file:///home/localadm/oss/CoolProp_CoolProp/include/CoolProp/fluids/Helmholtz.h) — `*_in_u` «какие члены присутствуют» (подмножества), не одно состояние.
- **ProfiledThread** (5) — [thread.h](file:///home/localadm/oss/DataDog_java-profiler/ddprof-lib/src/main/cpp/thread.h) — 3 флага разных подсистем; тип потока УЖЕ вынесен в `enum ThreadType` (подтверждает рубрику).
- **SQLite** (10) — [SQLite.h](file:///home/localadm/oss/Expensify_Bedrock/sqlitecluster/SQLite.h) — ортогональные латчи/режимы конкурентного объекта, истинны в произвольных комбинациях.
- **image_config_t** (7) — [BlueSCSI_disk.h](file:///home/localadm/oss/BlueSCSI_BlueSCSI-v2/src/BlueSCSI_disk.h) — смесь конфиг-опций, латчей и one-shot guard на SCSI-таргете.
- **NoteControlBar** (6) — [NoteControlBar.h](file:///home/localadm/oss/203-Systems_MatrixOS/Applications/Note/NoteControlBar.h) — ЖЦ-режим уже в `enum NoteControlBarMode`; остаток — per-key латчи.
- **Context** (5) — [peglib.h](file:///home/localadm/oss/Cockatrice_Cockatrice/libcockatrice_utility/libcockatrice/utility/peglib.h) — **вендор** (cpp-peglib); re-entrancy guard + const-конфиг.
- **Definition** (13) — [peglib.h](file:///home/localadm/oss/Cockatrice_Cockatrice/libcockatrice_utility/libcockatrice/utility/peglib.h) — **вендор**; ортогональные свойства правила грамматики.
- **Arg** (6) — [Arg.h](file:///home/localadm/oss/BelledonneCommunications_flexisip/src/tclap/Arg.h) — **вендор** (TCLAP); 4 свойства-спецификации + 2 рантайм-флага.

### Биты протокола / статуса
- **EV102** (10) — [messages.hpp](file:///home/localadm/oss/EVerest_EVerest/lib/everest/ieee2030_1_1/include/ieee2030/common/messages/messages.hpp) — биты fault/status CHAdeMO (`fault & (1<<n)`), несколько одновременно. Кандидат на `std::bitset`, не enum.
- **Charger109** (6) — [messages.hpp](file:///home/localadm/oss/EVerest_EVerest/lib/everest/ieee2030_1_1/include/ieee2030/common/messages/messages.hpp) — симметрично, биты статуса станции.
- **mosquitto** (16) — [mosquitto_internal.h](file:///home/localadm/oss/AetherSDR/third_party/mosquitto/src/mosquitto_internal.h) — **вендор**; ЖЦ уже в `enum mosquitto_client_state`, остаток — опции/рабочие флаги.

### ETL-таймеры (реальны только `enabled` + `repeating`, остальное — локальные FP)
- **icallback_timer_atomic** (6) — [callback_timer_atomic.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/callback_timer_atomic.h)
- **icallback_timer_locked** (6) — [callback_timer_locked.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/callback_timer_locked.h)
- **icallback_timer_interrupt** (5) — [callback_timer_interrupt.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/callback_timer_interrupt.h)
- **icallback_timer** (5) — [callback_timer.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/callback_timer.h)
- **imessage_timer_interrupt** (5) — [message_timer_interrupt.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/message_timer_interrupt.h)
- **imessage_timer_atomic** (5) — [message_timer_atomic.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/message_timer_atomic.h)
- **imessage_timer_locked** (5) — [message_timer_locked.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/message_timer_locked.h)
- **format_spec_t** (5) — [format.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/format.h) — независимые format-флаги (`#`,`0`,`L`); взаимоисключающие align/sign УЖЕ в отдельных enum.

---

## ⚪ FP экстрактора — это не поля структуры (2)

- **bit_stream** (7) — [bit_stream.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/bit_stream.h) — посчитаны локальные `bool success`/`bool result` в инлайн-методах. Полей-bool у структуры нет.
- **CodeGenerator** (5) — [code_generate.h](file:///home/localadm/oss/BoleynSu-Org_monorepo/legacy/BSL-AlgorithmW/src/code_generate.h) — имя `first` (локальный «первый-в-списке» разделитель) посчитано из 5 разных областей. Реальных bool-полей нет.

> Дополнительно: 7 из 8 ETL-таймеров формально 🔴, но в каждом из «5-6 bool» реальны только 2 поля (`enabled` + вложенное `repeating`), остальное — те же локальные FP. То есть фактический FP-шум экстрактора ещё выше, чем 2 строки выше.

---

## Связь с гипотезой #089

Это уточняет вердикт research #089 (**boolean-state growth = drift-сигнал? YES, с оговорками**):

- **YES** часть подтверждается: реальные извлекаемые состояния существуют (bitstream FSM, ImageBackingStore Backend, ReactorNet integrator, db transaction) — рост таких bool — настоящий drift.
- **Оговорки усиливаются**: «голый» счётчик 5+ bool даёт ~78% «оставить bool». Полезность правила целиком зависит от (а) исключения вендора/конфигов/`has_*`, (б) семантического анализа взаимоисключения — то есть от **semantic backend (#042)**, как и прогнозировалось в [boolean_state_drift_proposal.md](boolean_state_drift_proposal.md).
