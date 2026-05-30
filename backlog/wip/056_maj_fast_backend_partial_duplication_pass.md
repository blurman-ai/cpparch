# [RULES][SCAN] Fast-backend partial duplication pass (Type-3 / diverged copies)

**Дата создания:** 2026-05-30
**Дата старта:** 2026-05-30
**Статус:** wip
**Модуль:** RULES / SCAN
**Приоритет:** major
**Сложность:** L
**Целевой релиз:** v0.2
**Блокирует:** —
**Заблокирован:** #053 (fast_backend_line_duplication_pass — общий duplication plumbing, excludes и режимы лучше стабилизировать там)
**Related:** #006 (spec_refactor — fast backend как отдельный слой), #033 (ai_drift_dataset — duplication как AI-drift сигнал), #052 (cross_tu_duplication_detector — AST-слой), #053 (fast_backend_line_duplication_pass — точный Type-1 проход), #054 (ai_repo_duplication_run — будущий потребитель сигнала)

## Цель

Добавить в fast backend отдельный parser-free проход для **partial / diverged duplication** (Type-3):
ловить разошедшиеся копии на уровне токенов, которые пропускает точный line-based
блок-матч из #053.

## Контекст

`#053` закрывает verbatim / Type-1 слой: длинный точный блок, быстрый снимок,
headline через конкретные находки. Но главный болезненный класс копипасты другой:
код скопировали, потом в обеих копиях начали править по месту. Изменился
оператор, добавился токен, появилась вставка строки, а порядок действий
остался тем же.

Это ломает line-granularity подход. Если в каждой строке есть маленькая
не-нормализуемая правка, LCS по строкам видит почти ноль совпадений, хотя
фрагменты по сути остаются одной и той же процедурой. Значит, единица сравнения
должна быть **токен**, а не строка.

Нужен отдельный проход, а не усложнение `#053`:

- `#053` остаётся быстрым и точным Type-1 слоем.
- Эта задача добавляет дешёвый Type-3 слой без `compile_commands.json` и без AST.
- `#052` остаётся точным AST-слоем для более дорогого cross-TU анализа.

Позиционирование то же: дублирование между компонентами — это **missing reuse
edge** в смысле Lakos physical design. AI-нарратив допустим только в
commit/baseline-режиме, не в snapshot.

## Принятое решение

> **Пересмотрено по итогам спайка #056 (2026-05-30).** Исходная гипотеза —
> *weighted bag-of-tokens как единственный дефолт, token-LCS опционально* —
> **опровергнута**. См. `## Результаты спайка #056` ниже и
> `experiments/partial_duplication/SPIKE_REPORT.md`.

### Две фазы: bag (recall) → order-sensitive confirm (precision)

- **Фаза 1 — recall (bag).** Кандидаты через инвертированный индекс на
  низкочастотных токенах + bag-of-tokens overlap. Дёшево, линейно, масштабируется
  без квадрата по парам, не требует libclang и compile DB. Цель — **полнота**, не
  точность: bag намеренно пропускает порядок.
- **Фаза 2 — confirm (order-sensitive), ОБЯЗАТЕЛЬНАЯ.** token-LCS по всем
  кандидатам recall-фазы. Это не «re-rank верхушки» и не opt-in — порядок токенов
  и есть та ось, которая отделяет «та же процедура, флипнут оператор» (всё ещё
  копия) от «другая процедура, те же идиомы» (не копия). Bag эту ось выбрасывает,
  поэтому precision держится здесь.
- **Гранулярность:** сегменты функционального масштаба, не целые файлы.

**Обоснование (фикстуры спайка):**

| метрика | A (TP, `+→-`) | D (FP, switch) | вердикт |
|---------|--------------:|---------------:|---------|
| plain bag | **0.84** | 0.60 | разделяет |
| weighted (idf) bag | 0.36 | **0.45** | **инвертирует**: FP выше TP |

### idf-веса — опциональны и экспериментальны, НЕ защита от FP

- Раньше idf был дефолтным score и подавался как защита от FP. Спайк показал
  обратное: на headline-TP (оператор флипнут в каждой строке) расходятся именно
  редкие токены, idf даёт им максимальный вес → **штрафует за то самое отличие,
  которое для нас «всё ещё копия»**, и инвертирует ранжирование (FP 0.45 > TP 0.36).
