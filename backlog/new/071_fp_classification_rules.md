# #071: Правила FP Classification для дупликатов

**Дата создания:** 2026-06-03  
**Статус:** wip  
**Дата старта:** 2026-06-03  
**Приоритет:** high  

---

## Цель

Систематизировать и зафиксировать **объективные правила** различия между:
- **TP (True Positive)** — реальный copy-paste, нужен рефактор
- **FP (False Positive)** — легитимный паттерн, гарды должны его отфильтровать

Цель: в следующих сессиях при анализе дубликатов сразу применять правила и не выдумывать FP там, где их нет.

---

## Сделано

- [x] **P0.2 whole-file гард** — файл-агрегация (≥80% фрагментов меньшего файла совпали → whole-file clone), считается в `ScanResult.wholeFileClones`, пары убираются. Тесты зелёные.
- [x] **P0.9 generated гард** — `isGeneratedPath` (`.pb.cc`, `moc_`, `ui_`, `.tab.c`, `lex.yy`, `/generated/`). Единственный выживший path-гард.
- [x] **Вывод дубликатов структурирован** (`src/main.cpp`): `(TYPE, weighted, line)`, whole-file счётчик, пересортировка по weighted, поле `Pair::type` (EXACT/RENAMED/LITERAL/MIXED/STRUCTURAL) через `cloneType()`.
- ❌ **P0.7 (platform) и P0.8 (perf-variant) УДАЛЕНЫ** (2026-06-03): давили реальный TP. Идентичные POSIX-функции (`OS::sleep`/`truncateFile`, w=1.0) между os_macos/os_linux — выносимый копипаст, а path-гард не отличал их от platform-специфики. «identical = report, regardless of path».
- ⚠️ **НЕ гард:** status_bar `addText/Icon/ColorIndicator` — **TP высокого приоритета** (worst-kind: copy + сменили один тип). Репортить, не давить.
- 📊 **Эмпирика:** LibreSprite, все 16 коммитов → 10 уникальных пар, **все TP, 0 FP** (подтверждено). Корпусные «58% FP» — артефакт дженерик-детектора, наша архитектура их не производит.
- [ ] Прогнать триаж ([triage_dup_commits.py](../../experiments/triage_dup_commits.py)) на др. репах (vmecpp, corpus-репы) — проверить 0-FP на ширине
- [ ] ⚠️ Секции «Правила (Draft)» и «Внешняя опора» ниже всё ещё описывают P0.7/P0.8 как FP-гарды — переписать под факт (удалены)
- [ ] Написать правила в `docs/duplication_architecture.md` § **FP Classification Rules**

---

## Внешняя опора (authority, не мнение)

Наш extractability-тест и path-гарды совпадают с эмпирическим каталогом
**Kapser & Godfrey, «"Cloning considered harmful" considered harmful»** (Empirical
Software Engineering, 2008, 13(6):645–692) — это снимает упрёк «вкусовщина»
(принцип проекта «authority over opinion»):

- их **parameterized code** (копия, сводимая в одну параметризованную функцию) —
  именно наш **worst-kind TP** (скопировал блок, поменял один токен);
- их **platform variations** / **replicate & specialize** — наши гарды
  P0.7-P0.9 (platform-twins / perf-variants / generated);
