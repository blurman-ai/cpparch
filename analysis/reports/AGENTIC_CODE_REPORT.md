# Agentic C++ — структурный долг с примерами кода

> Ночной автономный прогон. Цель: найти C++ репы с агентной разработкой, прогнать archcheck,
> показать **реальный код** найденного долга — не только числа. Все фрагменты ниже извлечены
> verbatim из живых репозиториев под `/home/localadm/oss/`.

---

## TL;DR

1. **Критерий «>300 коммитов с мая 2025» выполнен** — 1543 новых чистых C++ winner'а
   ([new_winners_clean.tsv](ai_repo_run/new_winners_clean.tsv)), 1497 в целевой полосе 300–5000.
2. **Детектор скачка скорости построен** (без клонов, `gh api stats/commit_activity`) — 264 репы
   с cold-start ramp ([agent_switch_ranked.tsv](ai_repo_run/agent_switch_ranked.tsv)).
3. **Честная находка: скачок скорости ≠ агенты.** Топ-ramp репы (`citron-neo/emulator` 41×,
   `KweonTJ/...` робототехника) по трейлерам имеют **AI%=0**. Взрыв коммитов вызывает что угодно
   (новый мейнтейнер, ожившая форк-ветка), агенты лишь одна из причин. Надёжный сигнал агентности —
   **авторство бота** (`copilot-swe-agent[bot]`), а не темп.
4. **Подтверждённо-агентные специмены** (по авторству) прогнаны archcheck — код ниже.

---

## 1. Подтверждённо агентные репы (по авторству коммитов)

| репо | post-may коммитов | доля `copilot-swe-agent[bot]` | топ god-header (fan-in) | циклы |
|---|---|---|---|---|
| **shifty81/FreshVoxel** | 1762 | **1375 (78%)** | `engine/core/Logger.h` (80) | — |
| **Zero3K20/hpsx64** | 54 | **36 (67%)** | `common/types.h` (52) | `hps1x64 ↔ hps2x64`, 5 mutual |
| **HendrikGC02/Astroray** | 617 | 41 (+ Hendrik 530) | `include/raytracer.h` (66) | 5-SCC, Vec3-цикл |

«Агентность» здесь = доля коммитов, **авторских от агента**, а не темп. FreshVoxel и hpsx64 —
буквально написаны copilot-swe-agent. Astroray — человеко-ведомый с агент-ассистом (показателен
как контраст: тот же класс долга и без доминирования агента).

---

## 2. Код — что archcheck достаёт

### 2.1 FreshVoxel (78% агент) — god-header, который инклюдят 80 файлов

`engine/core/Logger.h` — fan-in **80**, 156 строк. Текстбук «все инклюдят логгер»: каждый файл
движка тянет весь логгер (с `<fstream>`, `<chrono>`, `<mutex>` транзитивно):

```cpp
// engine/core/Logger.h  (включён 80 файлами)
#pragma once
#include <chrono>
#include <fstream>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>
namespace fresh {
enum class LogLevel {
    INFO, WARNING,
    ERR,  // Renamed from ERROR to avoid Windows macro conflict
    FATAL
};
class ILogListener { /* ... */ };
```

**Скопированный bootstrap движка** — `editor/main.cpp:13-52` ↔ `server/main.cpp:9-42`
(STRUCTURAL 0.87). Две точки входа повторяют инициализацию; отличие — режим `Editor`/`Server`
и строки лога:

```cpp
// editor/main.cpp                          // server/main.cpp
(void)argc; (void)argv;                     (void)argc; (void)argv;
#ifdef _WIN32                               //  <-- DPI-блок есть только в editor
  SetProcessDpiAwarenessContext(...);
#endif
fresh::Logger::getInstance().initialize();  fresh::Logger::getInstance().initialize();
std::cout << "FreshVoxel Editor ...";       std::cout << "FreshVoxel Server ...";
fresh::Engine engine;                       fresh::Engine engine;
auto config = EngineConfig::createDefault(  auto config = EngineConfig::createDefault(
    EngineMode::Editor);                        EngineMode::Server);
```

**Дословный clone Windows-проверки версии** — `WindowsJumpListManager.cpp:322-331` ↔
`WindowsToastManager.cpp:165-174` (LITERAL). Отличие — только номер версии (6.1 vs 10.0):

