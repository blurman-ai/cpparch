# [SCAN] UTF-8 BOM перед первым `#include` не распознаётся, ломает DRIFT.1

**Дата создания:** 2026-05-29
**Дата старта:** 2026-05-29
**Дата завершения:** 2026-05-29
**Статус:** done
**Модуль:** SCAN/include_scanner
**Приоритет:** critical (даёт false-positive в DRIFT и, вероятно, в SF.8)
**Сложность:** S (≤ 15 мин: strip BOM на старте scan'а файла)
**Целевой релиз:** v0.1
**Блокирует:** надёжность DRIFT.1 на реальных репо
**Related:** #033 (ai_drift_dataset)

## Симптом

Прогон DRIFT.1 на BambuStudio PR #10794 (2026-05-29, см. milestones.md прогон 13)
выдал ложное срабатывание:

```
slic3r/GUI/MsgDialog.cpp: [DRIFT.1] shortcut edge: slic3r/GUI/MsgDialog.cpp -> slic3r/GUI/MsgDialog.hpp
```

Проверено вручную: `MsgDialog.cpp` в **обоих** ревизиях начинается с `#include "MsgDialog.hpp"`.

## Причина

Файл в before-ревизии имеет **UTF-8 BOM** на старте:

```bash
$ git show 2263815...:src/slic3r/GUI/MsgDialog.cpp | head -1 | od -c | head -1
0000000 357 273 277   #   i   n   c   l   u   d   e       "   M   s   g
        \---BOM---/
```

В after-ревизии BOM был удалён. Наш `include_scanner` не зачищает BOM
перед регулярным выражением `#include` → первая строка с BOM не матчится
→ include "MsgDialog.hpp" пропущен в baseline → в новой версии тот же
include видится как "новое ребро" → DRIFT.1 false-positive.

## Влияние

1. **DRIFT.1 / DRIFT.2** теряют доверие на реальных репозиториях — Windows-style
   файлы с BOM встречаются массово (особенно в legacy C++ проектах,
   wxWidgets-based приложениях, MSVC-сгенерированных файлах).
2. **SF.8** (no_include_guard): тот же баг должен поражать SF.8 — `#pragma once`
   на первой строке с BOM пропускается → ложное "no guard".
3. **Граф зависимостей** в целом теряет рёбра у BOM-файлов.

## Фикс

В `include_scanner::scan()` (или `scan_text()`) — на старте файла:

```cpp
constexpr std::string_view kUtf8Bom = "\xEF\xBB\xBF";
if (content.starts_with(kUtf8Bom))
    content.remove_prefix(kUtf8Bom.size());
```

Аналогично проверить UTF-16 BOM (`FF FE` / `FE FF`) и UTF-32 — но эти
сильно реже в C++ исходниках.

## Фикстура

`fixtures/scan/utf8_bom/`:
- `with_bom.h` — файл начинается с BOM + `#pragma once` + `#include "other.h"`
- `without_bom.h` — тот же контент без BOM
- Тест: оба должны давать одинаковый набор includes и одинаковый результат SF.8.

## Сделано

- Баг найден на BambuStudio PR #10794 (см. `/tmp/bambustudio_drift.txt`).
- Strip UTF-8 BOM добавлен в начало `scanIncludes()` в
  [src/scan/include_scanner.cpp](../../src/scan/include_scanner.cpp).
  Реализация через `string_view::compare` (не `starts_with`) —
  `GCC8-COMPAT`, libstdc++ 8 не имеет метода.
- Unit-тесты добавлены: 3 кейса в `[scan][scanner][bom]` —
  (1) strip перед первым `#include`, (2) эквивалентность результата
  с/без BOM, (3) embedded BOM в середине не считается first-significant.
- Фикстуры `fixtures/scan/utf8_bom/{with_bom.h, without_bom.h, other.h, README.md}`
  для документации и будущих integration-тестов.
- **SF.8 проверен и оказался невосприимчив к BOM**: `hasPragmaOnce`/
  `hasIfndefGuard` используют substring-`find()`, который находит
  ключевое слово даже после BOM. Тем не менее добавлены 2 регрессионных
  кейса в `[rules][sf8][bom]` — на случай, если кто-то перепишет
  предикаты через `starts_with`.
- Полный прогон: `archcheck_tests` 229 case / 754 assertions — green.
- Lizard: `--CCN 15 --length 30 --arguments 5 --warnings_only` — чисто.

## Изменённые файлы

- `src/scan/include_scanner.cpp` — strip BOM в `scanIncludes()`.
- `tests/unit/scan/include_scanner_test.cpp` — +3 BOM-теста.
- `tests/unit/rules/sf8_include_guard_test.cpp` — +2 регрессионных BOM-теста.
- `fixtures/scan/utf8_bom/` — новая фикстура.

## В работе

- (пусто)

## Следующие шаги

1. ~~Strip BOM в `include_scanner`~~ ✅
2. ~~Фикстура `fixtures/scan/utf8_bom/`~~ ✅
3. ~~Прогнать DRIFT повторно на BambuStudio PR #10794~~ ✅
   **2026-05-29 повтор на свежем clone `~/oss/BambuStudio`** (см.
   milestones.md «Прогон 14 — DRIFT re-run после BOM-фикса»): DRIFT.1 3 → **2**.
   FP `MsgDialog.cpp -> MsgDialog.hpp` исчез, два реальных hit'а сохранились.
4. Перепроверить SF.8 reports из прежних milestones (abseil, folly) —
   нет ли там скрытых BOM, формально не нужно после фикса, но полезно
   для уверенности. (Низкий приоритет — substring-find SF.8 был устойчив.)
