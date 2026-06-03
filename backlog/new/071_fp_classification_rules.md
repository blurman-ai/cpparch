# #071: Правила FP Classification для дупликатов

**Дата создания:** 2026-06-03  
**Статус:** new  
**Приоритет:** high  

---

## Цель

Систематизировать и зафиксировать **объективные правила** различия между:
- **TP (True Positive)** — реальный copy-paste, нужен рефактор
- **FP (False Positive)** — легитимный паттерн, гарды должны его отфильтровать

Цель: в следующих сессиях при анализе дубликатов сразу применять правила и не выдумывать FP там, где их нет.

---

## Сделано

- [x] **Path-based гарды P0.7-P0.9** в `duplication_scanner.cpp` (2026-06-03):
  - **P0.7** `isPlatformVariantPair` — twins под разными OS-дирами (`/mac/`↔`/win/`)
  - **P0.8** `isPerfVariantPair` — стрипает суффиксы `_nogamma/_r/_fast/...`, сравнивает stem
  - **P0.9** `isGeneratedPath` — `.pb.cc`, `moc_`, `ui_`, `.tab.c`, `lex.yy`, `/generated/`
  - флаг `ScannerOptions::enablePathGuards` (default on)
  - тесты: `tests/duplication_path_guards_test.cpp` (6 кейсов, все зелёные)
- ⚠️ **НЕ гард:** status_bar `addTextIndicator`/`addIconIndicator`/`addColorIndicator` — это **TP высокого приоритета** (worst-kind: скопировали блок, поменяли один тип/функцию), а НЕ FP. Должно репортиться, не подавляться. Сначала ошибочно записал в FP «template pattern» — неверно.
- [ ] Same-function if-elif rule-chain гард (skin_theme XML rules) — нужен анализ скоупа функции
- [ ] Написать правила в `docs/duplication_architecture.md` § **FP Classification Rules**
- [ ] Создать тест-фикстуру `fixtures/duplication/fp_classification/` с примерами каждого типа
- [ ] Переклассифицировать 223 пары с включёнными P0.7-P0.9 — измерить новый precision

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

## В коде (duplication_scanner.cpp)

Добавить фазы после P0.6:

```cpp
// P0.7: platform-specific variant suppression
void phase8aPlatformVariantSuppress(std::vector<Pair> &candidates) {
  // Если пути содержат /(mac|win|linux|posix|arm|x86)/ и weighted=1.0 → отфильтровать
}

// P0.8: performance-variant suppression  
void phase8bPerfVariantSuppress(std::vector<Pair> &candidates) {
  // Если суффиксы _nogamma, _r, _fast, _slow → отфильтровать
}

// P0.9: same-function rule-chain suppression
void phase8cRuleChainSuppress(std::vector<Pair> &candidates) {
  // Если оба в одной функции в if-elif цепи → отфильтровать
}
```

---

## Следующие шаги

1. Нарисовать decision tree в docs/duplication_architecture.md
2. Реализовать guards P0.7, P0.8, P0.9
3. Покрыть fixtures для каждого типа FP
4. Переклассифицировать 223 пары из SESSION_SUMMARY — найти реальный % TP

---

## Related

- mem:`fp_classification_rules` — буду обновлять по ходу работы

