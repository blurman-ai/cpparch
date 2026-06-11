# [SCAN] Copypaste per-commit drift (research; прекурсор продуктового new-clone-gate)

**Дата создания:** 2026-06-11
**Дата старта:** —
**Статус:** new
**Модуль:** SCAN][DUPLICATION
**Приоритет:** major
**Сложность:** unknown
**Блокирует:** —
**Заблокирован:** —
**Related:** #089 (boolean_state drift research), #077 (per_commit_graph_drift_export), #090 (boolean_state_drift_metric, future)

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

- (пусто)

## В работе

- (пусто)

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

1. Шаг 0–1: каркас + извлечение added-блоков (`--unified=0`, оба target_kind).
2. Шаг 2–3: base-индекс + клон-матчинг с self-match-исключением.
3. Шаг 4–5: CSV + синтетический контроль из 3 коммитов + eyeball топ-20.
4. Шаг 6: AI-vs-human разрез внутри смешанных реп; решение о new-clone-gate.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Per-commit, не per-diff | Нужно tracking per-PR (как graph/bool) |
| Added lines (новые файлы + вставки в существующие), не only-new-files | Клон чаще добавляется в существующий файл; only-new-files пропускает сценарий Juergens (неконсистентные правки клонов ≈ 50% дефектов) |
| EXACT/RENAMED, не LITERAL | Точная идентификация, не noise |
| CSV format как bool_history | Унификация слоёв, простой анализ |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `experiments/boolean_state/copypaste_per_commit.py` | Новый скрипт |
| `experiments/boolean_state/copypaste_per_commit_new185.csv` | Output |
| `experiments/RESUME_pending.sh` | Добавить фазу copypaste |

## Notes

- Может быть медленнее boolean (clone_classifier на каждый новый файл), но дешевле graph-drift (не архитектура)
- Baseline copypaste от baseline-снимка (до май2025)
- **Путь в продукт**: этот скрипт — корпусная валидация порогов для будущего
  продуктового правила **new-clone-gate** (клон ≥N токенов, у которого ≥1 инстанс
  целиком в diff против baseline; см. `docs/research/full_audit_and_research_2026_06.md`
  §5.2). В shipped-коде токеновый детектор и baseline-механика уже есть — недостаёт
  только diff-скоупа. Заголовок «PR-гейт» относится к этой будущей фазе, сама задача — research.