- Поэтому recall-bag по умолчанию **plain (невзвешенный или слабо-взвешенный)**.
  idf остаётся как `--experimental-idf` с предупреждением и цифрами выше; защита
  precision переехала на порядок (фаза 2), не на веса.

### Числовые data-блоки (embedded байт-код / таблицы) — сюда (из #054)

При прогоне #054 (`experiments/ai_repo_run`) на спайке #053 всплыл класс шума:
**числовые data-массивы** вида `const BYTE g_main[] = { 69, 70, 88, 4, 0, ... }`
(вывод `fxc`/HLSL-компилятора в Effekseer ShaderHeader DX-бэкендах). Это не код,
а данные; они дают огромные ложные cross-file блоки (196 строк чисел).

В #053 решено НЕ ловить (там line-based, числа неотличимы от кода без токенизации;
`#if 0`-обёртку #053 уже пропускает, но сами массивы — нет). По решению
пользователя (2026-05-30) детект числовых/data-блоков — задача этого прохода:
на уровне токенов «строка/блок, состоящий почти целиком из числовых литералов
и пунктуации» детектируется тривиально и должна исключаться (или не считаться
значимым кодом). Учесть при реализации P1 (token overlap pass).

### Что сознательно НЕ делаем

- не используем символьный Левенштейн как основную метрику;
- не уходим в AST / CFG / PDG;
- не схлопываем это в `#053`;
- не пытаемся ловить Type-4 / семантические клоны (`for` ↔ `while`, другой алгоритм);
- не гонимся за нулевым FP: **idiom-FP** (разные процедуры с общей идиомой —
  напр. два `string_view`-цикла) порядок уменьшит, но не уберёт; принимаем как
  остаточный floor precision.

## Алгоритм v1 (ревизия после спайка)

1. Разбить файлы на фрагменты функционального масштаба без парсера:
   эвристика по балансу `{...}` (в спайке — `)`→`{` как маркер тела функции/блока).
2. Лексить фрагменты и нормализовать токены:
   идентификаторы → `id`, литералы → `lit`, но ключевые слова, типы, операторы
   и скобки сохранять.
3. Построить inverted index по низкочастотным токенам. **`rare_df` —
   относительный** (перцентиль df корпуса, ≈10–15% от N), не абсолютная
   константа: на спайке абсолютный `df≤4` на 189 фрагментах отрезал ВСЕ 17766 пар.
   Желателен fallback на k-gram fingerprints, чтобы recall не терял копии, у
   которых разошлись как раз распознавательные токены.
4. Сгенерировать кандидаты по пересечению редких токенов (recall-фаза).
5. Посчитать **plain bag overlap**, отфильтровать по `similarity_threshold` и
   `min_tokens`. (idf — только под `--experimental-idf`.)
6. **Обязательно** прогнать ВСЕХ кандидатов через token-LCS (order-sensitive
   confirm) — это precision-фаза, не опция.
7. Репортить пары `file:line ↔ file:line`, similarity, размер и diff-вид
   расхождений (`changed`, смежные delete+insert схлопывать).

## План выполнения

### P0. Contract и shared plumbing

- [ ] Зафиксировать, что это **отдельный проход**, не правка line-based логики #053.
- [ ] Переиспользовать из #053 discovery corpus, excludes, baseline-модель и
      общий duplication CLI umbrella (`--duplication=...`), не копировать это заново.
- [ ] Уточнить naming/config contract:
      `--duplication=partial`, `--partial-precise`,
      `.archcheck.yml` → `thresholds.partial_duplication.*`.

### P1. Token overlap pass

- [x] Добавить scanner / matcher для tokenized fragments без libclang. *(спайк
      `experiments/partial_duplication/main.cpp`)*
- [x] Реализовать нормализацию токенов:
      имена/литералы схлопываются, операторы и keywords сохраняются.
- [x] Реализовать weighted overlap по редкости токенов. *(idf = log(N/df))*
- [x] Реализовать inverted index на low-frequency tokens. *(но `rare_df` обязан
      быть относительным — см. находку ниже)*
- [x] Ввести пороги `similarity_threshold` и `min_tokens`.

### P2. Explorer / gate semantics

- [ ] **Off by default.**
- [ ] Snapshot-режим = explorer: ранжирует горячие точки, не гейтит.
- [ ] Commit / baseline-режим = гейт: baseline морозит исторические partial-дубли,
      CI валится только на новые.
