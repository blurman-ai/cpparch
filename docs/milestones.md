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

---

## 2026-05-28 — Прогоны на Catch2, nlohmann/json, abseil-cpp

**Версия archcheck:** commit `c480e39`

---

### Прогон 5 — `Catch2` (catchorg/Catch2)

- **Масштаб:** 414 project files.
- **Домен:** C++ unit test framework, header-based, современный C++17/20.
- **Commit:** `69e0473f6e98d47c93518424c08ee69ee632c0f0`
- **Время:** 0.23 s (graph) + 0.30 s (rules).

**Результат (--graph):**
- nodes 414 / edges 1305 / **sccs_cyclic 0** — полностью ацикличен.
- external 828 / unresolved 0 / ambiguous 0 / macro_includes 0.

**Результат (default rules) — 24 нарушения:**
- SF.7 (18), GodHeader (5), ChainLength (1), SF.9 (0).

**Разбор нарушений и ручная проверка:**

**SF.7 — частичные ложные срабатывания.**
Три категории:

1. `extras/catch_amalgamated.hpp` — 9 hits. Это **сгенерированный** single-header amalgam, не исходный код. Сканировать его бессмысленно. Нужен механизм исключения vendor/generated файлов.

2. `src/catch2/catch_tostring.hpp:270,291` — **ложное срабатывание**. Контекст (проверено вручную):
   ```cpp
   struct StringMaker<bool> {
       static std::string convert(bool b) {
           using namespace std::string_literals;  // внутри тела функции!
           return b ? "true"s : "false"s;
       }
   };
   ```
   `using namespace` — внутри тела метода, не на глобальном уровне. SF.7 срабатывает, так как наш сканер не отслеживает глубину вложенности `{}`. Баг в реализации.

3. `src/catch2/generators/catch_generators.hpp:251,255,259` — **ложное срабатывание**. Контекст:
   ```cpp
   #define GENERATE(...) \
     Catch::Generators::generate(...,
       [](){ using namespace Catch::Generators; return makeGenerators(__VA_ARGS__); })
   ```
   `using namespace` — внутри лямбды в макросе. Не глобальная область. Баг тот же.

4. `src/catch2/reporters/catch_reporter_automake.hpp:30` и три других reporter — **вероятное реальное нарушение**. Подлежит отдельной проверке.

**GodHeader — структурные, не архитектурные дефекты:**
- `catch_test_macros.hpp` fan-in **108** — проверено: 116 прямых `#include` по всему репо. Это главный тестовый макро-заголовок библиотеки тестирования — он и обязан быть везде. Аналог `<cassert>` в assert-тестах.
- `catch_move_and_forward.hpp` fan-in 58 — utility-враппер `std::move/forward`, ожидаемо везде.
- `catch_compiler_capabilities.hpp` fan-in 31, `catch_enforce.hpp` fan-in 31, `catch_stringref.hpp` fan-in 36 — базовые утилиты.

**Вывод:** Собственный C++ код Catch2 — **чистый**. Все осмысленные SF.7 попали либо в amalgam (исключить), либо оказались ложными из-за отсутствия brace-depth tracking. GodHeaders — структурные (тест-фреймворк по определению имеет центральный макро-заголовок).

---

### Прогон 6 — `nlohmann/json` (nlohmann/json)

- **Масштаб:** 491 project files.
- **Домен:** Header-only JSON library, C++11/14/17/20. Один основной заголовок `json.hpp` + детальные `detail/`.
- **Commit:** `d10879bca8f0aa790105446075a9525b34a3f718`
- **Время:** 0.43 s (graph) + 0.61 s (rules).

**Результат (--graph):**
- nodes 491 / edges **376** (edges < nodes!) / **sccs_cyclic 0** — ацикличен.
- external 1502 / unresolved 10 / **ambiguous 333** / macro_includes 0.

**Аномалия — edges < nodes:** граф — лес. nlohmann/json организован как звезда: `json.hpp` тянет всё, остальные `detail/` заголовки включают мало чего кроме базовых утилит. Большинство узлов — листья без входящих рёбер.

**Аномалия — 333 ambiguous:** проверено вручную. Причина:
```
/tmp/json/include/nlohmann/json.hpp         ← основной исходник
/tmp/json/single_include/nlohmann/json.hpp  ← сгенерированный amalgam
```
Оба файла с именем `json.hpp`. Любой `#include <nlohmann/json.hpp>` помечается как ambiguous. Аналогично для `json_fwd.hpp`. Нужен механизм исключения `single_include/` / `_amalgamated` директорий.

**Результат (default rules) — 20 нарушений:**
- SF.7 (10), SF.8 (4), GodHeader (1), ChainLength (5).

**Разбор:**

**SF.7 (10)** — **все 10 в `tests/thirdparty/`**: doctest (9) + LLVM Fuzzer (1). Ни одного нарушения в собственном коде nlohmann/json.

**SF.8 (4):**
- `tests/abi/include/nlohmann/json_v3_10_5.hpp` — снапшот старой версии для ABI-тестов, не настоящий заголовок.
- `tests/cmake_target_include_directories/project/Bar.hpp` — тестовый helper.
- `tests/thirdparty/doctest/doctest.h`, `Fuzzer/FuzzerMerge.h` — сторонний код.

