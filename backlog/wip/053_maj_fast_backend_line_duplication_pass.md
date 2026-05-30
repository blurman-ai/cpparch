# [RULES][SCAN] Fast-backend line-based duplication pass (порт проверенного прототипа)

**Дата создания:** 2026-05-29
**Дата старта:** 2026-05-30
**Статус:** wip
**Модуль:** RULES / SCAN
**Приоритет:** major
**Сложность:** L (ядро: порт + plumbing + fixtures + perf note). Режим нормализации и P1/P2 — отдельными итерациями.
**Целевой релиз:** v0.2
**Блокирует:** —
**Заблокирован:** —
**Related:** #006 (spec_refactor — fast backend как часть двух-бекендной схемы), #033 (ai_drift_dataset — duplication как сильный AI-drift сигнал), #052 (cross_tu_duplication_detector — AST-слой, точный и дорогой; **комплементарен** этой задаче)

## Цель

Добавить в fast backend дешёвый snapshot-проход Type-1 line duplication: без
libclang, без `compile_commands.json`, с репортингом только кросс-компонентных
дублированных блоков как **missing reuse edge** и с метрикой duplication ratio.
**Off by default.**

## Зачем / место в слоёной схеме

Детекция дублей в archcheck строится тремя слоями по цене и точности:

1. **Эта задача:** line-based Type-1, дёшево, мгновенно, на любой build-системе.
   Fast backend. Понижает порог входа: проект без `compile_commands.json`
   получает хотя бы verbatim-проверку.
2. **#052:** AST `CloneDetector` Type-2/3, точно, но дорого (libclang-бекенд).
3. **Будущее:** IR-canon для рерайтов «другими словами». Не сейчас.

Рамка та же, что у AST-слоя: **кросс-компонентный дублированный блок = missing
reuse edge** в смысле Lakos physical design.

Атрибуция:

- **Lakos** — duplication между компонентами означает отсутствие reuse-edge и
  ухудшение physical design (*почему* плохо).
- **GitClear** — headline-метрика «блоки из 5+ дублированных строк» и
  duplication ratio как эмпирическая мотивация.

## Текущий статус: спайк закрыт, гейт перед переносом

Спайк прогнан на ~20 OSS и локальных репах (2026-05-29). **Вердикт: слой
жизнеспособен, идём в продукт — после закрытия P0.** Артефакты (коммит `35085ca`):

- [experiments/line_duplication/main.cpp](../../experiments/line_duplication/main.cpp) — рабочий standalone C++20 порт (1162 стр.), не в основном билде.
- [experiments/line_duplication/PERF.md](../../experiments/line_duplication/PERF.md) — time + peak RSS на референс-деревьях.
- [experiments/line_duplication/LOCAL_SWEEP_REPORT.md](../../experiments/line_duplication/LOCAL_SWEEP_REPORT.md) — детальный sweep по ~20 репам.

---

## ✅ Сделано (спайк — НЕ переоткрывать)

- [x] **Алгоритм портирован в standalone C++20** (`experiments/line_duplication/main.cpp`):
      поток значимых строк, нормализация, скользящее окно, хеш, группировка по
      хешу, **слияние в максимальные блоки**, ignore-маска, default excludes,
      duplication ratio. Логика прототипа воспроизведена 1:1.
- [x] **Перф измерен** на ~20 OSS/локальных репах (`PERF.md`, `LOCAL_SWEEP_REPORT.md`):
      тяжёлые прогоны — секунды, не минуты (grpc 773k LOC → 2.4 s; samastra
      938k → 3.5 s / 500 MB; cpparch 1.2M → 1.9 s / 231 MB). Для CI приемлемо.
- [x] **Референсы подтверждены по сигналу:** `fmt/include` 2.07% / 0 cross-file —
      попадание; `spdlog/include` — характерные пары `daily`↔`hourly`,
      `syslog`↔`systemd` всплывают наверх cross-file списка.
