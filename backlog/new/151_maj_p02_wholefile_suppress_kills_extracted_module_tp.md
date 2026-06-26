# [BUG][DUPLICATION] P0.2 whole-file suppress глушит TP «вынес код в модуль, оставил оригинал»

**Дата создания:** 2026-06-26
**Статус:** new
**Модуль:** SCAN / DUPLICATION
**Приоритет:** major
**Related:** #052/#056 (clone detector), #127 (vendored/generated exclusion), #147 (dup OOM),
P0.4-removal precedent (docs/duplication_architecture.md «History of P0.4»)

## Симптом (дофуд на реальном рефакторе, репа leadline)

В стороннем проекте (`leadline`) пользователь вынес классификатор в новый модуль
`src/geomorph/feature_support.cpp` (функции `classifyAll`, `Support`, `makeSupport`,
`featureSupported`, `bracketGap`, `medianSoundingSpacingGrid`, `soundingSupported`,
`quantile`, `faceDepthMean`, `triMinAngleDeg`, …), **но оставил почти идентичные копии
в `tools/inspect_at.cpp`** (точечный дедуп отложили). Это ровно «missing reuse edge» —
основной целевой TP-класс детектора (§1 `duplication_architecture.md`).

`archcheck --duplication .` по дереву leadline: **59 пар выше порога, НИ ОДНОЙ с участием
`feature_support.cpp`.** Всплыл только мелкий boilerplate `makeDirs`
(`gallery_export.cpp ↔ inspect_at.cpp`, STRUCTURAL 0.996).

Изоляция до двух файлов (только `feature_support.cpp` + `inspect_at.cpp`) подтверждает:

```
$ archcheck --duplication <2 files>
scanned 3 files, 82 fragments, 1521 candidate pairs
reported 0 pairs above threshold (1 whole-file clone group(s) suppressed)
```

**0 пар**, хотя тела функций почти посимвольно совпадают (у `bracketGap` разница — только
комментарий). Подавляет именно P0.2 whole-file guard (счётчик `1 ... suppressed`).

## Корневая причина (точная)

`wholeFileClonePairs` (`src/scan/duplication/duplication_scanner.cpp:281`), вызывается из
`phase9WholeFileSuppress` (там же `:316`), гейт `opts.enableWholeFileGuard`:

```cpp
const int minFrags = std::min(fragsPerFile[fileA], fragsPerFile[fileB]); // фрагменты МЕНЬШЕГО файла
if (minFrags >= 2 && n * 5 >= minFrags * 4)   // n матчей >= 80% фрагментов МЕНЬШЕГО файла
  result.insert(k);                            // -> вся пара файлов исключается из actionable
```

Условие одностороннее: «совпало ≥80% фрагментов **меньшего** файла». Для нашего кейса:

- `feature_support.cpp` (меньший) ≈ 10 фрагментов-функций, ~8+ матчат `inspect_at.cpp`
  → `n ≥ 0.8·minFrags` → срабатывает → **глушатся ВСЕ пары** между файлами.
- `inspect_at.cpp` (бо́льший) ≈ 80 фрагментов; эти ~8 матчей покрывают ~10% его объёма.

То есть «меньший файл ⊂ большего» детектор трактует как move/copy/vendored-twin. Но это
НЕ перенос файла: оба файла живут, оригинал не исчез (rename), а overlap — малая доля
бо́льшего файла. P0.2 **смешивает два разных класса**:

| класс | A vs B | overlap | это | P0.2 сейчас |
|---|---|---|---|---|
| move / vendored twin (целевой FP) | ~равны по размеру | ~весь A И ~весь B | FP, глушить верно | глушит ✅ |
| **extract-в-модуль, оригинал жив** (наш TP) | B мал, A велик | весь B, но малая доля A | **TP, репортить** | глушит ❌ |

Дискриминатор, которого не хватает: настоящий twin покрывает **обе** стороны
(`n ≥ 0.8·maxFrags` тоже), а экстракция — только меньшую. Сейчас проверяется лишь
меньшая сторона → ложное подавление TP.

## Почему это значимо

1. Подавляется **самый ценный** dup-класс — «вынесли общий код, но забыли удалить
   оригинал» (extract-leave-original) — это и есть «missing reuse edge», ради чего §1
   детектор существует. Чем ЧИЩЕ экстракция, тем вернее P0.2 её скроет (иронично).
2. Молча: только строка `N whole-file clone group(s) suppressed`, без перечня что именно.
3. Двойной промах для CI: `--duplication` advisory и не входит в дефолтный scan/diff-гейт,
   так что и напоминания не будет. Рефактор «оставил дубль» проходит незамеченным.
