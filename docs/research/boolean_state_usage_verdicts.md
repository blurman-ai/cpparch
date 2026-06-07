# Boolean-State — вердикты по ИСПОЛЬЗОВАНИЮ (не по именам)

**Дата:** 2026-06-07
**Задача:** #089 / #090
**Выборка:** 24 самых перспективных кандидата (non-vendor, 3+ state-подобных имени, ≥50%, ≤12 bool) из широкого прогона 790 репо + 5 контрольных гигантов.
**Метод:** агенты читали реальный код и grep использования каждого bool-поля. Вердикт — по СИГНАЛАМ использования (взаимоисключение `if/else-if`, групповое присваивание перехода, комбинаторный разрыв), а НЕ по именам.

## Итог

| Вердикт | Кол-во из 29 |
|---|---|
| 🟢 чистая машина состояний (почти все були — фазы) | **0** |
| 🟡 частично (есть взаимоисключающее подмножество) | **8** |
| 🔴 не машина состояний (независимые флаги/конфиг/биты) | **17** |
| ⚪ мёртвый код / FP | **2** |

## Три вывода, меняющие картину

### 1. Даже среди ЛУЧШИХ по неймингу кандидатов — 0 чистых FSM
Мы целенаправленно отобрали 24 структуры, которые *по именам* максимально похожи на машины состояний. По факту использования: ни одной 🟢, лишь ~28% имеют извлекаемое state-подмножество (🟡), большинство — независимые флаги.

### 2. Единственный флаг v0.2-правила (`SimulationData`) — это FALSE POSITIVE
Правило с нейминг-эвристикой пометило ровно `SimulationData` (EVerest) как TP. Анализ использования: настоящая машина состояний там **уже в `enum SimState`**, а 6 булей (`iso_pwr_ready`, `dc_power_on`, `v2g_finished`…) — независимые event/intent-сигналы, которые *драйвят* переходы, но сами состоянием не являются (часто consume-once). → Точность v0.2 на практике **0/1**, а не 100%.

### 3. Зрелые проекты УЖЕ держат состояние в enum — а булы рядом ортогональны
Повторяющийся паттерн: настоящая FSM вынесена в отдельный `enum`, и именно поэтому оставшиеся булы — НЕ состояние:
- `TransformOperator` → `enum TransformState`; `vrrp_router` → `fsm.state`; `I2C_helper` → `i2c_internal_state_e`; `SimulationData` → `SimState`; `mosquitto` → `mosquitto_client_state`; `BufferMgrDynamic` → `profile_state_t`/`port_state_t`.

Это объясняет «0 чистых 🟢» механически: как только разработчик осознаёт состояние — он заводит enum. Bool-суп-как-состояние в зрелом коде вырождается в «настоящая FSM в enum + ортогональные булы вокруг».