- [x] **Находки валидированы вручную** на живых деревьях: `gm_github` — 78-стр.
      `PacketLogger` через два модуля и 90-стр. идентичный `p_light.cpp` между
      `BAT2`/`IMR` (реальные missing-reuse-edges).
- [x] **Commit-aware режим реализован и проверен** в спайке (`--git-commit`,
      `--git-diff A..B` / `--git-diff A`, introduced-detection через blob-сравнение
      без полного checkout). Реальный коммит `Sync from gm` → 17 introduced,
      верхний подтверждён.

---

## ⬜ Осталось

### Гейт перед `src/scan` — P0 (блокирует перенос)

Закрыть **P0-A, P0-B, P0-C** до переноса кода в продукт. Можно итерировать в
спайке (дёшево), затем переносить.

- [ ] **P0-A. Авто-детект и аудит excludes.**
      Без project-local excludes метрика тонет (unigine 49.78% → 14.02% после
      `upgrade/`; cpparch 1.2M sig LOC → 7 949 после `sandbox/drift_repos/*`;
      spdlog забит `bundled/fmt`).
  - Авто-исключать **вложенные git-репозитории** (любой каталог со своим `.git`,
    если не переопределено) — одним правилом закрывает sandbox/drift_repos,
    вендоренные деревья, снапшоты.
  - Учитывать `.gitignore`.
  - Дефолтные паттерны снапшотов/копий: `upgrade/`, `*_old`, `*.bak`, `копия_*`,
    `* - копия*`, `*_backup`.
  - repo-specific `exclude` из `.archcheck.yml`.
  - **Аудит:** в отчёт — список исключённых каталогов и сколько sig LOC отрезано.
    Раз excludes так двигают метрику, пользователь должен видеть, что выкинули.
  - *Готово когда:* на cpparch/unigine без ручных флагов метрика осмысленна;
    отчёт показывает, что исключено.
- [ ] **P0-B. Разделить total ratio и cross-component ratio.**
      Сейчас `--cross-only` фильтрует только top-blocks; ratio всё ещё по ВСЕМ
      окнам, включая same-file. Гейтить и публиковать надо cross-component.
  - Считать и выводить **две** метрики: `total_duplication_ratio` и
    `cross_component_ratio` (гейт-метрика, headline для README).
  - `--cross-only` должен влиять на **расчёт ratio**, не только на вывод.
  - cross-**file** — это прокси; настоящая метрика — cross-**component**
    (module-map из `graph/component_graph`), фиксится на интеграции. До неё —
    явно маркировать число как «cross-file proxy».
  - *Готово когда:* обе метрики в отчёте; гейт на cross-component (или
    cross-file proxy с явной пометкой).