```cpp
OSVERSIONINFOEXW osvi = {};
osvi.dwOSVersionInfoSize = sizeof(osvi);
osvi.dwMajorVersion = 6;   osvi.dwMinorVersion = 1;   // JumpList: Windows 7
osvi.dwMajorVersion = 10;  osvi.dwMinorVersion = 0;   // Toast:    Windows 10
DWORDLONG dwlConditionMask = 0;
VER_SET_CONDITION(dwlConditionMask, VER_MAJORVERSION, VER_GREATER_EQUAL);
VER_SET_CONDITION(dwlConditionMask, VER_MINORVERSION, VER_GREATER_EQUAL);
return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION, dwlConditionMask) != 0;
```

archcheck dup-скан: `scanned 415 files … reported 9 pairs`.

### 2.2 hpsx64 (67% агент) — взаимный include-цикл между модулями

Эмулятор PS1/PS2. Два устройства включают заголовки друг друга напрямую — реальный 2-цикл
внутри 3-компонентного SCC (`[SF.9]`):

```cpp
// hps1x64/src/dma/src/PS1_Dma.h:32          // hps1x64/src/mdec/src/PS1_MDEC.h:27
#include "PS1_MDEC.h"            <----------->  #include "PS1_Dma.h"

// и databus замыкает SCC из трёх:
// hps1x64/src/databus/src/PS1DataBus.h:26  #include "PS1_Dma.h"
//                                      :30  #include "PS1_MDEC.h"
```

archcheck: `largest_scc=3`, `mutual_pairs=5`, модуль-цикл `hps1x64 ↔ hps2x64` (11 рёбер назад).

God-header `common/types.h` — fan-in **52**, 501 строка, **89 определений** (typedef/using/
define/struct/class в одном файле):

```cpp
// common/types.h  (включён 52 файлами)
#ifndef __TYPES_H__
#define __TYPES_H__
#include <stdint.h>
#if defined(_WIN32) || defined(_WIN64)
  #include <intrin.h>
#else
  #include <x86intrin.h>
  #include <cpuid.h>
#endif
// ... 89 определений типов на 501 строку, тянутся в половину проекта
```

### 2.3 Astroray — god-header на 3008 строк + цикл вокруг общего `Vec3`

`include/raytracer.h` — fan-in **66**, **3008 строк** в одном заголовке. И классический
type-sharing цикл: всем нужен `Vec3` из `raytracer.h`, а `raytracer.h` тянет спектр обратно:

```cpp
// include/astroray/emission_spectrum.h:32
#include "../raytracer.h"   // for Vec3
// include/raytracer.h:472
#include "astroray/emission_spectrum.h"   // <-- замыкает 5-компонентный SCC
```

---

## 3. Скачок скорости — ранкинг и честная оговорка

Детектор ([changepoint_scan.py](ai_repo_run/changepoint_scan.py)) нашёл точку перелома темпа
коммитов по 1215 репам. Валидация-контраст: `ThemisDB` ramp **1199×** (включили агента
2025-10-26) vs `mongodb/mongo` **1.3×** (стабильный мейнстрик — отвергнут). Топ cold-start:

| класс | репо | post-may | перелом | ramp |
|---|---|---|---|---|
| switched/revived | JulioJerez/newton-dynamics | 3608 | 2025-09-21 | **118×** |
| switched/revived | citron-neo/emulator | 1676 | 2026-03-15 | 41× |
| born-greenfield | KweonTJ/3D_perception… | 943 | 2026-04-12 | 268× |
| born-greenfield | defessler/Unreal-Engine-5-MCP | 403 | 2026-04-12 | 129× |
| born-greenfield | 1ay1/agentty | 417 | 2026-04-12 | 128× |

**Оговорка (важная):** ramp ловит *любой* всплеск активности, не только агентов. Первые
проверенные топ-ramp репы дали **AI%=0 по трейлерам** — то есть скачок и агентность
**расходятся**. Поэтому таргет-лист надо фильтровать по **авторству бота**, а не по темпу.
Это согласуется с более ранним выводом: причинно «AI → долг» не доказывается; демо-тезис —
*«вот быстро растущий C++ с агентом в авторах, и вот долг, который тулза достаёт»*.

---

---

## 4. Массовый скан 470 AI-гарантированных C++ реп

