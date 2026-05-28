# [CI][DOC] PR-видимость отчёта: sticky-comment + Check annotations + merge_group в example workflow

**Дата создания:** 2026-05-27
**Дата старта:** 2026-05-28
**Статус:** wip
**Модуль:** CI, DOC
**Приоритет:** major
**Сложность:** S (1-2 часа)
**Блокирует:** —
**Заблокирован:** — (#018 даёт текстовый отчёт; этого достаточно)
**Related:** #018 (git_diff_analysis), [docs/ci_integration.md](docs/ci_integration.md)

## Цель

Превратить [.github/workflows/example_archcheck_pr.yml](.github/workflows/example_archcheck_pr.yml)
из «здесь лежит artifact с отчётом» в **эталонный** CI-сниппет, который
показывает результат прямо в UI PR-а без download-artifact-step-а:

1. archcheck-output → `$GITHUB_STEP_SUMMARY` (виден одним кликом из
   run-страницы).
2. archcheck-output → sticky PR-comment через
   [marocchino/sticky-pull-request-comment](https://github.com/marocchino/sticky-pull-request-comment)
   (виден прямо в conversation-tab PR-а; update-in-place при push-ах).
3. Workflow триггерится **и** на `pull_request`, **и** на `merge_group` —
   иначе merge queue зависает в ожидании check-а ([docs.github.com — merge
   queue](https://docs.github.com/en/repositories/configuring-branches-and-merges-in-your-repository/configuring-pull-request-merges/managing-a-merge-queue)).
4. Скрыть от draft-PR через `if: github.event.pull_request.draft == false`
   (опционально).

## Контекст

Текущий пример просто uploads-artifact (1 строка `tee archcheck-diff.txt`
+ upload-step). Чтобы увидеть отчёт, ревьюер должен открыть Actions-tab,
найти прогон, скачать zip, распаковать. UX — никакой.

Research индустрии (см. отчёт research-агента 2026-05-27 в истории):

- **Sonar PR decoration** — Quality Gate как именованный Check, не комментарий.
- **dependency-cruiser** — `depcruise-pr-check` action, рисует mermaid-граф
  изменённых зависимостей прямо в PR-comment.
- **reviewdog таксономия** — `github-pr-check` (Checks-tab, file-level)
  подходит для архитектурного отчёта; `github-pr-review` (по строкам) —
  принципиально не подходит, т.к. новый цикл A→B→C→A может появиться от
  merge-from-main без правок в самих строках.
- **ArchUnit FreezingArchRule** — baseline-файл, не комментарий; ортогонально.

Sticky-comment — де-факто стандарт для tool-output в PR (CodeClimate,
size-limit-action, bundlewatch). `marocchino/sticky-pull-request-comment`
использует `header:` как идентификатор для update-in-place и обходится
`GITHUB_TOKEN` без PAT.

archcheck сам **не должен** ходить в GitHub API — это нарушит
git-host-agnostic свойство. Вся публикация — на стороне CI-шага.

## План выполнения

- [x] Расширить `example_archcheck_pr.yml`:
  - [x] Триггеры `pull_request` + `merge_group` с одним и тем же job-name (для merge queue).
  - [x] Step: capture archcheck output в файл + tee в stdout.
  - [x] Step: append `archcheck-diff.txt` в `$GITHUB_STEP_SUMMARY` через `cat archcheck-diff.txt >> "$GITHUB_STEP_SUMMARY"`.
  - [x] Step: post sticky-comment через `marocchino/sticky-pull-request-comment@v2` с `header: archcheck-diff` и `recreate: false`. Skip на `merge_group` event.
  - [x] `if: always()` на comment-step, чтобы регрессии (exit 1) тоже попадали в комментарий.
  - [x] `permissions: pull-requests: write` на job-уровне (минимум для sticky-comment).
- [x] Обновить шапку example_workflow с пояснением, какие три канала показа отчёта в нём задействованы.
- [x] В [docs/ci_integration.md](docs/ci_integration.md) §«Каналы публикации в PR» §2 — добавить прямой `uses:` snippet.
- [ ] Smoke: создать PR с заведомой регрессией в этом репо, убедиться что появляется sticky-comment.

## Что НЕ входит в эту задачу

- **Annotations с file:line** (`::warning file=...,line=...`). Сейчас
  `EdgeRef` location-метаданных не несёт; реальный file:line подъедет
  когда scanner начнёт писать координаты в edge — отдельная задача.
- **SARIF репортёр.** Третий канал из docs §«Каналы публикации»;
  v0.2 roadmap.
- **Distribution как marketplace composite-action** (`uses: archcheck/archcheck-action@v1`).
  Это требует pre-built бинаря и релизного пайплайна — отдельная история
  ближе к v0.2.

## Сделано

- `example_archcheck_pr.yml` полностью переписан: `merge_group` триггер,
  `permissions: pull-requests: write`, `$GITHUB_STEP_SUMMARY`, sticky-comment
  через `marocchino/sticky-pull-request-comment@v2`. Паттерн
  `continue-on-error: true` + финальный fail-шаг — отчёт публикуется
  даже при регрессии.
- `docs/ci_integration.md` §2 «Sticky PR comment» — добавлен полный ready-made
  snippet с объяснением каждого поля.
- Устаревшие ссылки на `backlog/new/025_...` заменены на `backlog/wip/025_...`.

## В работе

- (пусто)

## Следующие шаги

1. Пройти `act` или быстро PR-тест на форке, чтобы убедиться что `marocchino` отрабатывает на этом репо.
2. Если sticky-comment получится слишком длинным (много добавленных рёбер), добавить truncation в `archcheck` (`--diff --top N`) — отдельная задача.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Sticky-comment (а не плодящийся) | Force-push в PR-ветку не должен спамить ленту обсуждения. Header-ключ `archcheck-diff` обеспечивает update-in-place |
| `marocchino/sticky-pull-request-comment` как named-dependency | De facto стандарт (CodeClimate, size-limit, bundlewatch используют его); обходится `GITHUB_TOKEN` без PAT |
| `merge_group` рядом с `pull_request` | Без него GitHub Merge Queue блокируется на отсутствующем check-е. Цена — один лишний триггер, профит — поддержка merge queue из коробки |
| `$GITHUB_STEP_SUMMARY` плюс sticky-comment | Step Summary — для read-only ревьюера, кто открывает run; sticky-comment — для PR-treda. Не дублирование, разная аудитория |
| archcheck не публикует сам | Сохраняет git-host-agnostic (GitLab/Bitbucket/Gitea/self-hosted работают без модификации tool). Публикация — adapter-step в YAML |
| `permissions: pull-requests: write` явно | GitHub-default workflow permissions с осени 2024 — read-only; без явного grant sticky-comment не запишется |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `.github/workflows/example_archcheck_pr.yml` | расширение: triggers, step-summary, sticky-comment, permissions |
| `docs/ci_integration.md` | §«Sticky PR comment» — добавить готовый snippet с `marocchino/...` |
