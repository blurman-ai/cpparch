# archcheck в CI: как это работает на PR

Документ объясняет, как archcheck встроен в git-flow / GitHub Flow при ревью
pull request-а. **Кратко:** archcheck не читает unified-diff, не получает
patch-blob, не парсит GitHub API. Ему передаются два git-ref-а — он сам
материализует оба состояния и считает архитектурную дельту.

## Что archcheck считает «диффом»

archcheck не работает с текстовым diff-ом. Он считает разницу **двух графов
зависимостей**: «граф проекта на baseline» vs «граф проекта на current».
Дельта — это рёбра, добавленные/удалённые в графе, и циклы, появившиеся/
выросшие. Это principal point: tool отвечает на вопрос «эта PR ухудшила
архитектуру?», а не «какие строки изменились».

Следствие: для anchoring дельты ему нужны **полноценные снапшоты обоих
состояний дерева исходников**, а не патч. Поэтому интерфейс — git revspec,
а не файл `.diff`.

### Два режима gate-инга — `--diff` vs `--baseline`

Это разные сценарии, не конкуренты:

| Режим                  | Что фиксирует                                  | Когда                              |
|------------------------|------------------------------------------------|------------------------------------|
| `--diff <revspec>`     | регрессия конкретной PR                        | каждый PR-прогон                   |
| `--baseline <file>`    | долгосрочный технический долг (snapshot)       | релизные cut-ы, разовые рефакторы  |