- [ ] Вывод text/json/sarif в словаре текущих duplication-репортов.

### P3. Order-sensitive confirm (ОБЯЗАТЕЛЬНАЯ фаза, не opt-in)

> Переоценено спайком: token-LCS — несущая для precision, а не «опциональный
> re-rank верхушки». Bag даёт recall, порядок даёт precision.

- [x] token-LCS по кандидатам recall-фазы. *(спайк: `--partial-precise`,
      Dice-ratio `2·LCS/(|a|+|b|)`, candidate-net расширен — LCS даёт precision)*
- [x] Показывать alignment / diff-вид и место расхождения. *(diff с тегами
      `~`/`-`/`+`; кейс A → `6 changed: + -> -`, ровно расхождение)*
- [x] Смежные delete+insert схлопывать в `changed`. *(реализовано в `diffTokens`)*
- [ ] `--partial-precise` оставить как флаг детальности diff-вывода, но сам
      confirm идёт всегда. *(в спайке `--partial-precise` пока режим-тумблер;
      «confirm всегда» — продуктовое решение при интеграции)*
- **Находка спайка (НЕ переоткрывать):** token-LCS живёт на ДРУГОЙ шкале, чем
      bag — у нормализованного алфавита (`id`/`lit`/keywords/ops) высокий floor:
      любые два C++-тела дают LCS ~0.7 (C=0.734, D=0.736). При bag-пороге 0.60 LCS
      репортит 19 пар (FP C/D протекают). Граница TP/FP ≈ **0.80** → precise-режим
      дефолтит `--threshold` в 0.80 и даёт **ровно 3 истинные пары** (A 0.915,
      B 0.888, copy↔copy 0.812), C/D/шум отсечены. **LCS несущий для precision —
      но со своим калиброванным гейтом (bag ~0.6, LCS ~0.8), не drop-in.**

### P4. Fixtures и регрессия

- [x] Добавить отдельные fixtures для partial duplication. *(спайк-уровень:
      `experiments/partial_duplication/cases/` A–E; продуктовые fixtures под
      `fixtures/partial_duplication/` — при интеграции в `src/`)*
- [x] Покрыть contrast-case против line-based подхода:
      line-LCS бы промолчал, token overlap обязан найти. *(A и B: line-overlap
      ≈0.11, token plain ≈0.80–0.84)*
- [x] Зафиксировать границы: semantic rewrite не ловится и не считается багом.
      *(Type-4 явно вне scope; ограничения сегментации описаны в SPIKE_REPORT)*

## Acceptance criteria

- [ ] Новый проход не меняет алгоритм `#053`; это отдельный слой.
- [ ] C++20, без libclang, без `compile_commands.json`, без Python в runtime.
- [ ] Гранулярность сравнения — **токены**, не строки.
- [ ] Дефолт — **plain bag-of-tokens overlap (recall) + обязательный
      order-sensitive token-LCS confirm (precision)** (weighted-bag-как-дефолт
      опровергнут спайком: инвертирует ранжирование headline-TP).
- [ ] idf-веса — только под `--experimental-idf`, с предупреждением; не дефолт.
- [ ] Кандидаты строятся через inverted index на low-frequency tokens с
      **относительным** `rare_df` (перцентиль df, не абсолютная константа).
- [ ] Есть `similarity_threshold` и `min_tokens` в config/CLI.
- [ ] token-LCS confirm идёт **всегда**; `--partial-precise` управляет лишь
      детальностью diff-вида.
- [ ] Символьный Левенштейн не используется.
- [ ] Проход **off by default**.
- [ ] Snapshot = explorer, commit/baseline = gate.
- [ ] Text/JSON/SARIF умеют выводить пары фрагментов и similarity.
- [ ] Тесты на основные кейсы A–F и H зелёные; G фиксирует границу, а не баг.
- [ ] Ограничения сегментации и границы Type-4 явно описаны в доке прохода.

## Тестовые кейсы

- [ ] **A. Оператор в каждой строке:** копия из ~10 строк, в каждой `+`→`-`;
      token overlap должен дать высокий similarity, line-based матч — нет.
