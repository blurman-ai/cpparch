# Спайк #043 — libclang perf на spdlog: нужен ли fast-backend в v0.1

**Дата:** 2026-05-29
**Цель:** одна цифра, закрывающая вопрос «нужна ли двух-бекендная схема в v0.1, или libclang-only вытянет».
**Код:** [experiments/clang_perf/](../../experiments/clang_perf/) (throw-away, после спайка удаляется).

## Setup

- Хост: Astra Linux 1.7, GCC 8.3, CMake 3.18.4.
- libclang: **19.1.7** (`libclang-19-dev` из astra2-репы). Версия 11 в системе тоже есть, но мерять старую ветку нет смысла — archcheck потребует ≥18.
- Target: **spdlog** (master, `2e71fdf3`) с включёнными tests + examples → **141 translation units**. fmt master не подошёл — требует CMake ≥ 3.27.
- Замер: 3 прогона Release-сборки спайка, hot cache, single-thread.
- Метрика: `clang_parseTranslationUnit` (с `SkipFunctionBodies | DetailedPreprocessingRecord | KeepGoing`) + `clang_getInclusions` — ровно то, что нужно archcheck'у для include-графа и AST-фактов, без overhead'а на парс тел функций.

## Цифры

| Backend                              | Total wall | Median TU | Peak RSS |
|--------------------------------------|-----------:|----------:|---------:|
| **libclang-19** (parseTU + inclusions) | **14.9 s** | **73 ms** | **136 MB** |
| **regex baseline** (`#include`-grep)   |   **11 ms** | **0.04 ms** | **3.5 MB** |

Отношение: **libclang медленнее в ~1350× и тяжелее по памяти в ~40×.**

Цифры стабильные между прогонами: разброс ±1.3% по wall, ±2% по RSS.

> **Важно:** regex-baseline парсит только top-level `.cpp` (`#include`-строки без резолва путей и без рекурсивного спуска по заголовкам). «Реальный» fast-backend в archcheck должен ещё разрешать пути через `-I` и обходить транзитивные хедеры — это ~10× оверхед против naive regex. То есть реалистичная оценка fast-backend в нашем продакшен-коде: **~100 ms на spdlog**, не 11 ms.

## Масштабирование (приблизительная экстраполяция)

| Размер проекта | libclang single-thread | libclang ×8 thread | Реалистичный fast-backend |
|----------------|-----------------------:|-------------------:|--------------------------:|
| ~150 TU (spdlog)  |   15 s |   2 s | 0.1 s |
| ~500 TU (mid-size)|   50 s |   7 s | 0.4 s |
| ~2000 TU (Boost-like) | 3.5 min | 30 s | 1.5 s |
| ~5000 TU (LLVM, Chromium-ядро) | 9 min | 1.1 min | 4 s |

Параллелизм по TU — реалистичный (libclang `CXIndex` не thread-safe сам по себе, но per-TU `CXTranslationUnit` независимы; стандартная схема — один `CXIndex` на поток).

## Ответ на вопрос задачи

**Нужен ли fast-backend в v0.1? Да.**

Не потому что libclang «непригоден» — он терпим для CI типового проекта (~50s на 500 TU без параллелизма, ~7s с). А потому что:

1. **Zero-setup remit.** Запустить `archcheck` без аргументов и без LLVM-зависимости — это часть продуктового обещания (CLAUDE.md, `docs/architecture-spec.md`). libclang-only ломает этот use case в принципе.
2. **CI bottleneck.** На монорепе с 5000 TU libclang single-thread = 9 минут только на парсе. Это слишком дорого для guard-job'а, который должен срабатывать на каждый PR за секунды. Fast-backend держит этот сценарий в секундах.
3. **Memory ceiling.** 136 MB peak на 141 TU = ~1 MB/TU удерживаемой памяти. На 5000 TU — ~5 GB peak (без агрессивного `dispose` после каждой TU). Это превышает default-лимит GitHub Actions free-tier runner (7 GB).
4. **Семантика нужна не всем.** v0.1-правила (SF.7/8/9, Lakos: cycles/chains/god-headers) — все включаются на include-графе без AST. Заставлять пользователя ставить libclang ради include-cycle detection — это user-experience регресс.

## Рекомендация по #042

Двух-бекендная схема, как заложена в #006 и #042, **подтверждается**. Конкретно:

- Сохранить `--with-clang` как opt-in флаг.
- Сохранить разделение `IRule` (fast-backend) / `ISemanticRule` (libclang-backend) из открытого вопроса фазы 1.
- libclang в v0.2 — как «семантический preview»: SF.21, потом SF.2/5/10/11.
- В v0.3 — рассмотреть `--with-clang` как default-ON для опубликованной Docker-сборки (где libclang уже внутри), но в standalone-binary остаётся opt-in.

`--with-clang` НЕ удаляется. Скоуп #042 идёт как написан.

## Sanity-чек воспроизводимости

Спайк собирается одной командой при наличии `libclang-19-dev` + GNU `time`. См. [experiments/clang_perf/README.md](../../experiments/clang_perf/README.md). Артефакты прогонов (`spike_run*.csv`, `baseline_run*.csv`) лежат рядом с кодом — можно перезапустить и сравнить.
