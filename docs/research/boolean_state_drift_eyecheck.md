# Boolean-State DRIFT — ручная верификация (eye-check)

**Дата:** 2026-06-07 · **Задача:** #089
**Что проверяли:** 22 файла-кандидата из 19 агентских репо (топ по числу коммитов, добавлявших булы), отобранные из 219 «дрейф-файлов». По каждому — чтение кода + `git log` истории добавления булей. Вопрос: **растёт ли ОДНА содержательная структура** (реальный constraint decay), или это артефакт.

## Итог честно

| Вердикт | Кол-во из 22 | Доля |
|---|---|---|
| 🟢 **реальный дрейф** (одна содержательная структура копит флаги) | **5** | **23%** |
| 🟡 ожидаемый рост конфига (config/options/settings by design) | 7 | 32% |
| 🔴 FP (разные структуры / генерёнка / bool в сигнатурах / гигантский header) | 10 | 45% |

**Вывод: метрика «дрейф на уровне ФАЙЛА» хлипкая — точность ~23% на настоящий вредный дрейф.** Заявление «дрейф в 55/73 репо (75%)» сильно завышено: это «в файле прибавлялись булы», а не «одна структура распухала».

## Почему так много мимо

1. **Per-file ≠ per-struct.** Считал булы по файлу. Но `datalayer.h` — 6 доменных POD-структур; `datalayer_extended.h` — по структуре на каждую модель батареи; `LidarOdometry.h` — флаги разнесены по вложенным option-структурам. Файл «копит», структуры — нет. → главный источник FP.
2. **bool в сигнатурах методов и локальные.** `tree.hpp`/`graph.hpp` (hhds) — `+bool` это в основном `is_valid()`, `operator==`, аргументы `bool follow_subtrees`. `ur_print.hpp`, `python_annotation.h` — локальные `bool first=true;`/`bool found;` в теле. История-скан их не отфильтровал (нет скобок на строке).
3. **Гигантские многоклассовые заголовки.** `nms_objects.h` (6405 строк, 192 коммита) — 9 булей размазаны по десяткам классов.
4. **Генерёнка / реестры.** `ggml-webgpu-shader-lib.hpp` — по pipeline-структуре на операцию; `ur_print.hpp` — кодоген из API-спеки.
5. **Config-bags (🟡).** Растут в одну структуру, но им по дизайну положено (PlayerbotAIConfig, couchbase Settings, SentrySettings, carbon CompileOptions, workerd IsolateBase compat-flags). Это не архитектурный распад.

## 🟢 Подтверждённый реальный дрейф (5)

> Одна содержательная структура (god-class / виджет / доменный хэндл) медленно обрастает state/mode-флагами коммит за коммитом.

- **ToolboxUIElement.h** — [файл](file://~/oss/gwdevhub_GWToolboxpp/GWToolboxdll/ToolboxUIElement.h) — `gwdevhub_GWToolboxpp`. ⭐ Эталон: один UI-класс, ~24 bool, флаги фичу за фичей за 2 года (titlebar→breakout→mobile→collapse→snap).
- **platform.hpp** — [файл](file://~/oss/oneapi-src_unified-runtime/source/adapters/level_zero/platform.hpp) — `oneapi unified-runtime`. `ur_platform_handle_t_` — каждое новое расширение Level Zero добавляет `bool ...Supported{false}`, 2024→2026, по одному за коммит.
- **engine.hpp** — [файл](file://~/oss/ThomasGhione_chess_engine/engine/engine.hpp) — `ThomasGhione_chess_engine`. God-class `Engine` копит pondering/search-флаги координации (2025-10→2026-05).
- **channelrhiview.h** — [файл](file://~/oss/mne-tools_mne-cpp/src/libraries/disp/viewers/helpers/channelrhiview.h) — `mne-cpp`. UI-класс, ~20 bool, по флагу за коммит (crosshair/scalebars/butterfly/zscore…).
- **solidity_convert.h** — [файл](file://~/oss/esbmc_esbmc/src/solidity-frontend/solidity_convert.h) — `esbmc`. God-класс конвертера набрал режимные флаги (bound/reentry/pointer/unchecked) поверх состояния.

## 🟡 Рост конфига by design (7)

PlayerbotAIConfig.h, couchbase settings.h, SentrySettings.h, godot App.h (automation options), workerd setup.h (V8 compat-flags), carbon compile_subcommand.h (CLI options), donner ImageComparisonTestFixture.h (test params).

## 🔴 FP (10)

datalayer.h, datalayer_extended.h, LidarOdometry.h (флаги в разные структуры); hhds tree.hpp, hhds graph.hpp, esbmc python_annotation.h (bool в сигнатурах/локальные); ur_print.hpp, ggml-webgpu-shader-lib.hpp (генерёнка/реестр); nms_objects.h (гигантский header); MeshLib MRUIStyle.h (виджет-структуры + параметры функций).

## Что нужно, чтобы метрика перестала быть хлипкой

1. **Атрибуция по СТРУКТУРЕ, а не по файлу** — привязывать добавленный bool к конкретному struct/class (через контекст хунка/AST). Снимает FP №1 и №3.
2. **Считать только поля** — отбрасывать bool в сигнатурах методов и локальные в телах (как в статическом скане — depth-0). Снимает FP №2 и часть №4.
3. **Отделять config-bags** (🟡) от content-классов (🟢) — у первых рост ожидаем. Иначе сигнал тонет в настройках.
4. После этого — мерить «одна content-структура набрала ≥N bool за ≥M коммитов».

## Связь с вердиктом #089

Дрейф булей в одной структуре **существует и подтверждён глазами** (5 чётких случаев, ToolboxUIElement/platform.hpp — учебные). Но он **редкий и тонет в шуме**: сырой file-level детект даёт ~23% точности, ~45% — мусор, ~32% — ожидаемый рост конфига. Чтобы это стало пригодным сигналом, нужна per-struct атрибуция по git-истории (≈ направление #042 + diff-парсинг), а не file-level счётчик. Голые цифры «75% репо» — артефакт, снимаю.
