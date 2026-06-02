# [RULES][SCAN] Coincidental-clone / idiom-FP suppression for the partial-dup pass

**Дата создания:** 2026-05-31
**Дата старта:** —
**Статус:** new
**Модуль:** RULES / SCAN
**Приоритет:** major
**Сложность:** L (precision-слой: эвристика + информ-вес + LLM/семантический confirm)
**Целевой релиз:** v0.2 / v0.3
**Блокирует:** доверие к выводу partial-dup прохода (без этого explorer тонет в idiom-FP)
**Заблокирован:** #056 (fast_backend_partial_duplication_pass — нужен сам проход и его кандидаты)
**Related:** #056 (родитель — recall-слой), #053 (line-based Type-1), #052 (AST cross-TU), #033 (ai_drift_dataset — точность дубль-сигнала)

## Цель

Отделить **настоящие** partial-дубли от **coincidental / idiom-FP** — фрагментов,
совпадающих по форме, но не по смыслу: dispatch-`switch`/`case`/`return`,
enum→значение таблицы, data-массивы, boilerplate-каркасы. Это **precision-слой**
поверх дешёвого token-recall из #056.

## Контекст (как нашли, 2026-05-31)

При ручной проверке вывода спайка #056 на реальном дереве (`datavis-mw`)
**глазами за секунды** нашёлся ложный дубль:
`DDSTexture.h:1425` (формат пикселя→размер) ↔ `MantaMaterial.h:360` (controlId→index)
↔ `ModelHeader.cpp:356` (тип→строка) — три `switch(x){case…:return…}` из РАЗНЫХ
доменов, token-LCS 0.84–0.86. После нормализации (`id`/`lit`) у них один и тот же
каркас `switch ( id ) { case id : return lit ; … }` → различать нечего.

Усугубляется решением #056 понизить idf (ради operator-flip TP): без веса по
редкости частый switch-скелет ничем не придушен.

### Почему чисто-формальная эвристика НЕ решает (замер этой сессии)

В спайк добавлен `--min-diversity` (доля уникальных триграмм; скелет → низкая).
Замер diversity:

| фрагмент | diversity | класс |
|---|---|---|
| DDSTexture switch | 0.27 | FP |
| MantaMaterial switch | 0.28 | FP |
| ModelHeader switch | 0.25 | FP |
| Vegetations export | **0.32** | **TP** |
| ProcObjects export | 0.36 | TP |
| TerrainExport | 0.51 | TP |

**Распределения FP и TP перекрываются** (FP 0.25–0.28, но настоящий TP — 0.32).
Чистого порога нет: `--min-diversity 0.35` убивает switch-FP, но **заодно** kills
реальные `ProcObjects/Vegetations`. Зазор 0.28↔0.32 ≈ 0.04 — не робастно.
**Вывод: форма не отделяет idiom-FP от настоящего partial; нужен смысл.**

## Что делают в литературе (обзор, 2026-05-31)

- **TF-IDF / token informativeness** — классический ответ: частые (boilerplate)
  токены весят меньше, редкие — больше. Это ровно idf, который #056 понизил;
  тут он возвращается, но **на правильном слое** (precision-ранжирование, не
  recall-gate).
- **«Detecting Essence Code Clones via Information Theoretic Analysis» (arXiv
  2502.19219, 2025)** — *прямое попадание в идею пользователя*: считают TF-IDF
  «информацию» на токен → агрегируют в **информационный вес строки** → вес строки
  = её доля в суммарной информации; строки/фрагменты с низким информационным
  содержанием (повторяющийся каркас) отбрасывают, similarity считают по
  info-взвешенным строкам (cosine). Т.е. «вес стандартных паттернов высок →
  выкинуть», формализовано.
- **Kapser & Godfrey, «"Cloning Considered Harmful" Considered Harmful» (EMSE
  2008)** — таксономия паттернов клонирования; **до 71% клонов benign**, часть
  даже полезна. `switch`/boilerplate — узнаваемый ожидаемый класс. Подтверждает:
  фильтрация/ранжирование клонов — легитимная и нужная задача, не все «дубли» суть
  долг.