**GodHeader `doctest_compatibility.h` fan-in 77** — проверено: 77 тестовых файлов его включают. Compat-shim для тест-фреймворка, аналогично `catch_test_macros.hpp`.

**ChainLength `json.hpp` depth 14** — по дизайну: это single-header библиотека, `json.hpp` включает весь `detail/`. Ожидаемо.

**Вывод:** Собственный C++ код nlohmann/json — **чистый**. Все нарушения: сторонний код (doctest, Fuzzer), сгенерированные снапшоты (ABI) или test helpers. Основные проблемы — инфраструктурные (ambiguous из-за `single_include/`), а не архитектурные.

---

### Прогон 7 — `abseil-cpp` (abseil/abseil-cpp)

- **Масштаб:** 892 project files.
- **Домен:** Google C++ base library. Модульная, строго ацикличная по политике Google.
- **Commit:** `e7a10c8ec2ab4a251d1523812f10318431f1a14a`
- **Время:** 0.81 s (graph) + 1.06 s (rules).

**Результат (--graph):**
- nodes 892 / edges 4115 / **sccs_cyclic 0** — ацикличен.
- external 3533 / unresolved 466 / ambiguous 0 / macro_includes 1.
- 466 unresolved — Abseil использует `<absl/...>` include-пути; без `-I` они не резолвятся. Не ошибка.

**Результат (default rules) — 489 нарушений:**
- ChainLength (383), SF.8 (85), GodHeader (21), SF.7 (0!), SF.9 (0).

**Разбор нарушений и ручная проверка:**

**SF.7 (0) — Abseil полностью чист по этому правилу.** Подтверждает что Google Code Style запрещает `using namespace`.

**SF.8 (85) — преимущественно ложные срабатывания.** Причина выявлена вручную:
```cpp
// absl/base/config.h — строки 1-47: Apache 2.0 copyright header
// Copyright 2017 The Abseil Authors.
// Licensed under the Apache License...
// ...16 строк лицензии + пустые строки = 47 непустых строк...
#ifndef ABSL_BASE_CONFIG_H_     ← строка 48 (вне нашего лимита kScanLines=30)
#define ABSL_BASE_CONFIG_H_
```
Наш лимит `kScanLines = 30` не хватает. Abseil следует Apache 2.0, у него длинный copyright блок. Guard реально есть — мы его просто не видим. **Баг в реализации SF.8.**

Дополнительно: `spinlock_linux.inc`, `spinlock_posix.inc` и др. — это `.inc` платформенные фрагменты без guard по дизайну (они подключаются ровно один раз через `#if PLATFORM`). Сканировать их на SF.8 некорректно.

**GodHeader — реальные, структурные:**
- `absl/base/config.h` fan-in **507** — проверено: 508 прямых `#include` по репо. Это фундаментальный конфиг-заголовок всей библиотеки. По смыслу — уровень `<stddef.h>`. Намеренно везде.
- `absl/base/attributes.h` fan-in **191** — аннотации для компиляторов (ABSL_ATTRIBUTE_*). Аналогично.
- `absl/base/macros.h` fan-in 95, `absl/base/optimization.h` fan-in 82, `absl/base/internal/raw_logging.h` fan-in 98 — все базовые утилитные заголовки Abseil.

Эти god-headers — **не баги**, это архитектура большой библиотеки с четким фундаментальным слоем. Порог 30 слишком мал для таких проектов.

**ChainLength (383):** самое глубокое — `absl/time/flag_test.cc` depth **25**. Путь: тест → flags → logging → time → синхронизация → ... Реальный, не ложный. В большой монолитной библиотеке с богатой семантикой тест-файлы закономерно имеют длинные цепочки.

**Вывод:** Abseil — реально чистый проект (0 SF.7, 0 SF.9). Большинство из 489 "нарушений" — артефакты двух конкретных багов в нашей реализации (SF.8 kScanLines слишком мал; GodHeader порог 30 не применим к библиотечным фундаментальным заголовкам). ChainLength в 383 файлах — технически нарушение, но ожидаемо для крупного монорепо.

---

### Сводная таблица пяти OSS-прогонов

| Проект | Nodes | Edges | Cyclic SCCs | SF.7 | SF.8 | GodHeader | ChainLength | Время |
|---|---|---|---|---|---|---|---|---|
| fmt | 72 | 157 | 0 | 10* | 6* | 0 | 0 | 0.28 s |
| spdlog | 149 | 439 | 22† | 10* | 1* | 2 | ~30 | 0.16 s |
| Catch2 | 414 | 1305 | 0 | 18‡ | 0 | 5§ | 1§ | 0.30 s |
| nlohmann/json | 491 | 376 | 0 | 10* | 4* | 1§ | 5§ | 0.61 s |
| abseil-cpp | 892 | 4115 | 0 | 0 | 85‡‡ | 21§ | 383 | 1.06 s |