Концептуальный аналог в Java-мире — ArchUnit `FreezingArchRule`
([ArchUnit docs §«Freezing arch rules»](https://www.archunit.org/userguide/html/000_Index.html)):
один раз «замораживаешь» список существующих нарушений в файл, дальше CI
сообщает только о новых. archcheck покрывает оба паттерна — baseline-файл
ложится в репо рядом с конфигом, `--diff` работает on-the-fly без файла.

## CLI-контракт

```
archcheck --diff <revspec> [path]
```

`<revspec>` — стандартный git-revspec:

| Форма          | Семантика                                                       |
|----------------|-----------------------------------------------------------------|
| `a..b`         | baseline = `a`, current = `b` (оба git-ref-а)                   |
| `<ref>`        | baseline = `<ref>`, current = текущее рабочее дерево (WORKTREE) |

`path` — путь к корню проекта внутри репозитория (по умолчанию — cwd).
Поддержка `...` (three-dot) и пустых сторон **отвергается**, см.
[parseRevspec](include/archcheck/git/git_state.h#L25).

Exit codes — часть контракта (см. [CLAUDE.md](../CLAUDE.md) §«Exit codes»):

- `0` — регрессии нет (новых рёбер не появилось, циклы не выросли).
- `1` — регрессия. CI должен fail-нуть.
- `2` — git/IO ошибка, невалидный revspec, не git-репо.

Удаление рёбер регрессией **не считается** — PR, которая делает архитектуру
строже, проходит зелёной даже если legacy-проект всё ещё имеет нарушения.

## Что archcheck делает под капотом

```
parseRevspec("origin/main..HEAD")
  → baseline="origin/main", current="HEAD"

materializeRef(baseline)
  → git worktree add --detach /tmp/archcheck-XXXX origin/main
  → buildGraphForPath(/tmp/archcheck-XXXX/path) → G_baseline

materializeRef(current)
  → если current == WORKTREE: использовать рабочее дерево без checkout-а
  → иначе ещё одна worktree → G_current

buildRegressionReport(G_baseline, G_current)
  → addedEdges, removedEdges, grownSccs

worktree remove --force /tmp/archcheck-XXXX  (RAII)
```

Tool **fork/exec**-ает `git` через `execvp` без shell (нет injection-риска).
libgit2 умышленно не подключали — см. ключевое решение в
[#018](backlog/wip/018_crt_git_diff_analysis.md). Полная реализация:
[git_state.cpp](src/git/git_state.cpp), [graph_builder.cpp](src/graph/graph_builder.cpp).

## Анатомия PR на GitHub / GitLab

В терминах GitHub Actions:

| Переменная                     | Значение                                       |
|--------------------------------|------------------------------------------------|
| `github.base_ref`              | целевая ветка PR (например, `main`)            |
| `github.head_ref`              | source-ветка PR                                |
| `HEAD` после checkout          | tip PR-ветки (или merge-commit при `merge_group`) |
| `origin/${{ github.base_ref }}` | tip целевой ветки на момент CI-прогона        |

**Канонический revspec для PR:** `origin/${{ github.base_ref }}..HEAD`.

Tool не различает «merge-PR / squash-PR / rebase-PR» — он сравнивает два
снапшота. Стратегия слияния не влияет на результат **дельты графа**:
важны только состояния deree-source на двух коммитах. Это намеренное
свойство: одинаковая логика для всех git-хостингов и стратегий слияния.

## Минимальный CI snippet

См. полный пример: [example_archcheck_pr.yml](../.github/workflows/example_archcheck_pr.yml).

Ключевые моменты:

```yaml
- uses: actions/checkout@v4
  with:
    fetch-depth: 0          # ОБЯЗАТЕЛЬНО: shallow clone обрезает baseline

- run: ./archcheck --diff "origin/${{ github.base_ref }}..HEAD" .
```

### Почему `fetch-depth: 0`

`actions/checkout@v4` по умолчанию делает shallow clone (`--depth=1`).
`origin/main` тогда либо вообще не пришёл, либо обрезан. `git worktree add
--detach origin/main` упадёт с «invalid reference», и archcheck вернёт
exit 2. Полная история обязательна — иначе baseline нечем материализовать.

Для очень больших монорепо `fetch-depth: 0` неприемлем по объёму трафика.
Альтернатива — selective fetch двух нужных SHA-ев:

```yaml
- uses: actions/checkout@v4
  with:
    fetch-depth: 1
- run: |
    git fetch --depth=1 origin ${{ github.event.pull_request.base.sha }}
    git fetch --depth=1 origin ${{ github.event.pull_request.head.sha }}
    archcheck --diff "${{ github.event.pull_request.base.sha }}..${{ github.event.pull_request.head.sha }}" .
```

`git worktree add --detach <sha>` работает с любым известным локально
объектом — общая история между ref-ами не требуется (archcheck не вызывает
`git merge-base` сам).

### Merge queue (`merge_group`)

Если репо использует GitHub Merge Queue, workflow обязан триггериться
**и** на `pull_request`, **и** на `merge_group` ([docs.github.com — Managing
a merge queue](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/configuring-pull-request-merges/managing-a-merge-queue)).
Имя check-run должно совпадать у обоих триггеров, иначе очередь зависнет в
ожидании check-а, которого никогда не будет:

```yaml
on:
  pull_request:
    branches: [main]
  merge_group:
```

В `merge_group` `HEAD` — это уже сгенерированный merge-commit очереди;
revspec `origin/${{ github.base_ref }}..HEAD` остаётся корректным.

## Edge-cases

| Сценарий                                | Поведение                                              |
|-----------------------------------------|--------------------------------------------------------|
| Shallow clone, baseline недоступен      | exit 2, сообщение от git в stderr                      |
| Force-push в PR-ветку                   | OK — сравниваем по revspec, прошлый прогон не нужен    |
| Merge-commit как HEAD                   | OK — worktree берёт snapshot-tree, не diff с родителями. Покрытие тестом — [#022](backlog/new/022_min_diff_merge_commit_coverage.md) |
| PR не трогает C/C++                     | сейчас всё равно строит два графа (~74 c на gm). Fast-path — [#023](backlog/new/023_maj_diff_skip_when_no_cpp_changes.md) |
| Rebase/squash на стороне PR             | без разницы — снапшоты, не история                     |
| `compile_commands.json` отсутствует     | для дефолтных правил (SF.7/8/9/21, циклы) **не нужен** — include-scanner backend. Семантические правила (когда появятся) потребуют его в обоих worktree |
| Submodules                              | `git worktree add` **наследует `.gitmodules`, но не инициализирует**. Если архитектура зависит от кода в подмодуле — после `materializeRef` нужен `git -C <worktree> submodule update --init --recursive`. Сейчас archcheck этого не делает; задокументировать или автоматизировать — открытый вопрос |
| Draft PR                                | GitHub шлёт `pull_request` events и для draft-ов; tool отрабатывает как для обычной PR. Отключение — через `if: github.event.pull_request.draft == false` в workflow |

## Что отдаётся CI на руки

`stdout` — текстовый отчёт (формат: «added_edges / removed_edges /
grown_cycles» + перечисление путей). См.
[regression_report.h](include/archcheck/diff/regression_report.h),
[regression_report.cpp](src/diff/regression_report.cpp).

`exit code` — gate для CI (см. выше).

`stderr` — диагностика (git-ошибки, не-репо, и т.п.).

JSON/SARIF репортёры — v0.2 (см. [docs/architecture-spec.md](architecture-spec.md)
§«Roadmap»).

## Каналы публикации в PR

У CLI-тула, который вызывают из CI-шага, есть три канала «показать
результат в UI pull request-а». Они **не взаимозаменяемы** — archcheck
рассчитан на первые два:

### 1. Check Runs / Step Summary (рекомендуемый по умолчанию)

Failed step в workflow автоматически рисует красный Check рядом с PR —
этого достаточно как минимальный gate. Сверх того, archcheck-output
рекомендуется пайпить в `$GITHUB_STEP_SUMMARY`, тогда полный отчёт
открывается одним кликом из run-страницы без отдельного artifact-download.

Когда нужен именованный check с annotations (`::warning file=...,line=...`)
— см. формат
[GitHub workflow commands](https://docs.github.com/en/actions/using-workflows/workflow-commands-for-github-actions).
Привязка annotation к строке требует line-info в `EdgeRef` — пока
reporter показывает только пути, аннотации будут file-level.

### 2. Sticky PR comment

Для архитектурного отчёта (high-level, не построчно) sticky-comment —
правильный канал. De facto стандарт —
[marocchino/sticky-pull-request-comment](https://github.com/marocchino/sticky-pull-request-comment):
использует `header:` как идентификатор для update-in-place, не плодит шум
при force-push в PR-ветку, обходится `GITHUB_TOKEN` без PAT.

Близкий аналог из мира dependency-tools —
[depcruise-pr-check](https://github.com/marketplace/actions/depcruise-pr-check),
который рисует mermaid-граф добавленных зависимостей в PR-комментарии.

Готовый snippet (требует `permissions: pull-requests: write` на job-уровне):

```yaml
- name: Run archcheck --diff
  id: archcheck
  run: |
    ./archcheck --diff "origin/${{ github.base_ref }}..HEAD" . \
      | tee archcheck-diff.txt
  continue-on-error: true

- name: Post sticky PR comment
  if: always() && github.event_name == 'pull_request'
  uses: marocchino/sticky-pull-request-comment@v2
  with:
    header: archcheck-diff   # update-in-place key; prevents comment spam on force-push
    recreate: false
    path: archcheck-diff.txt

- name: Fail the job if archcheck found a regression
  if: steps.archcheck.outcome == 'failure'
  run: exit 1
```

`header: archcheck-diff` — идентификатор для update-in-place: при
повторном push в PR-ветку action находит свой предыдущий комментарий и
обновляет его, а не создаёт новый.

`if: always() && github.event_name == 'pull_request'` — два условия:
`always()` публикует отчёт даже при регрессии (exit 1);
`github.event_name == 'pull_request'` пропускает шаг для `merge_group`
(там нет PR для комментирования).

`continue-on-error: true` на шаге archcheck + финальный fail-шаг —
стандартный паттерн «дать всем шагам отработать, но завалить job».

Полный рабочий пример: [example_archcheck_pr.yml](../.github/workflows/example_archcheck_pr.yml).

### 3. Review comments на конкретные строки

Это инструмент линтеров (clang-tidy, reviewdog `github-pr-review`).
Для archcheck **принципиально не подходит** — архитектурную регрессию
часто не к чему якорить построчно: новый цикл `A→B→C→A` может появиться
от merge-from-main, при том что сама PR ни одну из этих строк не трогала.

### 4. SARIF + GitHub Code Scanning (опционально)

`github/codeql-action/upload-sarif@v3` принимает SARIF 2.1.0 (≤10 MB gzip,
≤25 000 results) и рисует результаты в tab «Security → Code scanning».
Важное ограничение:
[бесплатно только для public-репо](https://docs.github.com/en/code-security/code-scanning/troubleshooting-sarif-uploads/ghas-required);
для private требуется GitHub Advanced Security (платная лицензия).
Поэтому SARIF для archcheck — **опциональный** канал v0.2, а не основной.

### Таксономия reviewdog (для справки)

[reviewdog README](https://github.com/reviewdog/reviewdog) делит CI-репортёры
по тому же признаку:

| Репортёр              | Куда пишет                | Применимость к archcheck |
|-----------------------|---------------------------|--------------------------|
| `github-pr-check`     | Checks-tab (file-level)   | да — основной режим      |
| `github-pr-review`    | review-comments по строкам | нет — нечего якорить    |
| `github-check`        | Checks-tab вне PR (push)  | для main-ветки           |

### Что archcheck **не** делает (и почему)

- **Не вызывает GitHub API.** Tool остаётся git-host-agnostic. Public/
  private cloud, GitLab self-hosted, Bitbucket, Gitea — везде одинаково.
  Публикация — задача CI-шага после archcheck.
- **Не комментирует PR сам.** Публикация — adapter-step в YAML (канал 2);
  готовый snippet с `marocchino/sticky-pull-request-comment` — выше и в
  [example_archcheck_pr.yml](../.github/workflows/example_archcheck_pr.yml).
- **Не разбирает `.diff` / `.patch`.** Снапшоты графа дают точный ответ;
  построчный diff не показал бы «появился новый цикл A→B→C→A», если
  ни одного из этих рёбер сама PR не добавила.

## Локальный pre-push прогон

Эквивалент CI-валидации локально:

```bash
git fetch origin main
archcheck --diff origin/main..HEAD .
echo "exit=$?"
```

Или против предыдущего коммита:

```bash
archcheck --diff HEAD~1 .          # baseline = HEAD~1, current = WORKTREE
```

Если worktree «грязный» (uncommitted-правки), они попадают в `current`
автоматически — это удобно для проверки «не сломал ли я архитектуру тем,
что сейчас в редакторе».

## Связанные задачи

- [#018](backlog/wip/018_crt_git_diff_analysis.md) — реализация `--diff` (wip, готова к коммиту).
- [#022](backlog/new/022_min_diff_merge_commit_coverage.md) — тест на PR с merge-commit-ом.
- [#023](backlog/new/023_maj_diff_skip_when_no_cpp_changes.md) — fast-path при отсутствии C/C++ изменений.
- [#025](backlog/wip/025_maj_pr_comment_integration.md) — публикация отчёта комментарием на PR.
