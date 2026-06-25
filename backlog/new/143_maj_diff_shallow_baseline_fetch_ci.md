# [CI][DOCS] --diff CI: shallow base-fetch вместо fetch-depth: 0

**Дата создания:** 2026-06-25
**Статус:** new
**Модуль:** CI / DOCS
**Приоритет:** major
**Сложность:** S
**Блокирует:** adoption на больших репозиториях (медленный checkout = первое плохое впечатление)
**Заблокирован:** —
**Related:** #142 (release binary), #144 (unresolved-baseline footgun), docs/ci_usage.md, docs/ci_integration.md

## Цель

Убрать `fetch-depth: 0` (полная история git) из рекомендуемого `--diff` CI-сниппета.
На больших репозиториях полная история — это десятки секунд / сотни МБ на ровном
месте, хотя `archcheck --diff` нужен **один срез base-дерева**, а не история.

## Контекст / находка

`archcheck --diff origin/<base>..HEAD` делает только две git-операции
([src/git/git_state.cpp](../../src/git/git_state.cpp)):
- `git diff --name-only A B` — сравнение **двух деревьев**, не диапазона коммитов;
- `git worktree add --detach <tmp> <ref>` — checkout **одного дерева**.

Ни `rev-list`, ни `merge-base`, ни обхода диапазона `A..B`. `parseRevspec` просто
режет строку по `..`. Значит нужен **только base-снапшот** (один коммит + дерево),
а `fetch-depth: 0` — тупой over-fetch.

Проверено локально (shallow clone depth=1 + `git fetch --depth=1 origin <base>`):
в графе 2 коммита (не вся история), `merge-base` недоступен, а
`archcheck --diff origin/<base>..HEAD .` строит реальный baseline-граф
(`baseline_nodes: 284`), `gate: ok`, exit 0.

## Что сделать

- `.github/workflows/example_archcheck_pr.yml`: убрать `fetch-depth: 0`, добавить
  шаг depth=1 дофетча base-рефа (`git fetch --no-tags --depth=1 origin
  "+refs/heads/$BASE:refs/remotes/origin/$BASE"`), оставить revspec
  `origin/<base>..HEAD`. Поддержать base из `github.base_ref` и
  `github.event.merge_group.base_ref`.
- `docs/ci_usage.md` (Сценарий 2): тот же сниппет; объяснить, почему истории не надо.
- `docs/ci_integration.md`: сделать shallow base-fetch **дефолтом** (а не «для
  очень больших монорепо»); `fetch-depth: 0` оставить как «самый простой, но
  тяжёлый» fallback. Исправить неверную строку про «shallow без baseline → exit 2»
  (фактически — warning + пустое дерево + возможный ложный gate, см. #144).

## Acceptance

- [ ] Рекомендуемый `--diff` сниппет не содержит `fetch-depth: 0`.
- [ ] Есть шаг depth=1 дофетча base; revspec резолвится в CI.
- [ ] Доки объясняют: нужен base-снапшот, не история; merge-base не требуется.
- [ ] YAML валиден; проверено на shallow-песочнице (cpparch и/или leadline).

## Не делать

- Не менять C++ (поведение `--diff` уже корректно). Ужесточение нерезолва — #144.
