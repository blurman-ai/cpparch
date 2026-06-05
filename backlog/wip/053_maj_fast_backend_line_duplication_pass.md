# [RULES][SCAN] Fast-backend line-based duplication pass (порт проверенного прототипа)

**Дата создания:** 2026-05-29
**Дата старта:** 2026-05-30
**Статус:** wip
**Модуль:** RULES / SCAN
**Приоритет:** major
**Сложность:** L (ядро: порт + plumbing + fixtures + perf note). Режим нормализации, file-twin rate и P1/P2 — отдельными итерациями.
**Целевой релиз:** v0.2
**Блокирует:** #054 (ai_repo_duplication_run — cross-component ratio из P0-B желателен, но не блокер: на первом прогоне cross-file proxy)
**Заблокирован:** —
**Related:** #006 (spec_refactor — fast backend как часть двух-бекендной схемы), #033 (ai_drift_dataset — duplication как сильный AI-drift сигнал), #052 (cross_tu_duplication_detector — AST-слой, точный и дорогой; **комплементарен** этой задаче), #054 (ai_repo_duplication_run — потребитель сигнала), #055 (dogfood_json_escape_dedup — наш собственный helper-дубль `jsonEscape`, найденный этим проходом, см. §«Эталон находок»), #056 (fast_backend_partial_duplication_pass — **fast Type-3 / diverged-copy слой на токенах**; забирает «полный Type-3» и разошедшиеся копии, которые этот line-проход НЕ делает — граница: #053 = Type-1 verbatim, #056 = Type-3 diverged)

## Цель

Добавить в fast backend дешёвый snapshot-проход Type-1 line duplication: без
libclang, без `compile_commands.json`, с репортингом кросс-компонентных
дублированных блоков как **missing reuse edge**. В text-режиме headline-сигнал —
**конкретные находки (длинные блоки), а не общий `%`**; при этом ранжировать их
надо **не по голой длине** (длина наверх поднимает намеренные твины — см. уроки
ниже), а по эвристике «крупный частичный перенос». Ratio остаётся вторичной
диагностической метрикой. **Off by default.**

**Детектор уже доказан спайком + sweep'ом + ручной проверкой интента.** Оставшаяся
работа — операционная: режимы, excludes, ранжирование, метрика, перенос в продукт.

## Зачем / место в слоёной схеме

Детекция дублей в archcheck строится тремя слоями по цене и точности:

1. **Эта задача:** line-based Type-1, дёшево, мгновенно, на любой build-системе.
   Fast backend. Понижает порог входа: проект без `compile_commands.json`
   получает хотя бы verbatim-проверку.
2. **#052:** AST `CloneDetector` Type-2/3, точно, но дорого (libclang-бекенд).
3. **Будущее:** IR-canon для рерайтов «другими словами». Не сейчас.

Рамка та же, что у AST-слоя: **кросс-компонентный дублированный блок = missing
reuse edge** в смысле Lakos physical design (replication vs hierarchical reuse).

Атрибуция:

- **Lakos** — duplication между компонентами означает отсутствие reuse-edge и
  ухудшение physical design (*почему* плохо).
- **GitClear** — copy-paste как №1 измеренный сигнал AI-дрейфа и duplication
  ratio как эмпирическая мотивация. **NB:** AI-нарратив честен ТОЛЬКО в
  commit-режиме (per-commit, как GitClear), не на snapshot — см. «Два режима».

## Главные уроки спайка (НЕ переоткрывать)

Sweep по ~25 репам + ручная проверка КАЖДОЙ верхней находки глазами (Codex) дали
пять выводов, меняющих продуктовый scope:

1. **Детекция надёжна, но «дубль» ≠ «проблема».** Ложных срабатываний *детекции* —
   ноль. Но из топа находок бОльшая часть — *намеренные твины* (Catch2 два
   репортёра, abseil map/set, folly `SpookyHashV1`/`V2`, IrredenEngine demo-twins,
   LibreSprite command-twins, gm `encoder`/`encoder2`). Проблема в интенте, не в
   детекции.
2. **Интент машиной не классифицируется.** Надёжного авто-разделения
   «дыра vs намеренный твин» нет — **не притворяться, что есть.** Поэтому гейт
   не опирается на классификацию интента (см. «Два режима»: гейт = baseline).
3. **Ранжирование по длине — перевёрнутое.** Самые длинные near-100% блоки в
   зрелой базе — чаще намеренные твины (их держат синхронными специально).
   Случайная дыра со временем расходится и укорачивается. **Голая длина наверх
   поднимает НЕ то.** Главный сигнал — иной (§«Ранжирование»).
4. **Ratio exclude-зависим и обманчив.** Catch2 raw 75%; `spdlog/include` 4.5%
   (с вендоренным `fmt`) vs 8.26% (без). Никогда не выносить ratio в заголовок.
5. **Excludes — операционный центр тяжести.** Красивые цифры sweep получены
   ручной курацией (10 репам дописывали персональные exclude). Пользователю
   этого никто не сделает — **дефолты не должны позориться на первом запуске.**

## Два режима — две разные задачи (центральное решение)

### snapshot = explorer (НЕ гейт)

- Показывает горячие точки, ранжирует, **человек решает**. Не ломает CI.
  **Без AI-claim.**
- Намеренные твины честно присутствуют в выдаче — это ок, это режим разведки.

### commit / baseline = ГЕЙТ (основной продуктовый режим)

- `--git-commit <sha>`, `--git-diff A..B`. Строит baseline + current снапшоты,
  репортит только блоки, которые задевают changed set И новые относительно
  baseline.
