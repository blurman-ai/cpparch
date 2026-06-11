# [RESEARCH][CORPUS] Local Complexity Drift Prototype On 100 Drift-Positive Repos

**Дата создания:** 2026-06-10
**Дата старта:** 2026-06-10
**Статус:** wip
**Модуль:** EXPERIMENTS / CORPUS / DIFF / RESEARCH
**Приоритет:** major
**Сложность:** large
**Блокирует:** —
**Заблокирован:** —
**Related:** #079 (corpus_run_graph_dup_ai_correlation), #080 (manual_corpus_analysis_findings), #101 (local_complexity_drift)

## Цель

Сделать **отдельный прототип-проект**, который считает commit-level local complexity drift вне shipped `archcheck`,
и прогнать его по **100 локальным C/C++ репозиториям**, которые уже показали хотя бы один текущий drift-сигнал
в существующем корпусе (`graph_errors > 0` или `dup_pairs > 0`).

Результат нужен не для релиза как есть, а для ответа на три вопроса:

- даёт ли функция полезный сигнал на реальных коммитах;
- где она тонет в шуме;
- стоит ли после этого тащить её в shipped `archcheck` через #101.

## Контекст

- В [backlog/new/101_maj_local_complexity_drift.md](~/projects/cpparch/backlog/new/101_maj_local_complexity_drift.md)
  уже зафиксирован продуктовый advisory-сигнал, но его прямое внедрение в `src/` сейчас дороговато:
  там есть незакрытые вопросы по diff runtime, JSON output и report contract.
- В репо уже есть готовый корпус и артефакты, которые можно использовать без нового веб-сбора:
  - [experiments/ai_repo_run/corpus_summary.tsv](~/projects/cpparch/experiments/ai_repo_run/corpus_summary.tsv)
  - [docs/research/cpp_ai_drift_report.md](~/projects/cpparch/docs/research/cpp_ai_drift_report.md)
  - [backlog/wip/079_maj_corpus_run_graph_dup_ai_correlation.md](~/projects/cpparch/backlog/wip/079_maj_corpus_run_graph_dup_ai_correlation.md)
- Текущий `corpus_summary.tsv` уже даёт хороший positive pool:
  - всего строк: `492`
  - с ненулевым drift по текущим сигналам: `449`
  - `graph_only`: `81`
  - `dup_only`: `59`
  - `both`: `309`
- Пользовательская постановка здесь не про ещё один shipped check, а про **быстрый внешний validation loop**:
  сначала проверить метрику на корпусе и только потом решать, заслуживает ли она интеграции в продукт.
- Но перед корпусом нужен ещё более дешёвый слой валидации:
  сначала сгенерировать **synthetic suite типичного complexity drift** и прогнать scorer на ней,
  чтобы не выливать сырую метрику сразу на 100 реальных реп.

## Scope

- Прототип живёт **отдельно от shipped runtime**:
  не в `src/`, не в `include/archcheck/`, не в `tests/` как часть релизного бинаря.
- Допустимый формат:
  - self-contained подпроект в `experiments/local_complexity_drift/`, или
  - sibling prototype рядом с репо, если так удобнее по зависимостям.
- В сам репозиторий обязательно возвращаются:
  - sample manifest;
  - raw results;
  - markdown-отчёт;
  - короткая рекомендация для #101: `ship / revise / drop`.

## Набор

### Источник выборки

Брать только локально доступные C/C++ репозитории из
[experiments/ai_repo_run/corpus_summary.tsv](~/projects/cpparch/experiments/ai_repo_run/corpus_summary.tsv),
у которых:

- `graph_errors > 0` или `dup_pairs > 0`;
- есть живая локальная git-репа;
- есть минимум один C/C++ файл после vendor-filter;
- есть доступный first-parent history в окне корпусного прогона.

### Выборка 100 реп

Чтобы не превратить эксперимент в top-100 гигантов и не черрипикать руками, брать
**детерминированную стратифицированную выборку**:

- `25` реп из `graph_only`
- `25` реп из `dup_only`
- `50` реп из `both`

Внутри каждой корзины:

