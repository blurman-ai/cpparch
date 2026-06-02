# [SCAN] Полный порт #056 duplication-детектора в archcheck (+ FP-гардлы из #070)

**Дата создания:** 2026-06-02
**Дата старта:** —
**Статус:** new
**Модуль:** SCAN / scan/duplication
**Приоритет:** major
**Related:** #056 (спайк), #059 (precision-слой), #060 (харденинг/валидация), #070 (602 FP-фикса), #071 (clone-detection фичи в спайке), #067 (верификация)

## Цель

Влить standalone-спайк `experiments/partial_duplication/main.cpp` (~1883 стр, full 1:1)
в `src/` archcheck с правильной архитектурой (one class = one file, фикстуры обязательны),
добавить FP-гардлы из #070, юнит-тесты, и **перезапустить на корпусе** → новая precision
против baseline (round1 32% / round2 36%, обе устарели).

## Контекст

Сейчас в `src/` дубликат-детекции НЕТ вообще. Весь детектор — в спайке:
selective-normalization лексер + parser-free фрагментация (`)`→`{`) + rare-token inverted
index + bag-overlap (gate 0.60) + **обязательный** token-LCS confirm (gate 0.80) +
diff-классификатор (VERBATIM/RENAMED/EDITED-CONST/EDITED-LOGIC/EXPANDED/SHRUNK).
Плюс #071 дописал clone-density baseline (LD.14) и cross-module matrix (LD.16).

**ВАЖНО:** спайк активно меняется #071 — порт начинать ТОЛЬКО когда #071 устаканится,
иначе портируем движущуюся мишень. Перед стартом: `git log experiments/partial_duplication/`.

## Разбивка на модули src/scan/duplication/ (план)

| Файл | Из спайка | Содержание |
|------|-----------|-----------|
| `token_normalizer.{h,cpp}` | lexer + NormalizationLogic (стр ~75-434) | лексер, selective-norm (keepCalls, case-labels), raw-string→lit, dead-`#if 0` skip |
| `fragmenter.{h,cpp}` | фрагментация `)`→`{` | разбивка на function-scale фрагменты (minTokens 30 / maxTokens 400) |
| `clone_index.{h,cpp}` | inverted index (стр ~600-750) | rare-token index, relative rareDfPct, minSharedRare |
| `similarity.{h,cpp}` | bag overlap + LCS | weighted/plain Ruzicka (gate 0.60) + token-LCS Dice confirm (gate 0.80), trigramDiversity |
| `clone_classifier.{h,cpp}` | diff-классификатор (стр ~104-228) | VERBATIM/RENAMED/EDITED-*/EXPANDED/SHRUNK |
| `duplication_scanner.{h,cpp}` | main orchestration | конвейер: scan→fragment→index→pair→score→classify |
| `report/` reporter | вывод пар | text + json (file:line ⟵ file:line + метрики + класс) |

CLI: `--duplication <path>` (umbrella), флаги порогов через config (`thresholds:`).
CMake: новая lib `archcheck_duplication`, подключить к бинарю и тестам.

## FP-гардлы из #070 (Стадия 2) — свести 602 предложения в ~5 механизмов

| Механизм | Бьёт классы FP | Источник |
|----------|----------------|----------|
| **idiom stop-list** (logging-макросы LL_*/ostream-цепочки, Qt-scaffolding new Q*Layout/connect, LLView event-handler) | idiom (297) | #070 idiom |
| **AND-gating по contiguous-run** (требовать ≥N подряд совпавших токенов, не только bag) | coincidental (100), idiom | #070 coincidental |
| **file-local IDF** (штраф токенам, повторяющимся >K раз в файле) | idiom, coincidental | #070 |
| **header-skeleton suppression** (>70% токенов = объявления/access-specifier/Q_* макросы → down-weight) | header-impl, whole-file (76) | #070 whole-file/header-impl |
| **data-table / enum-ladder detect** (`case CONST:` / `else if(id==LIT)` ладдеры → low-entropy) | data-table (45) | #070 data-table |
| **move/rename + generated** (deleted-BASE-path suppress; генерёнка по marker) | other, generated (19) | #070 |

Фикстуры брать из **реальных размеченных FP/TP корпуса** (`VERIFICATION_REPORT*.md`):
напр. подтверждённый idiom-блок, обманувший детектор → `fixtures/duplication/fail_idiom/`.

## Критерий приёмки

- [ ] Детектор в `src/`, билд зелёный, archcheck проходит свой dogfood (#056 на самом себе).
- [ ] Фикстуры pass/ + fail_* на каждый гардл (из реальных FP корпуса).
- [ ] Юнит-тесты (Catch2) на normalizer/similarity/classifier/каждый гардл.
- [ ] Перезапуск на корпусе (376+ реп) → precision измерена, сравнена с 36% baseline.
- [ ] Стейджинг: коммит на модуль, ≤ разумного размера, билд зелёный на каждом.

## Стартовать когда
1. #071 устаканился (спайк стабилен).
2. Свежий контекст (порт 1883 стр требует бюджета).
