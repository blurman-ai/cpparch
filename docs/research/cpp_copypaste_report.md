# C++ копипаст в агентских репах — реальный код, пара к паре

> Прогон `archcheck --duplication` по корпусу `experiments/ai_repo_run/structural_census.tsv`.
> Цель — не числа, а **реальные пары дублей**: оригинал ↔ копия, рядом, с кликабельными
> ссылками на GitHub на **каждый** фрагмент. Каждая пара честно размечена:
> ✅ настоящий копипаст (extractable — можно было вынести в общую функцию) /
> ⚠️ легитимный или FP (генерёнка, вендоринг, платформенные интринсики, параллельные порты).

---

## Intro

- **Реп с непустым копипастом:** 763 (строки `verdict==VIOLATION_KEPT`, `dup_pairs>0` из census).
- **Метод:** census считал `dup_pairs` через быстрый токеновый проход archcheck v0.1. Отсортировал по
  `dup_per100f` (плотность дублей на 100 файлов) — самые «копипастные» сверху. Топ-20 разобрал руками:
  взял клон `~/oss/<owner_repo>`, зафиксировал `sha` (12 симв.), прогнал
  `archcheck --duplication`, прочитал **оба** фрагмента каждой яркой пары verbatim из клона.
- **Классы дублей** (формат вывода archcheck): `EXACT` (токены совпали 1:1), `LITERAL` (совпали с
  точностью до литералов/строк), `RENAMED` (совпали с точностью до имён), `STRUCTURAL` (совпал каркас,
  расходятся детали). `weighted` — взвешенная схожесть, `line` — построчная.
- **Честная рамка.** archcheck v0.1 сырой. Высокая плотность дублей часто означает не «грязный код
  агента», а **вендоринг** (втянутая в дерево чужая библиотека), **генерёнку** (SWIG/Cython/амальгамация
  одним файлом) или **параллельные платформенные порты** (linux/macosx, SSE/AVX/NEON). Это видно прямо
  по топу: №1 по плотности — вендорный `argparse.hpp`, №2 — сгенерированный компилятором `bootstrap.c`.
  Настоящий копипаст (один и тот же кусок логики продублирован вручную внутри проекта) — отдельный,
  меньший класс. Я его не раздуваю.

---

## Корпус-уровень: разделяем дублирование на природу (и главный вывод)

Ручной топ-20 ниже надёжен, но не масштабируется. Чтобы получить ответ по всему корпусу, каждую
фрагмент-пару разметили по **трём ортогональным осям** (скрипт `experiments/ai_repo_run/classify_dup.py`,
отчётный уровень — тул не трогали):