Нашли **470 новых C++ реп с гарантированными ИИ-коммитами** (трейлер/бот-автор доказан),
>300 коммитов, [ai_guaranteed_FINAL.tsv](ai_repo_run/ai_guaranteed_FINAL.tsv). Прогнали archcheck
по всем эфемерно ([ephemeral_scan.py](ai_repo_run/ephemeral_scan.py), дисциплина диска:
`--shallow-since=2025-05-01` → стрип до C++/CMake → KEEP стриппнутое для re-scan, удаление только
>150MB). Результат: [agentic_scan_470.tsv](ai_repo_run/agentic_scan_470.tsv).

**Два фильтра, без которых ранкинг врёт:**

1. **Гиганты с ревью — выкинуть.** Сырой топ долга = мейнстрим по размеру: `mame` `emu.h`
   fan-in **8530**, `cmssw`, `OCCT`, `LibreOffice`, `mongo`, `lammps`. У них много контрибуторов
   и code-review → долг ловится до HEAD, AI лишь 2-4%. Это **не наши клиенты** и слив времени на
   клонирование — фильтруем по низкому AI% + размеру/контрибуторам.
2. **Вендоренный код — выкинуть.** У высоко-AI реп долг часто сидит в третьесторонних SDK, не в
   авторском коде: `objeck-lang` цикл в `core/lib/onnx/QNN/`, god-header = SDL; `T5ynth` цикл в
   `JUCE/.../libvorbis-1.3.7/`, god-header = AAX SDK. Агентные репы — тонкий авторский слой поверх
   больших библиотек. Нужен фильтр вендор-путей ([authored_debt.py](ai_repo_run/authored_debt.py))
   — это пересекается с #081 Bug B (vendored-детекция).

После обоих фильтров — **настоящий авторский агентный долг**:

### 4.1 makodb/mako (AI 48%, БД) — god-header «свалка зависимостей»

`src/deptran/__dep__.h` — fan-in **123**, имя буквально «dependencies». 46 `#include` в одном
заголовке, который тянут 123 файла проекта:

```cpp
// src/deptran/__dep__.h  (включён 123 файлами)
#pragma once
//C++ standard library
#include <map>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <atomic>
#include <fstream>
// ... 46 #include в одном заголовке — каждый из 123 включающих платит за все
```

archcheck: авторские циклы `__dep__.h ↔ marshal-value.h` и `rcc/tx.h ↔ scheduler.h`.

### 4.2 larryseyer/OTTO (AI 41%, аудио-плагин) — god-header цветовой схемы + цикл

`src/otto-plugin/ui/OTTOColours.h` — fan-in **94**, 135 цветовых определений; AI-почерк в
шапке («v2.0, following UI_SYSTEM_SPECIFICATIONS.md, WCAG AA compliance»). Каждый UI-файл тянет
всю цветовую систему + `juce_gui_basics` транзитивно:

```cpp
// src/otto-plugin/ui/OTTOColours.h  (включён 94 файлами)
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
namespace otto { namespace Colours {
inline const juce::Colour bg0{0xff010000};  // App edges
inline const juce::Colour bg1{0xff010000};  // Main app background
// ... 135 определений цветов — весь UI зависит от одного заголовка
```

Авторский цикл `[SF.9]` — данные паттерна и библиотека паттернов включают друг друга:

```cpp
// src/otto-core/include/otto/library/PatternData.h:11
#include "PatternLibrary.h"
// src/otto-core/include/otto/library/PatternLibrary.h:763
#include "PatternData.h"          // <-- замыкание цикла
```

### 4.3 Прочие агентные с долгом (AI≥30%, не гигант)

| репо | AI% | god-header (fan-in) | циклы | dup |
|---|---|---|---|---|
| nickybmon/OpenEmu-Silicon | 35% | `Log.h` (328) | 17 SCC, 23 mut | 370 |
| iu8lmc/Decodium-4.0 | 31% | `commons.h` (39) | 0 | 47 |
| oddgames/bugpunch-sdk | 29% | — | 0 | 1 |

### 4.4 Копипаст в агентных репах — реальные клоны (вендор отфильтрован)

**mako (AI 48%) — Raft-вариант скопирован с базы, выкинуты комментарии** (EXACT):
`examples/mako-raft-tests/simpleTransactionRepRaft.cc:393-409` ↔ `examples/simpleTransactionRep.cc:646-664`