- [ ] **P0-C. Починить регрессионные референсы (correctness задачи).**
      Прежний референс «spdlog/include ≈ 4.50%» неверен — это число с вендоренным
      `bundled/fmt` внутри; с правильным исключением `bundled` → ~8%. Ratio
      **exclude-зависим**.
  - Регрессию ассертить на **наличие** характерных блоков (`daily`↔`hourly`,
    `syslog`↔`systemd`) и **порядок** cross-file count — НЕ на точный %.
  - Зафиксировать оба прогона spdlog: с `bundled` и без, с разными ожидаемыми
    ratio (демонстрирует exclude-зависимость).
  - *Готово когда:* ни один тест не ассертит точный ratio; тесты переживают
    дрейф версий.

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
- [ ] **Конфиг и CLI:**
  - Включение: `--duplication=line` (umbrella, место под `--duplication=ast` для
    #052) **и/или** `defaults.line_duplication`. **Off by default.**
  - `thresholds`: `min_lines` (5), `min_window_chars` (~60), `exclude` глобы
    (`third_party`, `bundled`, `generated`, `*_generated*`, `*.pb.*`).
  - Уважать `// archcheck: allow`.
  - `--changed-files <list>` — фильтр на **вывод**, корпус всегда = всё дерево.
  - Duplication ratio (обе метрики из P0-B) — в отчёт.
- [ ] **Вывод** в text/json/sarif: dup ratio + по каждому блоку
      `file:line <-> file:line` + длина блока в значимых строках.
- [ ] **`PERF-fast.md`:** перенести цифры из спайка + один большой прогон
      (`opencv`/`llvm`). Не делать из этого жёсткий CI-gate — проверка здравости,
      не контракт.

### Опциональный режим: нормализация идентификаторов (Type-2 lite)

> Ловит *переименованные* копии (агент скопировал и переименовал переменные) на
> той же текстовой машине, без AST. Повышает recall ценой precision → **off by
> default**, с поднятыми порогами. Детали — в секции «Режим нормализации» ниже.

- [ ] Type-2 lite за флагом `--normalize-identifiers` / `strictness: aggressive`
      (дефолт — `verbatim`). Нормализовать **только идентификаторы**; НЕ трогать
      ключевые слова, фундаментальные типы, операторы, литералы. Поднятые пороги
      + cross-component-only. Тесты E/F/G.

### P1 — до публичного релиза, не блокирует первый перенос

- [ ] **P1-D. Commit-aware режим → продуктовый скоуп.** Перенести из спайка
      (`--git-commit`, `--git-diff`, introduced-detection) — это и есть настоящая
      PR-gate форма. Документировать 2× стоимость (baseline + current снапшоты:
      ~1 GB / 7 s на тяжёлых репах). Известный edge-case (ложный негатив, если
      блок уже существовал как другая пара) — задокументировать как ограничение v1.
- [ ] **P1-E. Перф-guard от патологий.** Стоимость не линейна по LOC (драйвер —
      плотность дублей и форма файлов). Cap на occurrences-per-hash (окна с >K
      вхождений — пропускать/ограничивать pairing, иначе квадратичный взрыв на
      парах). `--timeout` / `--max-files` как страховка для CI.

### P2 — улучшения precision, отдельными итерациями

- [ ] **P2-F. Confidence/severity tiers.** Sweep показал реальные текстовые
      дубли, которые НЕ missing-reuse-edge (abseil `unordered_map`/`set`
      twin-тесты, fmt fuzzing-харнессы — параллельная структура by design).
      Понизить severity для дублей внутри `test/`-деревьев и структурно-
      симметричных пар; минимум — пометка «низкая ценность».
- [ ] **P2-G. Near-duplicate FILE отчёт.** 90-стр. идентичный `p_light.cpp` —
      near-duplicate файл целиком, самый высокий confidence в missing-reuse-edge,
      считается почти даром поверх блочных данных. Отдельная сводка «файлы A и B
      совпадают на N%».

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
  окном / все значимые строки. **Надёжная метрика; raw-count блоков хрупок —
  на него не ассертить.**
- **Ignore-маска:** окна целиком из тривиальной scaffolding-лексики (`{}`, `;`,
  `else`, `break;`, `return;`, `public:` и т.п.) ИЛИ с суммарной длиной текста
  меньше ~`60` символов — пропускать.
- **Default excludes:** `third_party`, `bundled`, `generated`, `*_generated*`,
  `*.pb.*`. ВАЖНО: `spdlog` тащит `fmt` в `fmt/bundled/` → `bundled` обязан быть
  в дефолтном exclude-листе.

### Подтверждённые референс-цифры (для sanity-check порта, НЕ pixel-perfect)

- `fmt/include` → ratio ~`2.07%`, `0` cross-file блоков (чистая библиотека).
- `fmt` whole tree → ~`2.24%`, `8` cross-file (41-стр. дубль между fuzz-харнессами).
- `spdlog/include` → характерные пары наверху: `daily_file_sink.h` ↔
  `hourly_file_sink.h` (15/14/12 строк), `systemd_sink.h` ↔ `syslog_sink.h`.
  - **NB про ratio (P0-C):** число **exclude-зависимо**. С вендоренным
    `bundled/fmt` → ~`4.50%`; с исключённым `bundled` (правильно) → ~`8%`.
    Поэтому ассертить на **наличие характерных блоков** и **порядок** cross-file
    count, НЕ на точный %.

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

## Non-goals (YAGNI)

- **Type-2 в дефолте.** Дефолт — только Type-1/verbatim. Дешёвый Type-2 через
  нормализацию идентификаторов — строго опционально, за флагом. *Консистентный*
  Type-2 (проверка, что переменные используются одинаково) — за AST-проходом #052.
- **Per-commit / temporal** GitClear-метрика. Только snapshot. `--changed-files`
  — фильтр отчёта, не сужение корпуса.
- Семантика / IR-уровень.
- **Никакого Python в продукте.** Чистый C++20 single-binary путь.

## Тесты

- [ ] **A / clean:** один файл без дублей → ratio `0`, `0` блоков.
- [ ] **B / cross-file:** два файла с общим блоком `>= 5` строк (стиль
      daily/hourly-sink) → найден один кросс-компонентный блок, длина верна, блок
      **слит в максимальный** (не N сдвинутых окон).
- [ ] **C / trivial-only:** совпадают только скобки и keyword-scaffolding →
      `0` блоков (ignore-маска работает).
- [ ] **D / vendored:** дубли в `bundled/` исключаются по умолчанию.
- [ ] **Regression / loose:** `fmt/include` ratio ~`2%` и `0` cross-file;
      `spdlog/include` находит блок `daily` ↔ `hourly`. Ассертить **наличие**
      известного блока, НЕ точный % (см. P0-C).

### Тесты режима нормализации (recall vs precision)

- [ ] **E / recall:** две копии функции, переменные переименованы консистентно
      (`acc`/`i` → `result`/`k`). В `verbatim` — НЕ найдено (ожидаемо). В
      `--normalize-identifiers` — найдено как кросс-компонентный блок.
- [ ] **F / precision (главный):** две функции одинаковой структуры, но разного
      смысла, различающиеся только идентификаторами. Короче поднятого порога →
      в `aggressive` **НЕ репортятся** (порог защищает). Accepted-FP, давится
      порогом, не доезжает до пользователя.
- [ ] **G / операторы спасают:** `add($,$){return $ + $;}` vs
      `sub($,$){return $ - $;}` после нормализации НЕ схлопываются (оператор
      сохранён).

## Критерий приёмки

- [ ] Чистый C++20, без libclang, без `compile_commands.json`, без Python.
- [ ] Слияние seeds в максимальные блоки реализовано; нет инфляции сдвинутых окон.
- [ ] Репортятся именно кросс-компонентные блоки как missing reuse edge.
- [ ] Duplication ratio (total + cross-component, P0-B) в отчёте и в json/sarif.
- [ ] Vendored/generated исключены по умолчанию; вложенные git-репо авто-исключены
      (P0-A); `// archcheck: allow` уважается.
- [ ] Проход за флагом, off by default.
- [ ] Один `IRule`, переиспользует существующий plumbing там, где он есть (OCP/DRY).
- [ ] Регрессионные тесты ассертят наличие блоков, не точный ratio (P0-C).
- [ ] Тесты B/C/D зелёные; свободная регрессия на `fmt`/`spdlog`.
- [ ] Режим `--normalize-identifiers`: off by default; нормализует только
      идентификаторы (не операторы/ключевые слова/литералы); поднятые пороги +
      cross-component-only; тесты E/F/G зелёные.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Fast-pass только Type-1 в дефолте | дешёвый zero-setup слой без libclang |
| Skip blanks/comment-only вместо blanking | разные пустые строки и комментарии не должны рвать матч |
| Слияние по file-pair + diagonal + overlap | без этого один регион распадается на множество сдвинутых окон |
| Duplication ratio важнее raw-count блоков | число блоков нестабильно и зависит от merge-эвристики |
| Регрессия на *наличие* блоков, не на % | ratio exclude-зависим (spdlog 4.5% с `bundled` vs ~8% без) — P0-C |
| Две метрики: total + cross-component | гейтить и публиковать надо cross-component (P0-B) |
| Авто-исключать вложенные git-репо | одним правилом закрывает sandbox/snapshot/vendored шум (P0-A) |
| Флаг `--duplication=line` (umbrella) | оставляет место под `--duplication=ast` (#052) под одним флагом. Решено 2026-05-30 |
| Off by default | нужен этап настройки FP до возможного промоушена в fast-defaults |
| Нормализация: precision > recall, off by default | один громкий FP ломает доверие к гейту (асимметрия, риск №3) |
| Vendored excludes по умолчанию | иначе шум на `bundled/generated/third_party` ломает ценность сигнала |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `experiments/line_duplication/*` | ✅ standalone спайк + PERF/SWEEP отчёты (коммит `35085ca`) |
| `src/scan/line_dup_scanner.h/.cpp` | fast line-dup scanner |
| `src/rules/duplication/...` | одно правило поверх fast scanner |
| `src/report/text_reporter.cpp` | text output для duplication ratio и блоков |
| `src/report/json_reporter.cpp` | json output |
| `src/report/sarif_reporter.cpp` | sarif output |
| `src/main.cpp` / CLI plumbing | `--duplication=line`, `--normalize-identifiers`, `--changed-files`, exclude-аудит |
| config schema/docs | `defaults.line_duplication`, `strictness`, thresholds, excludes |
| `fixtures/line_duplication/pass/` | clean / trivial / vendored pass-cases |
| `fixtures/line_duplication/fail_cross_component/` | реальный cross-component clone-case |
| `tests/...` | unit/integration coverage (A–G) |
| `PERF-fast.md` | time + peak RSS на референс-деревьях |

## Fixtures

- [ ] `fixtures/line_duplication/pass/no_duplicates_single_file/`
- [ ] `fixtures/line_duplication/pass/trivial_windows_only/`
- [ ] `fixtures/line_duplication/pass/bundled_excluded/`
- [ ] `fixtures/line_duplication/fail_cross_component/`
- [ ] `fixtures/line_duplication/normalize/recall_renamed/` (тест E)
- [ ] `fixtures/line_duplication/normalize/precision_different_meaning/` (тест F)
- [ ] `fixtures/line_duplication/normalize/operators_differ/` (тест G)

## Pointers

- **Референс-алгоритм:** `experiments/line_duplication/main.cpp` (проверенный
  спайк, ранее `copypaste.py`). Портировать логику 1:1: хеш → быстрый non-crypto,
  Python set/dict → `unordered_map`/`vector`.
- **Соседняя задача:** #052 (`cross_tu_duplication_detector`) — AST-слой, точный,
  дорогой. Делить маппинг в компоненты + репортинг, если helper уже есть.
- **Уроки спайка:** merge в максимальные блоки обязателен; **наличие характерных
  блоков** — надёжный регрессионный якорь; и raw block count, и ratio хрупкие
  (count раздувают сдвинутые окна; ratio зависит от набора excludes — P0-C);
  без project-local excludes отчёт тонет (P0-A).

## В работе

- (пусто — P0-гейт не начат)

## Следующие шаги

1. **P0-A** — авто-детект excludes (вложенные git-репо + `.gitignore` +
   snapshot-паттерны) и аудит-вывод исключённого. Итерировать в спайке.
2. **P0-B** — раздельные `total` / `cross_component` ratio; `--cross-only`
   влияет на расчёт, не только на вывод.
3. **P0-C** — переписать регрессионные тесты на assert-on-presence; зафиксировать
   spdlog с/без `bundled`.
4. После закрытия P0 — порт в `src/scan/line_dup_scanner.{h,cpp}` и `IRule`.