*все в bundled/third-party коде, не в собственном  
†все `foo.h ↔ foo-inl.h` паттерн под `#ifdef SPDLOG_HEADER_ONLY` — задача #032  
‡часть ложные (brace-depth не отслеживается) — баг SF.7  
‡‡все ложные (kScanLines=30 < длина Apache 2.0 copyright) — баг SF.8  
§структурные (библиотечный или тест-фреймворковый паттерн, не дефект)

### Выявленные баги в реализации (по итогам 5 прогонов)

| # | Правило | Описание бага | Пример |
|---|---|---|---|
| B1 | SF.7 | Не отслеживается глубина `{}` → `using namespace` внутри функций/лямбд репортится | `catch_tostring.hpp:270` |
| B2 | SF.8 | `kScanLines=30` < длина Apache/MIT copyright → guard не найден | все abseil заголовки |
| B3 | SF.8 | `.inc` фрагменты не должны проверяться на guard (они по дизайну без него) | `spinlock_linux.inc` |
| B4 | ambiguous | `single_include/` / amalgam-директории дублируют файлы → false ambiguous | nlohmann `single_include/` |
| B5 | GodHeader | Порог 30 слишком мал для фундаментальных заголовков библиотек | `absl/base/config.h` fan-in 507 |

---

## 2026-05-28 — folly (Meta)

**Версия archcheck:** commit `c480e39`

### Прогон 8 — `folly` (facebook/folly)

- **Масштаб:** 2311 project files.
- **Домен:** Meta C++ base library. Большой монорепо, корпоративный, реальный production-код.
- **Commit:** `acc9ce5aad8ca78c6bb498eefbfc569779edb19c`
- **Время:** 1.57 s (graph) + 2.17 s (rules).

**Результат (--graph):**
- nodes 2311 / edges 8763 / **sccs_cyclic 9** / largest_scc 6.
- external 4794 / unresolved 8 / ambiguous 0 / macro_includes 1.

**Результат (default rules) — 1719 нарушений:**
- ChainLength 1560, GodHeader 47, SF.8 86, SF.9 9, SF.7 17.

---

**SF.9 — 9 циклов, два разных класса:**

**Класс 1 — unconditional `-inl.h` паттерн (7 циклов):**
Аналог spdlog, но БЕЗ `#ifdef`-охраны. Примеры:
```
folly/channels/ChannelProcessor.h:252  #include <folly/channels/ChannelProcessor-inl.h>
folly/channels/MultiplexChannel.h      #include <folly/channels/MultiplexChannel-inl.h>
folly/fibers/Baton.h:321               #include <folly/fibers/Baton-inl.h>
```
Включение безусловное — templates всегда inline. Технически цикл существует. Не архитектурная проблема, но задача #032 (conditional includes) не покроет их — нужен отдельный подход к unconditional `-inl.h`.

**Класс 2 — кросс-компонентная цепочка (1 цикл, largest_scc=6):**
```
folly/fibers/Baton-inl.h
  → folly/fibers/FiberManagerInternal.h
  → folly/fibers/FiberManagerInternal-inl.h
  → folly/fibers/Baton.h
  → folly/fibers/Baton-inl.h
```
Baton и FiberManager взаимозависимы внутри одного модуля. Реальная coupling, но в пределах `folly/fibers/` — может быть принятым дизайном.

**Класс 3 — НАСТОЯЩИЙ архитектурный цикл (1 цикл):**
```
folly/futures/Future.h:39    #include <folly/futures/Promise.h>
folly/futures/Promise.h:490  #include <folly/futures/Future.h>
```
Проверено вручную — прямой взаимный include, **без охранного `#ifdef`**. `Future` (read-end) и `Promise` (write-end) одного async-примитива взаимно включают друг друга. Это первая настоящая архитектурная находка среди всех прогонов. Workaround: forward declarations или общий `FutureBase.h` — Facebook очевидно выбрали pragmatic coupling.

---

**SF.7 — новый тип ложных срабатываний:**

Помимо brace-depth (баг B1) обнаружен **баг B6: сканер не убирает блочные `/* ... */` комментарии**.

Примеры (проверено вручную):
```cpp
// folly/FixedString.h:447 — внутри Doxygen \code block:
 * \code
 *   using namespace folly;          ← SF.7 срабатывает — ложное, это doc-comment
 *   return makeFixedString("****");
 * \endcode

// folly/FixedString.h:2897 — аналогично:
 * \code
 * using namespace folly::string_literals;   ← внутри комментария
 * \endcode
```

```cpp
// folly/coro/safe/Captures.h:52 — inside named namespace (brace-depth > 0):
namespace lite_tuple {
    using namespace ::folly::detail::lite_tuple;  ← B1 bug, не глобальный scope
}
```

**SF.8 — тот же баг B2 что в abseil:**
`folly/AtomicHashMap.h` — `#pragma once` на строке **78**. Apache 2.0 license + длинный Doxygen-заголовок перед guard. Все 86 нарушений SF.8 — ложные.

---

**GodHeader (47) — смесь структурных и потенциально реальных:**