- **Baseline морозит исторические дубли (включая твины) → CI ломает только
  НОВЫЙ дубль.** Проблема намеренных твинов (уроки §1-2) здесь полностью
  растворяется — угадывать интент не нужно.
- Единственное честное место для GitClear/AI-нарратива.
- Стоимость 2× (два снапшота) — задокументировать; приемлемо.

> Реализация commit-режима — P1-D (после первого product-merge). Но дизайн-решение
> «snapshot=advisory / commit=gate» фиксируется СЕЙЧАС как центральное, потому что
> от него зависит, на что мы НЕ полагаемся в snapshot (на ранжирование/интент).

## Текущий статус: спайк закрыт, гейт перед переносом

Спайк прогнан на ~25 OSS и локальных репах (2026-05-29). **Вердикт: слой
жизнеспособен, идём в продукт — после закрытия P0.** Артефакты — каталог
`experiments/line_duplication/` убран из дерева, остаётся только в git-истории
(коммит `35085ca`):

- `experiments/line_duplication/main.cpp` — рабочий standalone C++20 порт (~1160 стр.), не в основном билде.
- `experiments/line_duplication/PERF.md` — time + peak RSS на референс-деревьях.
- `experiments/line_duplication/LOCAL_SWEEP_REPORT.md` — исторический raw-sweep по ~20 репам.
- `experiments/line_duplication/PROJECT_EXAMPLES_REPORT.md` — актуальный app-only прогон + `examples/`.

---

## ✅ Сделано (спайк — НЕ переоткрывать)

- [x] **Алгоритм портирован в standalone C++20** (`experiments/line_duplication/main.cpp`):
      поток значимых строк, нормализация, скользящее окно, хеш, группировка по
      хешу, **слияние в максимальные блоки**, ignore-маска, default excludes,
      duplication ratio. Логика прототипа воспроизведена 1:1.
- [x] **Перф измерен** на ~25 OSS/локальных репах (`PERF.md`, `LOCAL_SWEEP_REPORT.md`,
      `PROJECT_EXAMPLES_REPORT.md`): тяжёлые прогоны — секунды, не минуты (grpc
      773k LOC → 2.4 s; gm 2.77 s / 314 MB; cpparch 1.2M → 1.9 s / 231 MB). Для CI приемлемо.
- [x] **Референсы подтверждены по сигналу:** `fmt/include` 2.07% / 0 cross-file —
      попадание; `spdlog/include` — характерные пары `daily`↔`hourly`,
      `syslog`↔`systemd` всплывают наверх cross-file списка.
- [x] **Находки валидированы вручную** на живых деревьях: `gm_github` — 78-стр.
      `PacketLogger` через два модуля и 90-стр. идентичный `p_light.cpp` между
      `BAT2`/`IMR` (реальные missing-reuse-edges).
- [x] **Ручная проверка интента глазами (Codex)** по 21 уникальному кейсу: 0 ложных
      *детекций*; бОльшая часть топа — намеренные твины. Отсюда уроки §1-3 выше.
- [x] **Commit-aware режим реализован и проверен** в спайке (`--git-commit`,
      `--git-diff A..B` / `--git-diff A`, introduced-detection через blob-сравнение
      без полного checkout). Реальный коммит `Sync from gm` → 17 introduced,
      верхний подтверждён.
- [x] **Exclude-маски выведены эмпирически** (`FILTER_CLASSIFICATION_REPORT.md`,
      ~25 реп): «жёсткие» (always: `single_include/extras/amalgamate/dist/
      *_autogen/qrc_*/moc_*/build-*/CMakeFiles/third_party/3rdparty/bundled/vendor`)
      vs «мягкие» app-only (`test/examples/upgrade/legacy/hardware_regs/CMSIS/deps`).
      До/после: Catch2 75.68% → 6.27%, pico-sdk 42.05% → 21.85%. **Это research-вход
      для дефолтного exclude-листа P0-A** (механизм авто-детекта ещё не в коде).
- [x] **Test-файлы исключаются по умолчанию** в спайке (`main.cpp`, незакоммичено):
      каталоги `test/tests/testing/fixture/fixtures/selftest`, стемы `test_*`/`*_test`/
      `*_tests`/`*_unittest`; opt-out `--include-tests`. *Решение зафиксировано:
      exclude-by-default, а не severity-tier.*
- [x] **3-классовая таксономия находок** (`DUPLICATE_PATHS_REPORT.md`): generated/
      package/vendor | test twins / expected variants | real manual copy-paste.
      Это спроектированный вход для P2-F (confidence tiers) и file-twin rate.

---

## ⬜ Осталось

### Гейт перед `src/scan` — P0 (блокирует перенос)

Закрыть **P0-A, P0-B, P0-C** до переноса кода в продукт. Можно итерировать в
спайке (дёшево), затем переносить.

