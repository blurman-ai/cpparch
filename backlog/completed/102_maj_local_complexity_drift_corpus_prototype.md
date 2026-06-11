# [RESEARCH][CORPUS] Local Complexity Drift Prototype On 100 Drift-Positive Repos

**Дата создания:** 2026-06-10
**Дата старта:** 2026-06-10
**Дата завершения:** 2026-06-11
**Статус:** completed
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

- В [backlog/new/101_maj_local_complexity_drift.md](/home/localadm/projects/cpparch/backlog/new/101_maj_local_complexity_drift.md)
  уже зафиксирован продуктовый advisory-сигнал, но его прямое внедрение в `src/` сейчас дороговато:
  там есть незакрытые вопросы по diff runtime, JSON output и report contract.
- В репо уже есть готовый корпус и артефакты, которые можно использовать без нового веб-сбора:
  - [experiments/ai_repo_run/corpus_summary.tsv](/home/localadm/projects/cpparch/experiments/ai_repo_run/corpus_summary.tsv)
  - [docs/research/cpp_ai_drift_report.md](/home/localadm/projects/cpparch/docs/research/cpp_ai_drift_report.md)
  - [backlog/wip/079_maj_corpus_run_graph_dup_ai_correlation.md](/home/localadm/projects/cpparch/backlog/wip/079_maj_corpus_run_graph_dup_ai_correlation.md)
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
[experiments/ai_repo_run/corpus_summary.tsv](/home/localadm/projects/cpparch/experiments/ai_repo_run/corpus_summary.tsv),
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

**v1-формула (`branchScore + deepLinesCount + floor(indentComplexitySum / 20)`) реализована,
прогнана по корпусу и признана дефектной** — внешнее ревью 2026-06-11 с живыми репро
([docs/research/local_complexity_drift_scorer_review.md](/home/localadm/projects/cpparch/docs/research/local_complexity_drift_scorer_review.md)).
Не использовать её и не «чинить локально».

v2-формула обязана повторять «Scoring model» из #101 (cognitive-style, источник —
[docs/research/cognitive_complexity_delta_design.md](/home/localadm/projects/cpparch/docs/research/cognitive_complexity_delta_design.md) §4–§6):

- score = только структурные инкременты (`+1 + control-вложенность`) и flat-инкременты;
- `case`/`default` НЕ считаются; `else`/`else if` = +1; серия `&&`/`||` = +1; do-while = один счёт;
- вложенность = control-nesting (классифицированные скобки), не brace-depth и не отступы;
- `deep_lines` / indent-аккумуляция — только диагностические поля в выводе, в score не входят.

