# Приёмы устранения дублирования в C++ open-source проектах

Эмпирическая опора для определения **TP** (что archcheck обязан считать настоящим
копипастом): для проектов, у которых доля дублированного кода (`dup_ratio` из
`line_duplication`) снизилась со временем, по git-истории установлено, **какими
инженерными приёмами** разработчики убирали копипаст. То есть это не теория клонов
(она — в [code_clones.md](code_clones.md)) и не каталог FP (он —
в [../duplication_fp_analysis.md](../duplication_fp_analysis.md)), а ground truth с
обратной стороны: реальные коммиты, где дубль признали и вынесли.

В духе «authority over opinion»: каждый приём подтверждён через `git show` реального
коммита — это и есть авторитет практики Lakos поверх нашего мнения.

> **Провенанс.** Данные получены в корпусном прогоне (#079/#080), артефакты которого
> живут локально вне гита (`experiments/` — не трекается). Источник дат/окон тогда:
> `runs_history/all/<repo>.tsv` + сводка `drift_summary_v2.tsv`; коммиты проверялись
> в клонах `/home/localadm/oss/_aidev_run/<repo>`. Здесь сохранён итог — таблицы и
> выводы, которые не должны потеряться вместе с локальными дампами.

Тема — физический дизайн в духе John Lakos: вынос общего кода вверх по уровням,
устранение дублирующихся реализаций между платформами/типами/бэкендами.

---

## Что засчитано как реальная дедупликация

| Проект | Окно дат | dup% | Коммит SHA | Приём | Что именно вынесено / убрано | Текст коммита |
|---|---|---|---|---|---|---|
| **alpaka** | 2022-02 (idx10→11) | ~33→32* | `f060ddf7` | extract-to-common-header | Общий CUDA/HIP-код вынесен в `CudaHipCommon.hpp`; `Cuda.hpp` −376, `Hip.hpp` −356 (итог −729/+362). Один источник трейтов `DimType`/builtin-типов вместо двух копий | "refactor common CUDA/HIP code into common header" |
| **alpaka** | 2022-03 (idx15→16) | 23.6→22.5 | `7559f91c` | template-параметризация | Два класса `QueueUniformCudaHipRtBlocking` и `…NonBlocking` (по ~160 строк трейтов каждый) → один `QueueUniformCudaHipRt<bool TBlocking>` + `using`-алиасы (−481/+313) | "Refactor CUDA/HIP queues to reduce code duplication" |
| **alpaka** | 2022-11 (idx16→17) | 22.5→20.3 | `19bed293` | merge template specialization | Частичная специализация `ConcurrentExecPool<…,false>` слита с primary template (−223/+88, "pure refactoring, should not change behavior") | "Merge ConcurrentExecPool primary template with specialization" |
| **vgmtrans** | 2026-01 (idx≈18→19) | 15.6→14.3 | `1d92b84` | base-class API (NVI) | ~8 классов `*Samp` (MP2k/NDS/PSX/SNES/Konami/Dialogic…) имели свою `convertToStdWave(buf)` с одинаковой обвязкой (выдели буфер → распакуй). Введён единый невиртуальный `convertToStdWave()` в базе + protected `virtual decodeToNativePcm()`, переопределяемый подклассами | "VGMSamp refactor (part 1) (#722)" |
| **vgmtrans** | 2024-04 (idx≈17→18) | 16.3→15.6 | `be35fce` | extract common widget | Общий `TableView` (виджет) для `VGMFileListView` и `RawFileListView`; appearance/header-логика поднята в базу, `VGMFileListView.cpp` −52 | "add TableView widget to consolidate appearance and header logic …" |
| **Effekseer** | 2022→2024 (idx21→22) | 36.2→29.0 | `d1fece84` | extract-to-common-module | Логика vertex-буферов 5 рендер-бэкендов (DX9/DX11/GL/Metal/Vulkan) сведена в `EffekseerRendererCommon` (40 файлов, −1633/+528) | "Refactor vertex buffer (#935)" |
| **Effekseer** | 2022→2024 (idx21→22) | 36.2→29.0 | `5564dccb` | extract-to-common-module | Общий `VertexLayout` вместо per-backend копий (30 файлов, −924/+718) | "Refactor VertexLayout (#932)" |
| **acts** | 2024-03→06 (idx14→15) | 10.4→8.5 | `e4a3ec90a` | extract-to-common-header + helper class | 3 почти идентичных benchmark-файла (`Atlas/Eigen/StraightLineStepperBenchmark.cpp`, ~126 строк копипаста каждый) → ~10 строк каждый через общий `StepperBenchmarkCommons.hpp` (класс `BenchmarkStepper`: `parseOptions/makeField/run`) (−343/+209) | "refactor: Share code between stepper benchmarks (#3162)" |
| **acts** | 2024-03→06 (idx14→15) | 10.4→8.5 | `357480730` | extract-function | Дублированная запись треков в Core CKF вынесена в общую функцию | "refactor: Common function to store tracks in Core `CKF` (#3164)" |
| **acts** | 2024-03→06 (idx14→15) | 10.4→8.5 | `eb8677409` | extract-function (move impl) | Реализации boundary-check перенесены в общий `VerticesHelper` | "refactor: Move boundary check implementations to `VerticesHelper` (#3205)" |
| **oneDAL** | 2021-06→08 (idx10→11) | 37.2→33.6 | `80d98e7c2` | delete-dead / legacy-cleanup | Удалены целые deprecated-алгоритмы (adaboost/brownboost/logitboost старого API), дублировавшие новые интерфейсы: −40049/+186, 259 `.cpp` + 190 `.h`. Главная причина устойчивого спада dup и падения nodes 3479→3295 | "Clean old interfaces (#1611)" |
| **oneDAL** | 2021-06→08 (idx10→11) | 37.2→33.6 | `09354a9ac` | parametrize / unify | Унификация `label`→`response` для svm/kmeans/knn/df (94 файла) — единый параметр вместо алгоритм-специфичных копий | "replace of label with response for svm, kmeans, knn, df (#1755)" |
| **FastLED** | 2025-04 (idx9→11) | 25.4→20.3 | `e25c69209` | extract + template | Множество path-классов (Line/Circle/Heart/Rose/Phyllotaxis/Gielis…), наследовавших `XYPathGenerator`, реорганизованы и обобщены через `function<T>`, вынесены в `xypaths.{h,cpp}` (−473/+513) | "Refactor XYPath classes for common path generation" |
| **ESPAsyncWebServer** | 2025-10 (idx≈19→20) | 10.0→7.7 | `cf4f12232` | encapsulate / extract class | URI-matching логика (повторявшаяся для regex и не-regex путей) инкапсулирована в класс `AsyncURIMatcher`; `WebHandlers.cpp` −38. Кодобаза крошечная (~26 nodes) — приём реальный, вклад в метрику малый | "Introduce AsyncURIMatcher class … with and without regex support" |

\* alpaka: основное снижение dup растянуто на 2021–2023 (33.7%→17.4%); перечислены коммиты с самым явным дедуп-эффектом. Граница idx10→11 — точка слияния primary/specialization и common-header.

---

## Помечено отдельно — НЕ засчитано как ручная дедупликация (артефакты / шум)

| Проект | Почему не засчитано |
|---|---|
| **azure-sdk-for-cpp** | Заявленное "41.5%→31.4%" обманчиво: idx1 (41.5%) — одиночный выброс на ранней мелкой кодобазе (71 nodes), реальный диапазон стабилен ~30–33% всю историю. Доминирует **кодогенерация**: `077d32ff` "New protocol layer for blob/datalake/queue generated by CodeGen (#3261)" (+17795/−17769) — дублирование между REST-клиентами теперь генерируется из спецификации, а не убрано вручную. Чёткого ручного дедуп-окна нет. |
| **vgmtrans `8743874` "Remove common.h"** | 482 файла, +10911/−9841 — массовая замена шортхендов типов (`u8`→`uint8_t`), стилевая нормализация, НЕ дедупликация логики. Не засчитан (но другие коммиты vgmtrans засчитаны). |
| **PcapPlusPlus** | dup плоский ~9–11% всю историю (delta −1.8) — шум измерения, нет выраженной дедупликации. |
| **cpprestsdk** | dup плоский ~18–20% всю историю (delta −1.6) — шум, нет дедуп-окна. |
| **ESPAsyncWebServer (метрика)** | Кодобаза 8–27 nodes; dup_ratio шумный (одиночный выброс 13% на idx0, рост до 11% при добавлении форк-файлов). Приём (AsyncURIMatcher) реальный, но вклад в проценты нестабильный — уверенность средняя. |
| Пропущены по тематике (security / reverse-engineering / game-mod), как просили | SDRPlusPlusBrown, Nidhogg, HexRaysCodeXplorer, skyrim-community-shaders, Universal-Dear-ImGui-Hook, zed-open-capture. |

---

## Сводная статистика приёмов (засчитанные коммиты)

Всего разобрано проектов с реальной дедупликацией: **7**
(alpaka, vgmtrans, Effekseer, acts, oneDAL, FastLED, ESPAsyncWebServer).
Плюс **1 помечен как артефакт** (azure-sdk-for-cpp).

Частота приёмов по засчитанным коммитам (13 коммитов):

| Приём | Кол-во | Коммиты |
|---|---|---|
| **extract-to-common-header / -module** (вынос общего кода вверх) | **5** | alpaka `f060ddf7`, Effekseer `d1fece84` `5564dccb`, acts `e4a3ec90a`, FastLED `e25c69209` |
| **template-параметризация / merge specialization** (копии под типы/платформы → один шаблон) | **2** | alpaka `7559f91c` `19bed293` |
| **base-class / common-widget / encapsulate** (поднять общее в базу/класс) | **3** | vgmtrans `1d92b84` (NVI) `be35fce`, ESPAsyncWebServer `cf4f12232` |
| **extract-function** (повтор → одна функция/helper) | **2** | acts `357480730` `eb8677409` |
| **delete-dead / legacy-cleanup** (удаление дублирующего устаревшего кода) | **1** | oneDAL `80d98e7c2` |
| **parametrize / unify параметр** (N случаев → один параметризованный) | **1** | oneDAL `09354a9ac` |
| X-macro / кодоген ручной | 0 | (azure-кодоген — артефакт, не ручной приём) |

**Доминирующий приём — extract-to-common-header/module** (вынос общего кода в общий
заголовок/модуль вверх по физической иерархии). Это ядро методологии Lakos: общий код
должен жить на нижнем уровне, от которого зависят остальные.

Второй по силе мотив — **обобщение копий между «осями вариативности»**: платформы
(CUDA/HIP в alpaka), графические бэкенды (DX/GL/Metal/Vulkan в Effekseer), типы алгоритмов
(oneDAL), геометрические кривые (FastLED). Везде N почти одинаковых реализаций сводятся к
одной через template, базовый класс или общий модуль.

---

## Яркие примеры

1. **acts `e4a3ec90a` — extract-to-common-header.** Три бенчмарка степпера
   (`AtlasStepperBenchmark.cpp`, `EigenStepperBenchmark.cpp`,
   `StraightLineStepperBenchmark.cpp`) были копипастом ~126 строк каждый. Вынесли класс
   `BenchmarkStepper` (`parseOptions/makeField/run`) в `StepperBenchmarkCommons.hpp` — каждый
   файл сжался до ~10 строк. −343/+209.

2. **alpaka `7559f91c` — template-параметризация платформенных классов.**
   `QueueUniformCudaHipRtBlocking` и `…NonBlocking` (каждый со своими DevType/EventType
   трейтами) заменены на `QueueUniformCudaHipRt<bool TBlocking>` + `using`-алиасы.
   Дополнительно `f060ddf7` вынес весь общий CUDA/HIP-код в `CudaHipCommon.hpp` (−729 строк).

3. **Effekseer `d1fece84` / `5564dccb` — общий модуль для N графических бэкендов.**
   Vertex-буферы и VertexLayout, дублировавшиеся в DX9/DX11/GL/Metal/Vulkan-рендерерах,
   подняты в `EffekseerRendererCommon`. Суммарно −2557/+1246 на 70 файлов.

4. **oneDAL `80d98e7c2` — delete-dead.** "Clean old interfaces": удалены целые
   устаревшие алгоритмы (adaboost/brownboost/logitboost), дублировавшие новый API.
   −40049/+186 — единственный коммит, объясняющий устойчивый слом dup_ratio 37.2%→33.6%,
   который держится годами.

---

## Что это даёт archcheck

Каждый засчитанный приём — это **TP, который кто-то реально вынес**: если бы archcheck
сканировал предыдущий снапшот, он обязан был показать эту пару. Признак настоящего
копипаста по этой выборке — *экстрагируемость*: дубль убирается одним из шести приёмов
выше (см. также правило extractability-теста в mem:`fp_classification_rules`). То, что
убрать нельзя (идиомы, generated-код, стилевая нормализация типа `u8`→`uint8_t`) — в
правой таблице «не засчитано» и пересекается с классами FP из
[../duplication_fp_analysis.md](../duplication_fp_analysis.md).