- [ ] **P0-A. Дефолтные excludes, которые не позорятся на первом запуске.**
      🟡 *research закрыт; остался минимальный механизм в коде.*
  - **Жёсткие маски (всегда):** `single_include`, `extras`, `amalgamate*`,
    `dist`, `generated`, `*_generated*`, `*_autogen`, `qrc_*`, `moc_*`,
    `build-*`, `CMakeFiles`, `CompilerId*`, `third_party`, `3rdparty`,
    `thirdparty`, `bundled`, `vendor*`, `_deps`. (`bundled` обязателен: `spdlog`
    тащит `fmt` в `fmt/bundled/`.)
  - **4 авто-правила (закрывают ~80% ручной курации) — реализованы в спайке:**
    1. [x] **Build-деревья:** пропускать каталог, содержащий `CMakeCache.txt`
       (и всё поддерево ниже). `isBuildTreeDir`.
    2. [x] **Вложенные репозитории:** пропускать дочерний каталог со своим
       `.git` (или `.hg`) — одним правилом закрывает sandbox/snapshot/submodule,
       вендоренные репо. `isNestedRepoDir`. Корень скана не исключается.
    3. [x] **Тесты:** сегмент пути ∈ {`test`,`tests`,`testing`,`fixture`,`fixtures`,
       `selftest`} ИЛИ стем ∈ {`test_*`,`*_test`,`*_tests`,`*_unittest`}.
       opt-out `--include-tests`.
    4. [x] **Демо/примеры:** `examples`/`example`/`demos`/`demo` — в ту же
       непродуктовую корзину, что тесты (тот же opt-out `--include-tests`).
    - *Реализационный урок:* range-based `for (e : it)` итерирует копию
      `recursive_directory_iterator` → `disable_recursion_pending()` был no-op;
      переведено на явный цикл, иначе dir-level pruning не работает.
  - **Vendor-in-src не угадывать.** `minilzo/qhull/glad/CMSIS/libigl` и похожие —
    через `exclude` из `.archcheck.yml`; опциональный shipped-blocklist частых
    имён — отдельным шагом, если понадобится.
  - [x] **Аудит:** в text-отчёте строка `excluded subtrees: N (files dropped by
    mask: M)` + детерминированно отсортированный список запруненных корней с
    причиной (`[build-tree]`/`[nested-repo]`/`[mask]`/`[test]`/`[demo]`), cap 12 +
    `(+N more)`. `printExclusionAudit`.
    - *Решение:* **sig-LOC отрезанного НЕ считаем** — это потребовало бы обойти
      ровно те поддеревья, которые мы пропускаем (убило бы выигрыш). Список корней
      + причина + счётчик file-level масок отвечают «что вырезано» дёшево.
      Инфра-каталоги (`.git`/`.cache`/`.idea`/build-имена без `CMakeCache.txt`) в
      список не попадают как шум.
    - json/sarif-аудит — на этапе порта в продукт (репортеры там).
  - *Готово когда:* first run на типичном OSS-дереве не тонет в build/autogen/
    tests/demos, а vendor-in-src можно приглушить чисто конфигом. **Проверено:**
    на cpparch авто-правила вырезали `sandbox/drift_repos/*` (nested-repo),
    `experiments/.../examples` (demo), build-деревья; 62 файла, 0.02 s.
- [ ] **P0-B. Поменять headline-метрику и ранжирование отчёта.**
      Общий `%` нельзя делать главным сигналом (exclude-зависим, пугает на
      packaging-репах). И ранжировать находки по голой длине нельзя (длина
      поднимает намеренные твины — урок §3).
  - [ ] Headline text output = **конкретные находки (блоки)**, не процент.
    По каждому блоку: `length`, `file:line <-> file:line`,
    `cross-file/cross-component`, при наличии — компоненты.
  - [ ] **Ранжировать по эвристике «крупный частичный перенос»**, НЕ по голой
    длине (детали — §«Ранжирование»): длинный блок, который МЕНЬШЕ целого
    файла, между файлами с РАЗНЫМИ именами — верх листа.
  - [ ] Считать и выводить **две** версии ratio: `total_duplication_ratio` и
    `cross_component_ratio` (до интеграции с component-graph — явный
    `cross_file_proxy_ratio`). Гейт/headline — на cross-component, не на total.
  - [ ] `--cross-only` влияет **и на расчёт ratio, и на вывод**, не только на вывод.
  - *Готово когда:* взгляд на отчёт отвечает «где крупный частичный перенос?»,
    а не «почему здесь страшные 75%».
- [ ] **P0-C. Регрессионные якоря = известные блоки, а не %.** 🟡 *данные для
      ассертов собраны sweep'ом; сами тесты в продукте не написаны.*
  - [ ] Ассертить на **наличие** характерных блоков (`daily`↔`hourly`,
    `syslog`↔`systemd`) и длины/порядок top-blocks — НЕ на точный ratio.
  - [ ] Явно задокументировать, что ratio exclude-зависим и не stable contract.
  - *Готово когда:* ни один продуктовый тест не падает из-за дрейфа процента на
    новой версии upstream-библиотеки.

### Интеграция в продукт (после P0)

- [ ] **Порт в `src/scan/line_dup_scanner.{h,cpp}`** как сосед `include_scanner`:
      тот же preprocessor-only путь, **без libclang, без `compile_commands.json`**.
      Структуры: `vector<SigLine{string normalized; unsigned lineno;}>` на файл;
      `unordered_map<uint64_t, vector<pair<FileId, uint32_t>>>`; слияние в
      максимальные блоки по `(file_a, file_b, ia - ib, overlap)`. Хеш окна —
      быстрый non-crypto (`xxhash`/`wyhash` или аналог; в спайке blake2b взят
      ради ясности, не скорости).