| Заголовок | Fan-in | Оценка |
|---|---|---|
| `folly/portability/GTest.h` | **635** | структурный: gtest-враппер для всех тестов |
| `folly/Portability.h` | **317** | структурный: platform config (≈ `absl/base/config.h`) |
| `folly/Range.h` | **155** | потенциально реальный: строковый range, включается везде |
| `folly/Traits.h` | **116** | утилитный, пограничный |
| `folly/ScopeGuard.h` | **105** | утилитный, ожидаемо высокий |
| `folly/Utility.h` | **98** | утилитный |
| `folly/String.h` | **91** | строки — потенциально реальный |

`Range.h` (155) и `String.h` (91) интересны — это не platform-config, а конкретные библиотечные классы. Высокий fan-in может быть признаком плохой изоляции компонентов в большом монорепо.

**ChainLength (1560):** ожидаемо для крупного монорепо. Глубина до 21+ (например `folly/tracing/test/StaticTracepointTest.cpp` depth 21).

---

**Итог folly:** инструмент нашёл **одну реальную архитектурную находку** — взаимный include `Future.h ↔ Promise.h`. Остальное — либо известные паттерны (`-inl.h`), либо ложные срабатывания из-за уже выявленных багов (B1, B2, B6). **Баг B6 (блочные комментарии в SF.7) обнаружен впервые** на этом прогоне.

---

## 2026-05-28 — grpc (Google)

**Версия archcheck:** commit `c480e39`

### Прогон 9 — `grpc` (grpc/grpc)

- **Масштаб:** 4493 project files.
- **Домен:** Google RPC framework. Огромный монорепо: C++, C, Objective-C, Python. Встроенный `third_party/upb` (C-библиотека protobuf).
- **Commit:** `05ffa9265bd5f4846a5e3dc46d91ba739ad17ef6`
- **Время:** 3.66 s (graph) + 4.92 s (rules).

**Результат (--graph):**
- nodes 4493 / edges 29448 / **sccs_cyclic 1** / largest_scc 2.
- external 8384 / **unresolved 6434** / **ambiguous 492** / macro_includes 0.
- 6434 unresolved — protobuf-generated заголовки и внешние зависимости без `-I` путей. Ожидаемо.

**Результат (default rules) — 3501 нарушение:**
- ChainLength 3215, GodHeader 171, SF.8 115, SF.7 **0**, SF.9 **0**.

---

**SF.9: 0 в отчёте, но sccs_cyclic=1 в --graph.**
Несоответствие требует расследования. Единственный SCC размером 2 — предположительно в `third_party/upb` (C-код), где цикл между `.c` и `.h` файлами. SF.9 может не репортить его по причине фильтрации или особенностей обхода. **Требует отдельной проверки.**