```cpp
size_t nthreads = BenchmarkConfig::getInstance().getNthreads();
std::vector<std::thread> worker_threads;
worker_threads.reserve(nthreads);
spin_barrier barrier_ready(nthreads);
spin_barrier barrier_start(1);
for (size_t i = 0; i < nthreads; ++i) {
    worker_threads.emplace_back(run_worker_tests, db, i, &barrier_ready, &barrier_start);
}
barrier_ready.wait_for();      // база: "// Release workers once every thread..."  ← в Raft-копии стёрто
barrier_start.count_down();
for (auto& t : worker_threads) { t.join(); }   // база: "// Wait for all worker threads..." ← стёрто
```

**Δ:** дословная копия thread-pool сетапа; Raft-форк отличается только убранными комментариями.

**OTTO (AI 41%) — EQ-биндинг скопирован для каждого типа шины** (RENAMED ×3):
`BusEQBinding.cpp` ↔ `SendBusEQBinding.cpp` ↔ `MasterEQBinding.cpp` — `mixerBus_`→`sendBus_`→`masterBus_`,
комментарии вырезаны, тело идентично:

```cpp
if (!panel || !mixerBus_) return;            // SendBus: !sendBus_   Master: !masterBus_
resetBandBypassStates();
const auto& config = mixerBus_->getConfig(); // ← единственная содержательная разница
panel->setBypass(!config.eqEnabled);
panel->setBandFrequency(EQBandType::HP, config.eqHPFreq);
panel->setHPSlope(static_cast<float>(
    config.eqHPSlope == effects::FilterSlope::Slope6dB ? 6 :
    config.eqHPSlope == effects::FilterSlope::Slope12dB ? 12 :
    config.eqHPSlope == effects::FilterSlope::Slope18dB ? 18 : 24));
panel->setBandFrequency(EQBandType::Low, config.eqLowFreq);   // + Mid/High band — всё скопировано
```

**Δ:** вместо общей базы EQ-синхронизация продублирована на 3 типа шины; различие — имя поля шины.

**OTTO — 3-way EXACT клон responsive-layout** в `ControlPanel`/`SynthParamPanel`/`TransportBar` (`:325-337`):

```cpp
switch (currentBreakpoint_) {
    case Breakpoint::Phone:           layoutCompact(); break;
    case Breakpoint::TabletPortrait:
    case Breakpoint::TabletLandscape: layoutMedium();  break;
    case Breakpoint::Desktop: default: layoutFull();   break;
}
```

**Δ:** один и тот же breakpoint-switch скопирован дословно в 3 UI-компонента вместо базового метода.

### 4.5 Вендор-фильтр для dup — обязателен, иначе сигнал тонет

[filtered_dup.py](ai_repo_run/filtered_dup.py) выкидывает пару, если хотя бы одна сторона в
вендоренном пути. Эффект на high-AI репах драматичный:

| репо | raw dup | **authored** | срезано |
|---|---|---|---|
| esys-escript | 768 | **15** | −753 (Trilinos/p4est) |
| T5ynth | 93 | **3** | −90 (JUCE) |
| mako | 37 | 37 | 0 (не вендорит) |
| OTTO | 18 | 18 | 0 |
| cajeta | 14 | 14 | 0 |
| cpp-opengl-engine | 11 | 11 | 0 |

