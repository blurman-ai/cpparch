# [CLI][GRAPH] Git-based анализ: считать дельту графа, а не полный snapshot

**Дата создания:** 2026-05-26
**Дата старта:** —
**Статус:** new
**Модуль:** CLI, GRAPH
**Приоритет:** critical
**Сложность:** L (3-5 дней)
**Блокирует:** —
**Заблокирован:** #015 (graph_diff_primitives), #016 (graph_baseline_contract)
**Related:** #008 (dependency_graph_foundation), #009 (ai_drift_regression_rules, future)

## Цель

Сделать так, чтобы archcheck в CI говорил не «в проекте 27 циклов» (бесполезно
для PR-ревью legacy-проекта), а **«эта PR ввела 1 новый цикл и 3 новых
unresolved-импорта»**. То есть рассчитывать архитектурную **дельту** по
git-ref-у (commit, branch, range), не по полному snapshot-у.

## Контекст

Сейчас `archcheck --graph <path>` строит и анализирует **всё** дерево
проекта. На реальном `gm` (2192 файла) находит 2 цикла в Unigine SDK,
708 unresolved, 138 ambiguous. В PR-context это шум: новых нарушений нет,
старые исторические. CI должен фокусироваться на **изменениях, которые
внесла именно эта PR**.

Есть два пути сравнения, оба валидны:

1. **Baseline-файл (#016)** — `.archcheck/graph-baseline.yaml` в репо.
   PR-валидация: построил граф текущего состояния, сравнил с baseline,
   рапортовал только дельту. Минус: нужно «прикреплять» baseline к main и
   обновлять его при сознательных архитектурных изменениях.

2. **Git-based (эта задача)** — `archcheck --diff main..HEAD` или
   `archcheck --since HEAD~1`. Tool сам делает `git checkout` (или
   `git show:file`) для двух состояний, строит два графа, считает дельту.
   Минус: дороже (два прохода), нужна git-интеграция.

Часто эти подходы **дополняют** друг друга:
- baseline — для длинных периодов и больших разовых рефакторингов;
- git-diff — для каждой PR в режиме «не ухудшается ли тут».

Задача — реализовать git-diff путь. baseline-путь идёт в #016 параллельно
и переиспользует те же primitives из #015.

## План выполнения

- [ ] Определить CLI-форму: `--diff <ref1>..<ref2>` / `--since <ref>` / `--vs-base <ref>` (выбрать одну каноническую, остальные — синонимы)
- [ ] Решить как читать «прошлое» состояние: `git worktree add` временный, `git archive`, или `git show :path` per file. У каждого свои trade-offs (worktree быстрее на больших дельтах, show — экономит место на маленьких)
- [ ] Реализовать `git_state.h` / `git_state.cpp`: API для перечисления project files в данном ref-е и чтения их контента. Без shell-injection, через libgit2 или fork/exec git
- [ ] Построить два графа (baseline, current), переиспользовать pipeline из `--graph`
- [ ] Применить diff-примитивы из #015 (`added_edges`, `removed_edges`, `grown_sccs`)
- [ ] Reporter: «+N edges, -M edges, ±K cycles» с конкретными file:line
- [ ] Exit codes: 0 = нет регрессии, 1 = регрессия (новые циклы / новые unresolved выше threshold), 2 = git/io ошибка
- [ ] Тесты: интеграционные через temp git-репо в `fixtures/git_diff/`, scenarios: PR добавила ребро вне цикла, PR добавила ребро ОБРАЗУЮЩЕЕ цикл, PR убрала ребро, PR не трогала граф, PR с merge-commit-ом
- [ ] CI-пример: GitHub Action job `archcheck --diff origin/main..HEAD`, который комментирует PR-у при регрессиях

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Сначала закрыть зависимости — #015 (diff primitives) и #016 (baseline contract) дают почти весь нижний слой
2. Прототип `git_state` без libgit2 (fork/exec git), на нём проверить идею
3. Уже когда работает — решить, нужен ли libgit2 (зависимость) для скорости / надёжности

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Git-diff и baseline — два равноправных пути | Покрывают разные сценарии CI / разные команды; не конкурирующие |
| Сначала fork/exec git, потом если нужно — libgit2 | Меньше зависимостей, проще на старте; libgit2 — оптимизация по требованию |
| Exit code 1 только на «регрессии», не на «старых нарушениях» | CI должен пропускать PR, которая ничего не ухудшила, даже если legacy грязный |
| Передавать ref-ы как git revspec (`a..b`, `HEAD~1`) | Знакомый формат, парсит сам git |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `include/archcheck/git/git_state.h` | new — API чтения файлов в ref-е |
| `src/git/git_state.cpp` | new — реализация (fork/exec git) |
| `include/archcheck/diff/regression_report.h` | new — DTO с категориями регрессии |
| `src/diff/regression_report.cpp` | new |
| `src/main.cpp` | + `--diff <revspec>` subcommand |
| `tests/integration/git_diff/*` | new — fixtures с разными PR-сценариями |
| `docs/architecture-spec.md` | уточнение: два пути сравнения (baseline + git-diff), как они сосуществуют |
| `.github/workflows/example_archcheck_pr.yml` | new — пример CI job для пользователей |