**Resolved 2026-05-29 (задача #049):** причина была в сканере — внутри классического `#ifndef X / #define X / ... / #endif` guard'а все top-level `#include` помечались `conditional=true`, и логика #032 (`allEdgesConditional`) глушила цикл. Перепрогон grpc после фикса даёт **SF.9 = 4**, среди них реальный архитектурный цикл `src/core/tsi/alts/handshaker/alts_handshaker_client.h ↔ alts_tsi_handshaker.h`. Регрессия на остальном корпусе: fmt/Catch2/nlohmann_json/abseil = 0, folly = 9, spdlog = 0 — все без изменений.

**SF.7: 0** — Google Code Style строго запрещает `using namespace`. Подтверждено на всём grpc-коде.

---

**SF.8 (115 нарушений):**

Распределение:
- **103 в `third_party/upb`** — C-код с BSD лицензией и длинными docstring'ами перед `#ifndef`. Баг B2 (kScanLines).
- **12 в grpc собственном коде** — три категории:

  1. **Objective-C заголовки** — `examples/objective-c/` — **новый ложный паттерн** (B7). ObjC-файлы используют `#import` и `@interface`, не C++ guards:
     ```objc
     // AppDelegate.h
     #import <UIKit/UIKit.h>
     @interface AppDelegate : UIResponder <UIApplicationDelegate>
     ```
     Наш сканер проверяет их на `#pragma once` / `#ifndef` — не находит — репортит SF.8. Нужно исключать `.h` файлы с ObjC-синтаксисом.

  2. **Examples без guard** — `examples/cpp/interceptors/caching_interceptor.h` — Apache 2.0 header + нет `#pragma once`. Вероятно реальное нарушение.

  3. **third_party** — ложные B2.

---

**GodHeader (171 нарушений, 156 в собственном коде grpc):**

Топ:

| Заголовок | Fan-in | Характер |
|---|---|---|
| `include/grpc/grpc.h` | **626** | Главный публичный API grpc. Проверено: 632 прямых `#include`. Структурный. |
| `include/grpc/event_engine/event_engine.h` | **286** | Ядро EventEngine — используется везде внутри |
| `include/grpc/grpc_security.h` | **209** | Публичный security API |
| `include/grpc/impl/channel_arg_names.h` | **183** | Строковые константы channel args |
| `include/grpc/credentials.h` | **157** | Credentials API |
| `third_party/upb/upb/port/def.inc` | **930** | macro def-файл для всего upb — C-паттерн |
| `third_party/upb/upb/port/undef.inc` | **842** | macro undef-файл — парный к def.inc |

156 god-headers в grpc собственном коде — очень много. Часть обусловлена тем, что `include/grpc/` — это публичный C API библиотеки, предназначенный включаться везде. Но количество (156!) говорит о том, что внутренние заголовки `src/core/` тоже имеют очень высокий fan-in. Это может быть реальным архитектурным сигналом.

**ChainLength (3215):** grpc + upb + тесты дают цепочки глубиной до 28 (`src/core/ext/transport/chttp2/transport/write_cycle.h` depth 27).

---

**Итог grpc:** Собственный C++ код grpc — **0 SF.7, 0 SF.9**. Дисциплина высокая. Основной шум:
- third_party/upb (C, встроенная зависимость) — вызывает большинство SF.8 и GodHeader ложных
- ObjC-файлы (B7) — новый класс ложных SF.8
- 156 GodHeaders в grpc собственном коде — частично структурные (public API), частично возможный реальный сигнал

---

## Сводная таблица и известный шум (все прогоны)

### Результаты по проектам

| Проект | Nodes | Edges | SCCs | Raw | **Сигнал** | Время |
|---|---|---|---|---|---|---|
| fmt | 72 | 157 | 0 | 16 | **0** | 0.28 s |
| spdlog | 149 | 439 | 22 | ~43 | **2** (god-headers) | 0.16 s |
| Catch2 | 414 | 1305 | 0 | 24 | **0** | 0.30 s |
| nlohmann/json | 491 | 376 | 0 | 20 | **0** | 0.61 s |
| abseil-cpp | 892 | 4115 | 0 | 489 | **0** | 1.06 s |
| folly | 2311 | 8763 | 9 | 1719 | **1** (Future↔Promise) | 2.17 s |
| grpc | 4493 | 29448 | 1 | 3421 | **?** (нужно дочистить) | 4.92 s |

**Сигнал** = нарушения после исключения известного шума (см. ниже).

---

### Известный шум — пропускаем, не фиксим сейчас

При чтении отчётов следующие находки считаются **заведомо ложными** — игнорируем их, задачи уже заведены:

| Баг | Правило | Что пропускаем | Задача |
|---|---|---|---|
| B1 | SF.7 | `using namespace` внутри `{}` — функции, лямбды, namespace-блоки | #035 |
| B2 | SF.8 | Заголовки с длинным Apache/BSD copyright: `#pragma once` / `#ifndef` за пределами kScanLines=30 | #034 |
| B3 | SF.8 | `.inc` файлы (платформенные фрагменты без guard по дизайну) | #034 |
| B4 | ambiguous | `single_include/`, `amalgamate/` — дублирующие директории | #036 |
| B5 | GodHeader | fan-in > 100 у фундаментальных конфиг-заголовков (`config.h`, portability) | #037 |
| B6 | SF.7 | `using namespace` внутри `/* */` комментариев (Doxygen `\code`) | #038 |
| — | SF.9 | `foo-inl.h ↔ foo.h` паттерн (header-only / dual-mode библиотеки) | #032 |
| — | SF.7/SF.8 | Код в `third_party/`, `bundled/`, `single_include/` | конфиг exclude |
| — | SF.8 | ObjC-файлы с `@interface`/`#import` за пределами 30 строк | **FIXED** (#034) |

**ObjC-файлы с маркерами в первых 30 строках** — исправлено в commit текущей сессии: `isObjcFile()` проверка в SF.8.

---

## 2026-05-29 — Первый прогон DRIFT.1/DRIFT.2 на реальных AI-PR'ах

**Версия archcheck:** commit `cc5cca9`

Первая боевая валидация DRIFT-правил (реализованы в #009) на реальных публичных
репозиториях с верифицированными AI-assisted коммитами. Подробный отчёт:
[docs/research/ai_drift_cases.md](research/ai_drift_cases.md). Связанная задача:
[backlog/future/033_maj_ai_drift_dataset.md](../backlog/future/033_maj_ai_drift_dataset.md).

Метод: `archcheck --save-graph-baseline` на before-SHA → `archcheck --drift-baseline` на after-SHA.

### Прогон 10 — LibreSprite PR #581 (macOS / menu-search / toolbar badges)
- **Масштаб:** 1253 узла графа, src-tree LibreSprite.
- **Домен:** C++ GUI app (pixel-art editor, fork Aseprite). Серия Claude Opus 4.5 коммитов.
- **Before/After SHA:** `60eed0f` → `276fdbd` (PR head).
- **Команда:** `archcheck --drift-baseline /tmp/libresprite_before.graph.json src`
- **Результат:**
  - **DRIFT.1: 1 hit.** Shortcut edge: `app/ui/toolbar.cpp -> app/pref/preferences.h`.
  - **DRIFT.2: 0** (новых циклов нет).
  - 288 нарушений из дефолтных правил — pre-existing legacy (ChainLength, GodHeader, SF.8), не дрейф.
- **Источник находки:** коммит `0aa57ad` "Add keyboard shortcut badges to toolbar icons"
  (Co-Authored-By: Claude Opus 4.5). Агент добавил `#include "app/pref/preferences.h"`
  в `toolbar.cpp` чтобы прочитать новую preference `show_tool_shortcuts`.
- **Инсайт:** эталонный shortcut edge именно того класса, под который DRIFT.1
  и проектировался — локально удобно, глобально размывает архитектуру (UI-виджет
  начинает зависеть от preferences-слоя напрямую), обычными линтерами не ловится.
- **Верификация (skeptic-pre-empt, 2026-05-29):** ребро могло быть «не дрейфом, а
  схождением к upstream Aseprite, где include всегда был». Проверено тремя
  командами `git` против самого форка LibreSprite (sandbox клон):
  ```
  $ git show 60eed0f:src/app/ui/toolbar.cpp | grep -n 'app/pref/preferences.h'
  ABSENT at before-SHA
  $ git show 276fdbd:src/app/ui/toolbar.cpp | grep -n 'app/pref/preferences.h'
  15:#include "app/pref/preferences.h"
  $ git log --oneline 60eed0f..pr-581 -- src/app/ui/toolbar.cpp
  0aa57ad Add keyboard shortcut badges to toolbar icons
  ```
  Один коммит в PR-диапазоне трогает `toolbar.cpp`, он же добавляет include
  (`git log -p -S` подтвердил единственное `+#include "app/pref/preferences.h"`),
  отката внутри PR нет. **Вердикт: A — CONFIRMED.** Граф самого репозитория
  действительно изменился; ребро (re)introduced AI-коммитом, а не унаследовано.
  Контекст для демо: edge присутствует в upstream `aseprite/aseprite`
  `src/app/ui/toolbar.cpp`; форк LibreSprite на 60eed0f его не нёс; AI-коммит
  0aa57ad вернул зависимость. Это снимает возражение «просто re-convergence».
- **Следствие:** fixture `fixtures/drift_real_world/libresprite_pr581/`
  разблокирован — заведена задача #048 в `backlog/new/`.

### Прогон 11 — vmecpp PR #360 (asymmetric VMEC infrastructure)
- **Масштаб:** 232 узла графа, scientific C++ + Python bindings.
- **Домен:** Stellarator equilibrium solver. Крупный merged PR Claude Code.
- **Before/After SHA:** `df63271` → `5eabd51` (merge commit).
- **Результат:** **DRIFT.1: 0, DRIFT.2: 0.** 3 Lakos.GodHeader — pre-existing.
- **Инсайт:** большой архитектурный AI-рефактор без дрейфа. Полезный негативный сигнал.

### Прогон 12 — vmecpp PR #340 (consolidate algorithmic constants)
- **Масштаб:** 232 узла.
- **Before/After SHA:** `b44fb7f` → `a7797dc` (merge commit).
- **Результат:** **DRIFT.1: 0, DRIFT.2: 0.** Те же 3 pre-existing GodHeader.
- **Инсайт:** консолидация констант в общий header — корректный рефактор,
  не создаёт обратных рёбер.

### Прогон 13 — BambuStudio PR #10794 (color cutting / dovetail / sculpting)
- **Масштаб:** 3019 узлов графа, большой C++/wxWidgets monorepo.
- **Домен:** Slicer для 3D-печати, форк PrusaSlicer. Один большой merge commit
  "Integrate AG changes into root source tree" (adamgasoft) — 247 файлов, +16302/-6176.
- **Before/After SHA:** `2263815` → `a206a95` (PR-10794 head).
- **Команда:** `archcheck --drift-baseline /tmp/bambustudio_before.graph.json src`
- **Результат:**
  - **DRIFT.1: 3 hit'а** (из них **2 реальных**, 1 false-positive).
  - **DRIFT.2: 0** (новых циклов нет).
  - 884 нарушения из дефолтных правил — pre-existing legacy.
- **Реальные DRIFT.1:**
  - `slic3r/GUI/FilamentMapDialog.hpp -> slic3r/GUI/Widgets/CheckBox.hpp`
  - `slic3r/GUI/FilamentMapPanel.hpp -> slic3r/GUI/Widgets/SwitchButton.hpp`
  - Оба — диалог/panel уровень тащит include низкоуровневого виджета напрямую.
    Классический shortcut UI-слоя.
- **False-positive (1) — баг в нашем сканере:**
  `slic3r/GUI/MsgDialog.cpp -> slic3r/GUI/MsgDialog.hpp`. Проверка вручную: include
  существует в обеих ревизиях. Причина — UTF-8 BOM (`EF BB BF`) в первой строке
  before-файла, наш `include_scanner` BOM не зачищает, первая строка не матчит regex,
  ребро теряется в baseline, в after-ревизии (без BOM) видится как новое.
  Заведена задача **#047** (critical, ~15 мин фикс).

### Сводка по DRIFT-прогону
| Repo | PR | Files | DRIFT.1 | DRIFT.2 | Real | Вердикт |
|------|----|-------|---------|---------|------|---------|
| LibreSprite | #581 toolbar/menu/macOS | 1253 | 1 | 0 | 1 | hit |
| vmecpp | #360 asymmetric infra | 232 | 0 | 0 | 0 | clean |
| vmecpp | #340 consolidate constants | 232 | 0 | 0 | 0 | clean |
| BambuStudio | #10794 color cutting/dovetail | 3019 | 3 | 0 | 2 | hit + scanner bug |

**Главный результат:**
- DRIFT-правила работают как задумано — 3 из 4 PR подтвердили класс находки
  (shortcut edges через границы модулей), 2 чистых рефактора прошли без false-positive.
- Обнаружен **критичный баг в сканере** (UTF-8 BOM) — задача #047.
- Корпус кейсов для fixtures: 3 реальных hit'а + 2 clean'а на разнотипных проектах
  (pixel-art editor, scientific solver, 3D-print slicer).

Артефакты прогонов:
- Клоны: `sandbox/drift_repos/{LibreSprite,vmecpp,BambuStudio}`
- Graph baselines: `/tmp/libresprite_before.graph.json`, `/tmp/vmecpp_before.graph.json`,
  `/tmp/vmecpp_pr340_before.graph.json`, `/tmp/bambustudio_before.graph.json`
- Полные drift-отчёты: `/tmp/libresprite_drift.txt`, `/tmp/vmecpp_drift.txt`,
  `/tmp/bambustudio_drift.txt`

---

## 2026-05-29 — DRIFT re-run после BOM-фикса (#047)

**Версия archcheck:** working tree с патчем из #047 (strip UTF-8 BOM в `scanIncludes`).

Клоны репо переехали в постоянное хранилище `/home/localadm/oss/` (не sandbox).

### Прогон 14 — повтор всех четырёх DRIFT-кейсов на свежих клонах
- **LibreSprite #581** (`60eed0f` → `pr-581`): **1 DRIFT.1** — без изменений, та же находка `toolbar.cpp -> pref/preferences.h`.
- **vmecpp #360** (`df63271` → `5eabd51`): 0 DRIFT — clean.
- **vmecpp #340** (`b44fb7f` → `a7797dc`): 0 DRIFT — clean.
- **BambuStudio #10794** (`2263815` → `pr-10794`): **3 → 2 DRIFT.1.**
  False-positive `MsgDialog.cpp -> MsgDialog.hpp` (был артефакт UTF-8 BOM
  в before-ревизии) **исчез**. Реальные находки сохранились:
  `FilamentMapDialog.hpp -> Widgets/CheckBox.hpp`, `FilamentMapPanel.hpp -> Widgets/SwitchButton.hpp`.

**Итог:** BOM-фикс из #047 подтверждён на боевом проекте. Сигнал чистый,
шум устранён. Задача #047 → completed.

Артефакты (постоянные):
- Клоны: `/home/localadm/oss/{LibreSprite,vmecpp,BambuStudio}`
- Graph baselines: `/tmp/{libresprite,vmecpp,vmecpp_pr340,bambustudio}_before_v2.graph.json`
- Полные drift-отчёты: `/tmp/{libresprite,vmecpp,vmecpp_pr340,bambustudio}_drift_v2.txt`

---

## 2026-05-29 — DRIFT-корпус расширен до 33 PR на 10 репо

**Версия archcheck:** working tree после #047.
**Helper-скрипт:** `scripts/drift_run.sh` (clean checkout, см. #048).

Полный рабочий лог: [docs/research/ai_drift_runlog.md](research/ai_drift_runlog.md).
Развёрнутый анализ архетипов: [docs/research/ai_drift_cases.md](research/ai_drift_cases.md).

### Прогон 15 — массовый sweep AI-PR'ов в C++ репо

Цель: проверить, что находки DRIFT.1 из прогона 14 — не случайные, а
систематические. Найти больше hits на разнотипных проектах. Собрать корпус
≥ 10 PR'ов с подтверждёнными hit'ами для целевой демонстрации в README.

**Расширенный корпус (10 репо, 33 PR'а):**

| Repo | PR'ов | DRIFT.1 hit | Архетип |
|------|-------|-------------|---------|
| LibreSprite/LibreSprite | 1 | 1 | UI→pref shortcut |
| proximafusion/vmecpp | 2 | 0 | clean (scientific code) |
| bambulab/BambuStudio | 1 | 2 | UI→Widgets shortcut |
| sxs-collaboration/spectre | 5 | 0 | clean (numerical relativity) |
| gwdevhub/GWToolboxpp | 3 | 0 | clean |
| jakildev/IrredenEngine | 4 | 2 | system→component |
| openmoq/moqx | 3 | 0 | clean |
| aethersdr/AetherSDR | 3 | 0 | clean |
| OreStudio/OreStudio | 4 | 0 | clean (но +9762 LOC у одного!) |
| EtherAura/Kartend | 3 | 5 | data→ui-config shortcut |
| community-shaders/skyrim | 4 | 2 | generic→features shortcut |

**Итог: 12 DRIFT.1 hit'ов на 7 PR'ах из 33 (21%).** Все hits подтверждены
grep'ом против диффа коммита (реальные новые `#include`). DRIFT.2 (новые
циклы) не сработал ни разу.

### Найденные баги в archcheck

- **#047 (closed)** — UTF-8 BOM не зачищается. Фикс применён, регрессионные тесты
  добавлены. Подтверждено на BambuStudio.
- **#048 (new, critical)** — методологическая ловушка: `git checkout SHA -- src`
  без `git clean -fdx` оставляет файлы из других ревизий в working tree, что
  даёт массовые false-positive в DRIFT (например, Kartend #26: dirty=26 FP,
  clean=0; OreStudio дал 7 FP в dirty mode, 0 в clean). Helper `scripts/drift_run.sh`
  написан, долгосрочное решение — режим `--diff` с git worktree изоляцией.

### Что нового по сравнению с прогоном 14

- **+8 новых репо**, **+29 новых PR'ов** к baseline корпусу.
- **+4 архетипа DRIFT.1** подтверждены: UI→widgets (был), UI-config→core (был),
  generic→features (новый, Skyrim), system→component (новый, IrredenEngine),
  data→ui-config (новый, Kartend).
- **Большие clean PR'ы** доказывают, что DRIFT.1 не false-positive шумогенератор:
  - spectre #7238: +1352 LOC новой Filters::Filter иерархии — 0 drift
  - OreStudio #588: +9762 LOC composite/scripted instruments — 0 drift
  - moqx #327: +1183 LOC нового модуля CrossExecFilter — 0 drift
  - IrredenEngine #798: +427 LOC editor layer system — 0 drift

Корпус **превысил порог 5+ hit'ов**, заявленный в #033 §"Целевой вид
демонстрации". Можно начинать работу над README с реальными данными
(см. backlog/future/033 §"Следующие шаги" п. 7).

Артефакты (постоянные):
- Клоны: `/home/localadm/oss/{LibreSprite,vmecpp,BambuStudio,spectre,GWToolboxpp,IrredenEngine,moqx,AetherSDR,OreStudio,Kartend,skyrim-community-shaders,pico-sdk,sys-device}`
- Graph baselines: `/tmp/clean_*_base.json`
- Drift-отчёты: `/tmp/clean_*_drift.txt`
- Helper: `scripts/drift_run.sh`

---

## 2026-05-30 — Self-dogfood line-dup: нашли свой copy-paste в `cpparch`

**Версия archcheck:** standalone line-dup спайк из #053 (`experiments/line_duplication/main.cpp`), worktree 2026-05-30.

### Прогон 16 — `cpparch` сам в себе (duplication dogfood)
- **Масштаб:** `cpparch` source tree, app-only режим line-dup sweep.
- **Домен:** наш собственный CLI tool для архитектурных проверок.
- **Команда:** standalone line-dup прогон по `cpparch` с exclude `sandbox/` и
  test-like файлов по умолчанию.
- **Результат:**
  - top cross-file duplicate:
    `src/report/json_reporter.cpp:6 <-> src/report/violation_baseline.cpp:12`
  - длина блока: **21** значимая строка
  - похожесть: **100%**
  - тип находки: **малый локальный дубль / function-level copy-paste**
- **Ручная проверка:** дубль подтверждён. В обоих файлах живёт одинаковая
  `jsonEscape(const std::string&)`, экранирующая `"`, `\` и `\n`.
- **Почему это важно:** это не generated-код, не test twin и не packaging-шум,
  а настоящий внутренний copy-paste в shipped code.
- **Инсайт:** по форме это именно тот класс мелких локальных дублей, который
  часто оставляют AI-ассистенты: вместо общего helper-а функция копируется во
  второй файл «чтобы быстро поехало». Доказать авторство ИИ здесь нельзя, но
  паттерн — типичный AI-style copy-paste.
- **Следствие:** заведена отдельная dogfood-задача
  [#055](../backlog/completed/055_min_dogfood_json_escape_dedup.md) — вынести
  `jsonEscape()` в общий report-level helper и после фикса убедиться, что эта
  пара исчезла из top duplicate list.

**Почему это хороший milestone:**
- `archcheck` не только проверяет чужие репозитории, но и ловит собственный
  дублированный код;
- находка маленькая, точная и понятная — не «страшный процент», а конкретный
  21-строчный блок с двумя файлами;
- это честный dogfooding-сюжет для README/демо: **нашли своим же тулом,
  завели задачу, вынесли в общий util**.

---

## 2026-06-03 — Корпусный прогон: 317 C++ репо (graph-drift + duplication)

**Версия archcheck:** commit `b93729a` <!-- TODO: уточнить точный commit прогона -->

Первый массовый корпусный прогон через новый orchestrator (#079): параллельный
graph-drift + duplication анализ на 317 публичных C++ репозиториях. Связанная
ручная аналитика — #080. Прогон относится к research/preview ветке (corpus/AI вне
trusted gate, см. ROADMAP).

### Прогон 17 — корпус 317 репо
- **Масштаб:** 317 C++ репозиториев. <!-- TODO: суммарно файлов/узлов -->
- **Домен:** разнотипный OSS C++ (см. corpus-листинг). <!-- TODO -->
- **Команда:** corpus orchestrator (#079), parallel scan. <!-- TODO: точная команда/скрипт -->
- **Время:** <!-- TODO -->
- **Результат:** <!-- TODO: агрегаты graph-drift + duplication -->
- **Ключевая находка (#080):** **нет корреляции между долей AI-кода и качеством
  кода** (graph/duplication метрики). Подтверждает решение ROADMAP не строить
  продукт вокруг AI-attribution.
- **Инсайты:** <!-- TODO: дополнить пользователю -->

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