- разложить репы по квартилям `cpp_loc`;
- брать равное число из каждого квартиля;
- внутри квартиля сортировать по `name` и брать первые N.

Если в конкретной корзине после проверки локальных клонов не хватает реп,
добивать из `both`, сохраняя запись о backfill в manifest.

## Что именно меряем

Единица анализа: **commit**.

На коммите считаем:

- changed C/C++ files;
- matched functions / function-like bodies;
- old/new local complexity score;
- delta по функции;
- агрегат по файлу;
- агрегат по коммиту.

Минимальный shape finding-а:

- `repo`
- `sha`
- `parent`
- `file`
- `symbol`
- `old_score`
- `new_score`
- `delta_score`
- `delta_percent`
- `changed_loc`
- `branch_delta`
- `nesting_delta`
- `deep_lines_delta`

## Synthetic Test Pack

До любого corpus run собрать отдельный набор маленьких baseline/current пар, которые
репрезентируют **типичный** local complexity drift и типичные ложные срабатывания.

Минимальный набор сценариев:

- `flat_to_nested_if`
- `harmless_append_statement`
- `else_if_chain_growth`
- `switch_case_explosion`
- `loop_inside_loop`
- `lambda_nesting_growth`
- `guard_clause_refactor` как пример снижения сложности
- `comments_and_strings_noise`
- `macro_heavy_file`
- `formatting_only_change`
- `function_move_without_growth`
- `extract_helper_reduces_complexity`

Для каждого сценария нужен ожидаемый исход:

- `must_trigger`
- `must_not_trigger`
- `should_reduce`
- `low_confidence_allowed`

Synthetic suite здесь важна не меньше, чем реальный корпус:

- она быстро ломает плохой scorer;
- она даёт фиксированный regression set для будущего #101;
- она позволяет поправить matcher и noise filtering до дорогостоящего corpus run.

## Метод

### Источник коммитов

Первичный режим прогона:

- брать только те коммиты, которые уже попали в существующий structural drift pipeline
  из #079;
- если на репу уже есть `*_graph_drift.jsonl`, использовать его как commit manifest;
- если по конкретной репе такого файла нет, fallback: first-parent commits с `2025-05-01`.

Вторичный режим для sanity-check:

- на каждую репу добирать небольшой matched control set из zero-drift commits;
- это нужно, чтобы сразу увидеть, не срабатывает ли метрика на любом среднем коммите.

### Сама метрика

Базироваться на shape, уже описанном в #101, чтобы прототип не ушёл в нерелевантную сторону:

- branch/control tokens;
- brace nesting;
- deep lines;
- indent accumulation.

V1 формула может остаться простой:

- `localComplexityScore = branchScore + deepLinesCount + floor(indentComplexitySum / 20)`

Здесь цель не Sonar-совместимость, а стабильный proxy, который потом можно перенести в `archcheck`.

### Технический подход

- Не трогать `archcheck` binary и не встраивать это в `src/main.cpp`.
- Делать через отдельный runner, который читает `git show`, `git diff --name-only`,
  `git diff -U0` и работает по old/new contents без checkout всей репы на каждый шаг.
- Разрешён прагматичный стек, удобный для исследования:
  - Python 3
  - небольшой text-based parser
  - при необходимости `tree-sitter-cpp` только как вспомогательный extractor
- Но scorer надо держать как можно ближе к planned product heuristic, а не писать другой мир.

## Конкретный план реализации

1. Создать изолированный подпроект:
   `experiments/local_complexity_drift/`
   с минимумом:
   - `README.md`
   - `synthetic_cases/`
   - `requirements.txt`
   - `generate_synthetic_cases.py`
   - `run_synthetic.py`
   - `run_sample.py`
   - `select_sample.py`
   - `scan_commit.py`
   - `analyze_results.py`
2. Сначала собрать synthetic drift suite:
   - baseline/current пары маленьких `.cpp/.h` файлов;
   - manifest с expected outcome;
   - отдельный runner, который гоняет scorer только по synthetic cases;
   - текстовый summary, где сразу видно `expected vs actual`.
