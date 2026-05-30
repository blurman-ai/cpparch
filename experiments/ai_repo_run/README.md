# AIDev C++ corpus run — инструкция

Корпус AI-репозиториев + раннеры для **многократного** прогона метрик archcheck
(дублирование, дальше — граф) по реальным C++-проектам, где использовался AI.

Задача: `backlog/wip/054_maj_ai_repo_duplication_run.md`.
Справочник поля: `docs/research/ai_code_detection_landscape.md`.

Рабочий цикл, под который всё заточено:

```
правишь инструмент  ->  пересобираешь  ->  ./run_dup.sh <tag>  ->  ./diff_runs.sh <prev> <tag>
```

---

## 0. Что откуда взялось (один раз, уже сделано)

Корпус — 50 случайных C++-реп из датасета **AIDev** (`hao-li/AIDev`,
HuggingFace, CC BY 4.0). Фильтр: `language=C++`, `stars>=100`, `size<500MB`,
детерминированная выборка `seed=42`. Это репозитории, где по данным AIDev
использовались AI-агенты (Codex/Copilot/Cursor/Devin/Claude Code).

Воспроизвести список с нуля:
```bash
# нужен /tmp/aidev_repo.parquet (all_repository.parquet из hao-li/AIDev) + pyarrow
python3 sample_cpp.py          # -> /tmp/cpp_pool_shuffled.txt (119 реп ≥100★)
# затем отбор первых 50 с size<500MB через gh api (см. историю в PILOT_NOTES.md)
```
Готовый результат уже лежит в `corpus_50.tsv` — пересоздавать не нужно.

---

## 1. Где живут клоны (НЕ удалять, НЕ пересоздавать)

```
~/oss/_aidev_run/<repo-basename>/
```

Постоянное хранилище (не sandbox, не /tmp) — переживает сессии. Инструмент
дорабатывается и прогоняется многократно, поэтому корпус качается **один раз** и
остаётся локально. Клонировано через `git clone --depth 1 --filter=blob:none`.

Доклонировать недостающее (идемпотентно, пропускает существующие):
```bash
/tmp/clone_corpus.sh        # лог -> clone.log
```

---

## 2. Собрать инструмент (спайк дублирования)

```bash
cmake -S ~/projects/cpparch/experiments/line_duplication \
      -B /tmp/line_dup_build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/line_dup_build
```

Бинарь: `/tmp/line_dup_build/line_duplication`.

⚠️ **После каждой пересборки проверяй mtime бинаря**, прежде чем верить цифрам —
бывает, что edit не применился и бинарь не пересобрался:
```bash
stat -c '%y' /tmp/line_dup_build/line_duplication
```

Опции спайка (вывод **текстовый**, JSON НЕТ):
`<root> [--min-lines 5] [--min-window-chars 60] [--top 8] [--exclude PAT]
[--include-tests] [--cross-only] [--git-diff A..B] [--git-commit SHA]`.

---

## 3. Прогон по всему корпусу

```bash
cd ~/projects/cpparch/experiments/ai_repo_run
./run_dup.sh <tag> [доп. аргументы спайку...]
```

- `<tag>` — метка прогона, напр. `v1_baseline`, `v2_excludes`, `v3_minlines6`.
- Результат: `runs/<tag>/`
  - `<repo>.txt` — полное сырьё спайка по каждой репе (для ручной верификации);
  - `summary.tsv` — сводка: `repo, eligible, sigLOC, dupLOC, ratio, blocks, cross_file`;
  - `_run_context.txt` — что за бинарь, его mtime, аргументы (воспроизводимость).

Примеры:
```bash
./run_dup.sh v1_excludes                 # дефолтные настройки
./run_dup.sh v2_minlines6 --min-lines 6  # порог окна 6 строк
./run_dup.sh v3_crossonly --cross-only   # только cross-file в топе блоков
```

Переменные окружения (по умолчанию обычно не нужны):
- `BIN` — путь к бинарю (default `/tmp/line_dup_build/line_duplication`);
- `CORPUS_DIR` — корень клонов (default `~/oss/_aidev_run`).

---

## 4. Сравнить два прогона (что сдвинула правка инструмента)

```bash
./diff_runs.sh <tagA> <tagB>
```
По каждой репе печатает `ratio | blocks | cross_file` в виде `A->B` и помечает
`*` строки, где что-то изменилось. Так видно эффект правки сразу.

---

## 5. Как читать вывод одной репы

`runs/<tag>/<repo>.txt`:
```
significant LOC:   8027     # значимых строк кода (после фильтров/excludes)
duplicated LOC:    485      # покрыто хотя бы одним дублированным окном
duplication ratio: 6.04%    # duplicated / significant
duplicated blocks: 37       # всего блоков (после merge в максимальные)
cross-file blocks: 14       # из них между разными файлами
top duplicated blocks:
  [CROSS-FILE] 8 lines  api/resource.h:7 <-> clidll/resource.h:7
  [same-file ] 16 lines api/model.cpp:1059 <-> api/model.cpp:1300
```

**Что значимо:**
- `cross-file blocks` — кандидаты в «missing reuse edge» (дубли между файлами);
- `[same-file]` — дубли внутри одного файла, обычно менее интересны для архитектуры;
- `ratio` — общий уровень дублирования; чувствителен к excludes (вендоринг раздувает).

⚠️ **Дисциплина с цифрами:** в отчёты переносить только то, что реально лежит в
`summary.tsv`/`<repo>.txt`. Инлайн-вывод bash в этой среде иногда искажается
(двойная печать, обрезы) — источник истины это файлы в `runs/`, не консоль.

---

## 6. Известные нюансы (нужны при интерпретации)

- **Вендоринг раздувает ratio.** Закоммиченные `extern/`, `vendor/`, `bundled/`
  и т.п. — чужой код. Дефолтные excludes их режут (см. `excludePatterns` в
  `main.cpp`), но список не исчерпывающий: при странном ratio открой `<repo>.txt`
  и проверь top-блоки на чужие файлы. nested-`.git` и `.gitignore` НЕ помогают —
  вендоринг обычно закоммичен прямо в дерево.
- **header↔impl пары** (`X.hpp <-> X.cpp`) дают cross-file блоки, но это одна
  компонента, не архитектурный дубль. Для cross-**component** метрики нужен
  module-map (см. #053 P0-B) — пока это cross-**file** proxy.
- **whole-tree, не AI/human.** Сейчас меряем дубли по всему дереву репо —
  это «насколько грязно вообще», без разделения AI-коммитов и человеческих.
  Within-repo AI/human срез — отдельный следующий этап.

---

## 7. Файлы каталога

| Файл | Назначение |
|------|-----------|
| `corpus_50.tsv` | список корпуса (full_name, stars, size_kb) |
| `skipped_big.tsv` | отсеянные ≥500 МБ |
| `sample_cpp.py` | детерминированный сэмплер (seed=42) |
| `run_dup.sh` | раннер дублирования по всему корпусу |
| `diff_runs.sh` | сравнение двух прогонов |
| `runs/<tag>/` | результаты прогона (сырьё + summary + контекст) |
| `clone.log` | лог клонирования корпуса |
| `pilot_raw/` | сырьё раннего пилота на 5 репах |
| `PILOT_NOTES.md` | рабочие заметки / восстановление контекста |
