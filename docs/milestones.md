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