- **generated** — кодогенерёнка (`.pb.`, `moc_`, SWIG `_wrap`, lex/yacc) — никто не правит;
- **vendored** — чужой код: путь (BSP/HAL/`drivers/net/wireless/`/known-libs) **или копирайт на
  вендора** (STMicroelectronics, MediaTek, Clounix…), причём только если это **не** владелец репы
  (личное имя автора `Copyright by Sukchan Lee` в open5gs ложно не метит — капкан #081 закрыт);
- **file_dup** — тот же файл скопирован целиком: совпал basename **или** структурный twin
  (`xr819s↔xr829`, `stm32l5↔stm32u5`, `linux↔windows`). Это «дублирование файла», **не** копипаст куска;
- **authored** — фрагмент в **разноимённом не-вендорном** файле = чистый авторский копипаст.

**Распределение по корпусу (764 репы, 25 284 пары):**

| корзина | пар | доля |
|---|---|---|
| generated | 101 | 0.4% |
| vendored | 5 944 | 23.5% |
| **file_dup** (целый файл) | **13 905** | **55.0%** |
| **authored** (кусок логики) | **5 334** | **21.1%** |

**Вывод №1 — «копипаст у 99% реп» был иллюзией смешивания.** Больше половины «дублирования» — это
копирование **целых файлов**, ещё ~24% — вендоринг. Настоящего авторского копипаста фрагмента — 21%,
он есть у 77% реп, но **тонкий**: у 39% таких реп всего 1–3 пары, у 23% реп — вовсе ноль.

### Главный тест: agentic vs control (чистый authored)

Гипотезу «агентский C++ копипастнее / ИИ создаёт дрейф» проверили прямо: 50 дисциплинированных
control-реп (`group==control` из `cpp_ai_drift_metrics.tsv`: simc, dxvk, fmt, QuantLib…) склонировали
тем же конвейером и разметили тем же классификатором. Метрика — authored-пар на 100 файлов
(репы с ≥20 файлов).

| authored / 100 файлов | agentic (n=749) | control (n=46) |
|---|---|---|
| медиана | 0.78 | 1.00 |
| среднее | 1.52 | 1.53 |
| p90 | 3.24 | 2.86 |

**Mann-Whitney U: z=−1.46, p=0.144 → разница НЕзначима.**

**Вывод №2 — сигнала ИИ-копипаст-дрейфа в C++ нет.** Авторский копипаст в агентских репах **не выше**,
чем в control (медиана даже чуть ниже). Гипотеза «агенты насорили копипастом» не подтверждается. И
«слабая надежда, что C++-комьюнити дисциплинированнее» — тоже: для неё нужна была бы разница
agentic<control, а её нет — группы одинаковые.

Где агентские реально отличаются — это **file_dup** (2.19 vs 1.41 на 100 файлов) и состав корпуса
(firmware/BSP, копирование целых драйверов по платам), а **не** авторский стиль. Сырые `dup_pairs`,
смешивавшие всё в кучу, и создавали ложное «агентские грязнее».

**Усиление:** агентский набор — это *нарушители* (предотобраны за `dup_pairs>0`), control — все 50
подряд. Отбор играл **против** нуля — и всё равно ноль. Оговорки: снимок «сейчас», не before/after
(прошлый diff-in-diff тоже не нашёл дрейфа в 90-дн окне — сходится); control n=46 мал; в authored
остаётся платформенный шум (intel `linux↔windows`), который если что **завышает** агентские.

Данные: `experiments/ai_repo_run/classified_dup.tsv` (агентские), `control_classified.tsv` (control).

---

## Топ-20 — глубокий разбор

### 1. msoos/cryptominisat — `dup_pairs=25`, плотность **416.7** ⚠️

Лидер по плотности — артефакт: в дереве лежат **две копии** хедера CLI-парсера `argparse.hpp`
(основная и под `src/oracle/`). Все 25 пар — `EXACT` между ними. Это вендоринг одной библиотеки в
двух местах, не рукотворный копипаст логики проекта.

```cpp
// src/argparse.hpp:95-129  — class ArgumentParser, тот же файл
// (полностью идентичен src/oracle/argparse.hpp:95-129)
```
- A: https://github.com/msoos/cryptominisat/blob/78b3244c84ab/src/argparse.hpp#L95-L129
- B: https://github.com/msoos/cryptominisat/blob/78b3244c84ab/src/oracle/argparse.hpp#L95-L129

**Вердикт: ⚠️ FP** — вендорённая сторонняя библиотека, втянутая дважды. Лечится симлинком/одним include,
но это не «агент насорил».

---

### 2. c2lang/c2compiler — `dup_pairs=2`, плотность **200.0** ⚠️

Весь проект в census свёлся к одному файлу — `bootstrap/bootstrap.c`, это **сгенерированный**
самим компилятором C2 моноблоб (~40k строк). Обе пары — повторяющиеся `switch` по enum'у внутри
генерёнки.

```cpp
// bootstrap/bootstrap.c:18413-18425        // bootstrap/bootstrap.c:38592-38604
switch (k) {                                 // тот же каркас switch по build_target_Kind,
case build_target_Kind_Image:                // сгенерирован транслятором, не написан руками
  return false;
case build_target_Kind_Executable:
  return true;
...
```
- A: https://github.com/c2lang/c2compiler/blob/80dcf3d7f9a4/bootstrap/bootstrap.c#L18413-L18425
- B: https://github.com/c2lang/c2compiler/blob/80dcf3d7f9a4/bootstrap/bootstrap.c#L38592-L38604

**Вердикт: ⚠️ FP** — машинно-сгенерированный код. Дублирование тут — свойство кодогенератора.

---

### 3. Seagate/openSeaChest — `dup_pairs=24`, плотность **109.1** ✅

А вот это уже настоящий копипаст. Семейство CLI-утилит (`openSeaChest_Basics`, `_Info`, `_SMART`,
`_Configure`, …) — каждая копирует целые блоки обработки опций друг у друга. `EXACT`, длинные блоки.

```cpp
// openSeaChest_Basics.c:1376-1399          // openSeaChest_Info.c:1376-1399  (EXACT, 1:1)
concurrentRanges ranges;                     concurrentRanges ranges;
M_INITIALIZE_STRUCTURE(&ranges, sizeof(concurrentRanges));
ranges.size    = sizeof(concurrentRanges);
ranges.version = CONCURRENT_RANGES_VERSION;
switch (get_Concurrent_Positioning_Ranges(&deviceList[deviceIter], &ranges)) {
case SUCCESS:
    print_Concurrent_Positioning_Ranges(&ranges);
    break;
case NOT_SUPPORTED:
    if (VERBOSITY_QUIET < toolVerbosity)
        print_str("Concurrent positioning ranges are not supported on this device.\n");
    exitCode = UTIL_EXIT_OPERATION_NOT_SUPPORTED;
    break;
...
```
- A: https://github.com/Seagate/openSeaChest/blob/c8dc7b9e39bb/utils/C/openSeaChest/openSeaChest_Basics.c#L1376-L1399
- B: https://github.com/Seagate/openSeaChest/blob/c8dc7b9e39bb/utils/C/openSeaChest/openSeaChest_Info.c#L1376-L1399

Ещё одна, 55 строк подряд `LITERAL` (отличаются только строки сообщений):
- A: https://github.com/Seagate/openSeaChest/blob/c8dc7b9e39bb/utils/C/openSeaChest/openSeaChest_Info.c#L1114-L1168
- B: https://github.com/Seagate/openSeaChest/blob/c8dc7b9e39bb/utils/C/openSeaChest/openSeaChest_SMART.c#L1451-L1505

**Вердикт: ✅ настоящий копипаст.** Каждая опция-хендлер размножена по всем утилитам. Extractable:
блок `--concurrentRanges` просится в общий `handle_concurrent_ranges_option()` в shared-модуле.

---

### 4. ocornut/imgui — `dup_pairs=28`, плотность **75.7** ⚠️

Все пары — между `examples/example_<platform>_<backend>/main.cpp`: GLFW/SDL2/SDL3/Win32 × Vulkan/WGPU.
Это намеренно **самодостаточные** примеры: каждый backend — отдельный компилируемый main, и Vulkan
setup/teardown в них одинаков by design.

```cpp
// example_glfw_vulkan/main.cpp:247-256      // example_sdl3_vulkan/main.cpp:240-249  (EXACT)
vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);
#ifdef APP_USE_VULKAN_DEBUG_REPORT
    auto f_vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)
        vkGetInstanceProcAddr(g_Instance, "vkDestroyDebugReportCallbackEXT");
    f_vkDestroyDebugReportCallbackEXT(g_Instance, g_DebugReport, g_Allocator);
#endif
vkDestroyDevice(g_Device, g_Allocator);
vkDestroyInstance(g_Instance, g_Allocator);
```
- A: https://github.com/ocornut/imgui/blob/6acba3b47d2a/examples/example_glfw_vulkan/main.cpp#L247-L256
- B: https://github.com/ocornut/imgui/blob/6acba3b47d2a/examples/example_sdl3_vulkan/main.cpp#L240-L249

**Вердикт: ⚠️ легитимно.** Примеры должны быть копипастой — пользователь берёт один файл целиком.
Выносить в общий хелпер противоречит цели «один self-contained пример».

---

### 5. ender672/liboil — `dup_pairs=13`, плотность **52.0** ⚠️ / частично ✅

Смешанный случай. Часть пар — платформенные SIMD-реализации (`oil_resample_sse2.c` ↔
`oil_resample_avx2.c` ↔ `_neon.c`), где scalar-хелпер физически одинаков:

```cpp
// oil_resample_sse2.c:24-28                 // oil_resample_avx2.c:22-26  (EXACT)
__m128 newval, hi;
smp = (__m128)_mm_srli_si128((__m128i)smp, 4);
newval = _mm_set_ss(v);
hi = _mm_shuffle_ps(smp, newval, _MM_SHUFFLE(0, 0, 3, 2));
return _mm_shuffle_ps(smp, hi, _MM_SHUFFLE(2, 0, 1, 0));
```
- A: https://github.com/ender672/liboil/blob/156736708a49/oil_resample_sse2.c#L24-L28
- B: https://github.com/ender672/liboil/blob/156736708a49/oil_resample_avx2.c#L22-L26

Но есть и честные внутрифайловые дубли в `imgscale.c` (один и тот же блок на L390/L501/L701):
- A: https://github.com/ender672/liboil/blob/156736708a49/imgscale.c#L390-L394
- B: https://github.com/ender672/liboil/blob/156736708a49/imgscale.c#L501-L505

**Вердикт: ⚠️ SIMD-варианты легитимны** (идиома платформенных интринсиков), но **✅ внутри `imgscale.c`**
один блок повторён трижды — это уже extractable.

---

### 6. etlegacy/etlegacy — `dup_pairs=235`, плотность **51.9** ⚠️ / частично ✅

Три рендер-бэкенда живут параллельно: `src/renderer/` (OpenGL), `src/rendererGLES/`, `src/renderer_vk/`
(Vulkan). Огромные `EXACT`-блоки между ними — это форк рендерера под три API.

```cpp
// src/rendererGLES/tr_backend.c:250-325 ↔ src/renderer_vk/tr_backend.c:250-325  (EXACT, 76 строк)
```
- A: https://github.com/etlegacy/etlegacy/blob/5715d01fabf0/src/rendererGLES/tr_backend.c#L250-L325
- B: https://github.com/etlegacy/etlegacy/blob/5715d01fabf0/src/renderer_vk/tr_backend.c#L250-L325

**Вердикт: ⚠️ форк бэкендов / частично ✅.** Дублирование между API-бэкендами — осознанный приём
(разные шейдерные пути), но размер копий (десятки строк tr_shade/tr_backend) намекает, что общую
математику можно было вынести в `renderercommon/`.

---

### 7. DanielXMoore/Civet — `dup_pairs=4`, плотность **50.0** ⚠️

Две копии **вендорённого tree-sitter runtime** (`lsp/tree-sitter/` и `lsp/tree-sitter-hera/`).
`array.h`/`parser.h` совпадают `EXACT`.

```cpp
// lsp/tree-sitter-hera/src/tree_sitter/parser.h:155-169 ↔ lsp/tree-sitter/.../parser.h:155-169
```
- A: https://github.com/DanielXMoore/Civet/blob/4097b9324a7a/lsp/tree-sitter-hera/src/tree_sitter/parser.h#L155-L169
- B: https://github.com/DanielXMoore/Civet/blob/4097b9324a7a/lsp/tree-sitter/src/tree_sitter/parser.h#L155-L169

**Вердикт: ⚠️ вендоринг** двух генерёных tree-sitter-грамматик, тянущих идентичный runtime.

---

### 8. yhirose/culebra — `dup_pairs=10`, плотность **45.5** ⚠️

Дубли между `include/stdlib_interp.h` и `include/stdlib_jit.h` — это **амальгамации**
(single-header сборки одного и того же кода под два режима: интерпретатор и JIT), плюс
внутрифайловые повторы в `jit.h`.

- A: https://github.com/yhirose/culebra/blob/8230eb1fe494/include/stdlib_interp.h#L2374-L2382
- B: https://github.com/yhirose/culebra/blob/8230eb1fe494/include/stdlib_jit.h#L1077-L1085

**Вердикт: ⚠️ генерёнка** (амальгамированные хедеры) — дубль присущ упаковке single-header.

---

### 9. unum-cloud/USearch — `dup_pairs=8`, плотность **44.4** ✅

Честный случай. В JNI-биндинге три почти идентичных метода подряд (`STRUCTURAL ~0.71-0.73`,
50 строк каждый) — копипаст под разные типы скаляров.

```cpp
// cloud_unum_usearch_Index.cpp:796-845 ↔ :851-900 ↔ :906-954  (три копии одного тела)
```
- A: https://github.com/unum-cloud/USearch/blob/9fd6b0115dcd/java/cloud/unum/usearch/cloud_unum_usearch_Index.cpp#L796-L845
- B: https://github.com/unum-cloud/USearch/blob/9fd6b0115dcd/java/cloud/unum/usearch/cloud_unum_usearch_Index.cpp#L851-L900

**Вердикт: ✅ настоящий копипаст.** Три JNI-обёртки отличаются типом — просится шаблон/макрос.

---

### 10. sailfishos-mirror/protobuf — `dup_pairs=472`, плотность **42.9** ⚠️

Зеркало protobuf. Дубли — между **амальгамациями upb** (`php-upb.c`, `ruby-upb.c`, `ruby-upb.h`) и
исходными `upb/...` хедерами. Это сгенерированные one-file сборки runtime для PHP/Ruby.

- A: https://github.com/sailfishos-mirror/protobuf/blob/54b62411d722/php/ext/google/protobuf/php-upb.c#L4290-L4305
- B: https://github.com/sailfishos-mirror/protobuf/blob/54b62411d722/ruby/ext/google/protobuf_c/ruby-upb.c#L3096-L3111

**Вердикт: ⚠️ генерёнка/вендоринг.** Амальгамации upb по определению дублируют исходники.

---

### 11. shafferjohn/Prime95 — `dup_pairs=85`, плотность **40.5** ⚠️

Два слоя. (1) Дублированное дерево `gwnum/prime95/` ↔ `prime95/`. (2) Платформенные порты:
`linux64/`, `macosx/`, `macosx64/` держат **идентичные** `os_routines.c`/`menu.c`/`prime.c`, причём
внутри уже стоят `#ifdef __linux__ / __APPLE__` — то есть один и тот же мультиплатформенный файл
скопирован в три каталога.

```c
// linux64/os_routines.c:64-84              // macosx/os_routines.c:64-84  (EXACT)
#if defined (__linux__)
	fd = open ("/proc/loadavg", O_RDONLY);
	...
	count = sscanf (ldavgbuf, "%lf", &load_avg);
	return (load_avg);
#elif defined (__FreeBSD__) || defined (__APPLE__)
	if (getloadavg (load, sizeof(load)/sizeof(load[0])) < 0) return (-1.0);
	return (load[0]);
#endif
```
- A: https://github.com/shafferjohn/Prime95/blob/bd6969fa5e70/linux64/os_routines.c#L64-L84
- B: https://github.com/shafferjohn/Prime95/blob/bd6969fa5e70/macosx/os_routines.c#L64-L84

**Вердикт: ⚠️ параллельные порты / borderline ✅.** Поскольку `#ifdef` уже покрывает все платформы,
три копии одного файла избыточны — мог быть один общий `os_routines.c`. Но это «исторический» приём
сборки, не агентский мусор.

---

### 12. rapastranac/gempba — `dup_pairs=13`, плотность **39.4** ✅✅ (самый чистый кейс)

Эталонный настоящий копипаст. Два класса-шедулера — `mpi_centralized_scheduler` и
`mpi_semi_centralized_scheduler` — делят **дословно одинаковые** блоки сбора статистики по MPI.

```cpp
// mpi_centralized_scheduler.hpp:122-141     // mpi_semi_centralized_scheduler.hpp:106-125 (EXACT)
m_stats_vector.push_back(m_stats);
for (int v_src_rank = 1; v_src_rank < m_world_size; ++v_src_rank) {
    MPI_Status v_status;
    const int v_probe_err = MPI_Probe(v_src_rank, DUMMY_TAG, m_world_communicator, &v_status);
    if (v_probe_err != MPI_SUCCESS) {
        spdlog::error("rank {} failed to probe message from rank {}\n", m_world_rank, v_src_rank);
        continue;
    }
    int v_count{};
    MPI_Get_count(&v_status, MPI_BYTE, &v_count);
    task_packet v_rank_packet(v_count);
    const int v_recv_err = MPI_Recv(v_rank_packet.data(), v_count, MPI_BYTE,
                                    v_src_rank, DUMMY_TAG, m_world_communicator, &v_status);
    if (v_recv_err != MPI_SUCCESS) {
        spdlog::error("rank {} failed to receive stats from rank {}\n", m_world_rank, v_src_rank);
        continue;
    }
    default_mpi_stats v_rank_stats = default_mpi_stats::from_packet(v_rank_packet);
    m_stats_vector.push_back(std::move(v_rank_stats));
}
```
- A: https://github.com/rapastranac/gempba/blob/89a2629b6887/private/impl/schedulers/mpi_centralized_scheduler.hpp#L122-L141
- B: https://github.com/rapastranac/gempba/blob/89a2629b6887/private/impl/schedulers/mpi_semi_centralized_scheduler.hpp#L106-L125

Таких пар между двумя шедулерами — целая дюжина (`EXACT` и `STRUCTURAL` 0.88–0.96).

**Вердикт: ✅✅ хрестоматийный копипаст.** Два шедулера обязаны делить общий базовый класс или
свободную функцию `gather_stats()`. Это extractable на 100%.

---

### 13. ChristianGaser/CAT-Surface — `dup_pairs=87`, плотность **39.9** ⚠️

Дубли в `cat_surface_cython/cat_surf/_bbreg.c` ↔ `_volume.c` ↔ `_surf.c`. Это **Cython-генерёнка**:
`__Pyx_*` boilerplate (CPython list/refcount-обвязка), одинаковый в каждом скомпилированном модуле.

```c
// _bbreg.c:2727-2739 ↔ _volume.c:2730-2742  (__Pyx_PyList_Append helper, генерёнка Cython)
PyListObject* L = (PyListObject*) list;
Py_ssize_t len = Py_SIZE(list);
if (likely(L->allocated > len)) { Py_INCREF(x); ... }
```
- A: https://github.com/ChristianGaser/CAT-Surface/blob/e778f1c079ca/cat_surface_cython/cat_surf/_bbreg.c#L2727-L2739
- B: https://github.com/ChristianGaser/CAT-Surface/blob/e778f1c079ca/cat_surface_cython/cat_surf/_volume.c#L2730-L2742

**Вердикт: ⚠️ FP** — сгенерированный Cython runtime.

---

### 14. canmeng12/packages — `dup_pairs=507`, плотность **34.5** ⚠️

OpenWRT-зеркало пакетов. Дубли — между вендорёнными драйверами Wi-Fi MediaTek
(`mt7603e/` ↔ `mt7612e/` ↔ `mt7615d/`): параллельные форки одного SDK драйвера на разные чипы.

- A: https://github.com/canmeng12/packages/blob/8a28a6ca3edd/packages/mt/drivers/mt7603e/src/mt7603_wifi/os/linux/bb_soc.c#L36-L92
- B: https://github.com/canmeng12/packages/blob/8a28a6ca3edd/packages/mt/drivers/mt7612e/src/mt76x2/os/linux/bb_soc.c#L36-L92

**Вердикт: ⚠️ вендоринг** чужих драйверов, форкнутых по чипам. Не код проекта.

---

### 15. wolfSSL/wolfssh — `dup_pairs=28`, плотность **33.3** ✅ / частично ⚠️

Честный копипаст внутри проекта: хелпер именованного семафора скопирован из `apps/wolfssh/wolfssh.c`
в `examples/client/client.c`.

```c
// apps/wolfssh/wolfssh.c:289-300            // examples/client/client.c:257-268  (EXACT)
if (s != NULL) {
    snprintf(s->name, sizeof(s->name), "/wolfssh_winch_%d", (int)getpid());
    s->s = sem_open(s->name, O_CREAT | O_EXCL | O_RDWR, 0600, n);
    if (s->s == SEM_FAILED && errno == EEXIST) {
        /* named semaphore already exists, unlink the name and try once more */
        if (sem_unlink(s->name) == 0)
            s->s = sem_open(s->name, O_CREAT | O_RDWR, 0600, n);
    }
}
return (s != NULL && s->s != SEM_FAILED);
```
- A: https://github.com/wolfSSL/wolfssh/blob/aa04eca815a2/apps/wolfssh/wolfssh.c#L289-L300
- B: https://github.com/wolfSSL/wolfssh/blob/aa04eca815a2/examples/client/client.c#L257-L268

Отдельный класс — `examples/echoserver/echoserver.c` ↔ `ide/Espressif/ESP-IDF/.../echoserver.c`:
ESP32-вариант примера — это вендорная копия (⚠️).

**Вердикт: ✅ для app↔client** (хелпер просится в общий util), **⚠️ для Espressif-копии** (платформенный
порт примера).

---

### 16. ShimmerResearch/shimmer3-firmware — `dup_pairs=20`, плотность **32.3** ⚠️

Все пары — между `LogAndStream_Shimmer3/Shimmer_Driver/FatFs/` и `S3_Sleep/shimmer3_common_source/FatFs/`:
два firmware-приложения тащат **идентичную копию вендорной FatFs** (ChaN).

- A: https://github.com/ShimmerResearch/shimmer3-firmware/blob/db65e6c9f0c3/LogAndStream_Shimmer3/Shimmer_Driver/FatFs/ff.c#L1925-L1972
- B: https://github.com/ShimmerResearch/shimmer3-firmware/blob/db65e6c9f0c3/S3_Sleep/shimmer3_common_source/FatFs/ff.c#L1924-L1971

**Вердикт: ⚠️ вендоринг** одной библиотеки в двух прошивках (общий драйвер мог бы жить в shared-каталоге).

---

### 17. RuleWorld/bionetgen — `dup_pairs=145`, плотность **31.4** ⚠️

Несколько источников: дублированное дерево `bng-graph/BNGcore/` ↔ `src/core/`, SWIG-генерёнка
(`BNGcore_wrap.cxx`), и вендорная `nauty` в двух версиях.

- A: https://github.com/RuleWorld/bionetgen/blob/2abaf75d6e75/bng-graph/BNGcore/BNGcore_wrap.cxx#L1740-L1764
- B: https://github.com/RuleWorld/bionetgen/blob/2abaf75d6e75/src/core/BNGcore_wrap.cxx#L1740-L1764

**Вердикт: ⚠️ генерёнка + дублированное дерево.** SWIG-`_wrap.cxx` дублируется механически.

---

### 18. sysprog21/rv32emu — `dup_pairs=14`, плотность **28.0** ✅

Хороший настоящий кейс. Логика записи store-инструкций RISC-V продублирована между **интерпретатором**
(`src/emulate.c`) и **JIT fallback** (`src/jit.c`) — выровненный/невыровненный store слово в слово.

```c
// src/emulate.c:120-152                     // src/jit.c:1587-1609  (STRUCTURAL 0.93)
case rv_insn_sw:
    value = rv->X[ir->rs2];                  value = rv->X[vreg_idx];
    if ((addr & 1) == 0) {                    if ((addr & 1) == 0) {
        rv->io.mem_write_s(rv, addr, value & 0xFFFF);
        rv->io.mem_write_s(rv, addr + 2, (value >> 16) & 0xFFFF);
    } else {                                  } else {
        for (int i = 0; i < 4; i++)              for (int i = 0; i < 4; i++)
            rv->io.mem_write_b(rv, addr + i, (value >> (i * 8)) & 0xFF);
    }
```
- A: https://github.com/sysprog21/rv32emu/blob/c5e8819ada69/src/emulate.c#L120-L152
- B: https://github.com/sysprog21/rv32emu/blob/c5e8819ada69/src/jit.c#L1587-L1609

**Вердикт: ✅ настоящий копипаст.** Интерпретатор и JIT разъехались на одной и той же misaligned-store
логике — просится общий `static inline emit_store(...)`. Плюс внутрифайловые дубли в `cache.c`
(L421↔L445) и `rv32_v_template.c`.

---

### 19. ossappscollective/OSS-DocumentScanner — `dup_pairs=22`, плотность **28.6** ✅ / частично ⚠️

Чистый копипаст: `WhitePaperTransform.cpp` ↔ `WhitePaperTransform2.cpp` — функция размножена с
суффиксом `2` без единого изменения тела.

```cpp
// WhitePaperTransform.cpp:210-214           // WhitePaperTransform2.cpp:116-120  (EXACT)
void fastGaussianBlur(const cv::Mat &img, const cv::Mat &res, int kSize, double sigma) {
    cv::Mat kernel1D = cv::getGaussianKernel(kSize, sigma);   // fastGaussianBlur2 — то же тело
    cv::sepFilter2D(img, res, -1, kernel1D, kernel1D);
}
```
- A: https://github.com/ossappscollective/OSS-DocumentScanner/blob/8d9ef9a91cf8/cpp/src/WhitePaperTransform.cpp#L210-L214
- B: https://github.com/ossappscollective/OSS-DocumentScanner/blob/8d9ef9a91cf8/cpp/src/WhitePaperTransform2.cpp#L116-L120

Остальные пары — вендорный `jsoncons` (header-only JSON-библиотека в `cpp/src/include/jsoncons/`), ⚠️.

**Вердикт: ✅ для WhitePaperTransform** (`fastGaussianBlur` vs `fastGaussianBlur2` — копия с суффиксом,
классический агентский «сделай ещё один такой же»), **⚠️ для jsoncons** (вендоринг).

---

### 20. zephyrproject-rtos/trusted-firmware-m — `dup_pairs=966`, плотность **27.8** ⚠️

Самый большой `dup_pairs` в топе. TF-M — security firmware с множеством платформенных
портов (`platform/ext/target/<vendor>/<board>/`), где HAL/драйверы копируются по бордам. Это
параллельные платформенные деревья, а не рукотворный копипаст одной логики.

**Вердикт: ⚠️ платформенные порты.** Высокий счётчик — следствие десятков target-каталогов.
(детальные пары не выношу — однотипны: один HAL-драйвер размножен по бордам.)

---

## Итоговая разметка топ-20

| # | репо | плотность | вердикт | класс |
|---|---|---|---|---|
| 1 | msoos/cryptominisat | 416.7 | ⚠️ | вендорный argparse ×2 |
| 2 | c2lang/c2compiler | 200.0 | ⚠️ | сгенерированный bootstrap.c |
| 3 | Seagate/openSeaChest | 109.1 | ✅ | копипаст опций между CLI-утилитами |
| 4 | ocornut/imgui | 75.7 | ⚠️ | self-contained примеры backends |
| 5 | ender672/liboil | 52.0 | ⚠️/✅ | SIMD-варианты + дубли в imgscale.c |
| 6 | etlegacy/etlegacy | 51.9 | ⚠️/✅ | форк 3 рендер-бэкендов |
| 7 | DanielXMoore/Civet | 50.0 | ⚠️ | вендорный tree-sitter ×2 |
| 8 | yhirose/culebra | 45.5 | ⚠️ | амальгамации interp/jit |
| 9 | unum-cloud/USearch | 44.4 | ✅ | 3 JNI-обёртки копией |
| 10 | sailfishos-mirror/protobuf | 42.9 | ⚠️ | амальгамации upb |
| 11 | shafferjohn/Prime95 | 40.5 | ⚠️ | копии файлов по платформам |
| 12 | rapastranac/gempba | 39.4 | ✅✅ | 2 MPI-шедулера дословно |
| 13 | ChristianGaser/CAT-Surface | 39.9 | ⚠️ | Cython-генерёнка |
| 14 | canmeng12/packages | 34.5 | ⚠️ | вендорные MTK Wi-Fi драйверы |
| 15 | wolfSSL/wolfssh | 33.3 | ✅/⚠️ | app↔client хелпер + ESP-копия |
| 16 | ShimmerResearch/shimmer3-firmware | 32.3 | ⚠️ | вендорная FatFs ×2 |
| 17 | RuleWorld/bionetgen | 31.4 | ⚠️ | SWIG + дубл. дерево + nauty |
| 18 | sysprog21/rv32emu | 28.0 | ✅ | interp↔JIT store-логика |
| 19 | ossappscollective/OSS-DocumentScanner | 28.6 | ✅/⚠️ | WhitePaperTransform2 + jsoncons |
| 20 | zephyrproject-rtos/trusted-firmware-m | 27.8 | ⚠️ | платформенные HAL-порты |

**Разметка пар (по ярким примерам, не по полному dup_pairs):**
- ✅ чистый копипаст: **3** (openSeaChest, gempba, rv32emu)
- ✅ преимущественно копипаст с примесью: **2** (USearch — ✅; OSS-DocumentScanner — ✅ WhitePaper)
- ⚠️/✅ смешанные (платформенный форк + extractable хвост): **3** (liboil, etlegacy, Prime95, wolfssh частично)
- ⚠️ легитимные/FP: **остальные ~12** (вендоринг, генерёнка, self-contained примеры, платформенные порты)

Грубая доля: **~7/20 содержат настоящий extractable копипаст**, **~13/20 — легитимный шум**
(вендоринг/генерёнка/платформа). Это ровно то, что и ожидаешь от сырого токенового детектора:
плотность дублей сама по себе НЕ равна «грязному коду» — лидеры топа это вендоринг и генерёнка.

---

## Сводная таблица всех реп с копипастом (763, по убыванию плотности)

| repo | dup_pairs | dup_per100f | ссылка |
|---|---|---|---|
| msoos/cryptominisat | 25 | 416.7 | [↗](https://github.com/msoos/cryptominisat) |
| c2lang/c2compiler | 2 | 200.0 | [↗](https://github.com/c2lang/c2compiler) |
| Seagate/openSeaChest | 24 | 109.1 | [↗](https://github.com/Seagate/openSeaChest) |
| baidxi/buildroot | 367 | 97.9 | [↗](https://github.com/baidxi/buildroot) |
| ocornut/imgui | 28 | 75.7 | [↗](https://github.com/ocornut/imgui) |
| ender672/liboil | 13 | 52.0 | [↗](https://github.com/ender672/liboil) |
| etlegacy/etlegacy | 235 | 51.9 | [↗](https://github.com/etlegacy/etlegacy) |
| DanielXMoore/Civet | 4 | 50.0 | [↗](https://github.com/DanielXMoore/Civet) |
| yhirose/culebra | 10 | 45.5 | [↗](https://github.com/yhirose/culebra) |
| unum-cloud/USearch | 8 | 44.4 | [↗](https://github.com/unum-cloud/USearch) |

> Полная таблица всех 763 реп (с `dup_pairs`, `dup_per100f` и ссылкой на репо) — ниже одним блоком.
> Источник: `experiments/ai_repo_run/structural_census.tsv` (фильтр `verdict==VIOLATION_KEPT && dup_pairs>0`).

<!-- FULL_TABLE_START -->

| repo | dup_pairs | dup_per100f | ссылка |
|---|---|---|---|
| msoos/cryptominisat | 25 | 416.7 | [↗](https://github.com/msoos/cryptominisat) |
| c2lang/c2compiler | 2 | 200.0 | [↗](https://github.com/c2lang/c2compiler) |
| Seagate/openSeaChest | 24 | 109.1 | [↗](https://github.com/Seagate/openSeaChest) |
| baidxi/buildroot | 367 | 97.9 | [↗](https://github.com/baidxi/buildroot) |
| ocornut/imgui | 28 | 75.7 | [↗](https://github.com/ocornut/imgui) |
| ender672/liboil | 13 | 52.0 | [↗](https://github.com/ender672/liboil) |
| etlegacy/etlegacy | 235 | 51.9 | [↗](https://github.com/etlegacy/etlegacy) |
| DanielXMoore/Civet | 4 | 50.0 | [↗](https://github.com/DanielXMoore/Civet) |
| yhirose/culebra | 10 | 45.5 | [↗](https://github.com/yhirose/culebra) |
| unum-cloud/USearch | 8 | 44.4 | [↗](https://github.com/unum-cloud/USearch) |
| sailfishos-mirror/protobuf | 472 | 42.9 | [↗](https://github.com/sailfishos-mirror/protobuf) |
| shafferjohn/Prime95 | 85 | 40.5 | [↗](https://github.com/shafferjohn/Prime95) |
| rapastranac/gempba | 13 | 39.4 | [↗](https://github.com/rapastranac/gempba) |
| ChristianGaser/CAT-Surface | 87 | 39.9 | [↗](https://github.com/ChristianGaser/CAT-Surface) |
| canmeng12/packages | 507 | 34.5 | [↗](https://github.com/canmeng12/packages) |
| wolfSSL/wolfssh | 28 | 33.3 | [↗](https://github.com/wolfSSL/wolfssh) |
| ShimmerResearch/shimmer3-firmware | 20 | 32.3 | [↗](https://github.com/ShimmerResearch/shimmer3-firmware) |
| RuleWorld/bionetgen | 145 | 31.4 | [↗](https://github.com/RuleWorld/bionetgen) |
| sysprog21/rv32emu | 14 | 28.0 | [↗](https://github.com/sysprog21/rv32emu) |
| ossappscollective/OSS-DocumentScanner | 22 | 28.6 | [↗](https://github.com/ossappscollective/OSS-DocumentScanner) |
| zephyrproject-rtos/trusted-firmware-m | 966 | 27.8 | [↗](https://github.com/zephyrproject-rtos/trusted-firmware-m) |
| Palpitate-xus/DBMS_C_plus_plus | 9 | 26.5 | [↗](https://github.com/Palpitate-xus/DBMS_C_plus_plus) |
| carlos-sweb/scrapy-indicadores | 1 | 25.0 | [↗](https://github.com/carlos-sweb/scrapy-indicadores) |
| Porcupine-Factory/FirstPersonController | 8 | 24.2 | [↗](https://github.com/Porcupine-Factory/FirstPersonController) |
| FujiNetWIFI/fujinet-firmware | 433 | 24.4 | [↗](https://github.com/FujiNetWIFI/fujinet-firmware) |
| LINBIT/drbd | 12 | 23.5 | [↗](https://github.com/LINBIT/drbd) |
| sonic-net/sonic-buildimage | 467 | 22.1 | [↗](https://github.com/sonic-net/sonic-buildimage) |
| RefPerSys/RefPerSys | 27 | 22.9 | [↗](https://github.com/RefPerSys/RefPerSys) |
| Mellanox/mstflint | 8 | 22.9 | [↗](https://github.com/Mellanox/mstflint) |
| jimhester/libvroom | 22 | 21.2 | [↗](https://github.com/jimhester/libvroom) |
| AMReX-Codes/amrex | 53 | 21.8 | [↗](https://github.com/AMReX-Codes/amrex) |
| GregorGullwi/FlashCpp | 35 | 20.7 | [↗](https://github.com/GregorGullwi/FlashCpp) |
| pantoniou/libfyaml | 30 | 19.9 | [↗](https://github.com/pantoniou/libfyaml) |
| openwrt/mt76 | 42 | 19.1 | [↗](https://github.com/openwrt/mt76) |
| flux-framework/flux-coral2 | 9 | 19.1 | [↗](https://github.com/flux-framework/flux-coral2) |
| storaged-project/libblockdev | 16 | 18.0 | [↗](https://github.com/storaged-project/libblockdev) |
| nrfconnect/sdk-trusted-firmware-m | 602 | 18.5 | [↗](https://github.com/nrfconnect/sdk-trusted-firmware-m) |
| Azure/azure-sdk-for-c | 19 | 18.4 | [↗](https://github.com/Azure/azure-sdk-for-c) |
| ashvardanian/NumKong | 66 | 18.4 | [↗](https://github.com/ashvardanian/NumKong) |
| zeux/meshoptimizer | 9 | 17.3 | [↗](https://github.com/zeux/meshoptimizer) |
| qoretechnologies/qore | 31 | 17.5 | [↗](https://github.com/qoretechnologies/qore) |
| munich-quantum-toolkit/ddsim | 4 | 17.4 | [↗](https://github.com/munich-quantum-toolkit/ddsim) |
| cx20/hello | 84 | 17.4 | [↗](https://github.com/cx20/hello) |
| Xilinx/mlir-air | 35 | 16.1 | [↗](https://github.com/Xilinx/mlir-air) |
| scawful/yaze | 231 | 16.5 | [↗](https://github.com/scawful/yaze) |
| NLnetLabs/unbound | 8 | 16.0 | [↗](https://github.com/NLnetLabs/unbound) |
| MOLAorg/mola | 16 | 16.8 | [↗](https://github.com/MOLAorg/mola) |
| ermig1979/Simd | 2 | 16.7 | [↗](https://github.com/ermig1979/Simd) |
| zhao-shihan/MACESW | 3 | 15.0 | [↗](https://github.com/zhao-shihan/MACESW) |
| taodd/cephtrace | 2 | 15.4 | [↗](https://github.com/taodd/cephtrace) |
| sailfishos-mirror/valgrind | 96 | 15.2 | [↗](https://github.com/sailfishos-mirror/valgrind) |
| rossvideo/Catena | 3 | 15.8 | [↗](https://github.com/rossvideo/Catena) |
| paulfloyd/freebsd_valgrind | 96 | 15.2 | [↗](https://github.com/paulfloyd/freebsd_valgrind) |
| LouisBrunner/valgrind-macos | 97 | 15.1 | [↗](https://github.com/LouisBrunner/valgrind-macos) |
| k21971/EvilHack | 77 | 15.0 | [↗](https://github.com/k21971/EvilHack) |
| ispc/ispc | 32 | 15.5 | [↗](https://github.com/ispc/ispc) |
| EXP-code/EXP | 60 | 15.3 | [↗](https://github.com/EXP-code/EXP) |
| arkime/arkime | 22 | 15.9 | [↗](https://github.com/arkime/arkime) |
| AcademySoftwareFoundation/OpenImageIO | 36 | 15.1 | [↗](https://github.com/AcademySoftwareFoundation/OpenImageIO) |
| sciapp/gr | 52 | 14.8 | [↗](https://github.com/sciapp/gr) |
| plougher/squashfs-tools | 14 | 14.1 | [↗](https://github.com/plougher/squashfs-tools) |
| open-mpi/hwloc | 15 | 14.6 | [↗](https://github.com/open-mpi/hwloc) |
| onecoolx/picasso | 15 | 14.4 | [↗](https://github.com/onecoolx/picasso) |
| ETLCPP/etl | 2 | 14.3 | [↗](https://github.com/ETLCPP/etl) |
| Xilinx/XRT | 104 | 13.7 | [↗](https://github.com/Xilinx/XRT) |
| ovis-hpc/ldms | 12 | 13.2 | [↗](https://github.com/ovis-hpc/ldms) |
| OpenLI-NZ/openli | 21 | 13.0 | [↗](https://github.com/OpenLI-NZ/openli) |
| numpy/numpy-user-dtypes | 10 | 13.0 | [↗](https://github.com/numpy/numpy-user-dtypes) |
| libsdl-org/libtiff | 12 | 13.8 | [↗](https://github.com/libsdl-org/libtiff) |
| GNOME/libxml2 | 22 | 13.8 | [↗](https://github.com/GNOME/libxml2) |
| derekbsnider/madc | 16 | 13.1 | [↗](https://github.com/derekbsnider/madc) |
| daos-stack/daos | 96 | 13.2 | [↗](https://github.com/daos-stack/daos) |
| CodSpeedHQ/valgrind-codspeed | 87 | 13.8 | [↗](https://github.com/CodSpeedHQ/valgrind-codspeed) |
| simdutf/simdutf | 49 | 12.2 | [↗](https://github.com/simdutf/simdutf) |
| ringof/rx888-firmware | 9 | 12.7 | [↗](https://github.com/ringof/rx888-firmware) |
| owntone/owntone-server | 21 | 12.1 | [↗](https://github.com/owntone/owntone-server) |
| nanomq/NanoNNG | 39 | 12.3 | [↗](https://github.com/nanomq/NanoNNG) |
| mavlink/MAVSDK | 90 | 12.5 | [↗](https://github.com/mavlink/MAVSDK) |
| m-ab-s/aom | 102 | 12.0 | [↗](https://github.com/m-ab-s/aom) |
| LuminariMUD/Luminari-Source | 49 | 12.9 | [↗](https://github.com/LuminariMUD/Luminari-Source) |
| krrishnarraj/clpeak | 12 | 12.4 | [↗](https://github.com/krrishnarraj/clpeak) |
| ciyam/ciyam | 39 | 12.1 | [↗](https://github.com/ciyam/ciyam) |
| bytecodealliance/wasm-micro-runtime | 68 | 12.8 | [↗](https://github.com/bytecodealliance/wasm-micro-runtime) |
| ZuluSCSI/ZuluSCSI-firmware | 30 | 11.6 | [↗](https://github.com/ZuluSCSI/ZuluSCSI-firmware) |
| xfce-mirror/thunar | 28 | 11.2 | [↗](https://github.com/xfce-mirror/thunar) |
| typesense/typesense | 19 | 11.0 | [↗](https://github.com/typesense/typesense) |
| tonioni/WinUAE | 116 | 11.6 | [↗](https://github.com/tonioni/WinUAE) |
| TinyMUSH/TinyMUSH | 17 | 11.4 | [↗](https://github.com/TinyMUSH/TinyMUSH) |
| SimpleITK/SimpleITK | 2 | 11.1 | [↗](https://github.com/SimpleITK/SimpleITK) |
| profanity-im/profanity | 21 | 11.7 | [↗](https://github.com/profanity-im/profanity) |
| networkupstools/nut | 49 | 11.6 | [↗](https://github.com/networkupstools/nut) |
| keycard-tech/keycard-shell | 28 | 11.2 | [↗](https://github.com/keycard-tech/keycard-shell) |
| HansKristian-Work/vkd3d-proton | 12 | 11.4 | [↗](https://github.com/HansKristian-Work/vkd3d-proton) |
| ggml-org/ggml | 51 | 11.8 | [↗](https://github.com/ggml-org/ggml) |
| GaijinEntertainment/daScript | 59 | 11.8 | [↗](https://github.com/GaijinEntertainment/daScript) |
| danielnachun/recipe_staging | 27 | 11.7 | [↗](https://github.com/danielnachun/recipe_staging) |
| brazilofmux/tinymux | 51 | 11.1 | [↗](https://github.com/brazilofmux/tinymux) |
| uxlfoundation/oneDPL | 18 | 10.8 | [↗](https://github.com/uxlfoundation/oneDPL) |
| sailfishos-mirror/libpcap | 3 | 10.7 | [↗](https://github.com/sailfishos-mirror/libpcap) |
| sailfishos-mirror/bluez | 74 | 10.3 | [↗](https://github.com/sailfishos-mirror/bluez) |
| sailfishos/bluez5 | 74 | 10.3 | [↗](https://github.com/sailfishos/bluez5) |
| pytorch/FBGEMM | 43 | 10.8 | [↗](https://github.com/pytorch/FBGEMM) |
| PipeWire/pipewire | 98 | 10.1 | [↗](https://github.com/PipeWire/pipewire) |
| pantavisor/pantavisor | 1 | 10.0 | [↗](https://github.com/pantavisor/pantavisor) |
| osmocom/osmo-ttcn3-hacks | 6 | 10.2 | [↗](https://github.com/osmocom/osmo-ttcn3-hacks) |
| open-telemetry/opentelemetry-go-instrumentation | 3 | 10.0 | [↗](https://github.com/open-telemetry/opentelemetry-go-instrumentation) |
| official-pikafish/Pikafish | 7 | 10.9 | [↗](https://github.com/official-pikafish/Pikafish) |
| nxp-imx/imx-smw | 33 | 10.2 | [↗](https://github.com/nxp-imx/imx-smw) |
| NVIDIAGameWorks/dxvk-remix | 93 | 10.8 | [↗](https://github.com/NVIDIAGameWorks/dxvk-remix) |
| Mu2e/otsdaq-mu2e | 3 | 10.7 | [↗](https://github.com/Mu2e/otsdaq-mu2e) |
| josevcm/nfc-laboratory | 37 | 10.6 | [↗](https://github.com/josevcm/nfc-laboratory) |
| gvsoc/gvsoc-core | 12 | 10.8 | [↗](https://github.com/gvsoc/gvsoc-core) |
| facebook/openbmc | 179 | 10.1 | [↗](https://github.com/facebook/openbmc) |
| DMTF/libspdm | 35 | 10.1 | [↗](https://github.com/DMTF/libspdm) |
| copperwater/xNetHack | 58 | 10.7 | [↗](https://github.com/copperwater/xNetHack) |
| bluez/bluez | 74 | 10.3 | [↗](https://github.com/bluez/bluez) |
| bacnet-stack/bacnet-stack | 83 | 10.3 | [↗](https://github.com/bacnet-stack/bacnet-stack) |
| zephyrproject-rtos/mbedtls | 12 | 9.9 | [↗](https://github.com/zephyrproject-rtos/mbedtls) |
| Xastir/Xastir | 14 | 9.2 | [↗](https://github.com/Xastir/Xastir) |
| StreamUPTips/obs-streamup | 10 | 9.8 | [↗](https://github.com/StreamUPTips/obs-streamup) |
| seoklab/nurikit | 10 | 9.4 | [↗](https://github.com/seoklab/nurikit) |
| rzeronte/brakeza3d | 43 | 9.8 | [↗](https://github.com/rzeronte/brakeza3d) |
| ruby/openssl | 5 | 9.3 | [↗](https://github.com/ruby/openssl) |
| OpenChemistry/avogadrolibs | 65 | 9.7 | [↗](https://github.com/OpenChemistry/avogadrolibs) |
| nrfconnect/sdk-mcuboot | 19 | 9.0 | [↗](https://github.com/nrfconnect/sdk-mcuboot) |
| nrfconnect/sdk-mbedtls | 12 | 9.9 | [↗](https://github.com/nrfconnect/sdk-mbedtls) |
| mheily/libkqueue | 5 | 9.6 | [↗](https://github.com/mheily/libkqueue) |
| linux-application-whitelisting/fapolicyd | 9 | 9.9 | [↗](https://github.com/linux-application-whitelisting/fapolicyd) |
| kokkos/kokkos | 66 | 9.0 | [↗](https://github.com/kokkos/kokkos) |
| GNOME/epiphany | 29 | 9.1 | [↗](https://github.com/GNOME/epiphany) |
| ggml-org/whisper.cpp | 65 | 9.9 | [↗](https://github.com/ggml-org/whisper.cpp) |
| fluent/fluent-bit | 127 | 9.9 | [↗](https://github.com/fluent/fluent-bit) |
| conradhuebler/curcuma | 28 | 9.9 | [↗](https://github.com/conradhuebler/curcuma) |
| bloomberg/comdb2 | 50 | 9.2 | [↗](https://github.com/bloomberg/comdb2) |
| apache/thrift | 30 | 9.0 | [↗](https://github.com/apache/thrift) |
| y-scope/clp | 58 | 8.5 | [↗](https://github.com/y-scope/clp) |
| webserver-llc/angie | 38 | 8.9 | [↗](https://github.com/webserver-llc/angie) |
| sysprogs/openocd | 55 | 8.7 | [↗](https://github.com/sysprogs/openocd) |
| streetpea/chiaki-ng | 14 | 8.0 | [↗](https://github.com/streetpea/chiaki-ng) |
| simonsj/haproxy | 50 | 8.1 | [↗](https://github.com/simonsj/haproxy) |
| sailfishos-mirror/hostap | 66 | 8.5 | [↗](https://github.com/sailfishos-mirror/hostap) |
| ruby/prism | 7 | 8.3 | [↗](https://github.com/ruby/prism) |
| ROCm/rccl | 27 | 8.3 | [↗](https://github.com/ROCm/rccl) |
| pcornier/iigs_simulation | 6 | 8.6 | [↗](https://github.com/pcornier/iigs_simulation) |
| OpenVisualCloud/Media-Transport-Library | 25 | 8.2 | [↗](https://github.com/OpenVisualCloud/Media-Transport-Library) |
| oneapi-src/unified-runtime | 34 | 8.5 | [↗](https://github.com/oneapi-src/unified-runtime) |
| nss-dev/nss | 103 | 8.5 | [↗](https://github.com/nss-dev/nss) |
| MundyRepo/MuNDy | 17 | 8.8 | [↗](https://github.com/MundyRepo/MuNDy) |
| masc-ucsc/hhds | 2 | 8.3 | [↗](https://github.com/masc-ucsc/hhds) |
| llr-cta/calin | 82 | 8.0 | [↗](https://github.com/llr-cta/calin) |
| kokkos/kokkos-kernels | 64 | 8.0 | [↗](https://github.com/kokkos/kokkos-kernels) |
| InternationalColorConsortium/iccDEV | 3 | 8.1 | [↗](https://github.com/InternationalColorConsortium/iccDEV) |
| hexsen929/openwrt_packages | 3 | 8.8 | [↗](https://github.com/hexsen929/openwrt_packages) |
| HermanChen/mpp | 28 | 8.2 | [↗](https://github.com/HermanChen/mpp) |
| haproxy/haproxy | 50 | 8.1 | [↗](https://github.com/haproxy/haproxy) |
| grommunio/gromox | 37 | 8.5 | [↗](https://github.com/grommunio/gromox) |
| greearb/hostap-ct | 69 | 8.9 | [↗](https://github.com/greearb/hostap-ct) |
| GNOME/gdm | 7 | 8.9 | [↗](https://github.com/GNOME/gdm) |
| GNOME/evolution-data-server | 71 | 8.0 | [↗](https://github.com/GNOME/evolution-data-server) |
| espressif/esp-iot-solution | 108 | 8.0 | [↗](https://github.com/espressif/esp-iot-solution) |
| eclipse-openj9/openj9-omr | 230 | 8.6 | [↗](https://github.com/eclipse-openj9/openj9-omr) |
| eclipse-omr/omr | 230 | 8.6 | [↗](https://github.com/eclipse-omr/omr) |
| doxygen/doxygen | 38 | 8.2 | [↗](https://github.com/doxygen/doxygen) |
| ca2/operating_system-windows | 241 | 8.9 | [↗](https://github.com/ca2/operating_system-windows) |
| xfce-mirror/xfce4-power-manager | 6 | 7.7 | [↗](https://github.com/xfce-mirror/xfce4-power-manager) |
| tlapack/tlapack | 24 | 7.2 | [↗](https://github.com/tlapack/tlapack) |
| Srlive1201/LibRPA | 12 | 7.1 | [↗](https://github.com/Srlive1201/LibRPA) |
| simpleble/simpleble | 27 | 7.3 | [↗](https://github.com/simpleble/simpleble) |
| sailfishos-mirror/gnupg | 34 | 7.0 | [↗](https://github.com/sailfishos-mirror/gnupg) |
| rvdbreemen/OTGW-firmware | 1 | 7.7 | [↗](https://github.com/rvdbreemen/OTGW-firmware) |
| orioledb/orioledb | 11 | 7.5 | [↗](https://github.com/orioledb/orioledb) |
| OpenXiangShan/GEM5 | 50 | 7.1 | [↗](https://github.com/OpenXiangShan/GEM5) |
| OpenMagnetics/MKF | 14 | 7.3 | [↗](https://github.com/OpenMagnetics/MKF) |
| NVIDIA/edk2-platforms | 157 | 7.9 | [↗](https://github.com/NVIDIA/edk2-platforms) |
| netxms/netxms | 93 | 7.1 | [↗](https://github.com/netxms/netxms) |
| Linux-Comedi/comedi | 21 | 7.9 | [↗](https://github.com/Linux-Comedi/comedi) |
| lingo-db/lingo-db | 27 | 7.6 | [↗](https://github.com/lingo-db/lingo-db) |
| libressl/openbsd | 40 | 7.1 | [↗](https://github.com/libressl/openbsd) |
| libfuse/libfuse | 6 | 7.9 | [↗](https://github.com/libfuse/libfuse) |
| ioccc-src/mkiocccentry | 10 | 7.9 | [↗](https://github.com/ioccc-src/mkiocccentry) |
| infiniflow/infinity | 15 | 7.9 | [↗](https://github.com/infiniflow/infinity) |
| ibm-s390-linux/s390-tools | 53 | 7.8 | [↗](https://github.com/ibm-s390-linux/s390-tools) |
| haaspors/rlib | 22 | 7.1 | [↗](https://github.com/haaspors/rlib) |
| groonga/groonga | 36 | 7.1 | [↗](https://github.com/groonga/groonga) |
| greenbone/gsad | 5 | 7.5 | [↗](https://github.com/greenbone/gsad) |
| gpg/gnupg | 34 | 7.0 | [↗](https://github.com/gpg/gnupg) |
| gpac/gpac | 52 | 7.9 | [↗](https://github.com/gpac/gpac) |
| GNOME/gnome-calendar | 15 | 7.9 | [↗](https://github.com/GNOME/gnome-calendar) |
| GNOME/gegl | 43 | 7.3 | [↗](https://github.com/GNOME/gegl) |
| getsentry/sentry-native | 10 | 7.3 | [↗](https://github.com/getsentry/sentry-native) |
| freebsd/drm-kmod | 63 | 7.2 | [↗](https://github.com/freebsd/drm-kmod) |
| fernandotonon/QtMeshEditor | 29 | 7.8 | [↗](https://github.com/fernandotonon/QtMeshEditor) |
| duckdb/duckdb-spatial | 5 | 7.2 | [↗](https://github.com/duckdb/duckdb-spatial) |
| DPDK/dpdk | 356 | 7.1 | [↗](https://github.com/DPDK/dpdk) |
| DCMTK/dcmtk | 162 | 7.4 | [↗](https://github.com/DCMTK/dcmtk) |
| DangarStu/JACL | 7 | 7.8 | [↗](https://github.com/DangarStu/JACL) |
| chrisagrams/mscompress | 3 | 7.3 | [↗](https://github.com/chrisagrams/mscompress) |
| ca2/operating_system-posix | 335 | 7.4 | [↗](https://github.com/ca2/operating_system-posix) |
| BlazingRenderer/BRender | 36 | 7.1 | [↗](https://github.com/BlazingRenderer/BRender) |
| BLAST-WarpX/warpx | 17 | 7.0 | [↗](https://github.com/BLAST-WarpX/warpx) |
| bitaxeorg/ESP-Miner | 10 | 7.0 | [↗](https://github.com/bitaxeorg/ESP-Miner) |
| arthenica/fontconfig | 5 | 7.9 | [↗](https://github.com/arthenica/fontconfig) |
| alphaonex86/CatchChallenger | 67 | 7.0 | [↗](https://github.com/alphaonex86/CatchChallenger) |
| aegis-aead/boringssl | 1 | 7.7 | [↗](https://github.com/aegis-aead/boringssl) |
| xfce-mirror/xfce4-settings | 6 | 6.9 | [↗](https://github.com/xfce-mirror/xfce4-settings) |
| xfce-mirror/libxfce4ui | 5 | 6.2 | [↗](https://github.com/xfce-mirror/libxfce4ui) |
| vnotex/vnote | 52 | 6.5 | [↗](https://github.com/vnotex/vnote) |
| ValeevGroup/SeQuant | 15 | 6.5 | [↗](https://github.com/ValeevGroup/SeQuant) |
| uncrustify/uncrustify | 20 | 6.9 | [↗](https://github.com/uncrustify/uncrustify) |
| Tencent/ncnn | 138 | 6.0 | [↗](https://github.com/Tencent/ncnn) |
| Tatsh/bpmdetect | 2 | 6.2 | [↗](https://github.com/Tatsh/bpmdetect) |
| socnetv/app | 11 | 6.3 | [↗](https://github.com/socnetv/app) |
| rui314/mold | 4 | 6.2 | [↗](https://github.com/rui314/mold) |
| RTimothyEdwards/magic | 33 | 6.0 | [↗](https://github.com/RTimothyEdwards/magic) |
| rsyslog/rsyslog | 5 | 6.9 | [↗](https://github.com/rsyslog/rsyslog) |
| ros-navigation/navigation2 | 2 | 6.7 | [↗](https://github.com/ros-navigation/navigation2) |
| RebelToolbox/RebelEngine | 130 | 6.5 | [↗](https://github.com/RebelToolbox/RebelEngine) |
| pkgconf/pkgconf | 3 | 6.5 | [↗](https://github.com/pkgconf/pkgconf) |
| pjsip/pjproject | 53 | 6.0 | [↗](https://github.com/pjsip/pjproject) |
| openzfs/zfs | 57 | 6.2 | [↗](https://github.com/openzfs/zfs) |
| ofiwg/libfabric | 14 | 6.9 | [↗](https://github.com/ofiwg/libfabric) |
| northriv/KAME | 22 | 6.1 | [↗](https://github.com/northriv/KAME) |
| munich-quantum-toolkit/qmap | 8 | 6.0 | [↗](https://github.com/munich-quantum-toolkit/qmap) |
| mruby/mruby | 15 | 6.9 | [↗](https://github.com/mruby/mruby) |
| MaximumTrainer/MaximumTrainer_Redux | 32 | 6.3 | [↗](https://github.com/MaximumTrainer/MaximumTrainer_Redux) |
| lustre/lustre-release | 47 | 6.4 | [↗](https://github.com/lustre/lustre-release) |
| linux-nvme/nvme-cli | 21 | 6.9 | [↗](https://github.com/linux-nvme/nvme-cli) |
| libretro/beetle-psx-libretro | 14 | 6.4 | [↗](https://github.com/libretro/beetle-psx-libretro) |
| kotuku-group/kotuku | 63 | 6.6 | [↗](https://github.com/kotuku-group/kotuku) |
| intel/compute-runtime | 202 | 6.4 | [↗](https://github.com/intel/compute-runtime) |
| icl-utk-edu/papi | 37 | 6.5 | [↗](https://github.com/icl-utk-edu/papi) |
| hyyh619/irrlicht-1.8.3 | 58 | 6.7 | [↗](https://github.com/hyyh619/irrlicht-1.8.3) |
| greenbone/gvm-libs | 7 | 6.1 | [↗](https://github.com/greenbone/gvm-libs) |
| GNOME/gnome-software | 19 | 6.0 | [↗](https://github.com/GNOME/gnome-software) |
| glotzerlab/hoomd-blue | 41 | 6.6 | [↗](https://github.com/glotzerlab/hoomd-blue) |
| frstrtr/c2pool | 18 | 6.7 | [↗](https://github.com/frstrtr/c2pool) |
| FRRouting/frr | 88 | 6.4 | [↗](https://github.com/FRRouting/frr) |
| freetype/freetype | 31 | 6.5 | [↗](https://github.com/freetype/freetype) |
| fhoedemakers/pico-infonesPlus | 13 | 6.9 | [↗](https://github.com/fhoedemakers/pico-infonesPlus) |
| esrrhs/fakelua | 4 | 6.2 | [↗](https://github.com/esrrhs/fakelua) |
| espressif/esp-mqtt | 2 | 6.9 | [↗](https://github.com/espressif/esp-mqtt) |
| dds-bridge/dds | 6 | 6.2 | [↗](https://github.com/dds-bridge/dds) |
| davischw/frr | 90 | 6.5 | [↗](https://github.com/davischw/frr) |
| centreon/centreon-collect | 11 | 6.4 | [↗](https://github.com/centreon/centreon-collect) |
| aseprite/freetype2 | 31 | 6.5 | [↗](https://github.com/aseprite/freetype2) |
| analogdevicesinc/libiio | 7 | 6.9 | [↗](https://github.com/analogdevicesinc/libiio) |
| amule-project/amule | 28 | 6.2 | [↗](https://github.com/amule-project/amule) |
| aallamaa/alcove | 3 | 6.1 | [↗](https://github.com/aallamaa/alcove) |
| a1ive/nwinfo | 12 | 6.5 | [↗](https://github.com/a1ive/nwinfo) |
| 389ds/389-ds-base | 41 | 6.0 | [↗](https://github.com/389ds/389-ds-base) |
| xkbcommon/libxkbcommon | 7 | 5.6 | [↗](https://github.com/xkbcommon/libxkbcommon) |
| vowstar/qsoc | 20 | 5.7 | [↗](https://github.com/vowstar/qsoc) |
| tomconder/maze | 7 | 5.4 | [↗](https://github.com/tomconder/maze) |
| timescale/timescaledb | 11 | 5.0 | [↗](https://github.com/timescale/timescaledb) |
| thesofproject/sof | 85 | 5.6 | [↗](https://github.com/thesofproject/sof) |
| tempesta-tech/tempesta | 14 | 5.6 | [↗](https://github.com/tempesta-tech/tempesta) |
| sonic-net/sonic-swss | 20 | 5.7 | [↗](https://github.com/sonic-net/sonic-swss) |
| solvcon/modmesh | 1 | 5.0 | [↗](https://github.com/solvcon/modmesh) |
| sjg20/paperman | 13 | 5.2 | [↗](https://github.com/sjg20/paperman) |
| rxrbln/t2sde | 5 | 5.4 | [↗](https://github.com/rxrbln/t2sde) |
| reupen/columns_ui | 16 | 5.4 | [↗](https://github.com/reupen/columns_ui) |
| quakeforge/quakeforge | 68 | 5.0 | [↗](https://github.com/quakeforge/quakeforge) |
| pygame-community/pygame-ce | 9 | 5.8 | [↗](https://github.com/pygame-community/pygame-ce) |
| PhoshMobi/phoc | 10 | 5.7 | [↗](https://github.com/PhoshMobi/phoc) |
| pgEdge/spock | 4 | 5.4 | [↗](https://github.com/pgEdge/spock) |
| PackageKit/PackageKit | 13 | 5.9 | [↗](https://github.com/PackageKit/PackageKit) |
| ornladios/ADIOS2 | 49 | 5.5 | [↗](https://github.com/ornladios/ADIOS2) |
| OpenSC/OpenSC | 16 | 5.4 | [↗](https://github.com/OpenSC/OpenSC) |
| openocd-org/openocd | 35 | 5.7 | [↗](https://github.com/openocd-org/openocd) |
| open-enterprise-solutions/enterprise | 62 | 5.5 | [↗](https://github.com/open-enterprise-solutions/enterprise) |
| OpenChemistry/avogadroapp | 2 | 5.4 | [↗](https://github.com/OpenChemistry/avogadroapp) |
| openbgpd-portable/openbgpd-openbsd | 12 | 5.3 | [↗](https://github.com/openbgpd-portable/openbgpd-openbsd) |
| NVIDIA/edk2-nvidia | 51 | 5.2 | [↗](https://github.com/NVIDIA/edk2-nvidia) |
| navit-gps/navit | 29 | 5.3 | [↗](https://github.com/navit-gps/navit) |
| MOLAorg/mola_lidar_odometry | 1 | 5.3 | [↗](https://github.com/MOLAorg/mola_lidar_odometry) |
| mne-tools/mne-cpp | 49 | 5.2 | [↗](https://github.com/mne-tools/mne-cpp) |
| mltframework/mlt | 26 | 5.7 | [↗](https://github.com/mltframework/mlt) |
| mariadb-corporation/mariadb-columnstore-engine | 77 | 5.3 | [↗](https://github.com/mariadb-corporation/mariadb-columnstore-engine) |
| llvm/circt | 54 | 5.0 | [↗](https://github.com/llvm/circt) |
| LizardByte/Sunshine | 8 | 5.6 | [↗](https://github.com/LizardByte/Sunshine) |
| liudf0716/apfree-wifidog | 4 | 5.6 | [↗](https://github.com/liudf0716/apfree-wifidog) |
| linuxdeepin/treeland | 31 | 5.4 | [↗](https://github.com/linuxdeepin/treeland) |
| libssh2/libssh2 | 2 | 5.3 | [↗](https://github.com/libssh2/libssh2) |
| librats/rats-search | 4 | 5.8 | [↗](https://github.com/librats/rats-search) |
| KevinOConnor/klipper-dev | 37 | 5.4 | [↗](https://github.com/KevinOConnor/klipper-dev) |
| inxware/ert-components | 108 | 5.0 | [↗](https://github.com/inxware/ert-components) |
| ilia-maslakov/mcdev | 23 | 5.7 | [↗](https://github.com/ilia-maslakov/mcdev) |
| Hamlib/Hamlib | 41 | 5.7 | [↗](https://github.com/Hamlib/Hamlib) |
| halide/Halide | 47 | 5.4 | [↗](https://github.com/halide/Halide) |
| gstlearn/gstlearn | 42 | 5.2 | [↗](https://github.com/gstlearn/gstlearn) |
| greenbone/gvmd | 17 | 5.8 | [↗](https://github.com/greenbone/gvmd) |
| godot42x/ya | 27 | 5.4 | [↗](https://github.com/godot42x/ya) |
| GNOME/mutter | 53 | 5.2 | [↗](https://github.com/GNOME/mutter) |
| GNOME/glib | 54 | 5.0 | [↗](https://github.com/GNOME/glib) |
| FluidSynth/fluidsynth | 9 | 5.7 | [↗](https://github.com/FluidSynth/fluidsynth) |
| facebook/mvfst | 20 | 5.5 | [↗](https://github.com/facebook/mvfst) |
| espressif/openocd-esp32 | 36 | 5.4 | [↗](https://github.com/espressif/openocd-esp32) |
| ea4k/klog | 10 | 5.2 | [↗](https://github.com/ea4k/klog) |
| drowe67/freedv-gui | 22 | 5.6 | [↗](https://github.com/drowe67/freedv-gui) |
| djeada/Lightpad | 18 | 5.0 | [↗](https://github.com/djeada/Lightpad) |
| CoolProp/CoolProp | 13 | 5.2 | [↗](https://github.com/CoolProp/CoolProp) |
| ChibiOS/ChibiOS | 26 | 5.5 | [↗](https://github.com/ChibiOS/ChibiOS) |
| Azure/azure-sdk-for-cpp | 39 | 5.6 | [↗](https://github.com/Azure/azure-sdk-for-cpp) |
| aryx/xix | 21 | 5.0 | [↗](https://github.com/aryx/xix) |
| aravindkrishnaswamy/RISE | 61 | 5.0 | [↗](https://github.com/aravindkrishnaswamy/RISE) |
| zcash/zcash | 28 | 4.8 | [↗](https://github.com/zcash/zcash) |
| xfce-mirror/xfce4-panel | 6 | 4.5 | [↗](https://github.com/xfce-mirror/xfce4-panel) |
| wolfSSL/wolfProvider | 2 | 4.0 | [↗](https://github.com/wolfSSL/wolfProvider) |
| wiredtiger/wiredtiger | 18 | 4.4 | [↗](https://github.com/wiredtiger/wiredtiger) |
| Washington-University/workbench | 181 | 4.6 | [↗](https://github.com/Washington-University/workbench) |
| VTT-ProperTune/OpenPFC | 13 | 4.0 | [↗](https://github.com/VTT-ProperTune/OpenPFC) |
| VERITAS-Observatory/EventDisplay_v4 | 21 | 4.8 | [↗](https://github.com/VERITAS-Observatory/EventDisplay_v4) |
| veltzer/demos-os-linux | 54 | 4.5 | [↗](https://github.com/veltzer/demos-os-linux) |
| ton-blockchain/ton | 53 | 4.3 | [↗](https://github.com/ton-blockchain/ton) |
| tksuoran/erhe | 62 | 4.1 | [↗](https://github.com/tksuoran/erhe) |
| surge-synthesizer/shortcircuit-xt | 16 | 4.8 | [↗](https://github.com/surge-synthesizer/shortcircuit-xt) |
| stpaine/FERS | 6 | 4.1 | [↗](https://github.com/stpaine/FERS) |
| storaged-project/udisks | 7 | 4.5 | [↗](https://github.com/storaged-project/udisks) |
| Slicer/Slicer | 94 | 4.1 | [↗](https://github.com/Slicer/Slicer) |
| sipwise/rtpengine | 11 | 4.6 | [↗](https://github.com/sipwise/rtpengine) |
| shorepine/amy | 3 | 4.7 | [↗](https://github.com/shorepine/amy) |
| schwabe/openvpn | 11 | 4.2 | [↗](https://github.com/schwabe/openvpn) |
| sailfishos-mirror/curl | 28 | 4.5 | [↗](https://github.com/sailfishos-mirror/curl) |
| pmodels/mpich | 70 | 4.4 | [↗](https://github.com/pmodels/mpich) |
| PierreSenellart/provsql | 7 | 4.9 | [↗](https://github.com/PierreSenellart/provsql) |
| PhoshMobi/phosh | 23 | 4.8 | [↗](https://github.com/PhoshMobi/phosh) |
| OpenVPN/openvpn | 11 | 4.2 | [↗](https://github.com/OpenVPN/openvpn) |
| OpenSEMBA/dgtd | 3 | 4.0 | [↗](https://github.com/OpenSEMBA/dgtd) |
| OpenIDC/mod_auth_openidc | 4 | 4.3 | [↗](https://github.com/OpenIDC/mod_auth_openidc) |
| openeuler-mirror/umdk | 26 | 4.1 | [↗](https://github.com/openeuler-mirror/umdk) |
| openbabel/openbabel | 22 | 4.6 | [↗](https://github.com/openbabel/openbabel) |
| olafhering/grub | 60 | 4.2 | [↗](https://github.com/olafhering/grub) |
| NVIDIA/nccl | 17 | 4.0 | [↗](https://github.com/NVIDIA/nccl) |
| NVIDIA/Fuser | 25 | 4.2 | [↗](https://github.com/NVIDIA/Fuser) |
| munich-quantum-toolkit/core | 14 | 4.2 | [↗](https://github.com/munich-quantum-toolkit/core) |
| mltframework/shotcut | 19 | 4.8 | [↗](https://github.com/mltframework/shotcut) |
| microsoft/DirectXShaderCompiler | 132 | 4.1 | [↗](https://github.com/microsoft/DirectXShaderCompiler) |
| mickem/nscp | 28 | 4.0 | [↗](https://github.com/mickem/nscp) |
| memgraph/memgraph | 45 | 4.1 | [↗](https://github.com/memgraph/memgraph) |
| magao-x/MagAOX | 24 | 4.7 | [↗](https://github.com/magao-x/MagAOX) |
| LibreDWG/libredwg | 10 | 4.5 | [↗](https://github.com/LibreDWG/libredwg) |
| liblouis/liblouis | 6 | 4.1 | [↗](https://github.com/liblouis/liblouis) |
| libarchive/libarchive | 2 | 4.4 | [↗](https://github.com/libarchive/libarchive) |
| KJ7LNW/xnec2c | 8 | 4.2 | [↗](https://github.com/KJ7LNW/xnec2c) |
| KallistiOS/KallistiOS | 34 | 4.0 | [↗](https://github.com/KallistiOS/KallistiOS) |
| kadas-albireo/kadas-albireo2 | 12 | 4.0 | [↗](https://github.com/kadas-albireo/kadas-albireo2) |
| jwmcglynn/donner | 32 | 4.6 | [↗](https://github.com/jwmcglynn/donner) |
| intel/intel-graphics-compiler | 97 | 4.8 | [↗](https://github.com/intel/intel-graphics-compiler) |
| Icinga/icinga2 | 28 | 4.6 | [↗](https://github.com/Icinga/icinga2) |
| hexagon-geo-surv/trusted-firmware-a | 166 | 4.2 | [↗](https://github.com/hexagon-geo-surv/trusted-firmware-a) |
| grumpycoders/pcsx-redux | 13 | 4.9 | [↗](https://github.com/grumpycoders/pcsx-redux) |
| gnutls/gnutls | 26 | 4.5 | [↗](https://github.com/gnutls/gnutls) |
| FreeRDP/FreeRDP | 6 | 4.7 | [↗](https://github.com/FreeRDP/FreeRDP) |
| FireSourcery/FireSourcery_Library | 14 | 4.5 | [↗](https://github.com/FireSourcery/FireSourcery_Library) |
| falcosecurity/libs | 12 | 4.6 | [↗](https://github.com/falcosecurity/libs) |
| facebookincubator/fizz | 12 | 4.3 | [↗](https://github.com/facebookincubator/fizz) |
| facebookexperimental/edencommon | 5 | 4.4 | [↗](https://github.com/facebookexperimental/edencommon) |
| Emute-Lab-Instruments/uSEQ | 5 | 4.8 | [↗](https://github.com/Emute-Lab-Instruments/uSEQ) |
| ElementsProject/lightning | 35 | 4.4 | [↗](https://github.com/ElementsProject/lightning) |
| droidian/phosh | 22 | 4.9 | [↗](https://github.com/droidian/phosh) |
| drawpile/Drawpile | 51 | 4.1 | [↗](https://github.com/drawpile/Drawpile) |
| doldecomp/melee | 93 | 4.6 | [↗](https://github.com/doldecomp/melee) |
| djmdjm/openssh-wip | 5 | 4.7 | [↗](https://github.com/djmdjm/openssh-wip) |
| djmdjm/openbsd-openssh-src | 5 | 4.7 | [↗](https://github.com/djmdjm/openbsd-openssh-src) |
| digi-embedded/optee_os | 95 | 4.0 | [↗](https://github.com/digi-embedded/optee_os) |
| DataDog/java-profiler | 5 | 4.8 | [↗](https://github.com/DataDog/java-profiler) |
| curl/curl | 28 | 4.5 | [↗](https://github.com/curl/curl) |
| CueMol/cuemol2 | 55 | 4.6 | [↗](https://github.com/CueMol/cuemol2) |
| compiler-research/CppInterOp | 1 | 4.3 | [↗](https://github.com/compiler-research/CppInterOp) |
| colmap/colmap | 5 | 4.9 | [↗](https://github.com/colmap/colmap) |
| ColinIanKing/stress-ng | 22 | 4.2 | [↗](https://github.com/ColinIanKing/stress-ng) |
| christoph2/pyA2L | 7 | 4.8 | [↗](https://github.com/christoph2/pyA2L) |
| cea-hpc/mpc | 46 | 4.9 | [↗](https://github.com/cea-hpc/mpc) |
| BlueSCSI/BlueSCSI-v2 | 5 | 4.0 | [↗](https://github.com/BlueSCSI/BlueSCSI-v2) |
| Blosc/c-blosc2 | 6 | 4.7 | [↗](https://github.com/Blosc/c-blosc2) |
| Bipto/Nexus | 19 | 4.0 | [↗](https://github.com/Bipto/Nexus) |
| berrym/lush | 16 | 4.1 | [↗](https://github.com/berrym/lush) |
| beerda/nuggets | 6 | 4.4 | [↗](https://github.com/beerda/nuggets) |
| bdring/FluidNC | 22 | 4.5 | [↗](https://github.com/bdring/FluidNC) |
| ARM-software/arm-trusted-firmware | 166 | 4.2 | [↗](https://github.com/ARM-software/arm-trusted-firmware) |
| apache/mynewt-core | 100 | 4.4 | [↗](https://github.com/apache/mynewt-core) |
| apache/axis-axis2-c-core | 28 | 4.2 | [↗](https://github.com/apache/axis-axis2-c-core) |
| allen-cell-animated/agave | 10 | 4.2 | [↗](https://github.com/allen-cell-animated/agave) |
| 203-Systems/MatrixOS | 23 | 4.1 | [↗](https://github.com/203-Systems/MatrixOS) |
| 125jdavis/gauge | 1 | 4.8 | [↗](https://github.com/125jdavis/gauge) |
| zlib-ng/zlib-ng | 5 | 3.0 | [↗](https://github.com/zlib-ng/zlib-ng) |
| Z3Prover/z3 | 67 | 3.5 | [↗](https://github.com/Z3Prover/z3) |
| yandex/odyssey | 12 | 3.6 | [↗](https://github.com/yandex/odyssey) |
| xfce-mirror/xfdesktop | 3 | 3.9 | [↗](https://github.com/xfce-mirror/xfdesktop) |
| xfce-mirror/xfce4-notifyd | 1 | 3.6 | [↗](https://github.com/xfce-mirror/xfce4-notifyd) |
| xen-project/xen | 65 | 3.2 | [↗](https://github.com/xen-project/xen) |
| xapian/xapian | 23 | 3.8 | [↗](https://github.com/xapian/xapian) |
| vifm/vifm | 13 | 3.0 | [↗](https://github.com/vifm/vifm) |
| uxlfoundation/oneTBB | 1 | 3.3 | [↗](https://github.com/uxlfoundation/oneTBB) |
| USNavalResearchLaboratory/simdissdk | 32 | 3.2 | [↗](https://github.com/USNavalResearchLaboratory/simdissdk) |
| umkoin/umkoin | 37 | 3.3 | [↗](https://github.com/umkoin/umkoin) |
| topling/toplingdb | 36 | 3.6 | [↗](https://github.com/topling/toplingdb) |
| tomato64/tomato64 | 12 | 3.5 | [↗](https://github.com/tomato64/tomato64) |
| Starlink/ast | 18 | 3.0 | [↗](https://github.com/Starlink/ast) |
| SSSD/sssd | 26 | 3.3 | [↗](https://github.com/SSSD/sssd) |
| shadps4-emu/shadPS4 | 24 | 3.1 | [↗](https://github.com/shadps4-emu/shadPS4) |
| shader-slang/slang | 43 | 3.0 | [↗](https://github.com/shader-slang/slang) |
| sailfishos-mirror/qtmultimedia | 31 | 3.3 | [↗](https://github.com/sailfishos-mirror/qtmultimedia) |
| sailfishos-mirror/openssh-portable | 7 | 3.4 | [↗](https://github.com/sailfishos-mirror/openssh-portable) |
| RTEMS/rtems | 54 | 3.9 | [↗](https://github.com/RTEMS/rtems) |
| randombit/botan | 41 | 3.2 | [↗](https://github.com/randombit/botan) |
| qt/qtwebengine | 20 | 3.2 | [↗](https://github.com/qt/qtwebengine) |
| qt/qttools | 37 | 3.2 | [↗](https://github.com/qt/qttools) |
| qt/qtquick3d | 22 | 3.5 | [↗](https://github.com/qt/qtquick3d) |
| qt/qtmultimedia | 31 | 3.3 | [↗](https://github.com/qt/qtmultimedia) |
| qt/qtgraphs | 9 | 3.0 | [↗](https://github.com/qt/qtgraphs) |
| project-gemmi/gemmi | 8 | 3.1 | [↗](https://github.com/project-gemmi/gemmi) |
| pi-hole/FTL | 12 | 3.8 | [↗](https://github.com/pi-hole/FTL) |
| PhoshMobi/stevia | 3 | 3.8 | [↗](https://github.com/PhoshMobi/stevia) |
| Phobos-developers/Phobos | 10 | 3.1 | [↗](https://github.com/Phobos-developers/Phobos) |
| pg83/std | 11 | 3.4 | [↗](https://github.com/pg83/std) |
| OP-TEE/optee_os | 82 | 3.7 | [↗](https://github.com/OP-TEE/optee_os) |
| open-telemetry/opentelemetry-dotnet-instrumentation | 16 | 3.3 | [↗](https://github.com/open-telemetry/opentelemetry-dotnet-instrumentation) |
| openssh/openssh-portable-selfhosted | 7 | 3.4 | [↗](https://github.com/openssh/openssh-portable-selfhosted) |
| openssh/openssh-portable | 7 | 3.4 | [↗](https://github.com/openssh/openssh-portable) |
| openharmony/drivers_peripheral | 2 | 3.3 | [↗](https://github.com/openharmony/drivers_peripheral) |
| OpendTect/OpendTect | 129 | 3.7 | [↗](https://github.com/OpendTect/OpendTect) |
| OpenDataPlane/odp-dpdk | 38 | 3.3 | [↗](https://github.com/OpenDataPlane/odp-dpdk) |
| OpenChemistry/tomviz | 13 | 3.1 | [↗](https://github.com/OpenChemistry/tomviz) |
| ObjectVision/GeoDMS | 28 | 3.3 | [↗](https://github.com/ObjectVision/GeoDMS) |
| nxp-imx/imx-optee-os | 83 | 3.9 | [↗](https://github.com/nxp-imx/imx-optee-os) |
| nomacs/nomacs | 4 | 3.1 | [↗](https://github.com/nomacs/nomacs) |
| Neverball/neverball | 6 | 3.2 | [↗](https://github.com/Neverball/neverball) |
| monitoring-plugins/monitoring-plugins | 16 | 3.7 | [↗](https://github.com/monitoring-plugins/monitoring-plugins) |
| mjagdis-imports/sdcc-svn | 187 | 3.3 | [↗](https://github.com/mjagdis-imports/sdcc-svn) |
| Mbed-TLS/TF-PSA-Crypto | 8 | 3.5 | [↗](https://github.com/Mbed-TLS/TF-PSA-Crypto) |
| Matthew-McRaven/Pepp | 26 | 3.5 | [↗](https://github.com/Matthew-McRaven/Pepp) |
| markaren/threepp | 24 | 3.3 | [↗](https://github.com/markaren/threepp) |
| lttng/lttng-tools | 25 | 3.4 | [↗](https://github.com/lttng/lttng-tools) |
| LoveDaisy/ice_halo_sim | 4 | 3.7 | [↗](https://github.com/LoveDaisy/ice_halo_sim) |
| LoRaMesher/LoRaMesher | 5 | 3.9 | [↗](https://github.com/LoRaMesher/LoRaMesher) |
| leuat/TRSE | 27 | 3.6 | [↗](https://github.com/leuat/TRSE) |
| leo-arch/clifm | 4 | 3.3 | [↗](https://github.com/leo-arch/clifm) |
| Kyoril/mmo | 52 | 3.7 | [↗](https://github.com/Kyoril/mmo) |
| kofemann/ms-nfs41-client | 5 | 3.9 | [↗](https://github.com/kofemann/ms-nfs41-client) |
| Kitware/CMake | 74 | 3.4 | [↗](https://github.com/Kitware/CMake) |
| KeyWorksRW/wxUiEditor | 28 | 3.6 | [↗](https://github.com/KeyWorksRW/wxUiEditor) |
| kdave/btrfs-progs | 9 | 3.5 | [↗](https://github.com/kdave/btrfs-progs) |
| JeffersonLab/JANA2 | 9 | 3.3 | [↗](https://github.com/JeffersonLab/JANA2) |
| imperialCHEPI/healthgps | 10 | 3.9 | [↗](https://github.com/imperialCHEPI/healthgps) |
| IDNI/tau-lang | 3 | 3.0 | [↗](https://github.com/IDNI/tau-lang) |
| icssw-org/MeshCom-Firmware | 68 | 3.9 | [↗](https://github.com/icssw-org/MeshCom-Firmware) |
| harfbuzz/harfbuzz | 5 | 3.7 | [↗](https://github.com/harfbuzz/harfbuzz) |
| gwdevhub/GWToolboxpp | 15 | 3.6 | [↗](https://github.com/gwdevhub/GWToolboxpp) |
| grafana/pyroscope-dotnet | 12 | 3.0 | [↗](https://github.com/grafana/pyroscope-dotnet) |
| GNOME/gjs | 5 | 3.3 | [↗](https://github.com/GNOME/gjs) |
| GerbilSoft/rom-properties | 76 | 3.9 | [↗](https://github.com/GerbilSoft/rom-properties) |
| gerbera/gerbera | 10 | 3.2 | [↗](https://github.com/gerbera/gerbera) |
| gehelem/OST | 8 | 3.5 | [↗](https://github.com/gehelem/OST) |
| fwupd/fwupd | 59 | 3.4 | [↗](https://github.com/fwupd/fwupd) |
| FreeRADIUS/freeradius-server | 36 | 3.8 | [↗](https://github.com/FreeRADIUS/freeradius-server) |
| freebsd/pkg | 2 | 3.9 | [↗](https://github.com/freebsd/pkg) |
| flatironinstitute/finufft | 3 | 3.4 | [↗](https://github.com/flatironinstitute/finufft) |
| facebook/watchman | 7 | 3.0 | [↗](https://github.com/facebook/watchman) |
| facebook/proxygen | 21 | 3.4 | [↗](https://github.com/facebook/proxygen) |
| facebook/openr | 5 | 3.6 | [↗](https://github.com/facebook/openr) |
| ESPWortuhr/Multilayout-ESP-Wordclock | 2 | 3.2 | [↗](https://github.com/ESPWortuhr/Multilayout-ESP-Wordclock) |
| elf-audio/mzgl | 15 | 3.4 | [↗](https://github.com/elf-audio/mzgl) |
| eic/EICrecon | 16 | 3.7 | [↗](https://github.com/eic/EICrecon) |
| dovecot/core | 84 | 3.8 | [↗](https://github.com/dovecot/core) |
| crdroidandroid/android_hardware_interfaces | 1 | 3.0 | [↗](https://github.com/crdroidandroid/android_hardware_interfaces) |
| corepunch/openwarcraft3 | 8 | 3.0 | [↗](https://github.com/corepunch/openwarcraft3) |
| contour-terminal/endo | 26 | 3.7 | [↗](https://github.com/contour-terminal/endo) |
| codepointerapp/codepointer | 5 | 3.6 | [↗](https://github.com/codepointerapp/codepointer) |
| ciaranm/glasgow-constraint-solver | 10 | 3.9 | [↗](https://github.com/ciaranm/glasgow-constraint-solver) |
| cfengine/core | 15 | 3.5 | [↗](https://github.com/cfengine/core) |
| CesiumGS/cesium-unreal | 11 | 3.7 | [↗](https://github.com/CesiumGS/cesium-unreal) |
| ccrs/EG-Source | 76 | 3.5 | [↗](https://github.com/ccrs/EG-Source) |
| casadi/casadi | 32 | 3.8 | [↗](https://github.com/casadi/casadi) |
| blender/cycles | 23 | 3.3 | [↗](https://github.com/blender/cycles) |
| bertiniteam/b2 | 8 | 3.9 | [↗](https://github.com/bertiniteam/b2) |
| batocera-linux/batocera-emulationstation | 16 | 3.9 | [↗](https://github.com/batocera-linux/batocera-emulationstation) |
| autotools-mirror/gettext | 16 | 3.2 | [↗](https://github.com/autotools-mirror/gettext) |
| apache/trafficserver | 63 | 3.7 | [↗](https://github.com/apache/trafficserver) |
| apache/nuttx-apps | 55 | 3.0 | [↗](https://github.com/apache/nuttx-apps) |
| aladur/flexemu | 10 | 3.5 | [↗](https://github.com/aladur/flexemu) |
| ZeraGmbH/zenux-services | 16 | 2.7 | [↗](https://github.com/ZeraGmbH/zenux-services) |
| XRPLF/clio | 10 | 2.0 | [↗](https://github.com/XRPLF/clio) |
| Xeeynamo/sotn-decomp | 60 | 2.9 | [↗](https://github.com/Xeeynamo/sotn-decomp) |
| wtarreau/nolibc | 1 | 2.4 | [↗](https://github.com/wtarreau/nolibc) |
| wpilibsuite/allwpilib | 58 | 2.7 | [↗](https://github.com/wpilibsuite/allwpilib) |
| wled/WLED | 5 | 2.7 | [↗](https://github.com/wled/WLED) |
| wingtk/gvsbuild | 6 | 2.7 | [↗](https://github.com/wingtk/gvsbuild) |
| w3c/ift-encoder | 4 | 2.7 | [↗](https://github.com/w3c/ift-encoder) |
| vvaltchev/tilck | 11 | 2.3 | [↗](https://github.com/vvaltchev/tilck) |
| virtuoso/clap | 4 | 2.6 | [↗](https://github.com/virtuoso/clap) |
| valibali/cluu | 1 | 2.5 | [↗](https://github.com/valibali/cluu) |
| universal-ctags/ctags | 25 | 2.8 | [↗](https://github.com/universal-ctags/ctags) |
| Unispace365/DsQt | 4 | 2.2 | [↗](https://github.com/Unispace365/DsQt) |
| unikraft/unikraft | 14 | 2.3 | [↗](https://github.com/unikraft/unikraft) |
| UCLA-VAST/tapa | 2 | 2.2 | [↗](https://github.com/UCLA-VAST/tapa) |
| ts-factory/test-environment | 40 | 2.8 | [↗](https://github.com/ts-factory/test-environment) |
| tkadauke/raytracer | 17 | 2.7 | [↗](https://github.com/tkadauke/raytracer) |
| tiacsys/bridle | 2 | 2.8 | [↗](https://github.com/tiacsys/bridle) |
| taedlar/neolith | 4 | 2.1 | [↗](https://github.com/taedlar/neolith) |
| subsurface/subsurface | 12 | 2.3 | [↗](https://github.com/subsurface/subsurface) |
| strongswan/strongswan | 57 | 2.6 | [↗](https://github.com/strongswan/strongswan) |
| stillwater-sc/universal | 50 | 2.2 | [↗](https://github.com/stillwater-sc/universal) |
| stellar/stellar-core | 14 | 2.4 | [↗](https://github.com/stellar/stellar-core) |
| sswroom/SClass | 136 | 2.5 | [↗](https://github.com/sswroom/SClass) |
| spicelang/spice | 4 | 2.5 | [↗](https://github.com/spicelang/spice) |
| spice2x/spice2x.github.io | 16 | 2.3 | [↗](https://github.com/spice2x/spice2x.github.io) |
| sedited/rust-bitcoinkernel | 24 | 2.2 | [↗](https://github.com/sedited/rust-bitcoinkernel) |
| SciQLop/SciQLopPlots | 6 | 2.8 | [↗](https://github.com/SciQLop/SciQLopPlots) |
| savoirfairelinux/opendht | 2 | 2.6 | [↗](https://github.com/savoirfairelinux/opendht) |
| sailfishos-mirror/ffmpeg | 90 | 2.1 | [↗](https://github.com/sailfishos-mirror/ffmpeg) |
| ROCm/hip-tests | 36 | 2.7 | [↗](https://github.com/ROCm/hip-tests) |
| ROCm/composable_kernel | 145 | 2.9 | [↗](https://github.com/ROCm/composable_kernel) |
| raspberrypi/libcamera | 13 | 2.1 | [↗](https://github.com/raspberrypi/libcamera) |
| qt/qtsvg | 2 | 2.8 | [↗](https://github.com/qt/qtsvg) |
| qt/qtgrpc | 4 | 2.1 | [↗](https://github.com/qt/qtgrpc) |
| qtproject/pyside-pyside-setup | 9 | 2.0 | [↗](https://github.com/qtproject/pyside-pyside-setup) |
| pyside/pyside-setup | 9 | 2.0 | [↗](https://github.com/pyside/pyside-setup) |
| PhoshMobi/phosh-mobile-settings | 3 | 2.4 | [↗](https://github.com/PhoshMobi/phosh-mobile-settings) |
| parthenon-hpc-lab/parthenon | 7 | 2.4 | [↗](https://github.com/parthenon-hpc-lab/parthenon) |
| OvenMediaLabs/OvenMediaEngine | 39 | 2.7 | [↗](https://github.com/OvenMediaLabs/OvenMediaEngine) |
| osamu620/OpenHTJ2K | 2 | 2.9 | [↗](https://github.com/osamu620/OpenHTJ2K) |
| openvswitch/ovs | 1 | 2.7 | [↗](https://github.com/openvswitch/ovs) |
| open-telemetry/opentelemetry-cpp | 21 | 2.4 | [↗](https://github.com/open-telemetry/opentelemetry-cpp) |
| open-mpi/ompi | 62 | 2.6 | [↗](https://github.com/open-mpi/ompi) |
| openharmony/third_party_musl | 140 | 2.7 | [↗](https://github.com/openharmony/third_party_musl) |
| opendcdiag/opendcdiag | 4 | 2.8 | [↗](https://github.com/opendcdiag/opendcdiag) |
| Open-CMSIS-Pack/devtools | 8 | 2.6 | [↗](https://github.com/Open-CMSIS-Pack/devtools) |
| oktetlabs/test-environment | 40 | 2.8 | [↗](https://github.com/oktetlabs/test-environment) |
| NVIDIA/VisRTX | 14 | 2.9 | [↗](https://github.com/NVIDIA/VisRTX) |
| neomutt/neomutt | 24 | 2.5 | [↗](https://github.com/neomutt/neomutt) |
| mysql/mysql-shell | 24 | 2.1 | [↗](https://github.com/mysql/mysql-shell) |
| mpc-qt/mpc-qt | 2 | 2.5 | [↗](https://github.com/mpc-qt/mpc-qt) |
| microsoft/WSL | 12 | 2.1 | [↗](https://github.com/microsoft/WSL) |
| microsoft/terminal | 28 | 2.5 | [↗](https://github.com/microsoft/terminal) |
| maurymarkowitz/OpenNEC | 1 | 2.0 | [↗](https://github.com/maurymarkowitz/OpenNEC) |
| marekjm/viuavm | 9 | 2.4 | [↗](https://github.com/marekjm/viuavm) |
| manticoresoftware/manticoresearch | 13 | 2.0 | [↗](https://github.com/manticoresoftware/manticoresearch) |
| maflcko/bitcoin-core-with-ci | 24 | 2.2 | [↗](https://github.com/maflcko/bitcoin-core-with-ci) |
| luspi/photoqt | 4 | 2.1 | [↗](https://github.com/luspi/photoqt) |
| lukas-w/pdfium | 51 | 2.0 | [↗](https://github.com/lukas-w/pdfium) |
| libsdl-org/SDL_mixer | 5 | 2.5 | [↗](https://github.com/libsdl-org/SDL_mixer) |
| librempeg/librempeg | 93 | 2.0 | [↗](https://github.com/librempeg/librempeg) |
| libcamera-org/libcamera | 13 | 2.2 | [↗](https://github.com/libcamera-org/libcamera) |
| LBNL-ETA/Windows-CalcEngine | 19 | 2.9 | [↗](https://github.com/LBNL-ETA/Windows-CalcEngine) |
| kevlu8/PZChessBot | 1 | 2.1 | [↗](https://github.com/kevlu8/PZChessBot) |
| kernkonzept/fiasco | 39 | 2.6 | [↗](https://github.com/kernkonzept/fiasco) |
| KDE/kdeconnect-kde | 6 | 2.2 | [↗](https://github.com/KDE/kdeconnect-kde) |
| kbingham/libcamera | 13 | 2.2 | [↗](https://github.com/kbingham/libcamera) |
| jbalcomb/ReMoM | 10 | 2.6 | [↗](https://github.com/jbalcomb/ReMoM) |
| jank-lang/jank | 9 | 2.2 | [↗](https://github.com/jank-lang/jank) |
| ja2-stracciatella/ja2-stracciatella | 19 | 2.2 | [↗](https://github.com/ja2-stracciatella/ja2-stracciatella) |
| ihhub/fheroes2 | 13 | 2.8 | [↗](https://github.com/ihhub/fheroes2) |
| hydrobricks/hydrobricks | 5 | 2.6 | [↗](https://github.com/hydrobricks/hydrobricks) |
| hiero-ledger/hiero-sdk-cpp | 13 | 2.1 | [↗](https://github.com/hiero-ledger/hiero-sdk-cpp) |
| heatd/Onyx | 42 | 2.3 | [↗](https://github.com/heatd/Onyx) |
| haxscramper/haxorg | 9 | 2.8 | [↗](https://github.com/haxscramper/haxorg) |
| hathach/tinyusb | 9 | 2.5 | [↗](https://github.com/hathach/tinyusb) |
| HallofFamer/Lox2 | 2 | 2.0 | [↗](https://github.com/HallofFamer/Lox2) |
| glotzerlab/freud | 4 | 2.8 | [↗](https://github.com/glotzerlab/freud) |
| getsentry/sentry-unreal | 8 | 2.6 | [↗](https://github.com/getsentry/sentry-unreal) |
| FreeTDS/freetds | 4 | 2.1 | [↗](https://github.com/FreeTDS/freetds) |
| fifengine/fifechan | 5 | 2.5 | [↗](https://github.com/fifengine/fifechan) |
| FEX-Emu/FEX | 14 | 2.7 | [↗](https://github.com/FEX-Emu/FEX) |
| FEniCS/dolfinx | 4 | 2.3 | [↗](https://github.com/FEniCS/dolfinx) |
| facebook/fboss | 83 | 2.7 | [↗](https://github.com/facebook/fboss) |
| EVerest/EVerest | 41 | 2.0 | [↗](https://github.com/EVerest/EVerest) |
| esphome/esphome | 53 | 2.0 | [↗](https://github.com/esphome/esphome) |
| esbmc/esbmc | 195 | 2.4 | [↗](https://github.com/esbmc/esbmc) |
| erofs/erofs-utils | 2 | 2.0 | [↗](https://github.com/erofs/erofs-utils) |
| eigen-mirror/eigen | 28 | 2.5 | [↗](https://github.com/eigen-mirror/eigen) |
| dragonflydb/dragonfly | 8 | 2.0 | [↗](https://github.com/dragonflydb/dragonfly) |
| dpaulat/supercell-wx | 14 | 2.3 | [↗](https://github.com/dpaulat/supercell-wx) |
| diffblue/hw-cbmc | 10 | 2.0 | [↗](https://github.com/diffblue/hw-cbmc) |
| davea42/libdwarf-code | 5 | 2.4 | [↗](https://github.com/davea42/libdwarf-code) |
| dalathegreat/Battery-Emulator | 8 | 2.3 | [↗](https://github.com/dalathegreat/Battery-Emulator) |
| cyrusimap/cyrus-imapd | 11 | 2.0 | [↗](https://github.com/cyrusimap/cyrus-imapd) |
| cvanaret/Uno | 6 | 2.6 | [↗](https://github.com/cvanaret/Uno) |
| contour-terminal/contour | 5 | 2.1 | [↗](https://github.com/contour-terminal/contour) |
| containers/crun | 3 | 2.8 | [↗](https://github.com/containers/crun) |
| christianrauch/libcamera | 13 | 2.2 | [↗](https://github.com/christianrauch/libcamera) |
| CCExtractor/ccextractor | 4 | 2.9 | [↗](https://github.com/CCExtractor/ccextractor) |
| category-labs/monad | 14 | 2.6 | [↗](https://github.com/category-labs/monad) |
| Cantera/cantera | 12 | 2.9 | [↗](https://github.com/Cantera/cantera) |
| BoleynSu-Org/monorepo | 23 | 2.0 | [↗](https://github.com/BoleynSu-Org/monorepo) |
| bitcoin-core/gui | 24 | 2.2 | [↗](https://github.com/bitcoin-core/gui) |
| axboe/fio | 9 | 2.5 | [↗](https://github.com/axboe/fio) |
| aws/aws-ofi-nccl | 2 | 2.0 | [↗](https://github.com/aws/aws-ofi-nccl) |
| arthenica/gnulib | 71 | 2.8 | [↗](https://github.com/arthenica/gnulib) |
| artemsen/swayimg | 2 | 2.7 | [↗](https://github.com/artemsen/swayimg) |
| allyourcodebase/ffmpeg | 88 | 2.1 | [↗](https://github.com/allyourcodebase/ffmpeg) |
| AlexanderTaeschner/gnuplot | 6 | 2.6 | [↗](https://github.com/AlexanderTaeschner/gnuplot) |
| acts-project/traccc | 14 | 2.3 | [↗](https://github.com/acts-project/traccc) |
| zxlxz/sfc | 2 | 1.4 | [↗](https://github.com/zxlxz/sfc) |
| ZeraGmbH/zera-classes | 15 | 1.8 | [↗](https://github.com/ZeraGmbH/zera-classes) |
| zeek/spicy | 7 | 1.2 | [↗](https://github.com/zeek/spicy) |
| zackb/code | 3 | 1.4 | [↗](https://github.com/zackb/code) |
| XRPLF/rippled | 16 | 1.4 | [↗](https://github.com/XRPLF/rippled) |
| xord/all | 7 | 1.7 | [↗](https://github.com/xord/all) |
| xiphonics/picoTracker | 4 | 1.1 | [↗](https://github.com/xiphonics/picoTracker) |
| ViewTouch/viewtouch | 3 | 1.5 | [↗](https://github.com/ViewTouch/viewtouch) |
| userver-framework/userver | 46 | 1.3 | [↗](https://github.com/userver-framework/userver) |
| UBCFormulaElectric/Consolidated-Firmware | 22 | 1.6 | [↗](https://github.com/UBCFormulaElectric/Consolidated-Firmware) |
| TF-RMM/tf-rmm | 5 | 1.4 | [↗](https://github.com/TF-RMM/tf-rmm) |
| TF-Hafnium/hafnium | 3 | 1.2 | [↗](https://github.com/TF-Hafnium/hafnium) |
| teriflix/scrite | 5 | 1.8 | [↗](https://github.com/teriflix/scrite) |
| tadryanom/AdrOS | 5 | 1.1 | [↗](https://github.com/tadryanom/AdrOS) |
| syslog-ng/syslog-ng | 14 | 1.1 | [↗](https://github.com/syslog-ng/syslog-ng) |
| SWI-Prolog/swipl-devel | 1 | 1.4 | [↗](https://github.com/SWI-Prolog/swipl-devel) |
| simutrans/simutrans | 11 | 1.3 | [↗](https://github.com/simutrans/simutrans) |
| scylladb/seastar | 6 | 1.5 | [↗](https://github.com/scylladb/seastar) |
| saturneric/GpgFrontend | 7 | 1.6 | [↗](https://github.com/saturneric/GpgFrontend) |
| sailfishos-mirror/ltp | 47 | 1.0 | [↗](https://github.com/sailfishos-mirror/ltp) |
| Rtoax/test-linux | 44 | 1.3 | [↗](https://github.com/Rtoax/test-linux) |
| RTEMS/sourceware-mirror-newlib-cygwin | 53 | 1.4 | [↗](https://github.com/RTEMS/sourceware-mirror-newlib-cygwin) |
| rpm-software-management/rpm | 3 | 1.3 | [↗](https://github.com/rpm-software-management/rpm) |
| rpm-software-management/dnf5 | 12 | 1.4 | [↗](https://github.com/rpm-software-management/dnf5) |
| rockowitz/ddcutil | 6 | 1.6 | [↗](https://github.com/rockowitz/ddcutil) |
| rmyorston/busybox-w32 | 11 | 1.3 | [↗](https://github.com/rmyorston/busybox-w32) |
| revng/revng | 10 | 1.0 | [↗](https://github.com/revng/revng) |
| redpanda-data/redpanda | 28 | 1.0 | [↗](https://github.com/redpanda-data/redpanda) |
| RediSearch/RediSearch | 5 | 1.1 | [↗](https://github.com/RediSearch/RediSearch) |
| rauc/rauc | 1 | 1.3 | [↗](https://github.com/rauc/rauc) |
| rapidsai/rmm | 2 | 1.5 | [↗](https://github.com/rapidsai/rmm) |
| qt/qtdoc | 4 | 1.2 | [↗](https://github.com/qt/qtdoc) |
| qt/qtapplicationmanager | 5 | 1.6 | [↗](https://github.com/qt/qtapplicationmanager) |
| qemu/ipxe | 27 | 1.5 | [↗](https://github.com/qemu/ipxe) |
| pybricks/pybricks-micropython | 11 | 1.9 | [↗](https://github.com/pybricks/pybricks-micropython) |
| picolibc/picolibc | 8 | 1.4 | [↗](https://github.com/picolibc/picolibc) |
| PeterHallonmark/clang-tidy-automotive | 4 | 1.7 | [↗](https://github.com/PeterHallonmark/clang-tidy-automotive) |
| openucx/ucx | 9 | 1.4 | [↗](https://github.com/openucx/ucx) |
| OpenSourceRisk/Engine | 38 | 1.5 | [↗](https://github.com/OpenSourceRisk/Engine) |
| openMSX/openMSX | 13 | 1.1 | [↗](https://github.com/openMSX/openMSX) |
| OpenKneeboard/OpenKneeboard | 5 | 1.0 | [↗](https://github.com/OpenKneeboard/OpenKneeboard) |
| OpenEnroth/OpenEnroth | 9 | 1.1 | [↗](https://github.com/OpenEnroth/OpenEnroth) |
| OpenDataPlane/odp | 16 | 1.6 | [↗](https://github.com/OpenDataPlane/odp) |
| openbmc/openpower-vpd-parser | 1 | 1.6 | [↗](https://github.com/openbmc/openpower-vpd-parser) |
| openbmc/bmcweb | 7 | 1.9 | [↗](https://github.com/openbmc/bmcweb) |
| OmarAglan/Baa | 2 | 1.3 | [↗](https://github.com/OmarAglan/Baa) |
| nv-legate/legate | 11 | 1.5 | [↗](https://github.com/nv-legate/legate) |
| nunuhara/xsystem4 | 3 | 1.3 | [↗](https://github.com/nunuhara/xsystem4) |
| munich-quantum-toolkit/syrec | 1 | 1.4 | [↗](https://github.com/munich-quantum-toolkit/syrec) |
| MUME/MMapper | 10 | 1.6 | [↗](https://github.com/MUME/MMapper) |
| mpv-player/mpv | 6 | 1.6 | [↗](https://github.com/mpv-player/mpv) |
| mod-playerbots/mod-playerbots | 19 | 1.4 | [↗](https://github.com/mod-playerbots/mod-playerbots) |
| ModbusScope/ModbusScope | 2 | 1.0 | [↗](https://github.com/ModbusScope/ModbusScope) |
| MIPS/newlib | 53 | 1.4 | [↗](https://github.com/MIPS/newlib) |
| mingw-w64/mingw-w64 | 28 | 1.0 | [↗](https://github.com/mingw-w64/mingw-w64) |
| meshtastic/firmware | 16 | 1.4 | [↗](https://github.com/meshtastic/firmware) |
| MeshInspector/MeshLib | 25 | 1.6 | [↗](https://github.com/MeshInspector/MeshLib) |
| MerginMaps/mobile | 4 | 1.6 | [↗](https://github.com/MerginMaps/mobile) |
| meganz/sdk | 12 | 1.4 | [↗](https://github.com/meganz/sdk) |
| medicalopenworld/IncuNest | 4 | 1.4 | [↗](https://github.com/medicalopenworld/IncuNest) |
| mcu-tools/mcuboot | 3 | 1.5 | [↗](https://github.com/mcu-tools/mcuboot) |
| managarm/mlibc | 26 | 1.8 | [↗](https://github.com/managarm/mlibc) |
| MaaXYZ/MaaFramework | 7 | 1.8 | [↗](https://github.com/MaaXYZ/MaaFramework) |
| llnl/Silo | 3 | 1.8 | [↗](https://github.com/llnl/Silo) |
| linux-test-project/ltp | 47 | 1.0 | [↗](https://github.com/linux-test-project/ltp) |
| libjxl/libjxl | 8 | 1.4 | [↗](https://github.com/libjxl/libjxl) |
| lf-lang/reactor-c | 2 | 1.3 | [↗](https://github.com/lf-lang/reactor-c) |
| LedgerHQ/app-ethereum | 3 | 1.3 | [↗](https://github.com/LedgerHQ/app-ethereum) |
| kromenak/gengine | 15 | 1.6 | [↗](https://github.com/kromenak/gengine) |
| kernelslacker/trinity | 13 | 1.5 | [↗](https://github.com/kernelslacker/trinity) |
| katsuster/newlib-cygwin | 53 | 1.4 | [↗](https://github.com/katsuster/newlib-cygwin) |
| joypad-ai/joypad-os | 8 | 1.6 | [↗](https://github.com/joypad-ai/joypad-os) |
| jdolan/quetoo | 7 | 1.3 | [↗](https://github.com/jdolan/quetoo) |
| ipxe/ipxe | 27 | 1.5 | [↗](https://github.com/ipxe/ipxe) |
| intel/compile-time-init-build | 2 | 1.4 | [↗](https://github.com/intel/compile-time-init-build) |
| hexagon-geo-surv/barebox | 64 | 1.2 | [↗](https://github.com/hexagon-geo-surv/barebox) |
| google/XNNPACK | 102 | 1.1 | [↗](https://github.com/google/XNNPACK) |
| google/quiche | 25 | 1.9 | [↗](https://github.com/google/quiche) |
| git-for-windows/msys2-runtime | 52 | 1.4 | [↗](https://github.com/git-for-windows/msys2-runtime) |
| GenSpectrum/LAPIS-SILO | 5 | 1.1 | [↗](https://github.com/GenSpectrum/LAPIS-SILO) |
| foss-for-synopsys-dwc-arc-processors/newlib | 53 | 1.4 | [↗](https://github.com/foss-for-synopsys-dwc-arc-processors/newlib) |
| facebookincubator/katran | 3 | 1.8 | [↗](https://github.com/facebookincubator/katran) |
| facebook/bpfilter | 2 | 1.9 | [↗](https://github.com/facebook/bpfilter) |
| Expensify/Bedrock | 1 | 1.0 | [↗](https://github.com/Expensify/Bedrock) |
| exodusdb/exodusdb | 5 | 1.6 | [↗](https://github.com/exodusdb/exodusdb) |
| espressif/esp-matter | 3 | 1.3 | [↗](https://github.com/espressif/esp-matter) |
| ENZYME-APD/tapir-archicad-automation | 1 | 1.6 | [↗](https://github.com/ENZYME-APD/tapir-archicad-automation) |
| elogind/elogind | 10 | 1.5 | [↗](https://github.com/elogind/elogind) |
| eigendude/OASIS | 6 | 1.9 | [↗](https://github.com/eigendude/OASIS) |
| dxx-rebirth/dxx-rebirth | 7 | 1.7 | [↗](https://github.com/dxx-rebirth/dxx-rebirth) |
| DrylandEcology/SOILWAT2 | 1 | 1.6 | [↗](https://github.com/DrylandEcology/SOILWAT2) |
| DroneDB/DroneDB | 4 | 1.8 | [↗](https://github.com/DroneDB/DroneDB) |
| doyaGu/BallanceModLoaderPlus | 2 | 1.4 | [↗](https://github.com/doyaGu/BallanceModLoaderPlus) |
| dmlc/xgboost | 6 | 1.9 | [↗](https://github.com/dmlc/xgboost) |
| damacaa/weird-engine | 2 | 1.9 | [↗](https://github.com/damacaa/weird-engine) |
| cygwin/cygwin | 53 | 1.4 | [↗](https://github.com/cygwin/cygwin) |
| cucumber/gherkin | 2 | 1.2 | [↗](https://github.com/cucumber/gherkin) |
| couchbase/kv_engine | 19 | 1.8 | [↗](https://github.com/couchbase/kv_engine) |
| coreboot/depthcharge | 22 | 1.9 | [↗](https://github.com/coreboot/depthcharge) |
| Cockatrice/Cockatrice | 18 | 1.9 | [↗](https://github.com/Cockatrice/Cockatrice) |
| ClusterLabs/pacemaker | 9 | 1.9 | [↗](https://github.com/ClusterLabs/pacemaker) |
| cloudflare/workerd | 1 | 1.8 | [↗](https://github.com/cloudflare/workerd) |
| clEsperanto/CLIc | 3 | 1.6 | [↗](https://github.com/clEsperanto/CLIc) |
| cilium/tetragon | 2 | 1.9 | [↗](https://github.com/cilium/tetragon) |
| checkpoint-restore/criu | 10 | 1.6 | [↗](https://github.com/checkpoint-restore/criu) |
| Chatterino/chatterino2 | 11 | 1.2 | [↗](https://github.com/Chatterino/chatterino2) |
| CerebusOSS/CereLink | 1 | 1.5 | [↗](https://github.com/CerebusOSS/CereLink) |
| celeritas-project/celeritas | 18 | 1.0 | [↗](https://github.com/celeritas-project/celeritas) |
| carbon-language/carbon-lang | 9 | 1.5 | [↗](https://github.com/carbon-language/carbon-lang) |
| bylins/mud | 11 | 1.1 | [↗](https://github.com/bylins/mud) |
| brltty/brltty | 14 | 1.9 | [↗](https://github.com/brltty/brltty) |
| BlockstreamResearch/secp256k1-zkp | 2 | 1.3 | [↗](https://github.com/BlockstreamResearch/secp256k1-zkp) |
| BelledonneCommunications/flexisip | 9 | 1.1 | [↗](https://github.com/BelledonneCommunications/flexisip) |
| barebox/barebox | 64 | 1.2 | [↗](https://github.com/barebox/barebox) |
| axoflow/axosyslog | 24 | 1.5 | [↗](https://github.com/axoflow/axosyslog) |
| art-daq/otsdaq | 4 | 1.3 | [↗](https://github.com/art-daq/otsdaq) |
| AntaresSimulatorTeam/antares-xpansion | 6 | 1.9 | [↗](https://github.com/AntaresSimulatorTeam/antares-xpansion) |
| andoma/mios | 11 | 1.5 | [↗](https://github.com/andoma/mios) |
| alifsemi/alif_ml-embedded-evaluation-kit | 2 | 1.8 | [↗](https://github.com/alifsemi/alif_ml-embedded-evaluation-kit) |
| ahjragaas/busybox | 10 | 1.3 | [↗](https://github.com/ahjragaas/busybox) |
| acts-project/acts | 22 | 1.0 | [↗](https://github.com/acts-project/acts) |
| actor-framework/actor-framework | 16 | 1.3 | [↗](https://github.com/actor-framework/actor-framework) |
| aburch/simutrans | 11 | 1.3 | [↗](https://github.com/aburch/simutrans) |
| aardappel/lobster | 2 | 1.4 | [↗](https://github.com/aardappel/lobster) |
| XCSoar/XCSoar | 26 | 0.7 | [↗](https://github.com/XCSoar/XCSoar) |
| TimelordUK/node-sqlserver-v8 | 1 | 0.8 | [↗](https://github.com/TimelordUK/node-sqlserver-v8) |
| thoni56/c-xrefactory | 1 | 0.4 | [↗](https://github.com/thoni56/c-xrefactory) |
| ThomasGhione/chess_engine | 2 | 0.5 | [↗](https://github.com/ThomasGhione/chess_engine) |
| tagattie/FreeBSD-Electron | 199 | 0.5 | [↗](https://github.com/tagattie/FreeBSD-Electron) |
| sxs-collaboration/spectre | 14 | 0.4 | [↗](https://github.com/sxs-collaboration/spectre) |
| swoole/phpx | 1 | 0.4 | [↗](https://github.com/swoole/phpx) |
| strace/strace | 10 | 0.9 | [↗](https://github.com/strace/strace) |
| sailfishos-mirror/firejail | 2 | 0.9 | [↗](https://github.com/sailfishos-mirror/firejail) |
| Royna2544/c_cpp_telegrambot | 1 | 0.5 | [↗](https://github.com/Royna2544/c_cpp_telegrambot) |
| privatevoid-net/nix-super | 6 | 0.8 | [↗](https://github.com/privatevoid-net/nix-super) |
| pocl/pocl | 1 | 0.9 | [↗](https://github.com/pocl/pocl) |
| OutpostUniverse/OPHD | 1 | 0.3 | [↗](https://github.com/OutpostUniverse/OPHD) |
| OpenAtom-Linyaps/linyaps | 2 | 0.5 | [↗](https://github.com/OpenAtom-Linyaps/linyaps) |
| open5gs/open5gs | 52 | 0.7 | [↗](https://github.com/open5gs/open5gs) |
| nomadsinteractive/ark | 2 | 0.2 | [↗](https://github.com/nomadsinteractive/ark) |
| NixOS/nix | 5 | 0.7 | [↗](https://github.com/NixOS/nix) |
| netblue30/firejail | 2 | 0.9 | [↗](https://github.com/netblue30/firejail) |
| mozilla-mobile/mozilla-vpn-client | 4 | 0.6 | [↗](https://github.com/mozilla-mobile/mozilla-vpn-client) |
| microsoft/Teams-AdaptiveCards-Mobile | 1 | 0.2 | [↗](https://github.com/microsoft/Teams-AdaptiveCards-Mobile) |
| micropython/micropython | 2 | 0.2 | [↗](https://github.com/micropython/micropython) |
| mawww/kakoune | 1 | 0.7 | [↗](https://github.com/mawww/kakoune) |
| masc-ucsc/livehd | 2 | 0.8 | [↗](https://github.com/masc-ucsc/livehd) |
| lone-lang/lone | 1 | 0.9 | [↗](https://github.com/lone-lang/lone) |
| loda-lang/loda-cpp | 1 | 0.6 | [↗](https://github.com/loda-lang/loda-cpp) |
| linux-rdma/rdma-core | 1 | 0.8 | [↗](https://github.com/linux-rdma/rdma-core) |
| labwc/labwc | 1 | 0.5 | [↗](https://github.com/labwc/labwc) |
| kernkonzept/l4re-core | 6 | 0.7 | [↗](https://github.com/kernkonzept/l4re-core) |
| jjuran/metamage_1 | 23 | 0.6 | [↗](https://github.com/jjuran/metamage_1) |
| jameshball/osci-render | 1 | 0.4 | [↗](https://github.com/jameshball/osci-render) |
| inspektor-gadget/inspektor-gadget | 1 | 0.9 | [↗](https://github.com/inspektor-gadget/inspektor-gadget) |
| ibis-ssl/crane | 2 | 0.8 | [↗](https://github.com/ibis-ssl/crane) |
| hluk/CopyQ | 1 | 0.3 | [↗](https://github.com/hluk/CopyQ) |
| HIllya51/LunaTranslator | 5 | 0.7 | [↗](https://github.com/HIllya51/LunaTranslator) |
| guillemj/dpkg | 2 | 0.7 | [↗](https://github.com/guillemj/dpkg) |
| google/crubit | 1 | 0.4 | [↗](https://github.com/google/crubit) |
| GNOME/gparted | 1 | 0.7 | [↗](https://github.com/GNOME/gparted) |
| flux-framework/flux-core | 6 | 0.6 | [↗](https://github.com/flux-framework/flux-core) |
| error27/smatch | 10 | 0.7 | [↗](https://github.com/error27/smatch) |
| duanery/perf-prof | 3 | 0.6 | [↗](https://github.com/duanery/perf-prof) |
| deskull-m/bakabakaband | 17 | 0.8 | [↗](https://github.com/deskull-m/bakabakaband) |
| craigbarnes/dte | 1 | 0.4 | [↗](https://github.com/craigbarnes/dte) |
| cppalliance/mrdocs | 1 | 0.4 | [↗](https://github.com/cppalliance/mrdocs) |
| coreutils/coreutils | 2 | 0.9 | [↗](https://github.com/coreutils/coreutils) |
| coal-library/coal | 1 | 0.5 | [↗](https://github.com/coal-library/coal) |
| bpftrace/bpftrace | 1 | 0.3 | [↗](https://github.com/bpftrace/bpftrace) |

<!-- FULL_TABLE_END -->