3. Реализовать `scan_commit.py`, который для одного `repo + parent + sha` или для synthetic pair:
   - находит changed C/C++ files;
   - получает old/new text через `git show` или из synthetic case;
   - режет шум: comments, string literals, blank lines;
   - находит function-like bodies;
   - считает old/new metrics;
   - возвращает per-function и per-commit JSON.
4. Отдельно реализовать conservative function matcher:
   - сначала `file + symbol`;
   - fallback `file + nearest line anchor`;
   - если match нестабилен, помечать finding как `low_confidence`, а не угадывать.
5. Прогнать synthetic suite и не идти в corpus, пока не выполнены базовые ожидания:
   - `must_trigger` действительно триггерятся;
   - `must_not_trigger` молчат;
   - `should_reduce` отражают снижение score;
   - шум от comments/strings/macros не доминирует.
6. Только после synthetic pass в `select_sample.py` читать
   [experiments/ai_repo_run/corpus_summary.tsv](~/projects/cpparch/experiments/ai_repo_run/corpus_summary.tsv),
   строить positive pool и выпускать
   `sample_100.tsv` с колонками:
   - `name`
   - `repo_path`
   - `bucket`
   - `cpp_loc`
   - `graph_errors`
   - `dup_pairs`
   - `ai_pct`
   - `selected_by`
7. Для каждой репы научиться получать commit list без checkout-loop:
   - из существующего `*_graph_drift.jsonl`, если файл уже есть;
   - иначе из `git rev-list --first-parent --since=2025-05-01 HEAD`.
8. В `run_sample.py` сделать resumable batch-run:
   - manifest-in
   - per-repo progress
   - append-only jsonl outputs
   - retries на битых репах
   - параллелизм по репам
9. Собирать три слоя output:
   - `raw/findings.jsonl` — все findings
   - `raw/commit_summary.jsonl` — одна строка на commit
   - `reports/sample_100_summary.tsv` — одна строка на repo
10. В `analyze_results.py` склеить результаты с существующим корпусным контекстом:
   - `graph_errors`
   - `dup_pairs`
   - `ai_pct`
   - размер репы
   - число drift-positive commits
11. Выпустить итоговый markdown-отчёт:
   `docs/research/local_complexity_drift_corpus_report.md`
   с секциями:
   - synthetic suite results
   - coverage
   - top findings
   - obvious false positives
   - correlation hints
   - recommendation for #101
12. В конце зафиксировать короткое decision memo:
    - `ship`: если сигнал стабилен и даёт полезные review cues;
    - `revise`: если идея живая, но формула/threshold/mapper шумят;
    - `drop`: если noise floor слишком высокий.

## Что считать успехом

- Есть synthetic drift suite с ожидаемыми outcome-ами.
- Scorer сначала проходит synthetic suite.
- Есть детерминированный `sample_100.tsv`.
- Прототип не лезет в shipped `src/` и не меняет контракт `archcheck`.
- По 100 репам получены raw per-commit результаты.
- Есть хотя бы базовый control-срез по zero-drift commits.
- Есть отчёт с top true-positive / false-positive кейсами.
- Есть явное решение, как это влияет на #101.

## Критерии готовности

- Synthetic cases хранятся отдельно и воспроизводимо гоняются одним runner-ом.
- `must_trigger` / `must_not_trigger` / `should_reduce` сценарии дают ожидаемый результат.
- Прототип воспроизводимо запускается по manifest, а не руками по отдельным репам.
- Выборка действительно собрана из drift-positive pool, а не из произвольных любимых реп.
- Есть raw JSONL/TSV результаты и markdown summary.
- Есть минимум 10-20 разобранных глазами кейсов, а не только сухая агрегация.
- В отчёте отдельно зафиксировано:
  - где сигнал полезен;
  - где он шумит;
  - переносим ли scorer в продукт почти как есть или нет.

## Не делать

- Не тащить это сразу в `archcheck --diff`.
- Не ломать ради эксперимента `rules::Violation`, `json_reporter.cpp` или config schema.
- Не делать сразу multi-language версию.
- Не пытаться доказать причинность `AI -> complexity drift` на этом шаге.
- Не размазывать scope на весь корпус из 492 реп, если 100 уже достаточно для решения.

