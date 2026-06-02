# [RULES][SCAN][REPORT] Clone classification + explanation (LD.10 + LD.11)

**Дата создания:** 2026-06-02
**Дата старта:** 2026-06-02
**Статус:** wip
**Модуль:** RULES / SCAN / REPORT
**Приоритет:** minor
**Сложность:** S (один классификатор над уже посчитанным матчем + проброс raw-токенов)
**Целевой релиз:** v0.3 (research-спайк)
**Блокирует:** —
**Заблокирован:** —
**Related:** #071 (clone_detection_opportunities — родительский umbrella, отсюда вынуты LD.10+LD.11), #056 (token-проход — единственный детектор), #059 (precision-фильтры), #070 (FP-фиксы чекера)

## Цель

Навесить на каждую найденную пару дубликатов (1) метку характера клона —
`EXACT / RENAMED / LITERAL / MIXED / STRUCTURAL` (LD.10) — и (2) объяснение
совпадения: matched tokens, similarity %, и список переименований/смен литералов,
которые нормализация спрятала (LD.11). Чтобы разработчик сразу видел, опасен клон
или безвреден, и *почему* он вообще совпал — без чтения двух фрагментов глазами.

## Контекст

Вынесено из #071 как самый дешёвый и уже design-blessed пункт: конвейер в
[docs/duplication_architecture.md](../../docs/duplication_architecture.md) прямо
рисует стадию «классификатор характера → VERBATIM/RENAMED/EDITED», но в коде её
ещё нет.

