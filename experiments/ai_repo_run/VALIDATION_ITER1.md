# VALIDATION — Итерация 1 (#060): что подтвердилось, что лажа

**Дата:** 2026-05-31. Детектор: #056 token-precise (`--diff`, selective normalization +
token-LCS@0.80). Проверка: 200 пар `ADDED ⟵ BASE` из per-commit отчётов
(`per_commit_hits.jsonl`, 34 репы dense-корпуса), **прочитан реальный код** на коммите
(10 агентов × 20 пар, читали ADDED и BASE фрагменты, классифицировали).

## Результат: precision ≈ 16.5% (TP=33 / 200)

**Чекеру в режиме `--diff` доверять нельзя** — это измерено, не мнение.

| Вердикт | N | Доля |
|---|---|---|
| **TP** (реальная копипаста) | **33** | 16.5% |
| FP-coincidental | ~92 | 46% |
| FP-other (баг инструмента) | ~34 | 17% |
| FP-segmentation | ~30 | 15% |
| FP-idiom | ~6 | 3% |

(счётчики классов — по агентским сводкам, ±1 на батч; TP=33 точно.)

TP по типам: verbatim 11, renamed 10, edited 12.

## Что лажа — и ПОЧЕМУ (корневые причины → фиксы)

### 1. FP-coincidental (~46%) — ГЛАВНОЕ
Две **разные функции** (часто в одном файле) матчатся как дубль. Selective
normalization схлопывает `id`/`lit`, и разный код даёт похожий токен-поток;
token-LCS добивает до weighted=1.0, хотя `line` низкий (0.3–0.6).
Примеры (claims, проверены глазами как FP):
- ThemisDB `tcpConnect()` ⟵ `HMAC-sign+publish` (разные функции одного класса)
- aqemu `on_Memory_Size_valueChanged` ⟵ `on_RB_VM_Template_toggled` (разные Qt-слоты)
- gpu_materials switch-dispatch по одному enum, но разные операции BSDF
→ **корень:** недостаточная дискриминация при агрессивной нормализации;
порог по «сырым» строкам не применяется. **Фикс #063.**

### 2. FP-other (~17%) — БАГ ИНСТРУМЕНТА (дешёвый фикс, большой эффект)
`--diff` репортит пары, где BASE-путь **не существует** в анализируемом дереве
или диапазон **за концом файла**. Типовое: `engine/X.cpp` ⟵ `src/X.cpp` (файл
переехал), `Main_Window.cpp` (root) ⟵ реальный `src/Main_Window.cpp`, 9-строчный
redirect-stub с диапазоном 900+. Это фантомные пары — код не сопоставим.
→ **Фикс #061:** валидировать существование пути и границ строк BASE/ADDED на
ревизии перед выдачей пары. Убирает ~17% мгновенно.

### 3. FP-segmentation (~15%)
Фрагмент режет поперёк функций: `хвост func_A + голова func_B` сопоставляется с
чем-то. Не цельная единица. Примеры: «main() vs хвост теста+namespace close»,
«lambda-определение vs тело цикла, использующего lambda».
→ **Фикс #062:** фрагменты выравнивать по границам функций/блоков, не эмитить
куски, пересекающие границу.

### 4. Вендоринг среди «TP»
Часть подтверждённых TP-verbatim — это **две копии miniz** (`third_party/miniz/miniz.c`
↔ `borealis/.../SDL/.../miniz.h`), stb_image и пр. Технически копипаста, но
**вендоренная библиотека**, не код проекта → должно отсекаться excludes.
→ **Фикс #064:** регистронезависимый vendor-exclude в `--diff` (как в snapshot).

## Что подтвердилось (реальная копипаста — детектор иногда прав)
- UnleashedRecomp: `KeDelayExecutionThread`, `ExCreateThread`, `KeSetBasePriorityThread`
  скопированы дословно `kernel/threading.cpp` ↔ `kernel/imports.cpp` (TP-verbatim).
- neo (Dewm-3): `SendPacket` идентичен `sys/aros/aros_net.cpp` ↔ `sys/posix/posix_net.cpp`.
- Standard-of-Iron: `draw_unit_marker` ↔ `draw_building_marker` (TP-renamed).
- iccanalyzer-lite: Lut16↔Lut8 блоки (renamed); ASAN-frame parse loop (edited).
- matiec: visitor-скелеты `generate_c.cc` (renamed).
Эти TP — валидный сигнал missing-reuse; на них и должен срабатывать гейт.

## Наблюдение про режимы
Snapshot-режим (`DENSE_AUDIT_REPORT.md`, min-lines фильтр) давал куда более
правдоподобные пары (ThemisDB importers, FlashCpp 102 строки). `--diff` намного
шумнее: окно по одному коммиту + отсутствие min-line/cross-file гейта + фантомные
BASE. Часть фиксов сводит `--diff` к качеству snapshot.

## План фиксов (порядок по эффект/цена)
1. **#061** FP-other: валидация BASE/ADDED пути+границ → ~17% долой, low-risk. **Первый.**
2. **#064** vendor-exclude в `--diff` → чистит вендоренные «TP» и часть пар.
3. **#062** segmentation: границы фрагментов по функциям → ~15%.
4. **#063** coincidental precision (главное, hard): мин. порог по сырым строкам /
   structural discriminator (callee-сигнатура, форма control-flow) → ~46%.

После каждого — перегон ТОЙ ЖЕ выборки коммитов, precision до/после. Цикл ≤5.