- **SourcererCC (ICSE 2016)** — инвертированный индекс на **низкочастотных
  токенах** + sub-block / token-position filtering. Это наш recall-слой (#056),
  про эффективность, не про idiom-precision.
- **ML-фильтр spurious-клонов** (напр. Improving Clone Detection Precision, IWESEP
  2019) — decision tree на фичах: **repetitive tokens, parameter overlap,
  parameter consistency, fraction of repeated parameters**. Идея пользователя
  («вес повторяющихся паттернов») — это их фича `repetitive tokens`.

## Принятое направление

По возрастанию цены. **Слой 0 (нормализация) — основной рычаг**, остальное добивает.

### 0. Selective normalization — НЕ нормализовать различающие токены (ОСНОВНОЕ, измерено 2026-05-31)

Корень обоих FP-классов — в том, что мы схлопывали в `id` то, что несёт смысл.
Реализовано в спайке (`--keep-calls`, **теперь дефолт**; `--no-keep-calls` для A/B):
схлопывать только локальные имена, а **сохранять**:

- **callee-имена** — идентификатор перед `(` (вызовы API/хелперов: стабильны при
  копировании, локальные переменные — нет);
- **case-метки** — идентификатор сразу после `case` (enum-таблицы разных доменов
  перестают совпадать).

Замер на `datavis-mw` (precise @0.70):

| FP / TP | all-`id` | keep-calls+case |
|---|--:|--:|
| assign/call coincidental (WaterLevels↔EditorFrame) | 0.70 | **пары нет** |
| switch DDSTexture↔MantaMaterial | 0.86 | 0.73 |
| switch DDSTexture↔ModelHeader | 0.85 | 0.70 |
| switch MantaMaterial↔ModelHeader | 0.85 | 0.78 |
| настоящая MaterialSet↔MultiMaterial | высок. | выжила |

Все три switch-FP **ушли под дефолтный гейт precise 0.80** → отсекаются сами, без
порога. Настоящие копии (общие вызовы/метки) — выживают. Это чище и дешевле, чем
form-фильтр. **Цена:** кандидатов больше (матч теперь по общему API/меткам, не по
голому каркасу — осмысленно, но фильтруется порогом); теряем Type-2-копии, где
переименовали и сами вызовы (редко — обычно правят локальные).
*Ревизует normalization-step #056 (`идентификаторы → id`): теперь selective.*

### 1. Информ-вес / entropy пре-фильтр — ЗАПАСНОЙ, не основной

`--min-diversity` (доля уникальных триграмм) и essence-style TF-IDF-вес строки
оставить как опциональный добивающий фильтр для самых скелетных. **Понижен** со
«основного» до «запасного»: замер показал TP/FP-overlap по diversity (FP 0.25–0.28,
настоящий TP 0.32) — чистого порога нет, а selective normalization бьёт точнее.

### 2. Принять остаточный floor

Не гнаться за нулём FP формальными средствами.

### 3. LLM/семантический confirm — финальный precision-слой

Кандидаты от #056 (recall) → агент читает оба фрагмента и ставит вердикт
`REAL-COPY / RENAMED / EDITED-CONST / IDIOM-FP / DATA-TABLE` + строка обоснования.
На масштабе — fan-out. Именно это мгновенно отличило switch-таблицу от export-логики,
чего форма не смогла (но после слоя 0 нагрузка на confirm кратно меньше).

## Управляющий принцип: confirm оправдан ТОЛЬКО действием по итогу (2026-05-31)

LLM-confirm стоит токенов. Его стоимость оправдана **только если вердикт ведёт к
изменению кода** — иначе это дашборд, который игнорируют (а archcheck по спеке
**не дашборд**: CI-gate, exit≠0). Поэтому слой 3 не самоценен; он обязан питать:

- **(a) Gate (#056 P2).** baseline морозит исторические дубли; CI валит PR на
  **новый** подтверждённый дубль. Вот точка, где «по итогам происходит изменение
  кода»: автор PR упирается в гейт и выносит общее → дедуп. Confirm нужен здесь,
  чтобы гейт не валился на idiom-FP (иначе его отключат).
- **(b) Remediation hint.** Не «дубль тут», а «вынеси в общий заголовок/базовый
  класс» (словарь Lakos). #054 подтвердил: дрейф **обратим**, типовой приём —
  вынос общего кода.

Метрика успеха меняется: **не «сколько дублей нашли», а «сколько не выросло /
починили»**. Снимок-репорт без gate/remediation — это и есть «бесполезная
проверка» (явный non-goal).

## Non-goals

- Не пытаться формальной метрикой добиться нулевого idiom-FP (overlap доказан).
- Не Type-4 / семантические клоны как таковые (это #052/AST).
- Не переусложнять recall (#056) — фильтрация это отдельный precision-слой.
- **Не выпускать snapshot-репорт без пути к действию.** Confirm-слой без подключения
  к gate/remediation — анти-цель: проверка, не меняющая код, бесполезна.

## Acceptance criteria

- [x] **Selective normalization реализована и измерена** (спайк `--keep-calls`,
      дефолт): keep callee+case-метки; switch-FP 0.85→0.70–0.78 (<0.80 gate),
      assign/call-FP исчезает, MaterialSet↔MultiMaterial выживает.
- [ ] На наборе из этой сессии: три switch (DDSTexture/MantaMaterial/ModelHeader)
      → IDIOM-FP; `ProcObjects↔Vegetations`, `TerrainExport↔TriTerrain` → REAL.
- [ ] Информ-вес/паттерн-фильтр реализован и измерен (precision/recall vs ручная
      разметка), с честным указанием где режет TP.
- [ ] LLM-confirm-слой описан как контракт (вход = пара фрагментов, выход =
      ярлык+обоснование) и проверен на наборе выше.
- [ ] Зафиксировано: формальная метрика — пре-фильтр, не gate; precision = confirm.
- [ ] **Confirm-вердикт подключён к действию:** новый подтверждённый дубль валит
      CI-gate (#056 P2), idiom-FP — нет; либо выдаётся remediation-hint. Снимок без
      этого не считается готовым (метрика — «не выросло/починили», не «нашли»).

## Сделано

- (пусто — задача-постановка; эмпирика и обзор выше собраны в спайке #056)

## Ключевые решения

| Решение | Причина |
|---------|---------|
| **Selective normalization — основной рычаг** (keep callee-имена + case-метки, дефолт) | бьёт оба FP-класса в корне нормализации, без хрупкого порога; switch-FP уходят под гейт 0.80, assign/call-FP исчезают, настоящие выживают (измерено) |
| Entropy/`--min-diversity` понижен до запасного | TP/FP-overlap по diversity (0.28↔0.32) — чистого порога нет; нормализация точнее |
| Отдельная задача, не часть #056 | #056 = recall; это precision-слой, иной механизм и цена |
| idf/инфо-вес возвращается, но на precision-слое | на recall-gate он ронял operator-flip TP (#056), на ранжировании/фильтре — полезен |
| Чистая форма — только пре-фильтр | замер: FP/TP diversity перекрываются, робастного порога нет |
| LLM-confirm — несущий precision | смысл отделяет idiom-FP от копии, форма нет (подтверждено глазами) |
| Confirm питает gate/remediation, не самоценен | проверка без изменения кода = дашборд = ноль; archcheck — CI-gate, не дашборд |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `backlog/new/059_maj_coincidental_clone_filtering.md` | постановка precision-слоя |
| `experiments/partial_duplication/main.cpp` | спайк: selective normalization `--keep-calls` (дефолт, callee+case) + `--min-diversity` (запасной) |
| `src/scan/partial_duplication_*` / `src/rules/duplication/...` | future: инфо-вес фильтр + confirm-хук |

## Pointers

- arXiv 2502.19219 — Detecting Essence Code Clones via Information Theoretic Analysis (2025).
- Kapser & Godfrey, EMSE 2008 — patterns of cloning (benign clones, ~71%).
- SourcererCC, ICSE 2016 (arXiv 1512.06448) — low-frequency-token index + filtering.
- Improving Clone Detection Precision using ML, IWESEP 2019 — spurious-clone features.
- Эмпирика и эталоны: `experiments/partial_duplication/{SPIKE_REPORT,OSS_SWEEP_REPORT}.md`.
