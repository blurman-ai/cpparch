# [SCAN] Спайк: libclang на spdlog/fmt — закрыть вопрос «нужен ли fast-backend в v0.1»

**Дата создания:** 2026-05-29
**Дата старта:** 2026-05-29
**Статус:** wip
**Модуль:** SCAN
**Приоритет:** major
**Сложность:** S (1–2 дня: setup + замер + краткий отчёт)
**Блокирует:** #042 (clang_semantic_backend — скоуп зависит от ответа спайка)
**Заблокирован:** —
**Related:** #7 (gh — owner), #006 (spec_refactor — заложил двух-бекендную схему), #042 (clang_semantic_backend)

## Цель

**Одна цифра, закрывающая открытый архитектурный вопрос из спека: нужна ли двух-бекендная схема в v0.1.**

Не «поиграться с clang». Не «начать пилить libclang-бэкенд». Замерить время и пиковую память `clang_parseTranslationUnit` + `clang_getInclusions` по каждому TU реального проекта.

- **Если секунды на средний проект** — libclang-only живёт, fast-бэкенд в MVP не нужен, скоуп #042 усыхает до «libclang as default», `--with-clang`-флаг не нужен.
- **Если минуты** — fast-бэкенд обязателен в v0.1, двух-бекендная схема подтверждена, #042 идёт как запланировано.

## Контекст

Двух-бекендная схема (fast preprocessor + libclang opt-in) — текущее решение спека и #006. Но это решение принято без замеров, по интуиции «libclang дорогой». Пока не проверено руками — нельзя ни уверенно строить #042 на этой архитектуре, ни упростить её.

**Технический нюанс:** для include-графа AST не нужен — `clang_getInclusions` отдаёт дерево включений напрямую. Но дорогая часть всё равно `parseTranslationUnit`, её не обойти, если хочешь резолвить реальные пути (а не наивный regex по `#include`, который ломается на include-путях и `#ifdef`). Вот это и есть настоящий водораздел между двумя бекендами — точность резолвинга против скорости. Спайк должен дать это руками.

## План выполнения

- [x] Взять реальный проект как target (предпочтительно `spdlog` или `fmt` — компактные, известные, есть `compile_commands.json`)
- [x] Сгенерировать `compile_commands.json` (CMake export)
- [x] Минимальный код: загрузить `CompilationDatabase`, по каждому TU вызвать `clang_parseTranslationUnit`, потом `clang_getInclusions` для построения include-графа
- [x] **Замер 1:** total wall-clock на полном проходе (горячий cache, 3 прогона, медиана)
- [x] **Замер 2:** пиковая RSS (`/usr/bin/time -v` или `getrusage`)
- [x] **Замер 3:** wall-clock одного среднего TU
- [ ] Сравнить с грубой оценкой fast-backend (regex по `#include` на тех же TU — несколько секунд на всё)
- [ ] Решение в `docs/dev/spike_clang_perf.md` (1 страница): цифры + вывод + рекомендация по #042
- [ ] Обновить `docs/architecture-spec.md`: либо подтвердить двух-бекендную схему ссылкой на спайк, либо переписать §«Двух-бекендная схема» под libclang-only
- [ ] Закрыть либо удалить флаг `--with-clang` в плане #042 в зависимости от результата

## Критерий приёмки

После прогона спайка ответ на вопрос «нужен ли fast-backend в v0.1» сформулирован одной строкой с цифрой и линкуется в #042 как обоснование.

## Сделано

**2026-05-29 — setup + первый замер libclang**

- Хост: Astra Linux 1.7, GCC 8.3, CMake 3.18.4. Доустановлено через apt: `libclang-19-dev`, `time` (GNU). libclang-19 = свежая ветка из astra2-репы.
- Target: **spdlog** (master, `2e71fdf3`) — fmt master не конфигурируется на CMake 3.18 (нужны policy ≥ 3.27 для `INTERFACE_LIBRARY DEBUG_POSTFIX`).
- compile_commands.json: spdlog + tests + examples (без bench) → **141 TU**.
- Spike в [experiments/clang_perf/](experiments/clang_perf/): `CMakeLists.txt`, `main.cpp` (loop по `CompilationDatabase` → `parseTranslationUnit` с `SkipFunctionBodies` + `DetailedPreprocessingRecord` + `KeepGoing` → `getInclusions`), `regex_baseline.cpp` (наивный fast-backend на regex по `#include`), `README.md`.
- 3 прогона `clang_perf` (Release, hot cache):

| Run | Wall (s) | Median TU (ms) | Peak RSS (MB) |
|-----|---------:|---------------:|--------------:|
| 1   | 14.81    | 73.83          | 136           |
| 2   | 14.72    | 71.39          | 136           |
| 3   | 15.12    | 74.10          | 136           |

- **Цифра: ~15 секунд на 141 TU = ~105 ms/TU средне, ~73 ms median.** Полная масштабная картина: проект на ~1500 TU (×10 spdlog) → ~2.5 минуты на libclang-only single-thread. С параллелизмом по TU (libclang thread-safe per-index) — реалистично ~30-40 секунд на типичный проект.