## Сделано

- Создан внешний prototype project:
  [experiments/local_complexity_drift/](~/projects/cpparch/experiments/local_complexity_drift/README.md).
- Реализован text-based scorer:
  [scan_commit.py](~/projects/cpparch/experiments/local_complexity_drift/scan_commit.py).
- Сгенерирован synthetic suite из 13 типовых baseline/current cases:
  `flat_to_nested_if`, `else_if_chain_growth`, `switch_case_explosion`, `loop_inside_loop`,
  `lambda_nesting_growth`, `guard_clause_refactor`, `comments_and_strings_noise`,
  `macro_heavy_file`, `formatting_only_change`, `function_move_without_growth`,
  `extract_helper_reduces_complexity`, `large_linear_body_growth`.
- Synthetic runner проходит весь suite:
  `13/13 passed`.
- Зафиксировано решение по LOC-only росту:
  большой прирост meaningful LOC внутри существующей функции сам по себе не является finding-ом.
  Это контекст для ревью, но не зона `archcheck`; если линейная длина функции важна, это должен ловить
  статический анализатор/метрика длины функции.
- Собран детерминированный `sample_100.tsv` из локального positive pool:
  - `100` реп;
  - `both`: `67`;
  - `dup_only`: `25`;
  - `graph_only`: `8`;
  - `graph_only` пришлось недобрать из-за фактически доступных локальных клонов/manifest-ов.
- Прогнан corpus run по всем 100 репам sample с защитными лимитами:
  - команда:
    `python3 experiments/local_complexity_drift/run_sample.py --max-repos 100 --max-commits 20 --max-files-per-commit 30 --max-file-bytes 300000 --reset-output`
  - scanned commits: `1614`;
  - findings до author-code filtering / existing-function-only семантики: `8521`;
  - findings после переноса shipped vendor/test filtering и отсечения новых функций: `524`;
  - errors: `0`;
  - skipped oversized/excess/filter-excluded files: `9608`.
- Найдена и перенесена текущая shipped фильтрация из:
  [include/archcheck/scan/file_classification.h](~/projects/cpparch/include/archcheck/scan/file_classification.h)
  и [src/scan/project_files.cpp](~/projects/cpparch/src/scan/project_files.cpp):
  vendored dirs, test dirs, vendored basenames, license-header heuristic.
- Сформирован отчёт:
  [docs/research/local_complexity_drift_corpus_report.md](~/projects/cpparch/docs/research/local_complexity_drift_corpus_report.md).
- Сформирован отдельный отчёт с конкретными before/after примерами:
  [docs/research/local_complexity_drift_examples.md](~/projects/cpparch/docs/research/local_complexity_drift_examples.md).
- Проведён ручной разбор всех 6 сгенерированных before/after примеров:
  - clear TP: `6`;
  - clear FP: `0`;
  - caveat: `2` UI/settings renderer cases, branch-heavy by nature but still real in-function growth.
- Перегенерирован corpus report после удаления LOC-only trigger-а:
  - scanned commits: `1614`;
  - findings: `524`;
  - reason counts:
    `complexity_delta=477`, `zero_to_complex=32`, `relative_complexity_delta=12`,
    `body_and_complexity_growth=3`;
  - `large_linear_body_growth` synthetic case молчит: `must_not_trigger`.
- Добавлен analyzer:
  [analyze_results.py](~/projects/cpparch/experiments/local_complexity_drift/analyze_results.py).
- Зафиксирован первичный вывод для #101:
  `revise`, не `ship` as-is.

## В работе

- Расширение ручного разбора за пределы первых 6 concrete before/after examples.
- Отдельная классификация `existing function grew`:
  true complexity growth / parser mismatch / test-only / generated-like path.
