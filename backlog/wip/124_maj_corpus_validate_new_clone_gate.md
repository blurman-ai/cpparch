# [SCAN][DUPLICATION] Корпусная валидация new-clone-gate: archcheck --diff по выборке клон-коммитов

**Дата создания:** 2026-06-13
**Дата старта:** 2026-06-14
**Статус:** wip
**Модуль:** SCAN][DUPLICATION
**Приоритет:** major
**Сложность:** medium
**Блокирует:** —
**Заблокирован:** #123 (нужен shipped --diff new-clone-gate + parent-guard)
**Related:** #123 (продуктовый детектор+гейт), #103 (питон-разведка локализовала клон-коммиты → источник выборки)
**Верификация:** #131 (Группа 2: Part B — fire-rate `DRIFT.NEW_CLONE` по всему корпусу)

## Цель

Получить **продуктовые** числа precision/recall для new-clone-gate, прогнав
shipped `archcheck --diff` на **выборке реальных клон-коммитов** корпуса — а не
питоновским MD5-детектором (#103) и не реплеем всей истории. Эти же числа идут в
презентацию.

## Почему это правильный путь (развилка «walk vs реальные PR» — ложная)

Изначально казалось, что варианта два: (а) строить инкрементальный corpus-walk
(обработать каждый коммит дёшево), либо (б) ждать реальные PR (бесконечно долго).
**Оба не нужны.**

Ключ: в CI git **сам выкладывает полный код** на диск (`actions/checkout` →
рабочее дерево), а `archcheck --diff before..after` читает готовое дерево
(`DiskFileSource`, current=WORKTREE) + git-объекты для parent. archcheck **не
реконструирует слепок** — код подаётся «снаружи». Значит валидация = эмулировать
ровно это на выборке клон-коммитов:

- репы уже на диске (`~/oss`, с `.git`) → `--diff-mode=memory`
  читает оба состояния из объектов, **без checkout**;
- `archcheck --diff (N-1)..N` на десятках клон-коммитов = **минуты**, не десятки
  часов;
- обрабатывать КАЖДЫЙ коммит не нужно (нужна выборка) → инкрементальный walk
  избыточен (YAGNI). Эксперимент 1 (`experiments/incremental_state_check.py`,
  gitignored) доказал, что инкрементальная реконструкция состояния точна
  (0 mismatch / 304 коммита, ~45× меньше фрагментации, DF — точный аддитивный
  счётчик, без аппроксимации) — механизм рабочий, но для этой цели не требуется.

## План выполнения

- [x] Источник выборки: из #103 CSV — `gen_sample.py`, стратификация по
      target_kind × token-bucket, 109 коммитов / 20 реп.
- [x] Для каждого: `archcheck --diff <sha>^..<sha> --diff-mode=memory --format=json`
      через resumable parallel runner (`run_worklist.py`), собрать `DRIFT.NEW_CLONE`
      + все остальные категории.
- [x] Сводка + eyeball + сверка с питоном (`analyze_sample.py`).
- [x] Числа → `experiments/FINDINGS.md` (Part A).

## Прогресс (2026-06-14)

**Harness:** `experiments/per_commit/` (gitignored) — один тонкий runner поверх
shipped-бинаря, ни одна проверка не переписана. `archcheck --diff` бандлит ВСЕ
категории (граф-гейтинг + граф-дрифт + complexity + new-clone + SATD + test-coevo)
за один вызов. Скрипты: `gen_sample.py`, `run_worklist.py`, `analyze_sample.py`,
`gen_full.py`, `launch_full.sh`, `status.sh`.

**Валидация (#124, выборка 109 коммитов):**
- Пайплайн надёжен: 109/109 без падений; все категории срабатывают покоммитно
  (кроме grown-cycles — выборка клон-коммитов их не провоцирует, покрыто фикстурами).
- new-clone fires на 17/108 eligible питоновских клон-коммитов (16%), растёт до
  **40%** на existing_file >300 токенов — продуктовый детектор строго **чище**
  питоновского MD5-по-6-строкам (ожидаемо).
- Eyeball: TP подтверждён (FastLED `02274f1a5` — 10 EXACT-копий, спаны буквально
  сверены); крупнейший silent (gtk `25c7bd54` — split/move) = корректный
  non-detection + питоновский FP; большинство silent = diff-scope артефакты питона
  (added=0/merge/форк-история).
- Нашли gap: diff-**JSON** не отдаёт факт bulk-import-skip (#117) — только text.
  Runner обходит через `git numstat`; продукту стоит положить маркер в JSON.

**Расширение скоупа (запрос пользователя 2026-06-14):** запущен **полнокорпусный**
покоммитный прогон ВСЕХ проверок — 1 051 194 коммита / 1 685 реп, C++ с 2024-06,
`--no-merges`. Detached (`setsid nohup`, 64 воркера, 120с timeout, slow-repo
blacklist). Bottleneck = спавн процессов (CPU простаивает), не CPU/RAM. ETA полного
прохода ~1.5–2.5 дня, resumable. Числа Part B в `experiments/FINDINGS.md`.

## Свежий прогон на текущем бинаре (2026-06-20)

Прошлый full-прогон (`worklist_light`, 520177 коммитов) завершён 18.06, но на
**старом бинаре** — до полного приземления #127/#129 (vendored/generated exclusion
меняет набор сканируемых файлов → new-clone и graph fire-rate сдвигаются) и до
пересборки. Старые результаты сохранены: `results_full.oldbin_20260618.jsonl`.

**Перезапущен начисто на HEAD `4ec4445`** (debug-бинарь, runner default):
`launch_full.sh 8 60`, worklist_light (520177), detached/resumable, baloo suspended.
Старт здоров: done растёт, все категории fire (`new_clone`/`graph_edges`/`complexity`/
`bulk_skip`), parentless-guard 1307 skip. ETA @8w ~5.5–6.5 дн (cheapest-first →
1000-floor рано). Мониторинг: `status.sh`; стоп: `os.killpg` PID из `run_full.pid`.
Heavy-корпус (`worklist_heavy.tsv`, 226060) — отдельно после light.

## Следующие шаги

- [ ] Дождаться покрытия корпуса, написать Part B summary (corpus-wide fire rates
      по категориям, доля slow-repo-skip, топ-находки) — НА ТЕКУЩЕМ бинаре.
- [x] продукт: bulk-import-skip маркер в diff-JSON (`complexity_skipped_added_lines`)
      — сделано этой сессией (DiffJsonContext → diff_json_report.cpp, 2 e2e-теста,
      528/528 зелёные, dogfood 0). Закрыт пункт в `backlog/DEBT.md`.

## Операционка длинного прогона (важно для resume)

- Запуск/resume: `bash experiments/per_commit/launch_full.sh [workers] [timeout]`
  (дефолт **8 воркеров**, 60с — машина юзера, не переподписывать ядра, см.
  [[project_archcheck_diff_git_orphans]]).
- Статус: `bash experiments/per_commit/status.sh`. Стоп: `os.killpg` PID из
  `run_full.pid` + добить archcheck/git по PID (pkill заблокирован).
- На время прогона: `balooctl suspend` (транзиентный, может возобновиться; при
  ≤8 воркерах безвреден). `--diff-mode=memory` не делает checkout → oss-деревья
  на диске не меняются.
- ETA полного прохода @8w ~10–11 дней (throughput ~1.1/с); resumable, 347 медленных
  реп уже преблокированы.

## Нюанс для реального CI (для #123 Ступень 2)

`actions/checkout` по умолчанию **shallow (fetch-depth: 1)** — parent-блоб
недоступен, diff пустой. В workflow нужен `fetch-depth: 2` (PR) или `0` (push на
диапазон). Зафиксировать в примере workflow.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Выборка клон-коммитов, не вся история | для валидации/чисел репрезентативной выборки достаточно; реплей всей истории — десятки часов впустую |
| `--diff-mode=memory` (из `.git`, без checkout) | репы уже на диске; не плодим worktree-материализации на каждый коммит |
| Продуктовый `archcheck --diff`, не питон | питоновский MD5-детектор ≠ shipped токеновый; числа должны быть продуктовыми |
| Инкрементальный walk отменён | нужна выборка, не каждый коммит → механизм избыточен (доказан рабочим, но не нужен) |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `experiments/` (gitignored) | скрипт-обёртка: выборка → `archcheck --diff` per-commit → сводка |
| `experiments/FINDINGS.md` | продуктовые числа new-clone-gate на корпусе |
| `src/git/git_object_file_source.cpp`, `.h` | **фикс empty-blob десинка** (ждёт `/commit`) |
| `tests/unit/git/git_object_file_source_test.cpp` | тест: пустой блоб не десинкает batch |

## Автономный длинный прогон + 2 бага (2026-06-14)

Длинный покоммитный прогон всего корпуса (1200 дешёвых реп, 520k коммитов, все
категории, memory-режим) выявил **два бага корректности**, оба пойманы скептик-проверкой
до показа чисел:

1. **Беспарентные коммиты (артефакт харнесса).** Корпус склонирован shallow → граничный
   коммит беспарентный → `archcheck --diff sha^..sha` дифит против пустого дерева → весь
   репозиторий «добавлен» → ~50-62% граф-находок фейковые. Фикс: skip беспарентных
   (`--min-parents=1` + precompute-guard в раннере). Леджер вычищен.

2. **empty-blob десинк (баг ПРОДУКТА, dogfooding).** `GitObjectFileSource::read()` на
   0-байтном блобе не вычитывал хвостовой `\n` от `cat-file --batch` → сдвиг потока →
   битый include-граф в memory-режиме (memory 102/118/3 vs disk 0/0/0 на коммите, не
   трогавшем циклические файлы). Фикс: `parseBlobSize` → `optional<size_t>`. Verified
   memory==disk на 3 коммитах, 530/530, +юнит-тест. **Ждёт `/commit`** (продуктовое).

Прогон перезапущен начисто с обоими фиксами. Граф-категории теперь надёжны; числа — после
накопления. Доказательства: `experiments/FINDINGS.md` (Part B, fix #1/#2); открытые
продуктовые вопросы: `backlog/DEBT.md` (silent empty-baseline; graph vs bulk-гейт).