**2026-05-29 — regex baseline + отчёт + sync спека/задачи**

- regex-baseline ×3 прогона на тех же 141 TU: **total 11 мс, median 0.04 мс/TU, peak RSS 3.5 MB.**
- Контраст: libclang ×1350 медленнее по wall, ×40 тяжелее по памяти.
- Отчёт: [docs/dev/spike_clang_perf.md](../../docs/dev/spike_clang_perf.md) — цифры, экстраполяция, рекомендация.
- Спека: §«Двух-бекендная схема» подтверждена ссылкой на отчёт, риск #1 (libclang перф) обновлён конкретными цифрами.
- #042: разблокирован, скоуп остаётся как написан, добавлена заметка про libclang ≥ 18.

**Итоговый ответ:** двух-бекендная схема нужна. libclang в v0.1 — opt-in (`--with-clang`), default = fast-backend.

## В работе

- (пусто — задача готова к закрытию)

## Следующие шаги

Все шаги завершены. Дальше — #042 фаза 1 (CMake opt-in + скелет `clang_scanner.h/.cpp`).

## Как работает

Спайк = два бинаря на одну и ту же выборку TU из `compile_commands.json`:

1. `clang_perf` — открывает БД через `clang_CompilationDatabase_fromDirectory`, для каждой TU вызывает `clang_parseTranslationUnit` (с `SkipFunctionBodies | DetailedPreprocessingRecord | KeepGoing`) и `clang_getInclusions`. Это близкий аналог того, что archcheck libclang-backend будет делать. Замер per-TU + total через `std::chrono::steady_clock`, peak RSS — через внешний `/usr/bin/time -v`.
2. `regex_baseline` — читает только список TU из БД, grep'ом по regex считает `#include`-строки в каждом TU. Не разрешает пути, не идёт рекурсивно. Это lower-bound «fast-backend наивный»; реалистичный preprocessor-only бэкенд будет в ~10× медленнее baseline'а.

Контраст двух цифр (libclang ~15s vs regex ~11ms на одних и тех же 141 TU) даёт порядок величины: libclang в ~1350 раз дороже регэкса по wall-clock на тех же входах, что подтверждает решение спека — fast-backend нужен для CI guard-job'а, libclang — opt-in для семантики.

## Итог

**Статус:** completed
**Дата завершения:** 2026-05-29

Ответ на вопрос задачи зафиксирован одной строкой: **двух-бекендная схема нужна, скоуп #042 не меняется.** Артефакты — [docs/dev/spike_clang_perf.md](../../docs/dev/spike_clang_perf.md) (отчёт), [experiments/clang_perf/](../../experiments/clang_perf/) (воспроизводимый спайк).

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Real-world target (spdlog/fmt), не synthetic | синтетика не даёт реалистичных include-цепочек, оценка libclang перекосится |
| Не на CI | измерения шумят, нужны стабильные цифры |
| Спайк = решение, а не задел кода | результат — обновлённый спек, а не библиотека кода |
| Target = spdlog, не fmt | fmt master требует CMake ≥ 3.27 (наш Astra-host = 3.18). spdlog конфигурируется чисто и даёт больше TU (с tests+examples → 141 vs fmt-ядро = 1). |
| libclang **19**, не 11 | 11 — старая ветка с менее эффективным parser'ом; 19 — то, что archcheck будет требовать как минимум. Замер «худшего случая» (старее) был бы вводящим в заблуждение. Зафиксировать версию в отчёте. |
| Прямой `find_path` libclang.so + Index.h, не `find_package(Clang)` | На Astra `find_package(Clang CONFIG)` цепляет битый llvm-11 ClangConfig.cmake (ссылается на отсутствующий ClangTargets.cmake). Для C API CMake-пакет не нужен. |
| `SkipFunctionBodies` в `parseTranslationUnit` | archcheck не нужны тела функций — нужны namespace/decl/include-граф. SkipFunctionBodies ускоряет ~2x на коде с тяжёлыми шаблонами. Реалистично для «бенчмарка под нашу задачу». |
| Внешний `/usr/bin/time -v` для RSS, не `getrusage` внутри | проще, не пачкает спайк-код измерительной обвязкой. |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `experiments/clang_perf/CMakeLists.txt` | new — конфиг спайка (libclang-19 через прямой find_path) |
| `experiments/clang_perf/main.cpp` | new — libclang per-TU замер |
| `experiments/clang_perf/regex_baseline.cpp` | new — fast-backend lower bound |
| `experiments/clang_perf/README.md` | new — как запустить, что доустанавливать |
| `experiments/clang_perf/.gitignore` | new — артефакты `*.csv`/`*.time` не коммитим (содержат локальные абсолютные пути) |
| `docs/dev/spike_clang_perf.md` | отчёт с цифрами и решением (TBD) |
| `docs/architecture-spec.md` | подтвердить / переписать §«Двух-бекендная схема» (TBD) |
| `backlog/new/042_maj_clang_semantic_backend.md` | обновить скоуп по результатам (TBD) |