- Дополнительное подавление edge-case test/generated путей вроде `EngineTests`, если ручной разбор подтвердит шум.
- **Внешнее ревью скорера получено (2026-06-11)** — дефекты с живыми репро:
  [docs/research/local_complexity_drift_scorer_review.md](~/projects/cpparch/docs/research/local_complexity_drift_scorer_review.md).
  Главное: плоский 8-case switch даёт 0→19 (Sonar cognitive: 1) — `case`/`default`
  считает прототип, хотя **спека #101 их в списке токенов не имеет** (расхождение
  прототип↔спека); `&&` rvalue-ссылок считается ветвлением; выровненные продолжения
  строк дают фальшивую глубину (`deep_lines` на плоском коде; табы дают 0);
  do-while стоит 2–3 балла; `else` не считается; абсолютный порог `delta>=5`
  на метрике, растущей с размером (477/524 находок). Второе расхождение со спекой:
  brace-depth вместо control-nesting.

## Следующие шаги

1. Расширить ручной разбор до 10-20 before/after examples из
   [docs/research/local_complexity_drift_examples.md](~/projects/cpparch/docs/research/local_complexity_drift_examples.md).
2. Разметить каждый пример:
   true complexity growth / parser mismatch / test-only / generated-like path.
3. Добавить buckets в `analyze_results.py`:
   high-confidence existing growth / low-confidence match / zero-baseline existing growth.
4. При необходимости расширить test/generated suppression, но отдельно от shipped vendor rules.
5. **Выровнять scorer со спекой #101 и ревью**: убрать `case`/`default` из CONTROL_RE;
   control-nesting вместо brace-depth; серия `&&`/`||` = +1; фильтр rvalue-`&&`;
   do-while один счёт; `else` +1; indent-компоненту — вон из score (оставить
   диагностикой); относительные пороги вместо `delta>=5`; тест-фильтр `*tests`-суффиксов
   + блэклист символов TEST_F/TEST/TEST_P/TYPED_TEST/BENCHMARK + арность в ключе матчинга.
   Точная целевая семантика и токенные эвристики — в дизайн-доке
   [docs/research/cognitive_complexity_delta_design.md](~/projects/cpparch/docs/research/cognitive_complexity_delta_design.md)
   (§4 таблица реализуемости, §5 сигналы, §6 ядро ~200-300 строк).
   Контроль: репро-кейсы из ревью (`/tmp/lcd_test/`) + synthetic suite + 6/6 TP должны сохраниться.
6. Перегнать corpus-прогон после фиксов и сравнить топ (ожидание: switch-парсеры и
   TEST_F уйдут из топа) — затем обновить recommendation для #101:
   `ship / revise / drop`.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Отдельный prototype project | Нужно проверить полезность метрики без встраивания в shipped runtime |
| Сначала synthetic suite, потом corpus | Иначе слишком легко утонуть в шуме и не понять, сломан scorer или правда сложный код |
| 100 реп из positive pool, а не весь корпус | Для research-решения этого достаточно и это существенно дешевле |
| Детерминированная стратифицированная выборка | Иначе эксперимент будет нерепрезентативным и нерепродюсируемым |
| Commit-level, а не только HEAD-vs-baseline | Пользовательская постановка именно про drift на коммите |
| Scorer держать близко к #101 | Иначе результат плохо переносится в продукт |
| Отдельный отчёт с decision memo | Нужен чёткий выход: ship / revise / drop |

## Планируемые файлы

| Область | Изменение |
|---------|-----------|
| `experiments/local_complexity_drift/README.md` | Описание прототипа и запуска |
| `experiments/local_complexity_drift/synthetic_cases/` | Маленькие baseline/current пары типичного drift |
| `experiments/local_complexity_drift/generate_synthetic_cases.py` | Генерация synthetic suite |
| `experiments/local_complexity_drift/run_synthetic.py` | Прогон scorer-а по synthetic suite |
| `experiments/local_complexity_drift/select_sample.py` | Сборка `sample_100.tsv` |
| `experiments/local_complexity_drift/scan_commit.py` | Scorer одного коммита |
| `experiments/local_complexity_drift/run_sample.py` | Batch-run по 100 репам |
| `experiments/local_complexity_drift/analyze_results.py` | Агрегация и сводки |
| `experiments/local_complexity_drift/sample_100.tsv` | Фиксированная выборка |
| `docs/research/local_complexity_drift_corpus_report.md` | Итоговый отчёт |