Без фильтра `esys`/`T5ynth` выглядят как чемпионы копипаста (768/93), хотя их авторский
копипаст — 15/3; всё остальное — Trilinos и JUCE. **Вывод: dup-сигнал по агентным репам
бессмыслен без вендор-фильтра** (= #081 Bug B, vendored-детекция).

**cajeta (AI 47%, язык/компилятор) — LLVM-init скопирован в 3 GPU-бэкенда** (EXACT ×3):
`NvptxBackend.cpp` (NVIDIA) ≡ `SpirvBackend.cpp` (Vulkan) ≡ `AmdgpuBackend.cpp` (AMD):

```cpp
static std::once_flag once;
std::call_once(once, [] {
    llvm::InitializeAllTargets();
    llvm::InitializeAllTargetMCs();
    llvm::InitializeAllAsmPrinters();
    llvm::InitializeAllAsmParsers();
});
```

**Δ:** байт-в-байт во всех трёх бэкендах — вместо общего base-backend init. + `SignAction.cpp`
≡ `VerifySigAction.cpp`.

**cpp-opengl-engine (Copilot-bot-author) — два загрузчика сцены и камеры** (EXACT):
`SceneLoader.cpp:871` ≡ `SceneLoaderJson.cpp:641`, `Camera.cpp:52` ≡ `CameraInput.cpp:148`,
+ self-клоны рендереров (`GuiRenderer`, `RectRenderer`, `ShaderProgram`).

---

## 5. Динамика constraint decay во времени (главный вопрос)

Статический скан показывает что долг *есть*; чтобы показать что это **decay** (накопление, где
каждый коммит безобиден), мерим траекторию по месячным/бимесячным срезам
([time_dynamics.py](ai_repo_run/time_dynamics.py): на каждом срезе — циклы, cross-module рёбра,
авторский копипаст).

### 5.1 mako (размечено-агентный, AI 48%) — год жизни, помесячно

| срез | cross-mod рёбра | dup_auth | циклы |
|---|---|---|---|
| 2025-06 | 11 | 43 | 4 |
| 2025-09 | 5 | 45 | 1 |
| 2025-12 | 19 | 57 | 2 |
| 2026-01 | **34** | **59** | 3 |
| 2026-05 | **39** | 47 | 3 |

**Перекрёстные ссылки (cross-module coupling): 11 → 39, ×3.5** — главный сигнал decay, скачок
дек→янв. Копипаст: 43→59 пик→47 (нетто-рост с откатом). Циклы: 1-4 без тренда (агент их
рефакторит). Вывод: **decay сильнее всего в межмодульной связности, не в циклах.**

### 5.2 newton-dynamics (человеко-*атрибутирован*, зрелый) — 2 года, baseline

| срез | nodes | dup_auth | циклы |
|---|---|---|---|
| 2024-07 | 1647 | 44 | 1 |
| 2025-05 | 1548 | 38 | 0 |
| 2026-01 | 1643 | 38 | 0 |
| **2026-03** | **2634** | **435** | 0 |

**Архитектура держалась стабильной ~1.5 года** (nodes ~1530-1650, dup ~40, циклы 0-1) —
constraints НЕ деградировали. Скачок фев-мар 2026 = **дискретный импорт** `newton-4.00/applications`
+ `thirdParty` (+492k строк, 1483 файла), не постепенный decay.

### 5.3 Контраст и оговорки

- **Зрелый код может держать инварианты годами** (newton 1.5 года плоско), агентный молодой
  показывает **постепенный coupling creep** (mako ×3.5). Но сравнение **конфаундено** (возраст,
  домен, размер) — не причинное доказательство.
- **Детекция ИИ — только размеченного.** newton «0 AI» = 0 по автору/коммиттеру/трейлеру/co-author,
  но IDE-комплишены (Cursor/Copilot autocomplete) и снятые трейлеры **в git невидимы**. «Без ИИ»
  и «ИИ без разметки» по git неразличимы — это потолок метода.
- **Пересечение «2 года истории × размеченный ИИ» в выборке почти пусто** — агентная разработка
  феномен 2025, поэтому AI-репы молодые (нет pre-AI baseline), а 2-летние — человеко-атрибутированы.
  Чистую арку «baseline → переход на ИИ → decay» на одном репо показать пока не на чем.

---

## 6. Артефакты и скрипты

- [ai_guaranteed_FINAL.tsv](ai_repo_run/ai_guaranteed_FINAL.tsv) — 470 C++ реп с гарантией ИИ-коммитов.
- [agentic_scan_470.tsv](ai_repo_run/agentic_scan_470.tsv) — archcheck-метрики по всем 470.
- [new_winners_clean.tsv](ai_repo_run/new_winners_clean.tsv) — 1543 репы >300 коммитов.
- KEEP-деревья (стриппнутые до C++/CMake) — `/home/localadm/oss/_agentic_corpus/` для re-scan.
- скрипты: `ephemeral_scan.py`, `authored_debt.py`, `graph_metrics.py`, `find_ai_repos.py`,
  `agent_author_scan.py`, `changepoint_scan.py`, `commit_attribution.py`.

**Сигнальная иерархия (от выводов сессии):** надёжность ИИ-сигнала: трейлер-в-коммите >
бот-авторство > AI% по сообщениям ≫ скачок темпа (ramp — мусор, ловит любой всплеск). Таргет =
**высокий AI% + мало контрибуторов + нет ревью-гарда + авторский (не вендор) долг**. Гиганты с
ревью — мимо.