- [ ] **B. Вставка/удаление:** копия с вставленными строками всё ещё ловится.
- [ ] **C. Похожая форма без копии:** два несвязанных фрагмента ниже порога.
- [ ] **D. Большой switch FP:** два разных больших `switch` не репортятся —
      **благодаря order-sensitive confirm** (порядок/диспетчеризация различны),
      НЕ благодаря idf-весам (спайк: idf тут даёт 0.45 и роняет ещё и TP).
- [ ] **E. Короткий guard:** фрагмент короче `min_tokens` не рассматривается.
- [ ] **F. Precise re-rank:** diff-вид показывает изменённые токены.
- [ ] **G. Семантический рерайт:** `for` vs `while` не ловится, это documented boundary.
- [ ] **H. Baseline:** исторический partial-дубль заморожен, новый репортится.

## Сделано

- **Минимальный алгоритмический spike закрыт** (2026-05-30, «шаг 3»):
  `experiments/partial_duplication/` — standalone C++20, без libclang и
  `compile_commands.json`, **не в основном билде**, без продуктового plumbing.
  - `main.cpp`: lex+нормализация (`id`/`lit`, keywords/операторы verbatim) →
    parser-free сегментация по `)`→`{` → idf-веса → inverted index по редким
    токенам → weighted + plain Jaccard + иллюстративный line-overlap.
  - `cases/`: контраст-фикстуры A–E + filler-корпус для реалистичного idf.
  - Прогнан на фикстурах **и на реальном `src/`** (25 файлов, ~5000 LOC,
    189 фрагментов; <0.01 с, 4.5 МБ RSS).
  - Полный разбор: `experiments/partial_duplication/SPIKE_REPORT.md`.
- **Тезис Type-3 подтверждён:** A (`+→-` в каждой строке) и B (insert+rename)
  имеют line-overlap ≈0.11 (line-based слеп), но token-plain ≈0.80–0.84.
- **E подтвердил `min_tokens`:** короткие near-identical guards вообще не
  становятся фрагментами.
- **P3 order-sensitive confirm реализован в спайке** (`--partial-precise`):
  упорядоченный поток токенов → token-LCS (Dice-ratio) → diff-вид с тегами
  `~`/`-`/`+`, смежные del+ins схлопнуты в `changed`. Кейс A, который idf
  инвертировал, возвращается на **верх** (lcs=0.915, diff = ровно `6× + -> -`).
  При LCS-гейте 0.80 precise даёт **ровно 3 истинные пары** (A 0.915 / B 0.888 /
  copy↔copy 0.812), C (0.734) / D (0.736) / шум отсечены. **Тесты A/C/D/F на
  спайке зелёные.** Разбор — SPIKE_REPORT §«P3».

## Результаты спайка #056

**Статус гипотезы:** *weighted bag-of-tokens как дефолт* — **опровергнут**.
metric-contract пересмотрен в этом документе (разделы «Принятое решение»,
«Алгоритм v1», acceptance criteria) **до** интеграции в `src/`. Полный разбор и
цифры — `experiments/partial_duplication/SPIKE_REPORT.md`.

Обоснование (фикстуры спайка):

| метрика | A (TP, `+→-`) | B (TP, insert+rename) | C (FP, форма) | D (FP, switch) |
|---------|--------------:|----------------------:|--------------:|---------------:|
| plain | **0.84** | **0.80** | 0.62 | 0.60 |
| weighted (idf) | 0.36 | 0.78 | 0.38 | **0.45** |

### Выводы

1. **Дефолт был неверный — idf инвертирует ранжирование.** Взвешивание по
   редкости не просто душит TP сильнее FP, оно ставит FP (D=0.45) **выше** TP
   (A=0.36). Причина железная: в «та же процедура, флипнут оператор» расходятся
   именно редкие токены, idf даёт им максимальный вес — штрафует ровно за то
   отличие, которое для нас «всё ещё копия».
2. **Проблема глубже весов — в самом мешке.** Bag любой природы не отличает «та
   же процедура, флипнут оператор» от «другая процедура, те же идиомы»:
   различающий сигнал — **порядок токенов**, а bag его выбрасывает. Веса — попытка
   спасти подход, у которого выкинута нужная ось.
3. **Порядок — несущая для precision, не опция.** token-LCS перестаёт быть
   «опциональным re-rank верхушки» и становится **обязательной второй фазой по
   всем кандидатам**. Конструкция: bag = быстрый recall (кандидаты) →
   order-sensitive confirm = precision.