Цель по-прежнему не байт-в-байт Sonar-совместимость, а стабильный proxy, переносимый в `archcheck` (#101).

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
   [experiments/ai_repo_run/corpus_summary.tsv](/home/localadm/projects/cpparch/experiments/ai_repo_run/corpus_summary.tsv),
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
  [experiments/local_complexity_drift/](/home/localadm/projects/cpparch/experiments/local_complexity_drift/README.md).
- Реализован text-based scorer:
  [scan_commit.py](/home/localadm/projects/cpparch/experiments/local_complexity_drift/scan_commit.py).
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
  [include/archcheck/scan/file_classification.h](/home/localadm/projects/cpparch/include/archcheck/scan/file_classification.h)
  и [src/scan/project_files.cpp](/home/localadm/projects/cpparch/src/scan/project_files.cpp):
  vendored dirs, test dirs, vendored basenames, license-header heuristic.
- Сформирован отчёт:
  [docs/research/local_complexity_drift_corpus_report.md](/home/localadm/projects/cpparch/docs/research/local_complexity_drift_corpus_report.md).
- Сформирован отдельный отчёт с конкретными before/after примерами:
  [docs/research/local_complexity_drift_examples.md](/home/localadm/projects/cpparch/docs/research/local_complexity_drift_examples.md).
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
  [analyze_results.py](/home/localadm/projects/cpparch/experiments/local_complexity_drift/analyze_results.py).
- Зафиксирован первичный вывод для #101:
  `revise`, не `ship` as-is.

### v2-скорер (2026-06-11) — ревью отработано

- **Переписан `scan_commit.py` на токенный сканер Sonar Cognitive Complexity**
  (дизайн — [cognitive_complexity_delta_design.md](/home/localadm/projects/cpparch/docs/research/cognitive_complexity_delta_design.md) §4/§6):
  brace-стек с классификацией control/non-control, lastOp-стек серий `&&`/`||` по
  глубине скобок, do-while-спарка, `else`/`else if` hybrid, лямбда-nesting, braceless-тела.
  Построчный разбор v1 заменён целиком — серия `&&`, разбитая по строкам, теперь
  считается один раз (это и был корень FP на рефорсате условий).
- **Все 7 дефектов ревью закрыты**, проверено на `review_repros/` (a 0→1, b Δ0, c 0,
  d Δ0, e 0→1, f 2→2). Resolution-секция дописана в
  [scorer_review.md](/home/localadm/projects/cpparch/docs/research/local_complexity_drift_scorer_review.md).
- **D6/D7**: блэклист тест-символов `TEST*/BENCHMARK` в `signature_symbol`, суффикс-фильтр
  `*tests`-каталогов, арность верхнего уровня в ключе матчинга (`(symbol, arity)`).
  Эффект: TEST-находок в корпусе 0 (было 2 в топ-20), low-confidence 43→21.
- **`finding_reason` → иерархия LCX**: `crossed_25` (LCX.1) / `grew_when_already_above`
  (LCX.2) / `complexity_delta` Δ>=5 (LCX.3, нестрогое). Без нормировки на размер диффа.
- **Synthetic suite 13/13** под v2; `switch_case_explosion` переразмечен в
  `must_not_trigger` (это дефект D1, cognitive-дельта 0), `else_if_chain_growth`
  триггерится на границе Δ=5=K.
- **Корпус перепрогнан** той же командой: 1612 коммитов, **403 находки** (v1: 524).
  Reasons: `grew_when_already_above`=210, `complexity_delta`=127, `crossed_25`=66.
  Топ — настоящий рост сложности на здравой шкале; switch-парсеры и TEST_F ушли.
  **6/6 ручных TP сохранились** (3× crossed_25, 3× grew_when_already_above).
- Отчёты перегенерированы: corpus_report (v2 числа + сравнение с v1), examples-док
  (скоры переведены на v2-шкалу + пометка про 6/6 survival), analyze_results.py
  (noise-notes и recommendation под v2).
- **Рекомендация для #101 уточнена**: `revise`, leaning `ship` для ядра скорера —
  метрика готова к переносу (authority-backed, 13/13, D1-D7 закрыты, 6/6 TP), но перед
  дефолт-правилом нужен порог-пол на LCX.2 (Δ1-2 хвост на уже-огромных функциях,
  72/210, неактионабелен) и подтверждение K=5 на втором срезе корпуса.

### Ручной разбор round-2 (2026-06-11) — закрыт порог 10-20 кейсов

- Разобран **второй раунд из 10 кейсов**, намеренно из шумовых страт (не топ-TP):
  Δ1-хвост `grew_when_already_above`, low-confidence матчи, zero-baseline функции,
  средние дельты. Итого по двум раундам — **16 кейсов** с разметкой
  ([examples.md](/home/localadm/projects/cpparch/docs/research/local_complexity_drift_examples.md),
  секция «Manual Review Round 2»).
- Итог: `10` actionable TP, `3` non-actionable TP (Δ1-хвост / механические guard'ы),
  `2` FP, `1` low-confidence путаница.
- **Найдены 2 настоящих FP**: `__attribute__((unused))` принят за сигнатуру функции
  (parser mismatch текстового фрагментатора); `_pack_shoal_waves` Δ1 — фантомный +1 от
  эвристики «`!` рвёт серию» при упрощении условия. Оба — Δ-малые либо очевидно ложные.
- **Ключевая валидация v2**: `process_connection` 574→681 — рост целиком из вложенных
  `if/else` внутри новых `case`, а НЕ из числа case-меток (в v1 такие switch-диспетчеры
  доминировали в топе по числу меток). v2 меряет глубину ветвления, не размер диспетчера.
- Добавлены triage-buckets в `analyze_results.py`:
  `high_confidence_existing_growth`=354 / `zero_baseline_existing_growth`=28 /
  `low_confidence_match`=21.

## Как работает

**Прототип** (`experiments/local_complexity_drift/`, вне shipped runtime, в `.gitignore`)
считает commit-level рост локальной сложности и валидирует метрику до переноса в #101.

- **Скорер** (`scan_commit.py`, v2): токенный сканер **Sonar Cognitive Complexity**
  (Campbell 2018) — линейный код = 0 by construction. Single-pass по токенам очищенного
  тела функции:
  - structural (`if`/`for`/`while`/`switch`/`catch`/`?:`) = `+1 + nesting`;
  - hybrid (`else`/`else if`) = `+1` без nesting-бонуса, но поднимает вложенность;
  - fundamental (`goto`/`co_await`, каждая серия `&&`/`||`) = `+1`;
  - `case`/`default` бесплатны, `switch` = +1 один раз; do-while = один счёт;
  - вложенность — классифицированный brace-стек (control vs class/namespace/init/lambda),
    не brace-depth и не отступы; серии логических операторов — lastOp-стек по глубине `(`;
  - rvalue-`&&` (`Type&&`) отсеивается по «приклеенности» к токену; `!=`/`==`/`<=`/`>=`
    токенизируются цельно, чтобы `!=` не выглядел как логическое НЕ.
- **Матчинг функций**: ключ `(symbol, arity)` — перегрузки одного имени не путаются;
  тест-символы `TEST*/BENCHMARK` и `*tests`-каталоги отфильтрованы; неоднозначные —
  `low_confidence`.
- **Сигналы** (`finding_reason`, иерархия LCX): `crossed_25` (пересёк порог 25) /
  `grew_when_already_above` (рост уже выше 25) / `complexity_delta` (Δ≥5, нестрого).
  Без нормировки на размер диффа.
- **Пайплайн**: `select_sample.py` → детерминированный `sample_100.tsv` из drift-positive
  pool; `run_sample.py` → resumable batch по 100 репам (git show old/new, vendor/test
  фильтры, лимиты); `analyze_results.py` → агрегаты + corpus_report.
- **Validation-лесенка**: `generate_synthetic_cases.py`/`run_synthetic.py` (13 типовых
  drift-пар, гейт перед корпусом) → `review_repros/` (6 репро дефектов из внешнего ревью) →
  корпус 100 реп → двухраундовый ручной разбор 16 кейсов.

**Решение для #101**: `revise`, leaning `ship`. Ядро (cognitive complexity) готово; до
дефолт-правила — пороговая работа, не скоринговая: пол/down-rank на `grew_when_already_above`,
down-rank `low`-confidence из headline, блэклист `__attribute__`/`__declspec`.

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