- [ ] **Один `IRule`** под `src/rules/duplication/`, статическая регистрация, без
      универсального rule-engine. Если #052 уже вынес общий helper для component
      mapping / duplication reporting — переиспользовать; если нет (на сейчас
      #052 в `future/`, не стартовал) — не тянуть рефактор ради симметрии.
- [ ] **Маппинг блоков → компоненты** через `graph/component_graph`. По
      умолчанию репортить только **кросс-компонентные** блоки. Формулировка:
      `"missing reuse edge: A and B share a <N>-line block"`. Внутрифайловые/
      внутрикомпонентные — подавлять или прятать за verbosity.
- [ ] **Дедуп кейсов по паре файлов, не по scope.** Один дубль, попавший в два
      scope (напр. `spdlog` и `spdlog/include`), — ОДИН кейс, не два (в sweep
      22 показания → 21 уникальный). Считать уникальные пары файлов.
- [ ] **Конфиг и CLI:**
  - Включение: `--duplication=line` (umbrella, место под `--duplication=ast` для
    #052) **и/или** `defaults.line_duplication`. **Off by default.**
  - `thresholds`: `min_lines` (5), `min_window_chars` (~60), `min_complexity`,
    `exclude` глобы.
  - Дефолтные built-in excludes из P0-A (жёсткие маски + 4 авто-правила).
  - `exclude` из `.archcheck.yml` — обязательный путь для vendor-in-src.
  - Уважать `// archcheck: allow`.
  - `--changed-files <list>` — фильтр на **вывод**, корпус всегда = всё дерево.
  - `--baseline <file>` — заморозка снапшота (snapshot-режим).
  - Headline = находки; ratio (если считается) — вторичный diagnostic field.
- [ ] **Вывод** в text/json/sarif: две версии ratio + по каждому блоку
      `file:line <-> file:line` + длина в значимых строках. В text-режиме
      сначала top findings (по эвристике ранжирования), потом aggregate-поля.
- [ ] **`PERF-fast.md`:** перенести цифры из спайка + один большой прогон
      (`opencv`/`llvm`). Не делать из этого жёсткий CI-gate — проверка здравости.

### Ранжирование в explorer (хинты, не классификатор)

> Цель — отсортировать review-list, не выносить вердикт. **Все сигналы заведомо
> несовершенны — это веса, не правила. Гейт на ранжирование НЕ опирается** (он
> опирается на baseline: new vs old).

Эмпирика sweep (21 уникальный кейс): **0** строгих копий файла целиком, **2**
почти-файловые (обе намеренные: OreStudio core↔server форк, pico-sdk
board-варианты), **20** — перенос ЧАСТИ кода. В природе копируют не файл, а кусок.
Размерные классы:

- **крупный частичный перенос** (100–800+ строк, но < целого файла): AetherSDR,
  BambuStudio, Kartend, gm, grpc, samastra_itain — **верх листа**;
- **средний фрагмент**: GWToolboxpp, IrredenEngine, LibreSprite, folly, spectre;
- **маленький helper / одна функция** (как `jsonEscape`, 21 строка): Catch2,
  abseil, cpparch, moqx, nlohmann, spdlog, vmecpp — **низ** (реальные, но
  локальные, дёшево чинятся).

**Главный ранжирующий сигнал — НЕ голая длина** (она поднимает намеренные твины,
зеркалящие файл целиком). А: **длинный блок, который МЕНЬШЕ целого файла, между
файлами с РАЗНЫМИ именами** — режет ровно в «крупный частичный перенос»
(доминирующий и самый опасный класс), не требуя угадывать интент. Доп. сигналы:
меж-модульная дистанция; пометка «дублированные данные-таблицы».

- [ ] Реализовать ранжирование top-list по этой эвристике (вес, не фильтр).

### File-twin rate (справочный, НЕ гейт) — был P2-G

> **Мотивация — разошедшаяся копия опаснее verbatim.** Полный дубль чинится
> тривиально (вынес в функцию). Разошедшиеся копии — мина: непонятно, разошлись
> намеренно или случайно (баг пофиксили в одной, забыли в другой). Ирония:
> line-проход ловит как раз наименее опасный класс (verbatim), а разошедшиеся
> пропускает — одна изменённая строка рвёт совпадение. Это предел подхода.

Иерархия опасности **обратна** нашим возможностям:

| Класс | Опасность | Кто ловит |
|---|---|---|
| Verbatim копия | низкая (легко чинить) | line-проход ✓ |
| Переименованные идентификаторы | средняя | Type-2 / `--normalize-identifiers` |
| **Разошедшиеся (gapped)** | **высокая** | Type-3 — **#056** (fast token-overlap + LCS); вне одноимённых файлов — только там |
| Семантический рерайт | высокая, редкая | Type-4 — никто |

**Дешёвый прокси к gapped через имя файла.** Если матчить пары с ОДИНАКОВЫМ
basename в разных каталогах (или basename + цифровой суффикс: `encoder`/`encoder2`),
порог совпадения можно снизить (~40%) — имя уже несёт уверенность.

- [ ] **Правило rate:** пара файлов с одинаковым basename в разных каталогах;
  делят ≥1 значимый блок; порог совпадения снижен (~40%); сортировка по доле
  общих значимых строк; метка «file twin — вероятный форк». **Отдельный
  справочный rate, НЕ гейт** (намеренные device-варианты тоже одноимённые).
- [ ] Прототип формата уже есть: `DUPLICATE_PATHS_REPORT.md` — flat-индекс
  «файл A → файл B + строк + класс». Вынести в продуктовый репортер.

**Оговорка по приоритету (из sweep):** одноимённых файлов-близнецов в реальной
выборке почти нет (2 из 21, обе намеренные). Доминирующий класс — перенос блока
между файлами с РАЗНЫМИ именами (§«Ранжирование»), который file-twin rate НЕ
ловит. Поэтому это **дешёвый бонус, а не основная фича** — строить на нём hook нельзя.

### Опциональный режим: нормализация идентификаторов (Type-2 lite)

> Ловит *переименованные* копии (агент скопировал и переименовал переменные) на
> той же текстовой машине, без AST. Повышает recall ценой precision → **off by
> default**, с поднятыми порогами. Детали — §«Режим нормализации» ниже.
>
> **Граница с #056 (во избежание дубля):** токенная нормализация ради
> *разошедшихся* копий — это #056 (token overlap + token-LCS). Здесь нормализация
> остаётся **построчной** (нормализовал строку → матчишь те же строки в line-проходе):
> дешёвый Type-2 в рамках этого прохода, не замена #056. Если #056 поедет в продукт
> раньше — этот режим можно не реализовывать, а делегировать ему.

- [ ] Type-2 lite за флагом `--normalize-identifiers` / `strictness: aggressive`
      (дефолт — `verbatim`). Нормализовать **только идентификаторы**; НЕ трогать
      ключевые слова, фундаментальные типы, операторы, литералы. Поднятые пороги
      + cross-component-only. Тесты H (E/F/G).

### P1 — до публичного релиза, не блокирует первый перенос

- [ ] **P1-D. Commit-aware + baseline = нормальный CI-режим.** Перенести из
      спайка (`--git-commit`, `--git-diff`, introduced-detection) и состыковать
      с существующим baseline-механизмом. Исторические intentional twins и старые
      vendor-like дубли **замораживаются**, ломать CI должны только **новые
      introduced cross-component blocks**. Это и есть гейт-режим (§«Два режима»).
  - Документировать 2× стоимость (~1 GB / 7 s на тяжёлых репах).
  - Известный edge-case (ложный негатив, если блок уже существовал как другая
    пара) — задокументировать как ограничение v1.
- [ ] **P1-E. Перф-guard от патологий.** Стоимость не линейна по LOC (драйвер —
      плотность дублей и форма файлов). Cap на occurrences-per-hash (окна с >K
      вхождений — ограничивать pairing, иначе квадратичный взрыв на парах).
      `--timeout` / `--max-files` как страховка для CI.

### P2 — улучшения precision, отдельными итерациями

- [ ] **P2-F. Confidence/severity tiers.** 🟡 *таксономия спроектирована
      (`DUPLICATE_PATHS_REPORT`: generated/package · test-twin · real manual);
      в коде реализовано только test-exclusion.* Понизить severity для
      структурно-симметричных пар (variant/implementation twins вроде
      `SpookyHashV1`↔`V2`, `F14Map`↔`F14Set`); минимум — пометка «низкая ценность»
      вместо плоского списка. **Но: интент машиной не классифицируется (урок §2)** —
      это severity-хинт, не вердикт.

---

## Алгоритм (зафиксирован спайком — портировать 1:1)

- **Поток значимых строк** на файл:
  - пропускать пустые строки;
  - пропускать чисто-комментарные строки;
  - именно **пропускать**, а не занулять — чтобы разные пустые строки и
    комментарии между копиями не рвали матч.
- **Нормализация:** `strip`; схлопывание пробелов (`\s+` → ` `).
- **Скользящее окно** из `N` значимых строк (`N=5` по умолчанию — headline-порог
  GitClear), хеш каждого окна быстрым non-crypto хешем.
- Группировка окон по хешу; `>= 2` вхождений = seed duplicated window.
- **Слияние в максимальные блоки — ОБЯЗАТЕЛЬНО:** критерий схлопывания — одна
  пара файлов, одна диагональ (`ia - ib`), перекрытие. Без этого один регион
  раздувается в пачку сдвинутых окон (8 «блоков» = один регион — проверено).
- **Duplication ratio** = значимые строки, покрытые хотя бы одним дублированным
  окном / все значимые строки. Полезная **вторичная** метрика; raw-count блоков
  хрупок, headline для пользователя — про конкретные находки.
- **Ignore-маска:** окна целиком из тривиальной scaffolding-лексики (`{}`, `;`,
  `else`, `break;`, `return;`, `public:` и т.п.) ИЛИ с суммарной длиной текста
  меньше ~`60` символов — пропускать.

### Подтверждённые референс-цифры (для sanity-check порта, НЕ pixel-perfect)

- `fmt/include` → ratio ~`2.07%`, `0` cross-file блоков (чистая библиотека).
- `fmt` whole tree → ~`2.24%`, `8` cross-file (41-стр. дубль между fuzz-харнессами).
- `spdlog/include` → характерные пары наверху: `daily_file_sink.h` ↔
  `hourly_file_sink.h` (15/14/12 строк), `systemd_sink.h` ↔ `syslog_sink.h`.
  - **NB про ratio (P0-C):** число **exclude-зависимо**. С вендоренным
    `bundled/fmt` → ~`4.50%`; с исключённым `bundled` (правильно) → ~`8.26%`.
    Поэтому ассертить на **наличие характерных блоков** и **порядок** cross-file
    count, НЕ на точный %.

## Метрика

- Считать и выводить **две**: `total_duplication_ratio` и `cross_component_ratio`.
- **Гейт и любой headline — на `cross_component_ratio`**, не на total.
  `--cross-only` влияет на расчёт, не только на вывод.
- cross-**file** — это прокси; настоящая cross-**component** требует module-map из
  `component_graph` (на интеграции). До этого маркировать как
  `cross_file_proxy_ratio`.
- **В заголовок/README — находки (крупный частичный перенос), не процент.**
- **Дедуп кейсов по паре файлов, не по scope** (22 показания → 21 уникальный).

## Режим нормализации (детали Type-2 lite)

**Принцип асимметрии (держать в голове).** Это CI-гейт, ломающий сборку.
Пропущенный дубль (низкий recall) — тихо, никто не пострадал. Ложный дубль
(низкий precision) — красный CI на честном PR → человек злится → через 2-3 раза
выключает проход целиком. **Один громкий false positive дороже десяти тихих
false negative.** Recall можно недотягивать, precision — нельзя (риск №3 из спека).

**Что нормализовать:** идентификаторы (имена переменных, функций, пользовательских
типов) → один плейсхолдер (напр. `$`).

**Что НЕ трогать:** ключевые слова (`if`/`for`/`return`...), **фундаментальные
типы** (`int` vs `double` — реальная разница), **операторы** (`+` vs `-`
различает `add`/`sub`), **литералы** (разные константы часто и есть смысл).
Сохранённые операторы/ключевые слова — главная защита от ложных схлопываний.

**Токенизация** (решение за мейнтейнером при реализации):

- **(рекомендуется)** clang raw `Lexer` — корректный, буфер-only, **без AST и без
  `compile_commands.json`** (промис fast-бекенда сохраняется; LLVM в проекте и
  так линкуется ради AST-прохода — новой зависимости не добавляет).
- self-contained C++ токенайзер — держит fast-бекенд свободным от LLVM, но
  регэксп-токенизация C++ — известный footgun (raw-литералы, комментарии). Брать
  только если важна полная независимость от LLVM.
- Дефолтный verbatim-режим остаётся чистым текстом без токенайзера — разный
  footprint у двух режимов, это сознательно.

**Пороги в режиме нормализации выше дефолтных:** `min_lines` ↑ (напр. 8 вместо 5),
`min_window_chars` ↑; cross-component-only обязателен (мелкий боллерплейт обычно
внутри одного компонента — так его отсекаем).

## Перф

Подтверждён: секунды и десятки–сотни MB на 1M+ LOC (grpc 773k → 2.4 s; gm 2.77 s /
314 MB; cpparch 1.2M → 1.9 s / 231 MB). Нюанс: стоимость **не линейна по LOC**
(драйвер — плотность дублей / форма файлов).

- **Guard (P1-E):** cap на occurrences-per-hash (взрывная генерёнка → квадратичный
  взрыв на парах); `--timeout` / `--max-files` как страховка CI.

## Non-goals (YAGNI / out of scope)

- **Type-2 в дефолте.** Дефолт — только Type-1/verbatim. Дешёвый Type-2 через
  нормализацию идентификаторов — строго опционально, за флагом. *Консистентный*
  Type-2 — за AST-проходом #052.
- **Полный Type-3** (gapped-копии ВНЕ одноимённых файлов — разошедшийся блок
  inline в середине чужой функции) — НЕ в line-проход; это **отдельная задача
  #056** (fast token-overlap + token-LCS, parser-free). file-twin rate здесь ловит
  только gapped в одноимённых файлах — дешёвый частный случай того, что #056 делает
  общим токенным способом; при интеграции #056 проверить, не поглощает ли он rate.
- **Type-4 / семантика / IR-уровень.**
- **Авто-классификация интента** (дыра vs намеренный твин) — урок §2: машиной
  не делается, не притворяться.
- **Per-commit / temporal** GitClear-метрика помимо commit-режима. Snapshot —
  explorer. `--changed-files` — фильтр отчёта, не сужение корпуса.
- **Near-duplicate-FILE отчёт сверх file-twin rate** — позже.
- **Vendored-blocklist частых имён** — позже, если понадобится.
- **Никакого Python в продукте.** Чистый C++20 single-binary путь.

## Тесты

- [ ] **A / clean:** один файл без дублей → ratio `0`, `0` блоков.
- [ ] **B / cross-file:** два файла с общим блоком `>= 5` строк (стиль
      daily/hourly-sink) → найден один кросс-компонентный блок, длина верна, блок
      **слит в максимальный** (не N сдвинутых окон).
- [ ] **C / trivial-only:** совпадают только скобки и keyword-scaffolding →
      `0` блоков (ignore-маска работает).
- [ ] **D / excludes:** дубли в `bundled/`, вложенном `.git`, дереве с
      `CMakeCache.txt`, `*_autogen` — исключаются по умолчанию.
- [ ] **E / tests:** `*_test.cpp` и каталог `tests/` исключены по умолчанию;
      `--include-tests` их возвращает.
- [ ] **F / commit-gate:** synthetic repo (commit добавляет verbatim-копию) →
      baseline `0` cross-file, introduced `1`. Исторический твин в baseline → НЕ
      репортится; новый дубль → репортится.
- [ ] **G / regression (свободная):** `fmt/include` `0` cross-file;
      `spdlog/include` находит блок `daily` ↔ `hourly`. Ассертить **наличие**
      блока и порядок cross-file count, НЕ точный % (см. P0-C).
- [ ] **H / Type-2 режим:** консистентно переименованная копия найдена ТОЛЬКО с
      `--normalize-identifiers`; структурно-одинаковые разные функции ниже порога
      — НЕ репортятся; `add`/`sub` (разный оператор) не схлопываются.
- [ ] **I / file-twin:** два `foo.cpp` в разных каталогах, совпадение ~50%
      (разошлись) → попадают в **file-twin rate** (понижённый порог), но НЕ в
      обычный блочный гейт; пара с разными именами на 50% — НЕ попадает в rate.

## Критерий приёмки

- [ ] Чистый C++20, без libclang, без `compile_commands.json`, без Python.
- [ ] Слияние seeds в максимальные блоки реализовано; нет инфляции сдвинутых окон.
- [ ] Репортятся именно кросс-компонентные блоки как missing reuse edge.
- [ ] Default excludes: жёсткие маски + 4 авто-правила (CMakeCache, nested `.git`,
      tests, demos); аудит исключённого в отчёте.
- [ ] Headline = находки (крупный частичный перенос), ранжированные НЕ по голой
      длине; ratio вторичен и не главный UX-сигнал.
- [ ] Две метрики (total + cross-component / cross-file proxy); гейт на cross-component.
- [ ] Два режима: snapshot (explorer, без гейта/AI-claim) и commit/baseline (гейт).
- [ ] `// archcheck: allow` уважается.
- [ ] Проход за флагом, off by default.
- [ ] Один `IRule`, переиспользует существующий plumbing там, где он есть (OCP/DRY).
- [ ] Дедуп кейсов по паре файлов, не по scope.
- [ ] Регрессионные тесты ассертят наличие блоков, не точный ratio (P0-C);
      ratio нигде не ассертится по точному значению.
- [ ] Тесты A–G зелёные; H — если режим реализован; I — если file-twin rate реализован.
- [ ] File-twin rate: одноимённые файлы в разных каталогах, понижённый порог,
      отдельный справочный rate (не гейт).
- [ ] Режим `--normalize-identifiers`: off by default; нормализует только
      идентификаторы; поднятые пороги + cross-component-only.

## Эталон находок из sweep (review-эталон — что считать успехом)

Реальные missing-reuse-edges (НЕ самые длинные — длина обманывает; размерные
классы — §«Ранжирование»):

- **Крупный частичный перенос (верх review-листа):** AetherSDR
  `ClientCompEditor` ↔ `StripCompPanel`; Kartend `mainwindow_*`; gm — 78-стр.
  `PacketLogger` через два модуля.
- **Дублированные данные:** GWToolboxpp таблица диакритики `TextUtils` ↔
  `PluginUtils` (дублированные данные ≈ почти всегда дыра).
- **Средний фрагмент:** spdlog `daily_file_sink` ↔ `hourly_file_sink`.
- **Маленький helper (наш репо!):** `cpparch` — `jsonEscape` (21 строка)
  скопирована `json_reporter.cpp` ↔ `violation_baseline.cpp`. Helper-level
  дубль, не файловая копия → вынести в util. Честный dogfooding-сюжет → **задача
  #055** (`dogfood_json_escape_dedup`); без преувеличения масштаба.

Намеренные твины (детекция верна, флагать НЕ надо — в baseline): folly V1/V2,
abseil map/set, Catch2 репортёры, IrredenEngine demo-twins. Почти-файловые копии
(2/21) — OreStudio core↔server форк, pico-sdk board-варианты — тоже намеренные.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Fast-pass только Type-1 в дефолте | дешёвый zero-setup слой без libclang |
| Skip blanks/comment-only вместо blanking | разные пустые строки и комментарии не должны рвать матч |
| Слияние по file-pair + diagonal + overlap | без этого один регион распадается на множество сдвинутых окон |
| Headline = находки, НЕ процент | общий % слишком exclude-зависим и плохо объясняет, где проблема |
| Ранжировать НЕ по голой длине | длина наверх поднимает намеренные твины; сигнал — «крупный частичный перенос» (блок < файла, разные имена) |
| Гейт не опирается на ранжирование/интент | интент машиной не классифицируется; гейт = baseline (new vs old) |
| Два режима: snapshot=explorer / commit=gate | snapshot честно показывает твины (без AI-claim); baseline морозит их, ломает только новый дубль |
| Duplication ratio вторичен | полезен как aggregate, но не как главный UX-сигнал |
| Две метрики: total + cross-component | гейтить/смотреть надо cross-component, не total/headline |
| Дедуп кейсов по паре файлов, не по scope | один дубль в двух scope = один кейс (22→21) |
| Регрессия на *наличие* блоков, не на % | ratio exclude-зависим (spdlog 4.5% с `bundled` vs 8.26% без) — P0-C |
| Авто-исключать вложенные git-репо | одним правилом закрывает sandbox/snapshot/subrepo шум (P0-A) |
| Build-дерево = каталог с `CMakeCache.txt` | дешёвый и точный built-in exclude для first run |
| Qt autogen + demos/examples исключать | `*_autogen`/`moc_*`/`qrc_*` и демо — шум, не reuse-hole |
| Тесты exclude-by-default, `--include-tests` opt-out | test twins реальны, но по умолчанию это не missing reuse edge |
| Vendor-in-src через config, не угадывать по именам | `minilzo/qhull/glad/CMSIS` domain-specific |
| File-twin rate — справочный, не гейт | одноимённых форков почти нет (2/21), device-варианты тоже одноимённые |
| Флаг `--duplication=line` (umbrella) | оставляет место под `--duplication=ast` (#052). Решено 2026-05-30 |
| Off by default | нужен этап настройки FP до возможного промоушена в fast-defaults |
| Нормализация: precision > recall, off by default | один громкий FP ломает доверие к гейту (асимметрия, риск №3) |
| CI-гейт = baseline / introduced-only | старые intentional twins морозятся; ломается только новый дубль |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `experiments/line_duplication/*` | ✅ спайк + PERF/SWEEP (`35085ca`); ✅ P0-A в `main.cpp` (незакоммичено): test/demo-exclusion + `--include-tests`, авто-правила `isBuildTreeDir`/`isNestedRepoDir`, `directoryPruneReason`, расширенные маски, exclusion-аудит (`ExclusionAudit`/`printExclusionAudit`); фикс итерации `recursive_directory_iterator`; README обновлён; ✅ FILTER/DUPLICATE_PATHS/PROJECT_EXAMPLES отчёты + `examples/` (untracked, ~25 реп) |
| `src/scan/line_dup_scanner.h/.cpp` | fast line-dup scanner |
| `src/rules/duplication/...` | одно правило поверх fast scanner |
| `src/report/text_reporter.cpp` | text output: находки (ранжирование) + две метрики |
| `src/report/json_reporter.cpp` | json output |
| `src/report/sarif_reporter.cpp` | sarif output |
| `src/main.cpp` / CLI plumbing | `--duplication=line`, `--normalize-identifiers`, `--changed-files`, `--cross-only`, `--baseline`, exclude-аудит |
| config schema/docs | `defaults.line_duplication`, `strictness`, thresholds, excludes |
| `fixtures/line_duplication/pass/` | clean / trivial / vendored pass-cases |
| `fixtures/line_duplication/fail_cross_component/` | реальный cross-component clone-case |
| `tests/...` | unit/integration coverage (A–I) |
| `PERF-fast.md` | time + peak RSS на референс-деревьях |

## Fixtures

- [ ] `fixtures/line_duplication/pass/no_duplicates_single_file/` (A)
- [ ] `fixtures/line_duplication/pass/trivial_windows_only/` (C)
- [ ] `fixtures/line_duplication/pass/bundled_excluded/` (D)
- [ ] `fixtures/line_duplication/pass/nested_git_excluded/` (D)
- [ ] `fixtures/line_duplication/pass/cmakecache_tree_excluded/` (D)
- [ ] `fixtures/line_duplication/pass/tests_excluded/` (E)
- [ ] `fixtures/line_duplication/fail_cross_component/` (B)
- [ ] `fixtures/line_duplication/commit_gate/` (F — synthetic repo)
- [ ] `fixtures/line_duplication/file_twin/` (I)
- [ ] `fixtures/line_duplication/normalize/recall_renamed/` (H-E)
- [ ] `fixtures/line_duplication/normalize/precision_different_meaning/` (H-F)
- [ ] `fixtures/line_duplication/normalize/operators_differ/` (H-G)

## Pointers

- **Референс-алгоритм:** `experiments/line_duplication/main.cpp` (проверенный
  спайк, ранее `copypaste.py`). Портировать логику 1:1: хеш → быстрый non-crypto,
  Python set/dict → `unordered_map`/`vector`.
- **Соседняя задача:** #052 (`cross_tu_duplication_detector`) — AST-слой, точный,
  дорогой. Делить маппинг в компоненты + репортинг, если helper уже есть.
- **Commit/инкрементальная CI-модель** — референс Ericsson CodeChecker.
- **Sweep-отчёты (~25 реп, untracked):** `FILTER_CLASSIFICATION_REPORT.md`
  (маски: жёсткие/мягкие + до/после), `DUPLICATE_PATHS_REPORT.md` (3-классовая
  таксономия + flat-индекс файловых пар — вход в file-twin rate),
  `PROJECT_EXAMPLES_REPORT.md` + `examples/` (app-only top-блоки).
- **Уроки спайка:** merge в максимальные блоки обязателен; **наличие характерных
  блоков** — надёжный регрессионный якорь; и raw block count, и ratio хрупкие
  (count раздувают сдвинутые окна; ratio зависит от excludes — P0-C); без
  project-local excludes отчёт тонет (P0-A); **длина наверх поднимает намеренные
  твины** — ранжировать по «крупному частичному переносу» (§«Ранжирование»);
  **интент машиной не классифицируется** — гейт = baseline, не интент-вердикт.

## В работе

- **2026-05-30: спека консолидирована** — присланная итоговая версия (sweep по ~25
  репам + ручная проверка интента глазами на 21 кейсе) влита в этот файл. Этот
  документ теперь — авторитетная версия, замещает все предыдущие черновики/аддендумы.
- **P0-A закрыт в спайке (2026-05-30):** все 4 авто-правила (build-tree, nested-repo,
  tests, demos) + расширенные жёсткие маски + аудит исключённого. Проверено на
  синтетике и на cpparch (62 файла, 0.02 s; авто-вырезаны `sandbox/drift_repos/*`,
  `examples`, build-деревья). Незакоммичено в рабочем дереве:
  `experiments/line_duplication/main.cpp` + README.
- Незакрытый код-гейт: **P0-B** (находки + ранжирование «крупный частичный
  перенос» + две метрики), **P0-C** (regression on presence).

## Следующие шаги

1. ~~**P0-A (механизм)**~~ ✅ закрыт в спайке: 4 авто-правила + жёсткие маски +
   аудит. Осталось перенести в продукт вместе с портом (json/sarif-аудит — там).
2. **P0-B** — headline = находки; ранжировать по «крупному частичному переносу»
   (НЕ по голой длине); две метрики (`total` / `cross_component`), `--cross-only`
   влияет на расчёт.
3. **P0-C** — регрессионные тесты на assert-on-presence (данные из sweep готовы);
   перестать ассертить на точный %.
4. После закрытия P0 — порт в `src/scan/line_dup_scanner.{h,cpp}` и `IRule`,
   дедуп по паре файлов.
5. После первого product-merge — **P1-D**: baseline + introduced-only
   commit-aware режим как нормальная CI-форма этого сигнала (гейт).
6. Опционально, отдельными итерациями: file-twin rate (§), `--normalize-identifiers`
   (Type-2 lite), P2-F confidence tiers.
