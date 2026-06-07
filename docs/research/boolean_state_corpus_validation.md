# Boolean-State Rule 1 — Corpus Validation

**Candidates found:** 59 structs with 5+ boolean fields
**Repos with findings:** 22

> **Анализ использования каждого кандидата** (можно ли в `enum`, как реально используется, что предлагается) — см. [boolean_state_enum_analysis.md](boolean_state_enum_analysis.md). Кратко: 0 чистых машин состояний, 11 частичных (есть извлекаемое state-подмножество), 46 «оставить bool», 2 FP экстрактора.

## Top Candidates (sorted by bool count)

| Repo | Struct | Bools | File |
|------|--------|-------|------|
| Cockatrice_Cockatrice | SettingsCache | 76 | [cache_settings.h](file:///home/localadm/oss/Cockatrice_Cockatrice/cockatrice/src/client/settings/cache_settings.h) |
| AcademySoftwareFoundation_OpenImageIO | Oiiotool | 42 | [oiiotool.h](file:///home/localadm/oss/AcademySoftwareFoundation_OpenImageIO/src/oiiotool/oiiotool.h) |
| AetherSDR | mosquitto | 16 | [mosquitto_internal.h](file:///home/localadm/oss/AetherSDR/third_party/mosquitto/src/mosquitto_internal.h) |
| BlazingRenderer_BRender | cgltf_material | 15 | [cgltf.h](file:///home/localadm/oss/BlazingRenderer_BRender/core/fmt/cgltf.h) |
| Catch2 | ConfigData | 15 | [catch_config.hpp](file:///home/localadm/oss/Catch2/src/catch2/catch_config.hpp) |
| ElementsProject_lightning | daemon | 15 | [connectd.h](file:///home/localadm/oss/ElementsProject_lightning/connectd/connectd.h) |
| Cockatrice_Cockatrice | Definition | 13 | [peglib.h](file:///home/localadm/oss/Cockatrice_Cockatrice/libcockatrice_utility/libcockatrice/utility/peglib.h) |
| CoolProp_CoolProp | value_information | 13 | [rapidjson.h](file:///home/localadm/oss/CoolProp_CoolProp/include/CoolProp/detail/rapidjson.h) |
| AcademySoftwareFoundation_OpenImageIO | IvGL | 12 | [ivgl.h](file:///home/localadm/oss/AcademySoftwareFoundation_OpenImageIO/src/iv/ivgl.h) |
| Emute-Lab-Instruments_uSEQ | ConfigData | 12 | [catch.hpp](file:///home/localadm/oss/Emute-Lab-Instruments_uSEQ/test/catch.hpp) |
| Chatterino_chatterino2 | MessagePreferences | 11 | [MessageLayoutContext.hpp](file:///home/localadm/oss/Chatterino_chatterino2/src/messages/layouts/MessageLayoutContext.hpp) |
| DataDog_java-profiler | Arguments | 11 | [arguments.h](file:///home/localadm/oss/DataDog_java-profiler/ddprof-lib/src/main/cpp/arguments.h) |
| AcademySoftwareFoundation_OpenImageIO | print_info_options | 10 | [imageio_pvt.h](file:///home/localadm/oss/AcademySoftwareFoundation_OpenImageIO/src/include/imageio_pvt.h) |
| EVerest_EVerest | EV102 | 10 | [messages.hpp](file:///home/localadm/oss/EVerest_EVerest/lib/everest/ieee2030_1_1/include/ieee2030/common/messages/messages.hpp) |
| Expensify_Bedrock | SQLite | 10 | [SQLite.h](file:///home/localadm/oss/Expensify_Bedrock/sqlitecluster/SQLite.h) |
| Cantera_cantera | vcs_VolPhase | 8 | [vcs_VolPhase.h](file:///home/localadm/oss/Cantera_cantera/include/cantera/equil/vcs_VolPhase.h) |
| Chatterino_chatterino2 | MessageParseArgs | 8 | [MessageBuilder.hpp](file:///home/localadm/oss/Chatterino_chatterino2/src/messages/MessageBuilder.hpp) |
| DataDog_java-profiler | VMStructs | 8 | [vmStructs.h](file:///home/localadm/oss/DataDog_java-profiler/ddprof-lib/src/main/cpp/hotspot/vmStructs.h) |
| AlexanderTaeschner_gnuplot | QtGnuplotWidget | 7 | [QtGnuplotWidget.h](file:///home/localadm/oss/AlexanderTaeschner_gnuplot/src/qtterminal/QtGnuplotWidget.h) |
| BlueSCSI_BlueSCSI-v2 | image_config_t | 7 | [BlueSCSI_disk.h](file:///home/localadm/oss/BlueSCSI_BlueSCSI-v2/src/BlueSCSI_disk.h) |
| Cantera_cantera | Reaction | 7 | [Reaction.h](file:///home/localadm/oss/Cantera_cantera/include/cantera/kinetics/Reaction.h) |
| CoolProp_CoolProp | ResidualHelmholtzGeneralizedExponential | 7 | [Helmholtz.h](file:///home/localadm/oss/CoolProp_CoolProp/include/CoolProp/fluids/Helmholtz.h) |
| ESPWortuhr_Multilayout-ESP-Wordclock | GLOBAL | 7 | [Uhr.h](file:///home/localadm/oss/ESPWortuhr_Multilayout-ESP-Wordclock/include/Uhr.h) |
| ETLCPP_etl | bit_stream | 7 | [bit_stream.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/bit_stream.h) |
| 203-Systems_MatrixOS | NoteControlBar | 6 | [NoteControlBar.h](file:///home/localadm/oss/203-Systems_MatrixOS/Applications/Note/NoteControlBar.h) |
| AcademySoftwareFoundation_OpenImageIO | IteratorBase | 6 | [imagebuf.h](file:///home/localadm/oss/AcademySoftwareFoundation_OpenImageIO/src/include/OpenImageIO/imagebuf.h) |
| BelledonneCommunications_flexisip | Arg | 6 | [Arg.h](file:///home/localadm/oss/BelledonneCommunications_flexisip/src/tclap/Arg.h) |
| Cantera_cantera | Reactor | 6 | [Reactor.h](file:///home/localadm/oss/Cantera_cantera/include/cantera/zeroD/Reactor.h) |
| Cantera_cantera | InterfaceKinetics | 6 | [InterfaceKinetics.h](file:///home/localadm/oss/Cantera_cantera/include/cantera/kinetics/InterfaceKinetics.h) |
| CoolProp_CoolProp | TransportPropertyData | 6 | [CoolPropFluid.h](file:///home/localadm/oss/CoolProp_CoolProp/include/CoolProp/CoolPropFluid.h) |
| DataDog_java-profiler | VM | 6 | [vmEntry.h](file:///home/localadm/oss/DataDog_java-profiler/ddprof-lib/src/main/cpp/vmEntry.h) |
| ENZYME-APD_tapir-archicad-automation | AddElementNotificationClientCommand | 6 | [NotificationCommands.hpp](file:///home/localadm/oss/ENZYME-APD_tapir-archicad-automation/archicad-addon/Sources/NotificationCommands.hpp) |
| ETLCPP_etl | icallback_timer_atomic | 6 | [callback_timer_atomic.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/callback_timer_atomic.h) |
| ETLCPP_etl | icallback_timer_locked | 6 | [callback_timer_locked.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/callback_timer_locked.h) |
| EVerest_EVerest | Charger109 | 6 | [messages.hpp](file:///home/localadm/oss/EVerest_EVerest/lib/everest/ieee2030_1_1/include/ieee2030/common/messages/messages.hpp) |
| Emute-Lab-Instruments_uSEQ | GraphBuilder | 6 | [graph_builder.h](file:///home/localadm/oss/Emute-Lab-Instruments_uSEQ/uSEQ/src/signal_engine/graph_builder.h) |
| AcademySoftwareFoundation_OpenImageIO | TextureSystemImpl | 5 | [texture_pvt.h](file:///home/localadm/oss/AcademySoftwareFoundation_OpenImageIO/src/libtexture/texture_pvt.h) |
| AetherSDR | bitstream_state | 5 | [bitstream.h](file:///home/localadm/oss/AetherSDR/third_party/libmodem_core/bitstream.h) |
| AlexanderTaeschner_gnuplot | QtGnuplotWindow | 5 | [QtGnuplotWindow.h](file:///home/localadm/oss/AlexanderTaeschner_gnuplot/src/qtterminal/QtGnuplotWindow.h) |
| AlexanderTaeschner_gnuplot | wxtConfigDialog | 5 | [wxt_gui.h](file:///home/localadm/oss/AlexanderTaeschner_gnuplot/src/wxterminal/wxt_gui.h) |
| BambuStudio | GCodeChecker | 5 | [GCodeChecker.h](file:///home/localadm/oss/BambuStudio/bbs_test_tools/bbs_gcode_checker/GCodeChecker.h) |
| BlazingRenderer_BRender | cgltf_node | 5 | [cgltf.h](file:///home/localadm/oss/BlazingRenderer_BRender/core/fmt/cgltf.h) |
| BlueSCSI_BlueSCSI-v2 | ImageBackingStore | 5 | [ImageBackingStore.h](file:///home/localadm/oss/BlueSCSI_BlueSCSI-v2/src/ImageBackingStore.h) |
| BoleynSu-Org_monorepo | CodeGenerator | 5 | [code_generate.h](file:///home/localadm/oss/BoleynSu-Org_monorepo/legacy/BSL-AlgorithmW/src/code_generate.h) |
| Cantera_cantera | ReactorNet | 5 | [ReactorNet.h](file:///home/localadm/oss/Cantera_cantera/include/cantera/zeroD/ReactorNet.h) |
| Cockatrice_Cockatrice | Context | 5 | [peglib.h](file:///home/localadm/oss/Cockatrice_Cockatrice/libcockatrice_utility/libcockatrice/utility/peglib.h) |
| DataDog_java-profiler | ProfiledThread | 5 | [thread.h](file:///home/localadm/oss/DataDog_java-profiler/ddprof-lib/src/main/cpp/thread.h) |
| DataDog_java-profiler | ObjectSampler | 5 | [objectSampler.h](file:///home/localadm/oss/DataDog_java-profiler/ddprof-lib/src/main/cpp/objectSampler.h) |
| ETLCPP_etl | format_spec_t | 5 | [format.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/format.h) |
| ETLCPP_etl | imessage_timer_interrupt | 5 | [message_timer_interrupt.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/message_timer_interrupt.h) |

## All Findings by Repository

### 203-Systems_MatrixOS (1 candidates)

- **NoteControlBar** (6 bools) — [NoteControlBar.h](file:///home/localadm/oss/203-Systems_MatrixOS/Applications/Note/NoteControlBar.h)
  Fields: shift_event, keyOffsetMode, chordExtKeyOn, false, false, +1 more

### AcademySoftwareFoundation_OpenImageIO (5 candidates)

- **Oiiotool** (42 bools) — [oiiotool.h](file:///home/localadm/oss/AcademySoftwareFoundation_OpenImageIO/src/oiiotool/oiiotool.h)
  Fields: verbose, quiet, debug, dryrun, runstats, +37 more
- **IvGL** (12 bools) — [ivgl.h](file:///home/localadm/oss/AcademySoftwareFoundation_OpenImageIO/src/iv/ivgl.h)
  Fields: m_shaders_created, m_tex_created, m_dragging, m_selecting, m_use_shaders, +7 more
- **print_info_options** (10 bools) — [imageio_pvt.h](file:///home/localadm/oss/AcademySoftwareFoundation_OpenImageIO/src/include/imageio_pvt.h)
  Fields: verbose, filenameprefix, sum, subimages, compute_sha1, +5 more
- **IteratorBase** (6 bools) — [imagebuf.h](file:///home/localadm/oss/AcademySoftwareFoundation_OpenImageIO/src/include/OpenImageIO/imagebuf.h)
  Fields: m_valid, m_exists, m_deep, m_localpixels, m_readerror, +1 more
- **TextureSystemImpl** (5 bools) — [texture_pvt.h](file:///home/localadm/oss/AcademySoftwareFoundation_OpenImageIO/src/libtexture/texture_pvt.h)
  Fields: m_imagecache_owner, m_gray_to_rgb, m_flip_t, m_legacy_texture_blur, m_legacy_texture_blur

### AetherSDR (2 candidates)

- **mosquitto** (16 bools) — [mosquitto_internal.h](file:///home/localadm/oss/AetherSDR/third_party/mosquitto/src/mosquitto_internal.h)
  Fields: tls_insecure, ssl_ctx_defaults, tls_ocsp_required, tls_use_os_certs, want_write, +11 more
- **bitstream_state** (5 bools) — [bitstream.h](file:///home/localadm/oss/AetherSDR/third_party/libmodem_core/bitstream.h)
  Fields: searching, in_preamble, in_frame, complete, enable_diagnostics

### AlexanderTaeschner_gnuplot (3 candidates)

- **QtGnuplotWidget** (7 bools) — [QtGnuplotWidget.h](file:///home/localadm/oss/AlexanderTaeschner_gnuplot/src/qtterminal/QtGnuplotWidget.h)
  Fields: m_active, m_rounded, m_ctrlQ, m_antialias, m_replotOnResize, +2 more
- **QtGnuplotWindow** (5 bools) — [QtGnuplotWindow.h](file:///home/localadm/oss/AlexanderTaeschner_gnuplot/src/qtterminal/QtGnuplotWindow.h)
  Fields: m_ctrl, m_rounded, m_antialias, m_replotOnResize, m_statusBarActive
- **wxtConfigDialog** (5 bools) — [wxt_gui.h](file:///home/localadm/oss/AlexanderTaeschner_gnuplot/src/wxterminal/wxt_gui.h)
  Fields: raise_setting, persist_setting, ctrl_setting, toggle_setting, redraw_setting

### BambuStudio (1 candidates)

- **GCodeChecker** (5 bools) — [GCodeChecker.h](file:///home/localadm/oss/BambuStudio/bbs_test_tools/bbs_gcode_checker/GCodeChecker.h)
  Fields: m_wiping, check_gap_infill_width, has_scarf_joint_seam, is_wipe_tower, is_multi_nozzle

### BelledonneCommunications_flexisip (1 candidates)

- **Arg** (6 bools) — [Arg.h](file:///home/localadm/oss/BelledonneCommunications_flexisip/src/tclap/Arg.h)
  Fields: _required, _valueRequired, _alreadySet, _ignoreable, _xorSet, +1 more

### BlazingRenderer_BRender (2 candidates)

- **cgltf_material** (15 bools) — [cgltf.h](file:///home/localadm/oss/BlazingRenderer_BRender/core/fmt/cgltf.h)
  Fields: has_pbr_metallic_roughness, has_pbr_specular_glossiness, has_clearcoat, has_transmission, has_volume, +10 more
- **cgltf_node** (5 bools) — [cgltf.h](file:///home/localadm/oss/BlazingRenderer_BRender/core/fmt/cgltf.h)
  Fields: has_translation, has_rotation, has_scale, has_matrix, has_mesh_gpu_instancing

### BlueSCSI_BlueSCSI-v2 (2 candidates)

- **image_config_t** (7 bools) — [BlueSCSI_disk.h](file:///home/localadm/oss/BlueSCSI_BlueSCSI-v2/src/BlueSCSI_disk.h)
  Fields: ejected, reinsert_after_eject, tape_load_next_file, image_directory, use_prefix, +2 more
- **ImageBackingStore** (5 bools) — [ImageBackingStore.h](file:///home/localadm/oss/BlueSCSI_BlueSCSI-v2/src/ImageBackingStore.h)
  Fields: m_iscontiguous, m_israw, m_isrom, m_isreadonly_attr, m_isfolder

### BoleynSu-Org_monorepo (1 candidates)

- **CodeGenerator** (5 bools) — [code_generate.h](file:///home/localadm/oss/BoleynSu-Org_monorepo/legacy/BSL-AlgorithmW/src/code_generate.h)
  Fields: first, first, first, first, first

### Cantera_cantera (5 candidates)

- **vcs_VolPhase** (8 bools) — [vcs_VolPhase.h](file:///home/localadm/oss/Cantera_cantera/include/cantera/equil/vcs_VolPhase.h)
  Fields: m_singleSpecies, m_isIdealSoln, m_UpToDate, m_UpToDate_AC, m_UpToDate_VolStar, +3 more
- **Reaction** (7 bools) — [Reaction.h](file:///home/localadm/oss/Cantera_cantera/include/cantera/kinetics/Reaction.h)
  Fields: reversible, duplicate, allow_nonreactant_orders, allow_negative_orders, m_valid, +2 more
- **Reactor** (6 bools) — [Reactor.h](file:///home/localadm/oss/Cantera_cantera/include/cantera/zeroD/Reactor.h)
  Fields: m_chem, m_energy, m_jac_skip_flow_devices, m_jac_skip_walls, m_jac_skip_connector_composition_dependence, +1 more
- **InterfaceKinetics** (6 bools) — [InterfaceKinetics.h](file:///home/localadm/oss/Cantera_cantera/include/cantera/kinetics/InterfaceKinetics.h)
  Fields: m_redo_rates, m_ROP_ok, m_jac_skip_coverage_dependence, m_jac_skip_electrochemistry, m_has_electrochemistry, +1 more
- **ReactorNet** (5 bools) — [ReactorNet.h](file:///home/localadm/oss/Cantera_cantera/include/cantera/zeroD/ReactorNet.h)
  Fields: m_integratorInitialized, m_atolUserSpecified, m_verbose, m_timeIsIndependent, m_limit_check_active

### Catch2 (1 candidates)

- **ConfigData** (15 bools) — [catch_config.hpp](file:///home/localadm/oss/Catch2/src/catch2/catch_config.hpp)
  Fields: listTests, listTags, listReporters, listListeners, showSuccessfulTests, +10 more

### Chatterino_chatterino2 (2 candidates)

- **MessagePreferences** (11 bools) — [MessageLayoutContext.hpp](file:///home/localadm/oss/Chatterino_chatterino2/src/messages/layouts/MessageLayoutContext.hpp)
  Fields: enableRedeemedHighlight, enableElevatedMessageHighlight, enableFirstMessageHighlight, enableSubHighlight, enableWatchStreakHighlight, +6 more
- **MessageParseArgs** (8 bools) — [MessageBuilder.hpp](file:///home/localadm/oss/Chatterino_chatterino2/src/messages/MessageBuilder.hpp)
  Fields: disablePingSounds, isReceivedWhisper, isSentWhisper, trimSubscriberUsername, isStaffOrBroadcaster, +3 more

### Cockatrice_Cockatrice (3 candidates)

- **SettingsCache** (76 bools) — [cache_settings.h](file:///home/localadm/oss/Cockatrice_Cockatrice/cockatrice/src/client/settings/cache_settings.h)
  Fields: checkUpdatesOnStartup, startupCardUpdateCheckPromptForUpdate, startupCardUpdateCheckAlwaysUpdate, checkCardUpdatesOnStartup, alwaysEnableNewSets, +71 more
- **Definition** (13 bools) — [peglib.h](file:///home/localadm/oss/Cockatrice_Cockatrice/libcockatrice_utility/libcockatrice/utility/peglib.h)
  Fields: ret, recovered, ignoreSemanticValue, enablePackratParsing, is_macro, +8 more
- **Context** (5 bools) — [peglib.h](file:///home/localadm/oss/Cockatrice_Cockatrice/libcockatrice_utility/libcockatrice/utility/peglib.h)
  Fields: recovered, in_whitespace, enablePackratParsing, verbose_trace, ignore_trace_state

### CoolProp_CoolProp (3 candidates)

- **value_information** (13 bools) — [rapidjson.h](file:///home/localadm/oss/CoolProp_CoolProp/include/CoolProp/detail/rapidjson.h)
  Fields: isnull, isfalse, istrue, isbool, isobject, +8 more
- **ResidualHelmholtzGeneralizedExponential** (7 bools) — [Helmholtz.h](file:///home/localadm/oss/CoolProp_CoolProp/include/CoolProp/fluids/Helmholtz.h)
  Fields: delta_li_in_u, tau_mi_in_u, eta1_in_u, eta2_in_u, beta1_in_u, +2 more
- **TransportPropertyData** (6 bools) — [CoolPropFluid.h](file:///home/localadm/oss/CoolProp_CoolProp/include/CoolProp/CoolPropFluid.h)
  Fields: viscosity_using_ECS, conductivity_using_ECS, viscosity_using_Chung, viscosity_using_rhosr, viscosity_model_provided, +1 more

### DataDog_java-profiler (5 candidates)

- **Arguments** (11 bools) — [arguments.h](file:///home/localadm/oss/DataDog_java-profiler/ddprof-lib/src/main/cpp/arguments.h)
  Fields: _shared, _persistent, _wall_collapsing, _record_allocations, _record_liveness, +6 more
- **VMStructs** (8 bools) — [vmStructs.h](file:///home/localadm/oss/DataDog_java-profiler/ddprof-lib/src/main/cpp/hotspot/vmStructs.h)
  Fields: _has_class_names, _has_method_structs, _has_compiler_structs, _has_stack_structs, _has_class_loader_data, +3 more
- **VM** (6 bools) — [vmEntry.h](file:///home/localadm/oss/DataDog_java-profiler/ddprof-lib/src/main/cpp/vmEntry.h)
  Fields: _openj9, _hotspot, _zing, _can_sample_objects, _can_intercept_binding, +1 more
- **ProfiledThread** (5 bools) — [thread.h](file:///home/localadm/oss/DataDog_java-profiler/ddprof-lib/src/main/cpp/thread.h)
  Fields: _tls_key_initialized, _otel_ctx_initialized, _crash_protection_active, expected, _in_critical_section
- **ObjectSampler** (5 bools) — [objectSampler.h](file:///home/localadm/oss/DataDog_java-profiler/ddprof-lib/src/main/cpp/objectSampler.h)
  Fields: _active, _record_allocations, _record_liveness, _gc_generations, _disable_rate_limiting

### ENZYME-APD_tapir-archicad-automation (1 candidates)

- **AddElementNotificationClientCommand** (6 bools) — [NotificationCommands.hpp](file:///home/localadm/oss/ENZYME-APD_tapir-archicad-automation/archicad-addon/Sources/NotificationCommands.hpp)
  Fields: notifyOnNew, notifyOnModification, notifyOnReservationChanges, hasClientToNotifyOnNew, hasClientToNotifyOnModification, +1 more

### ESPWortuhr_Multilayout-ESP-Wordclock (1 candidates)

- **GLOBAL** (7 bools) — [Uhr.h](file:///home/localadm/oss/ESPWortuhr_Multilayout-ESP-Wordclock/include/Uhr.h)
  Fields: progInit, languageVariant, layoutVariant, bootLedBlink, bootLedSweep, +2 more

### ETLCPP_etl (9 candidates)

- **bit_stream** (7 bools) — [bit_stream.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/bit_stream.h)
  Fields: success, success, success, success, success, +2 more
- **icallback_timer_atomic** (6 bools) — [callback_timer_atomic.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/callback_timer_atomic.h)
  Fields: result, result, result, result, repeating, +1 more
- **icallback_timer_locked** (6 bools) — [callback_timer_locked.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/callback_timer_locked.h)
  Fields: result, result, result, result, repeating, +1 more
- **format_spec_t** (5 bools) — [format.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/format.h)
  Fields: hash, zero, width_nested_replacement, precision_nested_replacement, locale_specific
- **imessage_timer_interrupt** (5 bools) — [message_timer_interrupt.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/message_timer_interrupt.h)
  Fields: result, result, result, repeating, enabled
- **icallback_timer_interrupt** (5 bools) — [callback_timer_interrupt.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/callback_timer_interrupt.h)
  Fields: result, result, result, repeating, enabled
- **icallback_timer** (5 bools) — [callback_timer.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/callback_timer.h)
  Fields: result, result, result, repeating, enabled
- **imessage_timer_atomic** (5 bools) — [message_timer_atomic.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/message_timer_atomic.h)
  Fields: result, result, result, repeating, enabled
- **imessage_timer_locked** (5 bools) — [message_timer_locked.h](file:///home/localadm/oss/ETLCPP_etl/include/etl/message_timer_locked.h)
  Fields: result, result, result, repeating, enabled

### EVerest_EVerest (3 candidates)

- **EV102** (10 bools) — [messages.hpp](file:///home/localadm/oss/EVerest_EVerest/lib/everest/ieee2030_1_1/include/ieee2030/common/messages/messages.hpp)
  Fields: fault_battery_over_voltage, fault_battery_under_voltage, fault_battery_current_deviation_error, fault_high_battery_temperature, fault_battery_voltage_deviation_error, +5 more
- **Charger109** (6 bools) — [messages.hpp](file:///home/localadm/oss/EVerest_EVerest/lib/everest/ieee2030_1_1/include/ieee2030/common/messages/messages.hpp)
  Fields: charger_status, charger_malfunction, connector_lock, battery_incompatibility, system_malfunction, +1 more
- **api_connector** (5 bools) — [api_connector.hpp](file:///home/localadm/oss/EVerest_EVerest/applications/pionix_chargebridge/include/charge_bridge/everest_api/api_connector.hpp)
  Fields: m_evse_bsp_enabled, m_ovm_enabled, m_ev_bsp_enabled, m_cb_initial_comm_check, m_cb_connected

### ElementsProject_lightning (2 candidates)

- **daemon** (15 bools) — [connectd.h](file:///home/localadm/oss/ElementsProject_lightning/connectd/connectd.h)
  Fields: developer, dev_allow_localhost, always_use_proxy, use_dns, shutting_down, +10 more
- **db** (5 bools) — [common.h](file:///home/localadm/oss/ElementsProject_lightning/db/common.h)
  Fields: transaction_started, dirty, developer, readonly, in_migration

### Emute-Lab-Instruments_uSEQ (3 candidates)

- **ConfigData** (12 bools) — [catch.hpp](file:///home/localadm/oss/Emute-Lab-Instruments_uSEQ/test/catch.hpp)
  Fields: listTests, listTags, listReporters, listTestNamesOnly, showSuccessfulTests, +7 more
- **GraphBuilder** (6 bools) — [graph_builder.h](file:///home/localadm/oss/Emute-Lab-Instruments_uSEQ/uSEQ/src/signal_engine/graph_builder.h)
  Fields: has_error, reject_live_edit, symbols_initialized, form_table_sorted, ok, +1 more
- **Interpreter** (5 bools) — [interpreter.h](file:///home/localadm/oss/Emute-Lab-Instruments_uSEQ/uSEQ/src/lisp/interpreter.h)
  Fields: m_attempt_expr_eval_first, m_eval_expr_if_def_not_found, m_manual_evaluation, m_update_loop_evaluation, m_builtindefs_init

### Expensify_Bedrock (3 candidates)

- **SQLite** (10 bools) — [SQLite.h](file:///home/localadm/oss/Expensify_Bedrock/sqlitecluster/SQLite.h)
  Fields: _hctree, _insideTransaction, _mutexLocked, _enableRewrite, _shouldNotifyPluginsOnPrepare, +5 more
- **BedrockCommand** (5 bools) — [BedrockCommand.h](file:///home/localadm/oss/Expensify_Bedrock/BedrockCommand.h)
  Fields: repeek, escalateImmediately, _commitEmptyTransactions, _inDBReadOperation, _inDBWriteOperation
- **BedrockTester** (5 bools) — [BedrockTester.h](file:///home/localadm/oss/Expensify_Bedrock/test/lib/BedrockTester.h)
  Fields: ENABLE_HCTREE, VERBOSE_LOGGING, QUIET_LOGGING, remoteMode, _enforceCommandOrder