4. Совпадает с уже зафиксированным проектом принципом (removal P0.4/P0.7/P0.8,
   `duplication_architecture.md`): *«guard, который глушит реальный TP ради FP-класса,
   плохо отделённого текущим фрагментером, — чистый налог на recall»*. P0.2 в текущем виде
   ровно такой: чтобы поймать file-move, душит extract-leave-original.

## Repro (фикстура приложена + проверено)

Синтетическая фикстура (свой код, без внешних исходников):
`fixtures/duplication/wholefile_extract_fp/`
- `extracted_geom.cpp` — модуль из 3 хелперов (`triMinAngleDeg`/`quantileSorted`/`bracketGap`);
- `original_geom.cpp` — большой оригинал: те же 3 хелпера verbatim + 4 несвязанные функции.
B ⊂ A (экстракция, оригинал жив).

**Баг (текущее):**
```
$ archcheck --duplication fixtures/duplication/wholefile_extract_fp
scanned 2 files, 10 fragments, 14 candidate pairs
reported 0 pairs above threshold (1 whole-file clone group(s) suppressed)
```

**Контраст (доказывает, что глушит именно subset-форма, а не порог).** Если в меньший файл
добавить 4 СВОИ полноразмерные функции (оба файла ~по 7 фрагментов, 3 общих, ни один не ⊂):
```
scanned 2 files, 14 fragments, 36 candidate pairs
reported 3 pairs above threshold (0 whole-file clone group(s) suppressed)
  a.cpp:13-20  <->  b.cpp:11-18  (EXACT, weighted=1, line=1)
  a.cpp:25-28  <->  b.cpp:23-26  (EXACT, weighted=1, line=1)
  a.cpp:33-38  <->  b.cpp:31-36  (EXACT, weighted=1, line=1)
```
→ те же 3 клона детектятся EXACT/weighted=1; единственная разница — форма «B ⊂ A» против
«A≈B». Подтверждает односторонний `minFrags` как причину.

Реальный источник кейса — `leadline:src/geomorph/feature_support.cpp` ↔
`leadline:tools/inspect_at.cpp` (тот же результат: 0 пар, 1 suppressed).

## Предлагаемое направление (не предписание реализации)

Любой из вариантов (или комбо), плюс регресс-фикстура:

- **Двусторонняя покрываемость:** считать whole-file clone только когда overlap покрывает
  высокую долю **обоих** файлов: `n ≥ 0.8·minFrags && n ≥ 0.8·maxFrags`. Тогда move/twin
  (A≈B, оба покрыты) глушится, а extraction (B⊂A, A покрыт слабо) — репортится.
- **Сверка с git-rename:** P0.2 в доке мотивирован «git rename/move/vendored». Гейтить его
  фактическим rename-similarity из git (переименование/перенос), а не эвристикой overlap по
  фрагментам — у move оригинал ИСЧЕЗАЕТ из старого места, у extraction оригинал на месте.
- Минимум — сделать подавление **видимым/опциональным** (`--show-suppressed` или список
  заглушённых пар в отчёте), чтобы немой промах перестал быть немым.

## Acceptance (DoD)

- Регресс-фикстура `duplication_fp_guards_test.cpp`: «extract-в-модуль с живым оригиналом
  (B мал, B⊂A, A велик) → пара РЕПОРТИТСЯ, не whole-file-suppressed». Готовый вход и оба
  замера — в `fixtures/duplication/wholefile_extract_fp/` (subset → 0 пар; контраст-вариант
  «A≈B» → 3 пары). Inline-стиль теста — как в существующих P0.x кейсах.
- Существующие move/vendored-twin FP-фикстуры (A≈B, оба покрыты) по-прежнему глушатся.
- Self-scan archcheck и корпус — без новых FP (как при removal P0.4: проверить дельту пар).
- Если выбран git-rename-гейт — задокументировать в `duplication_architecture.md` (P0.2).

## Места в коде (для исполнителя)

- `src/scan/duplication/duplication_scanner.cpp:281` — `wholeFileClonePairs` (правило
  `n*5 >= minFrags*4`, односторонний `minFrags`).
- `src/scan/duplication/duplication_scanner.cpp:316` — `phase9WholeFileSuppress`
  (опц. `opts.enableWholeFileGuard`).
- `src/cli/preview_commands.cpp:139` — строка отчёта `... whole-file clone group(s) suppressed`.
- `docs/duplication_architecture.md:241` — описание P0.2 (обновить при правке).