Живёт в спайке [experiments/partial_duplication/main.cpp](../../experiments/partial_duplication/main.cpp)
(единственный живой детектор — токеновый #056). Спайк помечен `NOT product
plumbing` — это исследовательский заход, продуктовая интеграция в бинарь — потом.

**Ключевая находка дизайна:** `Token` хранит только нормализованный `sym`
(идентификаторы → `"id"`, литералы → `"lit"`), а raw-написание выбрасывается на
лексинге. Поэтому EXACT vs RENAMED vs LITERAL из текущих данных не различить —
нужно протащить raw-токен через `Token.raw` → `Fragment.rawSeq` и сравнивать его
в позициях, где нормализованные потоки совпали.

**Алгоритм классификатора:**
- нормализованные `seq` обоих фрагментов различаются (длина или поэлементно)
  → **STRUCTURAL** (Type-3 proper: вставки/удаления/перестановки/флип операторов);
- `seq` идентичны (выровнены 1:1) → смотрим, что разошлось в raw:
  - ничего → **EXACT**;
  - только `id`-позиции → **RENAMED**;
  - только `lit`-позиции → **LITERAL**;
  - и `id`, и `lit` → **MIXED**.

Callee-имена и операторы хранятся в `sym` как есть; если они различаются —
`seq` разойдётся → STRUCTURAL (другой вызов = структурно другой код, defensible).

## План выполнения

- [x] `Token`: добавить поле `raw` (после `line`, чтобы не ломать `{sym, line}` aggregate-init).
- [x] Лексер: заполнять `raw` в ветках `id` и литералов (string/char/number/raw-string).
- [x] `Fragment`: добавить `rawSeq`, заполнять в `makeFragment` параллельно `seq`.
- [x] Функция `cloneType(fa, fb)` — чистая, над `seq`/`rawSeq`.
- [x] Вывести метку в оба репортера (snapshot + diff-mode).
- [x] Фикстура `fixtures_clone_type/` с парами известного типа + сборка + прогон.
- [x] **LD.11:** `DiffOp` несёт исходные индексы `ai/bj` → `explainPair` читает `rawSeq` на `=`-позициях.
- [x] **LD.11:** вывод `explain: matched N tokens, similarity P%` + `ignored identifiers/literals` (precise-режим).

## Сделано

- **Проброс raw-токена.** `Token.raw` ([main.cpp:93](../../experiments/partial_duplication/main.cpp#L93)) —
  заполняется только там, где нормализация теряет написание: `id`-плейсхолдеры и
  литералы (string/char/number/raw-string). Keyword/operator/callee — `raw` пуст,
  `rawSeq` падает обратно на `sym`.
- **`Fragment.rawSeq`** — выровнен 1:1 с `seq`, заполняется в `makeFragment`
  (единственная фабрика фрагментов → OOB исключён).
- **`cloneType(fa, fb)`** — чистая функция: `seq != seq` ⇒ STRUCTURAL; иначе по
  raw-диффу в `id`/`lit`-позициях ⇒ EXACT / RENAMED / LITERAL / MIXED. CCN < 15
  (lizard её не флагает).
- **Метка в выводе** обоих репортеров: `[weighted=1 RENAMED] …`.
- **Верификация на фикстуре** `fixtures_clone_type/sample.cpp` (4 семейства × 2
  функции). Сборка Debug чистая, snapshot-прогон даёт ровно ожидаемое:
  `A1↔A2 EXACT`, `B1↔B2 RENAMED`, `C1↔C2 LITERAL`, `D1↔D2 STRUCTURAL`.
  В `--partial-precise` межсемейные пары (другой callee) корректно идут как
  STRUCTURAL, а diff-вью показывает `~ validateInput -> parseHeader` — зачаток LD.11.
- **LD.11 explanation** ([main.cpp:937](../../experiments/partial_duplication/main.cpp#L937)):
  `DiffOp` расширен исходными индексами `ai/bj` — переиспользуем существующий
  `diffTokens` LCS-backtrack (без дублирования DP), а на `=`-позициях читаем
  `rawSeq` обоих фрагментов. То, что нормализация спрятала (`id↔id`, `lit↔lit`
  с разным написанием), всплывает как `ignored identifiers/literals`. Печать
  вынесена в `printIgnored` (дедуп печати + длина функций под порогом).
  Прогон на фикстуре: `RENAMED` → `head -> alpha; conf -> beta; …`;
  `LITERAL` → `0.5 -> 0.9; 2 -> 3; …`; `STRUCTURAL` → matched/similarity + ignored
  в выровненной области + token-diff с callee-сменами. Ровно spec LD.11.
- **lizard** (`--CCN 15 --length 30 --arguments 5`): новых варнингов нет;
  `cloneType` / `explainPair` / `printIgnored` под порогами. Преэкзистентный долг
  `lex`/`main`/`diffTokens` не трогал (`diffTokens` нудж +2 NLOC на проброс индексов).

## В работе

- Ничего. LD.10 + LD.11 реализованы и проверены на фикстуре в спайке.

## Следующие шаги

1. (по запросу) закрыть задачу: `git mv wip → completed`, дописать секции «Как работает / Диагностика».
2. (потом, отдельно) при продуктовой интеграции #056 в бинарь — перенести классификатор+explain и завести настоящую `fixtures/clone_type/pass|fail_*`.
3. Дальше по #071: LD.14 (Clone Growth, садится на baseline/drift) или LD.16 (cross-module matrix, самый Lakos-родной). explain сейчас под `--partial-precise`; при интеграции — решить, нужен ли отдельный `--explain`.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Проброс `raw` через `Token`/`Fragment`, не пере-лексинг | EXACT/RENAMED/LITERAL без raw неразличимы; пере-лексить пару дорого и дублирует логику |
| Поле `raw` после `line` в `Token` | Сохраняет существующие `{sym, line}` aggregate-инициализаторы (их много) |
| `seq != seq` ⇒ STRUCTURAL до сравнения raw | Нормализованное расхождение = структурная правка по определению; raw сравнивать только при 1:1-выравнивании |
| Делать в спайке, не в бинаре | #056 ещё не в продуктовом дереве; интеграция — отдельный шаг |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `experiments/partial_duplication/main.cpp` | LD.10: `Token.raw`, `Fragment.rawSeq`, `cloneType()`, метка. LD.11: `DiffOp.ai/bj`, `explainPair()`, `printIgnored()`, explain-блок в precise-репортере |
| `experiments/partial_duplication/fixtures_clone_type/` | фикстура с парами известного типа (4 семейства × 2) |

## Fixtures (если правило)

- [ ] `fixtures_clone_type/` — пары EXACT / RENAMED / LITERAL / STRUCTURAL в одном файле, сверка меток в выводе спайка