4. **`rare_df` обязан быть относительным.** Абсолютный `df≤4` на 189 фрагментах
   отрезал ВСЕ 17766 пар. Порог редкости — доля корпуса (перцентиль df), не
   константа; желателен fallback на k-gram fingerprints для recall.
5. **Idiom-FP — остаточный floor.** `consumeDiffModeFlag` ↔
   `next_significant_line` (оба `string_view`-циклы) — разные процедуры с общей
   идиомой. Порядок его уменьшит, но не уберёт. Принять как floor precision, не
   гнаться за нулём.
6. **LCS — со своим гейтом, не drop-in (P3-находка).** token-LCS живёт на ДРУГОЙ
   шкале, чем bag: у крошечного нормализованного алфавита высокий floor — любые
   два C++-тела дают LCS ~0.7 (C=0.734, D=0.736). При bag-пороге 0.60 precise
   репортит 19 пар (FP протекают); граница TP/FP ≈ **0.80**. Поэтому два разных
   калиброванных гейта: bag ~0.6 (recall), LCS ~0.8 (precision). «Несущий» —
   да, «бесплатный/drop-in на bag-пороге» — нет.

## В работе

- (спайк завершён; интеграция в `src/` не начата — ждёт #053-plumbing)

## Следующие шаги

1. Довести `#053` до стабильного product-contract по corpus/excludes/reporting
   (его P0-A/B/C + перенос в `src/scan`).
2. ~~Пересмотреть metric-contract~~ — **сделано в этом документе** (bag-recall +
   обязательный order-sensitive confirm; idf понижен до `--experimental-idf`).
   Осталось: реализовать token-LCS confirm-фазу (P3) при переносе в `src/`.
3. Сделать `rare_df` относительным (перцентиль df); продумать
   fingerprint-fallback для recall.
4. Когда #053-plumbing стабилен — выделить, что реально переиспользуется
   (corpus/excludes/baseline/CLI umbrella), и переносить spike-алгоритм в `src/`
   с продуктовыми fixtures под `fixtures/partial_duplication/`.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Отдельный проход, а не усложнение `#053` | line-based и token-based слои решают разные классы дублей и имеют разную цену |
| Bag = recall-фаза (кандидаты), не финальный score | bag дёшев и масштабируется индексом, но выбрасывает порядок токенов |
| Order-sensitive token-LCS confirm — обязательная precision-фаза *(пересмотрено #056)* | bag не отличает «флипнут оператор» от «другая логика, та же идиома»; различает только порядок |
| Дефолтный score — plain, idf лишь `--experimental-idf` *(пересмотрено #056)* | idf инвертирует ранжирование headline-TP (FP 0.45 > TP 0.36) — см. «Результаты спайка» |
| `rare_df` относительный (перцентиль df) *(пересмотрено #056)* | абсолютная константа отрезает все кандидаты при росте корпуса (0 из 17766 на 189 фрагментах) |
| Snapshot advisory, commit/baseline gate | не надо притворяться, что explorer умеет угадывать интент дубля |
| Type-4 явно вне scope; idiom-FP — остаточный floor | семантические клоны требуют AST/CFG; общая идиома (`string_view`-цикл) даёт FP, который порядок снижает, но не убирает |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `backlog/wip/056_maj_fast_backend_partial_duplication_pass.md` | постановка + результаты спайка, пересмотр metric-contract |
| `experiments/partial_duplication/main.cpp` | **спайк**: token-overlap detector (standalone C++20) |
| `experiments/partial_duplication/cases/*.cpp` | **спайк**: контраст-фикстуры A–E + filler |
| `experiments/partial_duplication/{SPIKE_REPORT,README}.md`, `CMakeLists.txt` | **спайк**: отчёт, инструкция, билд |
| `src/scan/partial_duplication_*` | будущий token-based scanner / matcher |
| `src/rules/duplication/...` | future rule plumbing и регистрация |
| `src/main.cpp` / config loader / reporters | future CLI, config и output plumbing |
| `fixtures/partial_duplication/...` | future pass/fail fixtures |

## Fixtures

- [ ] `fixtures/partial_duplication/pass/`
- [ ] `fixtures/partial_duplication/fail_operator_per_line/`
- [ ] `fixtures/partial_duplication/fail_insert_delete/`
- [ ] `fixtures/partial_duplication/fail_cross_component/`
- [ ] `fixtures/partial_duplication/pass_semantic_rewrite_boundary/`
