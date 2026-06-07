# [SCAN] #092: scale-independent candidate generation (rare-token index missed clones in big repos)

**Дата создания:** 2026-06-07
**Дата старта:** 2026-06-07
**Статус:** completed
**Дата завершения:** 2026-06-07
**Модуль:** SCAN (duplication)
**Приоритет:** critical (корректность детектора)
**Related:** #091 (large-function fragmentation), #056 (token dedup), [duplication_architecture.md §3.3](../../docs/duplication_architecture.md)

## Цель / баг
Кандидат-генерация была **зависима от размера проекта**: один и тот же копипаст
обнаруживался в маленьком репозитории и **пропадал** в большом. Это дефект
корректности — копипаст остаётся копипастом при любом размере проекта.

## Первопричина (замерено)
Кандидаты строились через инвертированный индекс по «редким» токенам:
[`clone_index.cpp`](../../src/scan/duplication/clone_index.cpp) — токен «редкий»,
если его document-frequency `≤ rareDfCap (=4)` **по всему корпусу**, паре нужно
`≥ minSharedRare (=2)` общих редких токена. В большом проекте общие токены пары
становятся частотными → перестают быть «редкими» → пара **не попадает в
кандидаты** → вообще не скорится.

**Доказательство** — та же `LibreSprite/src/doc/algo.cpp`, те же сплайн-функции,
растёт только корпус:

| корпус | фрагментов | сплайн-клон найден? |
|---|---:|:---:|
| 1 файл | 10 | YES |
| 51 | 99 | YES |
| 501 | 1090 | YES |
| **1211 (весь репо)** | 3067 | **НЕТ** |

Точно та дыра, что [§3.3](../../docs/duplication_architecture.md) и предсказывал:
*«absolute df fails on large trees… Fallback needed: k-gram fingerprints».*

## Фикс
Добавлена **scale-independent** генерация кандидатов — robust winnowing k-gram
fingerprints (MOSS-стиль) поверх normalized token-seq фрагмента:
- хэшируем каждый `fpK=5`-gram (FNV-1a), оставляем min каждого окна `fpWindow=8`
  (re-emit при смене минимума) → плотность ~1/8, **зависит только от содержимого пары**;
- две функции с общим fingerprint'ом → кандидаты при любом размере корпуса;
- **additive**: объединяется с rare-token кандидатами (старое поведение сохранено).

**Анти-взрыв:** `fpMaxPostings=20` — fingerprint, встречающийся в > 20 фрагментах,
отбрасывается как boilerplate-идиома. Отличительный клон-ран редок в абсолюте при
любом N, поэтому это **не** возвращает scale-зависимость для настоящих клонов.
(Без кэпа кандидатов было 972k на LibreSprite; с кэпом=20 — 65k.)

## Проверка
- [x] сплайн-троица `algo.cpp` теперь репортится **по умолчанию** (`465-533 ↔ 540-611 ↔ 618-689`).
- [x] hermetic регресс-тест `duplication_fingerprint_test.cpp`: клон-пара, чьи общие
      токены сделаны частотными филлерами, — **не кандидат** без fingerprints, **кандидат** с ними.
- [x] full suite зелёный (**356 тестов**); precision-eval (`duplication_fp_corpus_eval`) не просел.
- [x] **recall на корпусе вырос, и новые пары — настоящие клоны** (выборка irrlicht:
      `CD3D8NormalMapRenderer ↔ CD3D9NormalMapRenderer` EXACT, `CTRTextureFlatWire ↔
      CTRTextureGouraudWire` EXACT, `CTarReader ↔ CWADReader` EXACT — все проходят
      строгий gate w≥0.75 ∧ line≥0.50). Пары были скрыты scale-зависимым индексом.

Пары по корпусу, **до → после** (fingerprints):

| repo | до | после |
|---|---:|---:|
| Catch2 | 0 | 0 |
| GWToolboxpp | 12 | 149 |
| Kartend | 42 | 62 |
| IrredenEngine | 4 | 116 |
| LibreSprite | 10 | 78 |
| irrlicht-1.8.3 | 41 | 467 |
| AetherSDR | 72 | 390 |
| BambuStudio | 129 | 1152 |

## Цена
- Время ~×3-4 (BambuStudio 7 с → 20 с — всё ещё gate-scale, <30 с на 3173 файла).
- Кандидатов больше (BambuStudio ~320k) — память std::map ~десятки МБ, приемлемо.
- Репортируемых пар намного больше — это **корректно** (раньше ~90% клонов терялись);
  объём для CI глушится baseline-режимом (отдельная забота, не детектор).

## Изменённые файлы
- `include/archcheck/scan/duplication/clone_index.h` — IndexOptions: `fingerprints/fpK/fpWindow/fpMaxPostings`.
- `src/scan/duplication/clone_index.cpp` — `kGramHashes` + `fingerprintsOf` + `addFingerprintCandidates`, вызов в `buildIndex`.
- `tests/duplication_fingerprint_test.cpp` (new), `tests/CMakeLists.txt`.

## Хвост
- `fpMaxPostings=20` режет клон-**классы** размером >20 (обычно boilerplate). Если
  понадобится ловить большие классы — поднять кэп точечно.
- Отчёты `reports/nicad_*` считались до #092 — числа archcheck там теперь занижены
  (recall вырос); пересчитать при следующем обновлении сравнения.