- их тезис: вред определяется **контекстом и коэволюцией**, не сходством текста —
  ровно поэтому severity выносим в отдельный сигнал (см. #078, co-change).

Сводка типов клонов, эмпирика вреда и методы детекта со ссылками:
[../../docs/research/code_clones.md](../../docs/research/code_clones.md).

## Правила (Draft)

### TP — Real Copy-Paste

**Сигналы TP:**
1. **Идентичный код в разных файлах** (weighted ≥ 0.95)
   - Пример: PNG clipboard в clip_win.cpp:521-529 ↔ 617-625
   - Решение: вынести в `readPngFromClipboard(handle, &data, &size)`

2. **Полностью скопированный code block** (weighted = 1.0, cross-file)
   - Пример: switch-statement в cmd_move_mask.cpp ↔ cmd_scroll.cpp
   - Решение: вынести в `formatUnitText(unit)`

3. **Повторение одинакового кода 3+ раза в одном файле** (same-file, но не part of if-elif-else)
   - Пример: skin_theme.cpp:563, 803, 1294, 1771 (4 раза одинаков XML parsing)
   - Решение: вынести в helper loop или template function

4. **Код extractable в функцию с параметрами**
   - Тест: можно ли добавить параметры и унифицировать? → TP
   - Если нельзя (разные семантики) → FP

---

### FP — Legitimate Patterns

**FP Сигналы (гарды должны отфильтровать):**

1. **Template Pattern** — одна структура, разные типы параметров
   - Пример: `addTextIndicator()` vs `addColorIndicator()` (status_bar.cpp)
   - Признак: одна и та же логика для `TextIndicator` и `ColorIndicator` типов
   - Гард: если типы параметров различаются → FP

2. **Platform-Specific Intentional Duplication**
   - Пример: hidapi/mac/hid.c ↔ hidapi/win/hid.c ↔ hidapi/linux/hid.c (weighted=1.0)
   - Признак: директория платформы (mac/, win/, linux/) или `_posix`, `_win` суффикс
   - Гард: P0.7 — если пути содержат `/(mac|win|linux|posix|arm|x86)/` и weighted=1.0 → FP

3. **Performance Variants** — intentional code duplication для разных режимов
   - Пример: `agg_rasterizer_scanline_aa.h` vs `agg_rasterizer_scanline_aa_nogamma.h` (weighted=1.0)
   - Признак: суффиксы `_nogamma`, `_r` (thread-safe), `_fast`, `_slow`
   - Гард: P0.8 — если суффикс `_(nogamma|_r|fast|slow|debug|release)` → FP

4. **Part of if-elif-else rule processing chain**
   - Пример: skin_theme.cpp обработка background/icon/text/border rules в одной функции
   - Признак: код повторяется внутри `if (ruleName == "X")` блоков в одной функции
   - Гард: P0.9 — если оба фрагмента в одной функции AND в if-elif цепи AND обрабатывают разные rule-типы → FP

5. **Library/Third-party Template Code**
   - Пример: safe_list.h, shared_ptr.h template specializations
   - Признак: директория `third_party/` или `_deps/`
   - Гард: P0.10 — если в third_party → либо FP либо low-priority

---

## Ключевые решения

- **«identical = report, regardless of path»** — path-based FP-гарды неверны: идентичный код выносим = TP, даже между платформами. P0.7/P0.8 удалены.
- **similarity-score ≠ refactor-priority** — низкий weighted (разные типы разбавляют bag-of-words) ≠ FP; такие пары рискуют быть ПРОПУЩЕННЫМИ, не over-reported.
- **worst-kind TP** = copy block + сменить один токен (status_bar indicator-триплет) — высокий приоритет, не «средний».
- **Наша архитектура ≠ дженерик-детектор** — function-фрагменты ≥30 токенов + P0.6 не производят большинство корпусных «FP» (decl-хидеры, coincidental). 42% precision к нам неприменимо.
- **Generated (P0.9)** — единственный честный path-FP; но маркеры ловят мало реального, нужен banner-scan сырого файла.

## Изменённые файлы (uncommitted)

- `src/scan/duplication/duplication_scanner.cpp` — P0.2 whole-file (`phase8WholeFileSuppress`), P0.9 generated (`isGeneratedPath`); P0.7/P0.8 удалены.
- `include/archcheck/scan/duplication/duplication_scanner.h` — `ScanResult.wholeFileClones`, опции `enableWholeFileGuard`/`enablePathGuards`.
- `src/main.cpp` — структурированный вывод дубликатов (TYPE/weighted/line, счётчик, сортировка).
- `tests/duplication_path_guards_test.cpp` — P0.9 + whole-file кейсы (P0.7/P0.8 удалены).
- `experiments/triage_dup_commits.py`, `experiments/dup_triage_libresprite.md` — триаж-инструмент + отчёт.
- mem: `fp_classification_rules`.

Коммитов пока нет (жду команды).

## В работе / Следующие шаги

1. Прогнать триаж на vmecpp / corpus-репах — проверить 0-FP на ширине (ждёт go).
2. Переписать устаревшие секции «Правила (Draft)» / «Внешняя опора» — там P0.7/P0.8 ещё как FP-гарды (неверно).
3. (опц.) generated banner-scan; data-table гард (риск прибить biquad-TP); P1.3 header decl↔def.

---

## Related

- mem:`fp_classification_rules` — буду обновлять по ходу работы

