# [SCAN] Copypaste per-commit drift (research; прекурсор продуктового new-clone-gate)

**Дата создания:** 2026-06-11
**Дата старта:** 2026-06-13
**Статус:** wip
**Модуль:** SCAN][DUPLICATION
**Приоритет:** major
**Сложность:** unknown
**Блокирует:** —
**Заблокирован:** —
**Related:** #089 (boolean_state drift research), #077 (per_commit_graph_drift_export), #090 (boolean_state_drift_metric, future), #123 (продуктовый new-clone-gate в `--diff`)

> **MVP:** втянута в v0.1 — release-блокер (решение 2026-06-13, [MVP.md](../../docs/MVP.md) §Acceptance #10). Больше НЕ post-release: per-commit copy-paste precision на корпусе нужна, чтобы знать, что в сундуке реальное золото, до публикации. Прежняя рамка «research, post-release» в тексте ниже — устарела этим решением.

## ПРОДУКТОВАЯ precision получена (2026-06-21)

Ключевой MVP-остаток закрыт на **продуктовом** детекторе (не питон-MD5). Триаж
стратифицированной выборки 70 находок из идущего #124-прогона (25 062 new-clone
коммита / 752 репы): **precision ≈ 86–91%**. Разбор: `experiments/per_commit/PRECISION_103.md`
(+ `nc_precision_triage.py`).

- 60/70 TP — настоящая внесённая копипаста (блок/функция, 1-2 токена изменены);
  **0 ложных совпадений несвязанного кода**.
- FP-шум (6-10) — генерёный код (STM32CubeMX HAL, амальгамированный single-header
  `epiworld.hpp`) + вендоренное поддерево (вложенная копия проекта). Это РОВНО те же
  exclusion-gap'ы #127/#129, что нашлись в #131; их закрытие поднимет precision к ~95%+.
- Скептик-чек: первый прогон харнесса дал 0 — баг регекса (`^` без MULTILINE), не
  детектора; исправлено до выводов.

Recall / полный fire-rate — из #124 (corpus-прогон идёт). Вместе закрывают критерий 10 MVP.

## Цель

Реализовать per-commit анализ дупликатов кода (как гейт на PR): отслеживать добавление новых клонов в истории коммитов май2025–май2026 для 185 агентских проектов, выдавать CSV с метриками дупликации per-commit.

## Контекст

Сейчас есть:
- ✅ Graph-drift per-commit (9700+ записей, `*_graph_drift.jsonl`)
- ✅ Boolean-drift per-commit (5514 коммитов, `bool_history_new185.csv`)
- ❌ Copypaste per-commit (только HEAD-снимок в `EXAMPLES_50.md`)

Для полного PR-гейта нужны все три слоя in-window (май25–май26). Copypaste per-commit позволит блокировать коммиты, добавляющие новые клоны кода (по Juergens et al. ICSE 2009: 52% клонов неконсистентны → техдолг).

## План выполнения

- [ ] Написать скрипт `copypaste_per_commit.py` (аналог `bool_history_scan.py`)
  - For each commit in window (shallow-since=2025-05-01)
  - Scan **added lines** коммита: новые файлы ЦЕЛИКОМ + добавленные фрагменты
    в существующих файлах (unified diff `--unified=0`). Ограничение «только новые
    файлы» снято: самый частый клон-сценарий — функция, скопированная рядом в
    СУЩЕСТВУЮЩИЙ файл (ровно inconsistent-clone кейс Juergens), only-new-files
    его не видит
  - Сравнить добавленный код с базой (existing code before commit) на предмет EXACT/RENAMED клонов
  - Фиксить: repo, sha, date, author, subject, dup_pairs_added, files_affected, target_kind (new_file | existing_file)
- [ ] CSV output: `copypaste_per_commit_new185.csv` (аналог `bool_history_new185.csv`)
- [ ] Интегрировать в `RESUME_pending.sh` или отдельный скрипт
- [ ] Тест на 2-3 репах (OloEngineBase, FastLED, llama.cpp)
- [ ] Merge с existing `EXAMPLES_50.md` или отдельный report

## Сделано

- **Шаги 0–4**: написан `copypaste_per_commit.py` — streaming `git log -p -U0`, rolling hash-index (`seen`), move-фильтр (del_blocks per commit), CSV с полями `repo/sha/date/author_kind/subject/target_kind/dup_pairs_added/max_clone_tokens/files_affected/lines_added/example_file`.
- **author_kind**: определяется через `%(trailers:key=co-authored-by)` + heuristics по author/subject (обнаруживает Claude/Copilot и т.п.).
- **Шаг 5 — синтетический тест**: 3 корректных детекции (new_file copy 1 пара, full new_file copy 2 пары, existing_file verbatim copy 1 пара), уникальный код и короткие копии (<6 строк) — не детектируются (ожидаемо).
- **Eyeball на FastLED** (5 мес, 3072 коммита, 281 клон-коммит): move-фильтр снижает FP от reorg (17→3 пары на FFT-move). Обнаружены 2 класса остаточного FP: (1) cross-commit refactoring (код удалён в коммите A, добавлен в B — move-фильтр не ловит), (2) boilerplate coincidental matches. Для Шага 6 нужна нормализация по `lines_added` (пары/KLOC).

## Результат разведки (2026-06-13)

**Решение: full scan 185 реп НЕ добиваем.** На 22 разнородных репах (openwrt-форки,
ML, embedded, браузерный движок, llama.cpp) порядок величины устаканился — добивать
остальные ради третьего знака смысла нет. Презентационные числа всё равно возьмём
с продуктового **archcheck-прогона**, не с этого питоновского MD5-детектора.

Доля коммитов, в которых детектор сработал ≥1 раз (числитель — clone-commits из CSV,
знаменатель — `git log --since=2025-05-01 --until=2026-05-31` по C++-файлам):

| Проект | clone-commits | доля |
|---|---|---|
| llama.cpp | 617 | 23% |
| react-native | 290 | 21% |
| nrf-sdk | 416 | 20% |
| FastLED | 876 | 19% |
| gtk | 397 | 12% |
| git | 145 | 6% |
| scylladb | 158 | 4% |
| bitcoin | 64 | 3% |

**Вывод:** доля — единицы–двадцатки процентов, зависит не от агента (гипотеза
agentic-vs-human отвалилась ещё на HEAD-снимке, p=0.144), а от культуры проекта.
bitcoin/git (строгий code review) — 3–6%; быстрые фичевые проекты — ~20%. Разброс ×6–7.

**Три оговорки (почему эти числа — только порядок величины, не продуктовая метрика):**
1. Это доля *коммитов с ≥1 срабатыванием*, а не объём скопированного кода (коммит с
   одним 6-строчным совпадением = коммит с 66 парами).
2. Внутри известный FP-хвост (cross-commit refactoring + boilerplate `namespace fl {…}`),
   поэтому «20%» завышены.
3. Алгоритм — питоновский MD5-по-6-строкам, **не** токеновый archcheck-детектор;
   пороги отсюда в продукт не переносятся напрямую.

## Не делаем (осознанно отброшено)

- ~~Запустить full scan по 185 репо~~ — порядок величины ясен на 22, см. выше.
- ~~Шаг 6: agentic vs human clone-rate~~ — гипотеза отвалилась, разрез не нужен.
- Калибровка продуктовых порогов и презентация — **переезжают на archcheck-прогон**
  (продуктовый детектор на diff-скоупе), не на этот research-скрипт.

## Детальная инструкция (алгоритм, функции, тесты)

**Шаг 0 — реюз:** каркас обхода коммитов/реп из `bool_history_scan.py`
(итерация по `git log` в окне, CSV-writer, RESUME-идемпотентность); токеновый
детектор клонов — shipped C++ (`archcheck --duplication`) либо python-обёртка
`clone_classifier.py`, что уже есть в experiments.

**Шаг 1 — извлечение добавленного кода коммита** (`collect_added_code(repo, sha)`):
- `git show --unified=0 --diff-filter=AM <sha> -- '*.c' '*.cc' '*.cpp' '*.cxx' '*.h' '*.hpp'`;
- для A-файлов: весь файл = один added-блок (`target_kind=new_file`);
- для M-файлов: hunks `+`-строк, склеенные в блоки по соседству
  (`target_kind=existing_file`); блоки короче `MIN_TOKENS` (≈30, порог shipped
  фрагментера) отбрасывать сразу;
- vendor/test-фильтр путей — те же правила, что `file_classification.h`
  (скопировать список, зафиксировать в скрипте константой).

**Шаг 2 — база для сравнения** (`base_index(repo, sha)`):
- содержимое `sha^` (`git ls-tree -r` + `git cat-file`) по тем же расширениям,
  тот же vendor/test-фильтр;
- индексировать один раз на коммит; для скорости — кэш индекса между соседними
  коммитами (инвалидация по изменённым файлам).

**Шаг 3 — поиск клонов**: каждый added-блок → кандидаты по базе → классификация
EXACT/RENAMED (LITERAL/STRUCTURAL не считать — noise); **self-match исключить**
(блок не сравнивать с самим собой в новой версии файла; для M-файлов сравнивать
added-блок против `sha^`-версии, в которой его ещё нет — это исключает FP
автоматически).

**Шаг 4 — выход** `copypaste_per_commit_new185.csv`:
`repo, sha, date, author_kind(agentic|human|unknown), subject, target_kind,
dup_pairs_added, max_clone_tokens, files_affected, example_pair(fileA:line<->fileB:line)`.
`author_kind` — те же трейлер/бот-эвристики, что в bool_history (нужно для
AI-vs-human разреза, дизайн №1 из методологической критики).

**Шаг 5 — тесты на 2–3 репах** (OloEngineBase, FastLED, llama.cpp):
- синтетический контроль: создать тестовый репо из 3 коммитов — (а) коммит
  копирует функцию в существующий файл → 1 пара existing_file; (б) коммит
  добавляет новый файл-копию → 1 пара new_file; (в) коммит добавляет уникальный
  код → 0 пар. Прогнать скрипт, сверить CSV;
- eyeball топ-20 пар реального прогона: доля TP ≥ 70%, иначе поднять MIN_TOKENS.

**Шаг 6 — анализ:** per-commit rate (пары/KLOC added) agentic vs human коммиты
**внутри смешанных реп** (repo fixed effects — главный дизайн из критики);
сравнить с нулевым результатом HEAD-снимка (p=0.144) — ожидание: per-commit
introduction rate чувствительнее, т.к. vendored-FP не «добавляются» коммитами.

## Следующие шаги

1. Запустить `python3 copypaste_per_commit.py` overnight (185 реп, ~5–10 ч, resume через `.done_repos`).
2. Шаг 6: загрузить CSV, построить per-commit pairs/KLOC, сравнить agentic vs human внутри смешанных реп (repo fixed effects), оформить в `experiments/FINDINGS.md` или отдельный doc.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Per-commit, не per-diff | Нужно tracking per-PR (как graph/bool) |
| Added lines (новые файлы + вставки в существующие) | Клон чаще добавляется в существующий файл; only-new-files пропускает сценарий Juergens |
| Rolling `seen` hash-index (не git archive per commit) | Один `git log -p` subprocess на репо вместо N git-show; производительность ~0.2–1 с/коммит |
| Move-фильтр (del_blocks per commit) | Убирает FP от reorg: del_hashes исключаются при проверке added-блоков |
| `lines_added` в CSV | Нормализация на KLOC для шага 6; необходима, т.к. cross-commit refactoring создаёт outlier-коммиты с >100 парами |
| WARMUP_START = 2023-01-01 | Baseline для реп с историей до мая 2025; для новых реп (0 warmup) seen строится внутри окна |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `experiments/boolean_state/copypaste_per_commit.py` | Новый скрипт (streaming parser, move-filter, resume, CSV) |
| `experiments/boolean_state/copypaste_per_commit_new185.csv` | Output (создаётся при запуске) |
| `/tmp/test_copypaste_repo/` | Синтетический тест-репо (4 коммита, не в git) |

## Notes

- Может быть медленнее boolean (clone_classifier на каждый новый файл), но дешевле graph-drift (не архитектура)
- Baseline copypaste от baseline-снимка (до май2025)
- **Путь в продукт**: этот скрипт — корпусная валидация порогов для будущего
  продуктового правила **new-clone-gate** (клон ≥N токенов, у которого ≥1 инстанс
  целиком в diff против baseline; см. `docs/research/full_audit_and_research_2026_06.md`
  §5.2). В shipped-коде токеновый детектор и baseline-механика уже есть — недостаёт
  только diff-скоупа. Заголовок «PR-гейт» относится к этой будущей фазе, сама задача — research.
