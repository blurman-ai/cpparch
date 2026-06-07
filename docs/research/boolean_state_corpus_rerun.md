# Boolean-State Rule — повторный прогон (улучшённое правило)

**Что флагает правило сейчас:** 1 структур (было 59 при «голом» счётчике 5+ bool).

**Применённые фильтры:** глубина-0 (только поля, не локальные переменные), исключение вендора, исключение конфиг-имён (*Settings/*Config/*Cache/...), стоп-поля has_*/use_*/can_*/*_enabled, state-ratio ≥ 60%.

| Struct | Bools | State | Ratio | File |
|---|---|---|---|---|
| SimulationData | 6 | 4 | 66% | [simulation_data.hpp](file:///home/localadm/oss/EVerest_EVerest/modules/EV/EvManager/main/simulation_data.hpp) |

## Поля по каждому

- **SimulationData** (6 bool, 4 state-like, 66%) — [simulation_data.hpp](file:///home/localadm/oss/EVerest_EVerest/modules/EV/EvManager/main/simulation_data.hpp)
  `v2g_finished, iso_stopped, iso_charger_paused, iso_pwr_ready, bcb_toggle_C, dc_power_on`
  → Реальный TP: фазы симуляции зарядной сессии EV (`finished`/`stopped`/`paused`/`ready`) + 2 ортогональных тумблера. State-подмножество явное.

---

## Before / After

| | Голый счётчик (5+ bool) | Улучшённое правило |
|---|---|---|
| Кандидатов | 59 | **1** |
| Из них «оставить bool» (шум) | ~46 (78%) | 0 |
| FP экстрактора (локальные переменные) | 2 (+8 ETL-таймеров с мусором) | 0 (глубина-0) |
| Вендоренные либы | ≥7 | 0 (исключены) |
| Точность по флагам | ~19% | 100% (1/1 — реальный TP) |

## Что сработало

1. **Глубина-0** — все ETL-таймеры (`bool result`/`success` в телах методов) и `CodeGenerator`/`bit_stream` отсеялись: считаются только прямые поля.
2. **Исключение вендора** — `cgltf`, `Catch2`, `cpp-peglib`, `tclap`, `mosquitto`, `bitstream_state` (он в `third_party/`) ушли из выборки.
3. **Имена-конфиги** — `SettingsCache` (76!), `*Options`, `*ConfigData`, `*Arguments` отсеяны по суффиксу имени структуры.
4. **Стоп-поля** — `has_*`/`use_*`/`can_*`/`*_enabled`/`*UpToDate*` больше не считаются state, поэтому capability-мешки (`cgltf_material`, `VMStructs`) не добирают ratio.

## Цена: recall

Правило стало почти «немым» — 1 флаг на 50 репо. Сильные 🟡-кейсы из [анализа](boolean_state_enum_analysis.md) **не сработали** по объективным причинам:

- **bitstream_state** — в `third_party/` → корректно пропущен (вендор, чинить нельзя).
- **ImageBackingStore** (`m_israw/m_isrom/m_isfolder`), **VM** (`_hotspot/_openj9/_zing`), **db** (`transaction_started`), **BedrockCommand** (`_inDBRead/WriteOperation`) — их взаимоисключающее подмножество **не названо lifecycle-словами** и/или составляет <60% полей. Нейминг их не ловит.

Это ровно тот вывод, что заложен в [proposal](boolean_state_drift_proposal.md): **нейминг-эвристика даёт высокую точность, но низкий recall; чтобы ловить state-машины с «нелайфцикловыми» именами (raw/rom/folder, hotspot/zing), нужен семантический анализ взаимоисключения (if/else-if + групповое присваивание) — semantic backend (#042).**

Для CI это приемлемый компромисс на v0.2: правило не шумит (избегаем «5000 нарушений на первом прогоне»), флагает только очевидные state-машины. Recall добираем в v0.3 через #042.
