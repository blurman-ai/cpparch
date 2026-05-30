# [RESEARCH] Прогон по новым AI-источникам: граф + дублирование

**Дата создания:** 2026-05-30
**Дата старта:** 2026-05-30
**Статус:** wip
**Модуль:** RESEARCH
**Приоритет:** major
**Сложность:** unknown
**Блокирует:** —
**Заблокирован:** — (детектор дублей — standalone-спайк #053; cross-component ratio из P0-B желателен, но не блокер: на первом прогоне cross-file proxy)
**Related:** #033 (ai_drift_dataset — первый корпус, DRIFT по 33 PR), #053 (fast_backend_line_duplication_pass — детектор), #052 (cross_tu_duplication_detector — AST-слой), #048 (drift_clean_checkout_methodology), #029 (metric_regression_detection)

## Цель

Прогнать по корпусу AI-репозиториев (готовый публичный датасет, не самодельный
grep) **обе** метрики archcheck — структуру/дрейф include-графа (DRIFT.1/DRIFT.2
из #009 + абсолютные граф-метрики) и дублирование (line-dup из #053) — и
зафиксировать измеренные цифры без интерпретации. Корпус новый, поэтому
граф-прогон идёт заново: честная перепроверка прошлого узкого результата (#033)
на несмещённом наборе и прямое сравнение «где сигнал сильнее — граф или дубли».

## Контекст

Первый корпус (#033) искал AI-PR'ы по 1-2 git-маркерам и мерил **дрейф
include-графа** (DRIFT.1/DRIFT.2). Улов оказался узким: 12 DRIFT.1 hit'ов на
7 из 33 PR, DRIFT.2 — ноль. Разбор поля (2026-05-30) показал две вещи:

1. **Источники AI-реп есть готовые** — не нужно грепать GitHub руками. См.
   [docs/research/ai_code_detection_landscape.md](../../docs/research/ai_code_detection_landscape.md):
   - **Configs dataset** ([2605.08435](https://arxiv.org/html/2605.08435)) —
     4 738 репо с AI-конфигами; 71.6% из них имеют AI-коммит.
   - **AIDev** — 932k agentic PR / 116k репо; есть и human-PR (для within-repo
     контроля).
   - **Debt Behind the AI Boom** ([2603.28592](https://arxiv.org/abs/2603.28592))
     — 304k AI-коммитов по 4 git-сигналам (29 инструментов).
2. **Мерим ОБЕ метрики.** На первом корпусе (#033) граф-дрейф почти молчал, но
   корпус был узкий и ручной. На несмещённом датасете надо перепроверить граф
   заново И добавить дублирование (где сигнал поля сильнее — GitClear: блоки 5+
   дублей ×8 за 2024; «AI Code in the Wild»: AI-код концентрируется в
   glue/тестах/boilerplate). Цель — прямое сравнение силы сигнала на одном корпусе.

Эта задача — **только прогон и сбор цифр**. Гипотезы и проектные выводы
(«дублирование как детектор AI-дрейфа», позиционирование archcheck) выносятся
отдельно и сюда не пишутся, чтобы данные оставались нейтральными.

Все методы/датасеты/ссылки уже сведены в
[ai_code_detection_landscape.md](../../docs/research/ai_code_detection_landscape.md)
— это справочный вход для задачи.

## Разведка (2026-05-30, выполнено — цифры реальные)

Источник выбран — **AIDev** ([hao-li/AIDev](https://huggingface.co/datasets/hao-li/AIDev),
HF, CC BY 4.0). Скачана `all_repository.parquet` (**116 211 репо**), посчитано
по колонке `language` (pandas + pyarrow):

| Язык | Репо |
|------|------|
| Python | 25 660 |
| TypeScript | 19 709 |
| JavaScript | 15 247 |
| (none) | 11 986 |
| HTML | 10 336 |
| Java | 3 481 |
| C# | 3 463 |
| Go | 2 511 |
| PHP | 2 483 |
| **C++** | **2 445** |
| Rust | 1 958 |

C++ по звёздам: `≥10` → 233, `≥50` → 150, **`≥100` → 119**, `≥500` → 73,
`≥1000` → 64, `≥5000` → 33. (Из 2 445 C++-реп лишь 233 имеют ≥10 звёзд —
остальное мелочь/форки/учебные.)

Top C++ по звёздам (для понимания состава): bitcoin/bitcoin (84.8k),
ggml-org/llama.cpp (83.7k), opencv/opencv (83.3k), tesseract-ocr/tesseract
(68.5k), x64dbg/x64dbg, ggml-org/whisper.cpp, microsoft/WSL, gabime/spdlog,
keepassxreboot/keepassxc, PaddlePaddle/Paddle, Tencent/ncnn,
microsoft/onnxruntime, …

**Вывод разведки:** корпус жизнеспособен, но скромнее ожиданий — рабочее ядро
~119 реп (≥100★) / ~233 (≥10★). Топ — крупные зрелые проекты (opencv/bitcoin
клонируются долго и тяжелы). Configs dataset (2605.08435) не нужен: там C++
спрятан в «remaining 27.5%» без отдельной цифры, AIDev даёт точный язык + stars
+ human/agentic PR для будущего within-repo.

Колонки `all_repository`: `id, url, license, full_name, language, forks, stars`.
В датасете также есть `pr_commit_details` / `pr_commits` / `human_pull_request`
— для within-repo AI/human среза на следующем этапе.

## Решённые развилки

- **Источник:** AIDev (по разведке выше).
- **Срез:** сначала **whole-tree** (дёшево, есть инструмент); within-repo
  AI/human — отдельным следующим этапом, когда whole-tree покажет сигнал.
- **Детектор дублирования:** standalone-спайк
  [experiments/line_duplication/main.cpp](../../experiments/line_duplication/main.cpp)
  с явной пометкой «cross-file proxy» (порт #053 в `src/` не ждём).
- **Детектор графа:** DRIFT.1/DRIFT.2 уже в `src/` (#009). Для whole-tree
  снимаем абсолютные граф-метрики (cycles, CCD/ACD/NCCD, god-headers, chain
  length); DRIFT-дельта — где применима (на AI-PR диапазоне).

## Отбор корпуса (решено 2026-05-30)

- **Выборка:** ~50 **случайных** C++-реп из слоя `≥100★` (119 шт.), seed
  фиксирован для воспроизводимости.
- **Лимит размера:** брать только репо **< 500 МБ** (размер через
  `gh api repos/{full_name}` → поле `size` в КБ, порог < 512000). Мега-проекты
  (opencv/bitcoin/paddle) отсекаются этим лимитом естественно.
- **Частичный чекаут:** тянуть только анализируемые text/code файлы, исключать
  бинарь/картинки/документы (`*.png/jpg/gif/ico/pdf/zip/bin/...`), которые
  archcheck и спайк всё равно не читают. Способ: `git clone --filter=blob:none`
  + sparse-checkout по code-расширениям, либо post-clone чистка. Экономит диск.

## План выполнения

- [ ] Зафиксировать N и критерий отбора C++-реп из AIDev (см. открытый вопрос).
- [ ] Выгрузить список `full_name`, склонировать в `~/oss` (постоянное хранилище, не sandbox).
- [ ] **Граф-метрики:** прогнать archcheck (`src/`) по каждому репо — абсолютные (cycles, CCD/ACD/NCCD, god-headers, chain length) + DRIFT-дельта где применима.
- [ ] **Дублирование:** прогнать line-dup спайк по каждому репо (whole-tree).
- [ ] Применить default excludes + авто-исключение вложенных git-репо/вендоринга (урок #053 P0-A).
- [ ] Зафиксировать на репо: sig LOC; граф — cycles/CCD/god-headers/chain; дубли — total ratio, cross-file proxy ratio, top-N блоков; time/RSS.
- [ ] Выборочно проверить находки обеих метрик вручную (валидный сигнал / шум).
- [ ] Записать результат в `docs/research/` отдельным run-логом (как `ai_drift_runlog.md`), **только цифры + ручная верификация**.

## Критерий приёмки

- [ ] Корпус собран из AIDev (готовый датасет), критерий отбора и N задокументированы.
- [ ] Прогон воспроизводим: команды + версия archcheck + версия спайка + excludes зафиксированы.
- [ ] В run-логе по каждому репо есть **и граф-, и duplication-цифры** (сравнение силы сигнала возможно).
- [ ] Находки обеих метрик выборочно проверены вручную.
- [ ] Ограничения названы явно: смещение датасета по языкам; cross-file proxy ≠ cross-component; whole-tree не отделяет AI от human (within-repo — следующий этап; корреляция ≠ каузальность).
- [ ] Файл не содержит проектных выводов — только измерения.

## Сделано

- **2026-05-30 разведка:** скачана AIDev all_repository.parquet (116211 репо);
  C++=2445, ≥100★=119. Артефакты в `experiments/ai_repo_run/`: corpus_50.tsv
  (50 случайных C++ ≥100★ <500МБ, seed=42), skipped_big.tsv, sample_cpp.py.
- **2026-05-30 пилот (5 реп), реальные цифры** (бинарь
  `/tmp/line_dup_build/line_duplication`, `--top 8`, текстовый вывод; спайк
  НЕ поддерживает `--json`). Сырьё: `experiments/ai_repo_run/pilot_raw/*.txt`.

  | repo | eligible files | sigLOC | dupLOC | ratio% | blocks | cross-file |
  |------|---:|---:|---:|---:|---:|---:|
  | mqtt_client | 2 | 1395 | 184 | 13.19 | 13 | 1 |
  | rii | 6 | 6798 | 586 | 8.62 | 33 | 0 |
  | guetzli | 47 | 5982 | 289 | 4.83 | 18 | 1 |
  | pict | 42 | 8027 | 485 | 6.04 | 37 | 14 |
  | implot3d | 7 | 7806 | 490 | 6.27 | 32 | 1 |

  Время <0.01 s/репо, RSS ~4 МБ. Пайплайн рабочий. Реальные cross-file находки
  (дословно): mqtt_client `MqttClient.hpp:2 <-> MqttClient.cpp:2` (19 lines,
  header↔impl, низкополезно); guetzli `jpeg_data_encoder.cc:56 <->
  jpeg_data_writer.cc:56` (5); pict `api/resource.h:7 <-> clidll/resource.h:7`
  (8) +13; implot3d `implot3d.cpp:3286 <-> implot3d_demo.cpp:1975` (10).
- **Замечено:** (1) `rii` ratio 8.62% раздут вендоренным `src/extern/sse2neon.h`
  — дефолтные excludes папку `extern` не ловят; (2) `mqtt_client` 13.19% — почти
  весь дубль header↔impl одной компоненты, для cross-component не сигнал
  (whole-tree раздувает).
- **2026-05-30 фикс excludes (по решению «просто добавить паттерны»).** Проверено
  эмпирически (см. `PILOT_NOTES.md`): nested-git=0 во всех 5 репах, .gitignore
  вендоринг не ловит (закоммичен) — поэтому только паттерны. В
  `experiments/line_duplication/main.cpp` к дефолтам добавлены: `extern`,
  `external`, `external_libs`, `vendor`, `vendored`, `deps`, `3rdparty`,
  `3rd_party`. Пересобрано (бинарь mtime 15:35), перепрогон (сырьё
  `pilot_raw/*_v2.txt`):

  | repo | eligible | sigLOC | ratio% | blocks | cross-file | Δ к v1 |
  |------|---:|---:|---:|---:|---:|---|
  | mqtt_client | 2 | 1395 | 13.19 | 13 | 1 | без изм. |
  | rii | 5 | 769 | 3.90 | 2 | 0 | было 6798 sigLOC / 8.62% |
  | guetzli | 47 | 5982 | 4.83 | 18 | 1 | без изм. |
  | pict | 42 | 8027 | 6.04 | 37 | 14 | без изм. |
  | implot3d | 7 | 7806 | 6.27 | 32 | 1 | без изм. |

  Фикс валиден: изменился только rii. Показательно: его sigLOC упал 6798→**769**
  — `extern/sse2neon.h` составлял ~90% «кода» репо.

- **2026-05-30 корпус склонирован полностью.** Все 50 реп в
  `~/oss/_aidev_run/` (2.8 ГБ, 0 фейлов, `git clone --depth 1
  --filter=blob:none`). Лог: `experiments/ai_repo_run/clone.log`.

- **2026-05-30 инфраструктура многократного прогона** в `experiments/ai_repo_run/`:
  `run_dup.sh <tag>` (прогон по всему корпусу → `runs/<tag>/` сырьё+summary+контекст),
  `diff_runs.sh <a> <b>` (сравнение прогонов), `README.md` (пошаговая инструкция).

- **2026-05-30 первый полный прогон `v1_excludes` (50 реп).** Сводка:
  `runs/v1_excludes/summary.tsv`, сырьё `runs/v1_excludes/<repo>.txt`. Перф ок
  (секунды на репо). Видны выбросы: CnC_Generals_Zero_Hour 90.56%,
  ReP_AL-3D-Lawn-Mower 84.55%, ncnn 68.40%, alpaka 62.58% — кандидаты на
  невычищенный вендоринг/генерёнку (проверять по `<repo>.txt`).

- **🐞 РЕАЛЬНЫЙ БАГ спайка (доказан на данных, блокер достоверности).**
  `isCommentOnly` (main.cpp:275) фильтрует комментарии **построчно** по началу
  строки (`//`, `/*`, `*`), но НЕ отслеживает состояние внутри блока `/* ... */`.
  Лицензионные шапки `/* ... */`, где строки начинаются с обычного текста
  (`MIT License`, `Copyright...`), проходят как «значимый код».
  Доказательство: `mqtt_client` top-блок «19 lines MqttClient.hpp:2 <->
  MqttClient.cpp:2» = дословно текст MIT-лицензии (проверено `sed`), строки 1=`/*`,
  25=`*/`. Раздувает sigLOC и cross-file. Это тот же класс бага, что чинили в
  боевом SF.7 (#038 block-comment stripping) — спайк его не унаследовал.

- **2026-05-30 фикс block-comment ЗАКРЫТ + прогон v2.** Переиспользована
  проверенная пара из боевого SF.7 (`src/rules/sf7_using_namespace.cpp`):
  `stripLineComment` + `updateBlockCommentState(raw, inBlockComment)` — не свой
  велосипед. Пересобрано (mtime 16:34, exit 0; IDE-диагностики clangd были
  ложные). Прогон `runs/v2_blockcomment/`, сравнение `diff_runs.sh v1 v2`.
  Эффект (примеры): mqtt_client cross-file 1→0 (лицензия ушла);
  HexRaysCodeXplorer 11.29→6.92%; itksnap 11.38→8.34%; HPCC cross-file 3066→2725.
- **Вскрылся НОВЫЙ класс шума (доказан).** FTXUI ratio вырос 8.45→10.45% —
  не баг: фикс теперь срезает и **хвостовые** комментарии (`foo(); // A`), а
  старый код их оставлял. Из-за этого совпали блоки `#include` с разными
  `// for ...` комментариями. Доказательство: новый cross-file
  `input.cpp:9 <-> radiobox.cpp:5` (11 строк) = одинаковый список
  `#include "ftxui/..."` с разными хвостовыми комментариями (проверено `sed`).
  Технически дубль, архитектурно — общие зависимости, не «missing reuse edge».
  **Отдельная задача: фильтровать include-блоки** (не блокер, но завышает сигнал).

- **2026-05-30 разбор выбросов + фикс вендоринга (v3_vendor_ci).** Проверены
  топ-блоки выбросов по `<repo>.txt` — оказались ТРИ разных класса, не один:
  - **alpaka 62%, AMICI 28%** — вендоринг `thirdParty/catch2` и т.п. ШУМ.
  - **ncnn 68%** — `src/layer/{loongarch,mips,arm,riscv}/` SIMD-бэкенды.
    РЕАЛЬНЫЙ дубль кода проекта, НЕ шум — не трогать.
  - **CnC_Generals 90%** — `Generals/` vs `GeneralsMD/` (идентичные файлы по
    3000+ строк). Форк-в-репо (две версии игры). Реальный, не вендоринг.
  - **ReP_AL 84%** — ESP32 camera boilerplate (Espressif), скопирован в 2 места.
    Полу-вендоринг.

  Вывод: **по ratio резать вслепую нельзя** — ncnn 68% правда, alpaka 62% мусор.
  Фикс — только однозначный вендоринг: матчинг excludes сделан
  **регистронезависимым** (вендор-папки в корпусе: `thirdParty/ThirdParty/
  3rdParty/External/Submodules` — все варианты регистра), + добавлены
  `vendors/submodules/subprojects`. Пересобрано (mtime 16:52). Эффект (diff v2→v3):
  alpaka 61.9→**20.3%**, AMICI 28.1→**23.9%** (blocks 1296→297), PcapPlusPlus
  10.6→9.4%. ncnn/CnC не сдвинулись (правильно — там не вендоринг).

- **Вскрылся ещё класс шума (доказан, НЕ чинен):** Effekseer — `ShaderHeader/
  *.h` это GLSL-шейдеры, зашитые в C++ строкой (`static const char ...[] =
  R"(#version 120 ...)"`). Дубли между ними — сгенерённый шейдерный код, не
  архитектура. Универсального паттерна по имени НЕТ (у каждого проекта свои
  имена) → нужен detect-by-content, это БОЛЬШОЕ решение, отложено. Не блокер
  (1 репо). Зафиксировано как известное ограничение.

## Состояние v3 (после 3 фиксов: excludes+comment+vendor-CI)

Распределение ratio по 50 репам: ≥40% — 3 (CnC/ReP_AL/ncnn, все объяснены),
20-40% — 8, 10-20% — 18, 3-10% — 17, <3% — 4. Сводка `runs/v3_vendor_ci/summary.tsv`.
Известные остаточные искажения: shader-headers (Effekseer), форк-в-репо (CnC),
include-блоки (FTXUI), header↔impl (cross-file proxy, не cross-component).

## В работе

- Решить: чинить ли include-блоки / shader-headers, или принять остаточный шум
  с явной пометкой и переходить к граф-метрикам.

## Следующие шаги

1. Починить block-comment stripping в `experiments/line_duplication/main.cpp`, пересобрать (проверить mtime), `./run_dup.sh v2_blockcomment`, `./diff_runs.sh v1_excludes v2_blockcomment`.
2. Разобрать выбросы (CnC/ReP_AL/ncnn/alpaka) по `<repo>.txt` → возможно ещё exclude-паттерны.
3. Граф-метрики: собрать archcheck (build/debug), прогнать по корпусу.
4. Свести в run-лог `docs/research/`. По итогам whole-tree решить про within-repo AI/human.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Корпус из AIDev, не grep | поле уже собрало списки; самодельный grep смещён к «аккуратным» (подписывающим) репо; AIDev даёт точный язык + stars |
| Мерим ОБЕ метрики (граф + дубли) | граф на первом корпусе молчал, но корпус был узкий; на несмещённом надо перепроверить граф И сравнить с дублированием на одном наборе |
| Сначала whole-tree, within-repo потом | дёшево и есть инструмент; AI/human-срез дороже (commit-level) — отдельный этап при наличии сигнала |
| Только цифры, выводы отдельно | держать данные нейтральными; интерпретация — отдельная задача |
| Корпус хранится локально, не пересоздаётся | инструмент (спайк excludes/ratio) будем дорабатывать и прогонять по корпусу МНОГОКРАТНО; качать 50 реп заново каждый прогон расточительно. Канонический путь: `~/oss/_aidev_run/` (постоянное хранилище). |
| Прогоны версионируются (`runs/<tag>/`) | правка инструмента → новый tag → `diff_runs` показывает сдвиг; не затирать прошлые цифры |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `experiments/line_duplication/main.cpp` | excludes: добавлены extern/vendor/deps/3rdparty/...; (далее) фикс block-comment stripping |
| `experiments/ai_repo_run/corpus_50.tsv` | список корпуса (AIDev C++, 50 реп, seed=42) |
| `experiments/ai_repo_run/sample_cpp.py`, `skipped_big.tsv`, `clone.log` | сэмплер, отсев, лог клонирования |
| `experiments/ai_repo_run/run_dup.sh`, `diff_runs.sh`, `README.md` | раннер, сравнение прогонов, инструкция |
| `experiments/ai_repo_run/runs/v1_excludes/` | первый полный прогон (50 реп): summary + сырьё |
| `experiments/ai_repo_run/PILOT_NOTES.md` | рабочие заметки/восстановление контекста |
| `docs/research/ai_code_detection_landscape.md` | справочник поля (методы/датасеты/цифры) |
| `docs/research/<новый run-лог>.md` | (далее) сведённые результаты прогона |

## Pointers

- Справочник поля: [docs/research/ai_code_detection_landscape.md](../../docs/research/ai_code_detection_landscape.md)
- Датасет: [hao-li/AIDev](https://huggingface.co/datasets/hao-li/AIDev) (HF; `all_repository.parquet` скачан в `/tmp/aidev_repo.parquet` на момент разведки; нужен `pyarrow` для чтения)
- Детектор дублей: [experiments/line_duplication/main.cpp](../../experiments/line_duplication/main.cpp) (standalone) / #053 (порт в `src/`)
- Детектор графа: DRIFT.1/DRIFT.2 (#009) + абсолютные граф-метрики в `src/`
- Прошлый корпус и методология: [ai_drift_runlog.md](../../docs/research/ai_drift_runlog.md), #048
