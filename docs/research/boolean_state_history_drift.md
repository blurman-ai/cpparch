# Boolean-State DRIFT по истории коммитов (агентские репо)

**Репо:** 73 агентских (локально склонированных). Скан `git log -p` по заголовкам, добавленные `+ bool field;` (не параметры).

## Что считается дрейфом

**Дрейф ≠ один коммит, вываливший 100 булей** (это «написали класс» = фича). **Дрейф = инкрементальное накопление**: в один и тот же заголовок булы капают по 1-2 за раз, в РАЗНЫХ коммитах, разными авторами, месяцами/годами (boiling-frog, constraint decay — см. `docs/research/constraint_decay.md`).

**Метрика:** число РАЗНЫХ коммитов, добавивших bool в один файл (после вычета переформатирований, вендора, генерёнки), и временной размах.

**Отфильтрованный шум:** clang-format/crlf-коммиты (перетрогивают старые строки), вендор (SDL/ImGui/ESPAsync), генерёнка (`.pb.h`), разовые feature-дампы.

**Найдено:** 219 файлов с ≥3 разными коммитами, добавлявшими булы по чуть-чуть.

## Топ-30 дрейфующих заголовков (по числу коммитов-добавлений)

| Коммитов | из них по 1-2 | всего +bool | дней | репо | файл |
|---|---|---|---|---|---|
| **39** | 35 | 49 | 707 (2024-06-22→2026-05-30) | mod-playerbots_mod-playerbots | [PlayerbotAIConfig.h](file://~/oss/mod-playerbots_mod-playerbots/src/PlayerbotAIConfig.h)<br><sub>src/PlayerbotAIConfig.h</sub> |
| **35** | 33 | 53 | 457 (2025-02-19→2026-05-22) | couchbase_kv_engine | [settings.h](file://~/oss/couchbase_kv_engine/daemon/settings.h)<br><sub>daemon/settings.h</sub> |
| **32** | 19 | 215 | 570 (2024-10-30→2026-05-23) | dalathegreat_Battery-Emulator | [datalayer_extended.h](file://~/oss/dalathegreat_Battery-Emulator/Software/src/datalayer/datalayer_extended.h)<br><sub>Software/src/datalayer/datalayer_extended.h</sub> |
| **32** | 29 | 46 | 495 (2024-09-05→2026-01-13) | dalathegreat_Battery-Emulator | [datalayer.h](file://~/oss/dalathegreat_Battery-Emulator/Software/src/datalayer/datalayer.h)<br><sub>Software/src/datalayer/datalayer.h</sub> |
| **26** | 23 | 41 | 706 (2024-06-27→2026-06-03) | getsentry_sentry-unreal | [SentrySettings.h](file://~/oss/getsentry_sentry-unreal/plugin-dev/Source/Sentry/Public/SentrySettings.h)<br><sub>plugin-dev/Source/Sentry/Public/SentrySettings.h</sub> |
| **25** | 21 | 36 | 716 (2024-06-15→2026-06-01) | MOLAorg_mola_lidar_odometry | [LidarOdometry.h](file://~/oss/MOLAorg_mola_lidar_odometry/module/include/mola_lidar_odometry/LidarOdometry.h)<br><sub>module/include/mola_lidar_odometry/LidarOdometry.h</sub> |
| **23** | 22 | 26 | 234 (2025-07-24→2026-03-15) | godot42x_ya | [App.h](file://~/oss/godot42x_ya/Engine/Source/Core/App/App.h)<br><sub>Engine/Source/Core/App/App.h</sub> |
| **19** | 13 | 47 | 711 (2024-06-14→2026-05-26) | masc-ucsc_hhds | [tree.hpp](file://~/oss/masc-ucsc_hhds/hhds/tree.hpp)<br><sub>hhds/tree.hpp</sub> |
| **17** | 12 | 51 | 116 (2026-01-30→2026-05-26) | ggml-org_ggml | [ggml-webgpu-shader-lib.hpp](file://~/oss/ggml-org_ggml/src/ggml-webgpu/ggml-webgpu-shader-lib.hpp)<br><sub>src/ggml-webgpu/ggml-webgpu-shader-lib.hpp</sub> |
| **17** | 12 | 51 | 116 (2026-01-30→2026-05-26) | ggml-org_whisper.cpp | [ggml-webgpu-shader-lib.hpp](file://~/oss/ggml-org_whisper.cpp/ggml/src/ggml-webgpu/ggml-webgpu-shader-lib.hpp)<br><sub>ggml/src/ggml-webgpu/ggml-webgpu-shader-lib.hpp</sub> |
| **16** | 16 | 17 | 564 (2024-10-04→2026-04-21) | cloudflare_workerd | [setup.h](file://~/oss/cloudflare_workerd/src/workerd/jsg/setup.h)<br><sub>src/workerd/jsg/setup.h</sub> |
| **16** | 14 | 23 | 214 (2025-10-09→2026-05-11) | ThomasGhione_chess_engine | [engine.hpp](file://~/oss/ThomasGhione_chess_engine/engine/engine.hpp)<br><sub>engine/engine.hpp</sub> |
| **14** | 11 | 33 | 365 (2025-05-26→2026-05-26) | masc-ucsc_hhds | [graph.hpp](file://~/oss/masc-ucsc_hhds/hhds/graph.hpp)<br><sub>hhds/graph.hpp</sub> |
| **13** | 11 | 19 | 651 (2024-05-22→2026-03-04) | oneapi-src_unified-runtime | [ur_print.hpp](file://~/oss/oneapi-src_unified-runtime/include/ur_print.hpp)<br><sub>include/ur_print.hpp</sub> |
| **12** | 8 | 28 | 397 (2025-04-05→2026-05-07) | godot42x_ya | [Render.h](file://~/oss/godot42x_ya/Engine/Source/Render/Render.h)<br><sub>Engine/Source/Render/Render.h</sub> |
| **11** | 9 | 20 | 28 (2026-03-27→2026-04-24) | mne-tools_mne-cpp | [channelrhiview.h](file://~/oss/mne-tools_mne-cpp/src/libraries/disp/viewers/helpers/channelrhiview.h)<br><sub>src/libraries/disp/viewers/helpers/channelrhiview.h</sub> |
| **9** | 9 | 10 | 625 (2024-06-08→2026-02-23) | netxms_netxms | [nms_objects.h](file://~/oss/netxms_netxms/src/server/include/nms_objects.h)<br><sub>src/server/include/nms_objects.h</sub> |
| **9** | 9 | 9 | 578 (2024-10-18→2026-05-19) | carbon-language_carbon-lang | [compile_subcommand.h](file://~/oss/carbon-language_carbon-lang/toolchain/driver/compile_subcommand.h)<br><sub>toolchain/driver/compile_subcommand.h</sub> |
| **9** | 9 | 11 | 421 (2024-11-20→2026-01-15) | oneapi-src_unified-runtime | [platform.hpp](file://~/oss/oneapi-src_unified-runtime/source/adapters/level_zero/platform.hpp)<br><sub>source/adapters/level_zero/platform.hpp</sub> |
| **9** | 7 | 19 | 36 (2026-03-16→2026-04-21) | godot42x_ya | [DeferredRenderPipeline.h](file://~/oss/godot42x_ya/Engine/Source/Runtime/App/DeferredRender/DeferredRenderPipeline.h)<br><sub>Engine/Source/Runtime/App/DeferredRender/DeferredRenderPipeline.h</sub> |
| **8** | 7 | 10 | 621 (2024-09-11→2026-05-25) | jwmcglynn_donner | [ImageComparisonTestFixture.h](file://~/oss/jwmcglynn_donner/donner/svg/renderer/tests/ImageComparisonTestFixture.h)<br><sub>donner/svg/renderer/tests/ImageComparisonTestFixture.h</sub> |
| **8** | 8 | 11 | 558 (2024-11-04→2026-05-16) | esbmc_esbmc | [python_annotation.h](file://~/oss/esbmc_esbmc/src/python-frontend/python_annotation.h)<br><sub>src/python-frontend/python_annotation.h</sub> |
| **8** | 6 | 16 | 524 (2024-12-21→2026-05-29) | gwdevhub_GWToolboxpp | [ToolboxUIElement.h](file://~/oss/gwdevhub_GWToolboxpp/GWToolboxdll/ToolboxUIElement.h)<br><sub>GWToolboxdll/ToolboxUIElement.h</sub> |
| **8** | 8 | 11 | 520 (2024-06-26→2025-11-28) | MeshInspector_MeshLib | [MRUIStyle.h](file://~/oss/MeshInspector_MeshLib/source/MRViewer/MRUIStyle.h)<br><sub>source/MRViewer/MRUIStyle.h</sub> |
| **8** | 7 | 11 | 441 (2025-02-18→2026-05-05) | esbmc_esbmc | [solidity_convert.h](file://~/oss/esbmc_esbmc/src/solidity-frontend/solidity_convert.h)<br><sub>src/solidity-frontend/solidity_convert.h</sub> |
| **7** | 5 | 13 | 558 (2024-10-04→2026-04-15) | official-pikafish_Pikafish | [search.h](file://~/oss/official-pikafish_Pikafish/src/search.h)<br><sub>src/search.h</sub> |
| **7** | 5 | 15 | 545 (2024-09-21→2026-03-20) | casadi_casadi | [fmu_impl.hpp](file://~/oss/casadi_casadi/casadi/core/fmu_impl.hpp)<br><sub>casadi/core/fmu_impl.hpp</sub> |
| **7** | 6 | 17 | 514 (2024-08-19→2026-01-15) | colmap_colmap | [bundle_adjustment.h](file://~/oss/colmap_colmap/src/colmap/estimators/bundle_adjustment.h)<br><sub>src/colmap/estimators/bundle_adjustment.h</sub> |
| **7** | 7 | 7 | 507 (2024-10-17→2026-03-08) | MeshInspector_MeshLib | [MRSurfaceManipulationWidget.h](file://~/oss/MeshInspector_MeshLib/source/MRViewer/MRSurfaceManipulationWidget.h)<br><sub>source/MRViewer/MRSurfaceManipulationWidget.h</sub> |
| **7** | 7 | 7 | 421 (2024-10-29→2025-12-24) | mod-playerbots_mod-playerbots | [RandomPlayerbotMgr.h](file://~/oss/mod-playerbots_mod-playerbots/src/RandomPlayerbotMgr.h)<br><sub>src/RandomPlayerbotMgr.h</sub> |

## Флагман: `PlayerbotAIConfig.h` (mod-playerbots)

**39 коммитов** за **707 дней** (2024-06 → 2026-05) добавили 49 bool-флагов — почти все по одному, каждый под свою фичу. Никто не виноват по отдельности; структура распухла капля за каплей. Это и есть измеримый дрейф:

| Дата | Добавлено | (фича) |
|---|---|---|
| 2024-06-22 | `disableDeathKnightLogin` | |
| 2024-06-27 | `randomBotFixedLevel` | |
| 2024-07-05 | `sayWhenCollectingItems` | |
| 2024-07-10 | `botRepairWhenSummon` | |
| 2024-07-11 | `allowSummonInCombat,allowSummonWhenMasterIsDead,al` | |
| 2024-07-12 | `reviveBotWhenSummoned` | |
| 2024-07-27 | `randomBotTalk,randomBotEmote` | |
| 2024-07-28 | `randomBotGuildTalk` | |
| 2024-08-01 | `enableBroadcasts,enableGreet,randomBotSayWithoutMa` | |
| 2024-08-05 | `fastReactInBG` | |
| 2024-08-14 | `dynamicReactDelay` | |
| 2024-08-19 | `applyInstanceStrategies` | |
| 2024-09-04 | `twoRoundsGearInit` | |
| 2024-10-03 | `botActiveAloneAutoScale` | |
| 2024-10-09 | `botActiveAloneSmartScale` | |
| 2024-11-30 | `enableNewRpgStrategy` | |
| 2024-12-06 | `dropObsoleteQuests` | |
| 2024-12-15 | `BotActiveAloneForceWhenInMap` | |
| 2024-12-15 | `BotActiveAloneForceWhenInZone,BotActiveAloneForceW` | |
| 2024-12-18 | `BotActiveAloneForceWhenInGuild` | |
| 2024-12-19 | `BotActiveAloneForceWhenInGuild` | |
| 2025-02-09 | `limitTalentsExpansion` | |
| 2025-03-01 | `enablePeriodicOnlineOffline` | |
| 2025-03-28 | `enableRandomBotTrading` | |
| 2025-05-24 | `disabledWithoutRealPlayer` | |
| 2025-05-24 | `keepAltsInGroup` | |
| 2025-06-11 | `EnableICCBuffs` | |
| 2025-07-05 | `incrementalGearInit` | |
| 2025-07-27 | `restrictHealerDPS` | |
| 2025-08-03 | `randomBotLogoutOutsideLoginRange` | |
| 2025-09-06 | `enableWeightTeleToCityBankers` | |
| 2025-12-27 | `enableFishingWithMaster` | |
| 2026-02-23 | `allowLearnTrainerSpells` | |
| 2026-03-20 | `lootGreedRollLevel,lootRollRecipe,lootRollDisencha` | |
| 2026-04-24 | `preferClassArmorType,preferredSpecWeapons` | |
| 2026-05-09 | `enableAutoTradeOnItemMention` | |
| 2026-05-30 | `botSendMailEnabled` | |
| 2026-05-30 | `equipAndSpecPersistence` | |
| 2026-05-30 | `resetInstanceIdForAltBots` | |

## Вывод

Это **самый сильный и честный результат всего исследования** и он переопределяет вердикт #089:

- Статический счётчик «5+ bool» — шум (78% конфиги).
- Один коммит с кучей булей — фича, не дрейф.
- **Инкрементальное накопление булей в один заголовок через много коммитов во времени — реальный, измеримый сигнал дрейфа.** Он temporal, name-independent, дёшево считается из git (число коммитов-добавлений на файл) и прямо отвечает первопричине проекта (constraint decay).

Для archcheck это укладывается в его суть лучше статической проверки: это **drift-метрика во времени** (как cycles/coupling drift #086/#087), а не линтер-проверка одного среза. Кандидат-правило: «заголовок X накопил N bool-полей за M коммитов — рассмотрите enum/State».

