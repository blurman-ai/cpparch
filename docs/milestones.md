# Validation milestones — реальные прогоны archcheck

Хронологический лог боевых прогонов tool-а на реальных проектах. Каждая запись
фиксирует масштаб, домен, результат и инсайты для следующих итераций.

Шаблон записи — в конце документа.

---

## 2026-05-26 — Старт разработки и первые dogfood-прогоны

Стартовала разработка archcheck (после периода спецификации). Текущий статус:
- `scan/` подсистема: `include_scanner` (textual, без libclang), `discover_files`,
  `include_resolver` — `Project / External / Unresolved / Ambiguous`.
- `graph/` подсистема: `DependencyGraph` (forward + reverse adjacency),
  `compute_scc` (iterative Tarjan), `reachable_from`, `reverse_reachable_from`,
  `has_path`.
- CLI: preview-команды `archcheck --scan <path>` и `archcheck --graph <path>`.
- Покрытие: 88/88 unit + integration тестов, lizard зелёный
  (`--CCN 15 --length 30 --arguments 5`).

**Версия archcheck:** commit `c9ba9e0` (после #008a–h, #011, #012, #013, #014).

### Прогон 1 — archcheck сам в себе (self-dogfood)
- **Масштаб:** 28 project files (включая 6 тестовых фикстур).
- **Домен:** CLI tool, C++20, CMake + Catch2, ~1500 LOC исходников.
- **Команда:** `archcheck --graph .`
- **Время:** < 0.1 s wall clock.
- **Результат:**
  - nodes 28 / edges 35 / **sccs_cyclic 0** (граф ацикличен).
  - external 79 / unresolved 5 / ambiguous 0 / macro_includes 1.
- **Заметка:** 5 unresolved — это наши же фикстуры
  (`fixtures/scan/include_scanner/*.cpp`) с намеренными
  `#include "real.h"` / `"split.h"` / `"local.h"` для проверки сканера.
  Когда появится `.archcheckignore` (или config-режим exclude), фикстуры
  стоит исключить из dogfood-а.

### Прогон 2 — `gm` (внешний C++ проект)
- **Масштаб:** 2192 project files (после exclude `build/` на любой глубине;
  без него — 2367).
- **Домен:** Qt-based desktop симулятор / control-station, multi-component
  CMake monorepo. Включает векторизованный Unigine SDK как third-party
  headers, Pixar USD plugin templates, кастомные network/audio/joystick
  компоненты.
- **Команда:** `archcheck --graph /home/localadm/projects/gm`
- **Время:** 7.7 s wall clock (Debug build, single core, без параллелизма).
- **Результат:**
  - nodes 2192 / edges 3538.
  - external 2388 / unresolved 708 / ambiguous 138 / macro_includes 0.
  - **sccs_cyclic 2 / largest_scc 3.**
- **Найденные циклы (оба внутри Unigine SDK headers, не в коде `gm`):**

  **SCC размер 3** (по форме — два сцепленных 2-цикла через `UnigineBase.h`):
  ```
  UnigineBase.h:105   #include "UnigineLog.h"
  UnigineBase.h:313   #include "UnigineMemory.h"
  UnigineLog.h:16     #include "UnigineBase.h"
  UnigineMemory.h:16  #include "UnigineBase.h"
  ```

  **SCC размер 2** (классический mutual include):
  ```
  UnigineMathLib.h:2641   #include "UnigineMathLib2d.h"
  UnigineMathLib2d.h:16   #include <UnigineMathLib.h>
  ```

  Циклы проверены grep-ом по исходникам, не false positive.

- **708 unresolved breakdown:** значительная часть — Pixar USD code-generation
  templates (`bin/plugins/.../codegenTemplates/*.h` с jinja-плейсхолдерами
  вроде `"{{ libraryPath }}/api.h"`). Это не C++, а шаблоны для генерации.
  Остальное — третьесторонние пути (`pxr/...`), которые не лежат в репо.
- **138 ambiguous breakdown:**
  - `<process.h>` → 36 кандидатов (Windows API vs локальные `process.h`).
  - `<utils.h>` → 39 кандидатов (типичный helper-name в monorepo).
  - `<pch.h>` → 2 кандидата (per-module precompiled headers).

### Инсайты с первых прогонов
- Tool работает на боевом коде уже сегодня: без `compile_commands.json`,
  без libclang, 7.7 s на 2200 файлов в Debug-сборке.
- Выходы интерпретируемы: file-paths циклов, конкретные токены unresolved,
  candidate counts для ambiguous. Можно сразу принимать решения (исключить
  third-party? выставить baseline? обновить структуру?).
- Подтвердился риск из spec §«Юзабилити дефолтных правил»: первый запуск
  на legacy-проекте сразу даёт «много шума». Нужны: (а) exclude
  паттерны, (б) `--baseline` режим, (в) git-diff анализ (#018).
- Tarjan iterative корректно работает на реальном графе (2192 узла), не
  упал по stack. Детерминизм output подтверждён.

---

## 2026-05-28 — Первые прогоны на известных OSS-проектах: fmt и spdlog

**Версия archcheck:** commit `c480e39`

### Прогон 3 — `fmt` (fmtlib/fmt)
- **Масштаб:** 72 project files.
- **Домен:** Строковое форматирование, header-only + compiled modes. C++11/14/17/20.
- **Команда:** `archcheck --graph /tmp/fmt` + `archcheck /tmp/fmt`
- **Commit:** `0acf106c52f5c7f068ce6313f2ca310c7d5e8b63`
- **Время:** 0.19 s + 0.28 s wall clock.
- **Результат (--graph):**
  - nodes 72 / edges 157 / **sccs_cyclic 0** (граф ацикличен).
  - external 311 / unresolved 9 / ambiguous 0 / macro_includes 0.
- **Результат (default rules) — 16 нарушений:**
  - **SF.7 (10):** все в `include/fmt/base.h`, `chrono.h`, `format.h`, `compile.h`, `format-inl.h` + один в `test/gtest/gmock/gmock.h`. Паттерн: `using namespace fmt::v11::detail` внутри implementation headers — намеренный приём. Не баг архитектуры.
  - **SF.8 (6):** `include/fmt/core.h` — заглушка-редирект на `base.h` (deprecated compatibility shim), не настоящий заголовок; gtest/gmock-заголовки без guard; `test/scan.h`, `test/util.h` — тест-хелперы.
  - **GodHeader: 0, ChainLength: 0, SF.9 (cycles): 0.**
- **Вывод:** fmt — эталонно чистый проект по include-архитектуре. Нулевые циклы, нет god-headers, нет длинных цепочек. Все SF.7/SF.8 находки объяснимы специальными паттернами или third-party (gtest). Подтверждает роль fmt как baseline-референса.

---

### Прогон 4 — `spdlog` (gabime/spdlog)
- **Масштаб:** 149 project files.
- **Домен:** Logging library, C++11/14/17. Поддерживает header-only и compiled режимы одновременно.
- **Команда:** `archcheck --graph /tmp/spdlog` + `archcheck /tmp/spdlog`
- **Commit:** `2e71fdf3c1e8d6299b8129e402ab705e6fb494ba`
- **Время:** 0.09 s + 0.16 s wall clock.
- **Результат (--graph):**
  - nodes 149 / edges 439 / **sccs_cyclic 22** / largest_scc 2.
  - external 305 / unresolved 2 / ambiguous 0 / macro_includes 0.
- **Результат (default rules) — нарушения:**
  - **SF.9 / cycles (22):** ВСЕ имеют форму `foo-inl.h ↔ foo.h`. Это намеренный паттерн dual-mode: в compiled-режиме `spdlog.h` включает `*-inl.h` которые back-include соответствующий `.h`. Технически цикл — архитектурно это форма conditional inline. Не баг, но SF.9 выдаёт правомерно.
  - **SF.7 (10):** `include/spdlog/fmt/bundled/` — встроенная копия fmt; `sinks/win_eventlog_sink.h` — один `using namespace`. Bundled fmt объясняет большинство.
  - **SF.8 (1):** `fmt/bundled/core.h` — тот же deprecated redirect что и в fmt.
  - **GodHeader (2):** `common.h` fan-in **38** (порог 30), `details/null_mutex.h` fan-in **31**. Реальные god-headers — почти весь проект зависит от `common.h`.
  - **ChainLength (много):** глубина 11–15 у sink-файлов и bench/. Обусловлено embedded fmt + разветвлённым include-деревом spdlog.
- **Ключевой инсайт:** "22 цикла" — не провал дизайна, а fingerprint dual-mode паттерна `-inl.h`. При появлении конфига нужен exclude для `*-inl.h`-пар или специальная настройка threshold для этого паттерна. `common.h` как god-header — настоящая находка: это центр тяжести всего графа.

---

### Сравнительная таблица fmt vs spdlog

| Метрика | fmt | spdlog |
|---|---|---|
| Nodes | 72 | 149 |
| Edges | 157 | 439 |
| Cyclic SCCs | **0** | **22** |
| God-headers | 0 | 2 (`common.h` fan-in 38) |
| ChainLength violations | 0 | ~30 |
| SF.7 | 10 (intentional) | 10 (bundled fmt) |
| SF.8 | 6 (deprecated + gtest) | 1 (bundled fmt) |
| Scan time (Debug) | 0.28 s | 0.16 s |

---

## Шаблон записи

```
## YYYY-MM-DD — <короткое название milestone-а>

**Версия archcheck:** commit `XXXXXXX`

### Прогон 1 — <Project name>
- **Масштаб:** N project files
- **Домен:** <чем занимается проект, 1-2 строки>
- **Команда:** `archcheck --graph <path>` (или другая)
- **Время:** N s wall clock
- **Результат:**
  - nodes / edges / sccs_cyclic / largest_scc
  - external / unresolved / ambiguous / macro_includes
- **Найденные циклы / нарушения:** <конкретика, file paths>
- **Инсайты:** <что узнали, что менять, какие задачи это рождает>
```