### Что это значит для вердикта #089
Гипотезу «рост boolean-state = drift-сигнал» приходится **смягчить с YES → ближе к MAYBE**: явление существует, но (а) реже и ортогональнее, чем предполагалось; (б) нейминг-детект почти бесполезен (его единственный «улов» — FP); (в) даже usage-детект даёт в основном *подмножества* (🟡), а не целые машины. Реальная ценность правила = найти 🟡-ядра (групповое присваивание), что требует **semantic backend (#042)**. Подробности в [proposal](boolean_state_drift_proposal.md).

---

## 🟡 Частично — есть извлекаемое взаимоисключающее подмножество (8)

### MonocularInertialSlamNode (7 bool) — `eigendude_OASIS` ⭐ сильнейший
- **Файл:** [MonocularInertialSlamNode.h](file:///home/localadm/oss/eigendude_OASIS/oasis_perception_cpp/src/nodes/MonocularInertialSlamNode.h)
- **Сигнал:** Классическое групповое присваивание перехода: вход в recovery `recoveryPending=true; initRetryPending=false; stableInputPaused=false; startupArmed=false`; в retry/arming — аналогично один true, остальные гасятся; полный сброс гасит все 4. Комбинаторный разрыв 16→единицы.
- **Предлагается:** `enum class TrackingPhase { StartupUnarmed, Armed, StableInputPaused, PostStallRecovery, InitRetryBackoff }`. `m_imageWorkerStop/Wake` (CV-сигналы) и диагностическую защёлку — оставить bool.

### VisualScriptPropertySelector (12 bool) — `RebelToolbox_RebelEngine`
- **Файл:** [visual_script_property_selector.h](file:///home/localadm/oss/RebelToolbox_RebelEngine/modules/visual_script/visual_script_property_selector.h)
- **Сигнал:** Тройка `{properties, seq_connect, visual_script_generic}` всегда задаётся ГРУППОЙ в сеттерах `select_from_*` и читается как селектор в `_update_search` (`if(properties||seq_connect)`, `if(seq_connect && !visual_script_generic)`). Из 2³ встречается ~5.
- **Предлагается:** `enum class SelectorMode {…}` по реальным комбинациям; `connecting/virtuals_only` — bool.

### chiaki_stream_connection_t (5 bool) — `streetpea_chiaki-ng`
- **Файл:** [streamconnection.h](file:///home/localadm/oss/streetpea_chiaki-ng/lib/include/chiaki/streamconnection.h)
- **Сигнал:** `state_finished`/`state_failed` — взаимоисключающая пара исхода под мьютексом, групповой сброс перед каждым шагом. Линейный прогресс при этом в `int state`.
- **Предлагается:** `enum class StepOutcome { Pending, Succeeded, Failed }`; `should_stop/remote_disconnected/feedback_sender_active` — bool.

### BufferMgrDynamic (8 bool) — `sonic-net_sonic-swss`
- **Файл:** [buffermgrdyn.h](file:///home/localadm/oss/sonic-net_sonic-swss/cfgmgr/buffermgrdyn.h)
- **Сигнал:** 4 init-флага — монотонный прогресс `pending→poolReady→portInitDone→completed` (частичный групповой откат). Остальные 4 — независимые capability. Профайл/порт уже имеют свои enum.
- **Предлагается:** `enum class InitPhase {…}` для прогресса; `m_support*` и пр. — bool.

### WiFiManager (6 bool) — `FujiNetWIFI_fujinet-firmware`
- **Файл:** [fnWiFi.h](file:///home/localadm/oss/FujiNetWIFI_fujinet-firmware/lib/hardware/fnWiFi.h)
- **Сигнал:** Ось «фаза подключения» `_scan_in_progress/_trying_stored/_all_stored_failed` разруливается `if/else if`. `_started/_connected/_disconnecting` ортогональны.
- **Предлагается:** `enum class ConnectPhase { idle, scanning, connecting_current, trying_stored, all_failed }`; остальное — bool.

### mt76x02_calibration (6 bool) — `openwrt_mt76`
- **Файл:** [mt76x02.h](file:///home/localadm/oss/openwrt_mt76/mt76x02.h)
- **Сигнал:** В основном независимые `*_done` латчи прогресса калибровки (накапливаются). Лёгкое подмножество — `tssi_cal_done`+`tssi_comp_pending`.
- **Предлагается:** в основном bool; опционально `enum` для стадии TSSI. Выгода мала.

### MM_GCExtensionsBase (104 bool) — `eclipse-omr_omr` (гигант)
- **Файл:** [GCExtensionsBase.hpp](file:///home/localadm/oss/eclipse-omr_omr/gc/base/GCExtensionsBase.hpp)
- **Сигнал:** Мешок опций -Xgc, НО внутри — взаимоисключающий селектор политики `_isStandardGC/_isVLHGC/_isMetronomeGC/_isSegregatedHeap` (ровно один true per-config).
- **Предлагается:** `enum GcPolicy` для 4 полей (низкий приоритет: API намеренно compile-time через `#if`). Остальные ~100 — bool.

### DxcOpts (73 bool) — `microsoft_DirectXShaderCompiler` (гигант)
- **Файл:** [HLSLOptions.h](file:///home/localadm/oss/microsoft_DirectXShaderCompiler/include/dxc/Support/HLSLOptions.h)
- **Сигнал:** Мешок CLI-флагов, но есть явно-исключающие пары с runtime-проверкой «оба заданы → ошибка»: `DefaultColMajor/DefaultRowMajor`, `AvoidFlowControl/PreferFlowControl`.
- **Предлагается:** `enum {Default,Col,Row}` / `enum FlowControl` для пар; масса остального — bool 1:1 с argv.

---

## 🔴 Не машина состояний — независимые флаги/конфиг/биты (17)

- **ipahal_reg_tx_wrapper** (11) — [ipahal_reg.h](file:///home/localadm/oss/LineageOS_android_kernel_asus_sm8350/techpack/dataipa/drivers/platform/msm/ipa/ipa_v3/ipahal/ipahal_reg.h) — зеркало hardware-регистра, независимые idle/empty-биты подблоков.
- **TestState** (12) — [test_state.h](file:///home/localadm/oss/aegis-aead_boringssl/ssl/test/test_state.h) — независимые one-shot «callback-ready» латчи теста, свободно комбинируются.
- **coronAlign** (11) — [coronAlign.hpp](file:///home/localadm/oss/magao-x_MagAOX/gui/widgets/coronAlign/coronAlign.hpp) — N пер-осевых pending/refresh латчей; настоящее состояние в строковом `m_*State` от контроллера.
- **Schema_info** (10) — [dump_reader.h](file:///home/localadm/oss/mysql_mysql-shell/modules/util/load/dump_reader.h) — монотонные прогресс-защёлки + `has_*` capability из JSON.
- **wrapper_ostream** (9) — [ostream-wrapper.h](file:///home/localadm/oss/dovecot_core/src/lib/ostream-wrapper.h) — накопительный lifecycle + ортогональные reentrancy-guard (bitfield), сосуществуют.
- **TransformOperator** (8) — [TransformOperator.h](file:///home/localadm/oss/fernandotonon_QtMeshEditor/src/TransformOperator.h) — FSM уже в `enum TransformState`; булы — конфиг + независимые drag-флаги.
- **ShtUsermod** (8) — [ShtUsermod.h](file:///home/localadm/oss/wled_WLED/usermods/sht/ShtUsermod.h) — конфиг + idempotent `*Done`-защёлки разных подсистем.
- **I2C_helper** (8) — [i2c_helper.hpp](file:///home/localadm/oss/gvsoc_gvsoc-core/models/devices/i2c/helper/i2c_helper.hpp) — FSM уже в `i2c_internal_state_e`; булы — intent/pin-driver/guard (+2 мёртвых).
- **ChargePointImpl** (6) — [charge_point_impl.hpp](file:///home/localadm/oss/EVerest_EVerest/lib/everest/ocpp/include/ocpp/v16/charge_point_impl.hpp) — независимые флаги + log-once защёлки (+1 мёртвый).
- **SimulationData** (6) — [simulation_data.hpp](file:///home/localadm/oss/EVerest_EVerest/modules/EV/EvManager/main/simulation_data.hpp) — **FP v0.2-правила**: FSM уже в `enum SimState`, булы — независимые intent-сигналы (consume-once).
- **WiFiManager-смежные / DATALAYER_SYSTEM_STATUS_TYPE** (6) — [datalayer.h](file:///home/localadm/oss/dalathegreat_Battery-Emulator/Software/src/datalayer/datalayer.h) — permission-биты от независимых акторов (батарея1/2/3, инвертор), `&&`-агрегация.
- **CustomAgent** (5) — [AgentRenderer.h](file:///home/localadm/oss/gwdevhub_GWToolboxpp/GWToolboxdll/Widgets/Minimap/AgentRenderer.h) — independent override-чекбоксы рендера.
- **MCPServer** (5) — [MCPServer.h](file:///home/localadm/oss/fernandotonon_QtMeshEditor/src/MCPServer.h) — флаги 4 разных подсистем (stdio/MCP/Ogre/HTTP).
- **CameraInputState** (5) — [RCameraController.hpp](file:///home/localadm/oss/solvcon_modmesh/cpp/modmesh/pilot/RCameraController.hpp) — сигналы ввода, именно комбинируются (chord-жесты).
- **vrrp_router** (5) — [vrrp.h](file:///home/localadm/oss/FRRouting_frr/vrrpd/vrrp.h) — FSM уже в `fsm.state`; булы — is_active/is_owner + независимые pending-пакеты.
- **rc2014Fuji** (5) — [rc2014Fuji.h](file:///home/localadm/oss/FujiNetWIFI_fujinet-firmware/lib/device/rc2014/rc2014Fuji.h) — независимые busy/scan/ssid/config маркеры.
- **TeslaBattery** (215) / **PQCSettings** (199) / **SettingsCache** (76) — [TESLA-BATTERY.h](file:///home/localadm/oss/dalathegreat_Battery-Emulator/Software/src/battery/TESLA-BATTERY.h), [pqc_settings.h](file:///home/localadm/oss/luspi_photoqt/cplusplus/header/pqc_settings.h), [cache_settings.h](file:///home/localadm/oss/Cockatrice_Cockatrice/cockatrice/src/client/settings/cache_settings.h) — CAN-телеметрия / Qt property-bag / UI-preferences. Чистые data/config-мешки.

## ⚪ Мёртвый код / FP (2)

- **DatabaseManager** (7) — [DatabaseManager.h](file:///home/localadm/oss/OpenMagnetics_MKF/src/support/DatabaseManager.h) — 7 `_*Loaded` полей нигде не читаются/пишутся; `is*Loaded()` идёт через `!container.empty()`. Удалить.
- **macFuji** (6) — [macFuji.h](file:///home/localadm/oss/FujiNetWIFI_fujinet-firmware/lib/device/mac/macFuji.h) — недопиленный порт, 4 из 6 не используются.

---

## Что отсюда следует для правила

1. **Нейминг-детект подтверждённо тупиковый** — его единственный улов (`SimulationData`) оказался FP. Имена `israw/hotspot/recoveryPending` либо не совпадают со словарём, либо совпадают у не-состояний.
2. **Реальный сигнал — групповое присваивание перехода** (`a=true; b=false; c=false;` в одном месте) + взаимоисключающие `if/else-if`. Именно он отделил MonocularInertialSlamNode/VisualScriptPropertySelector/chiaki от шума. Это требует AST/dataflow → **#042**.
3. **Полезный детект должен ещё и проверять, нет ли УЖЕ enum-состояния рядом** — иначе будем флагать ортогональные булы у объектов, где FSM уже корректно вынесена (TransformOperator, vrrp, I2C, SimulationData).
4. Возможный «дешёвый» сигнал без полного AST: искать в `.cpp` **групповые присваивания** ≥3 булей одной структуры в одном блоке (`x.a=...; x.b=...; x.c=...;`) — это прокси для перехода состояния, не зависящий от имён.
