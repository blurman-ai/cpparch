# Git workflow — archcheck

OSS-стандарты процесса. Каждый блок — со ссылкой на источник, чтобы у любого external contributor не было вопросов «почему так».

## Branching: GitHub Flow

Источник: [GitHub Flow](https://docs.github.com/en/get-started/quickstart/github-flow).

- `master` всегда зелёный.
- Feature-ветка: `<type>/<NNN>-<short-slug>` — например `feat/004-project-skeleton`, `docs/006-spec-refactor`, `chore/007-workflow-setup`.
- `<type>` ∈ `feat / fix / docs / refactor / chore / test / build / perf / ci` — те же, что в Conventional Commits.
- `<NNN>` — ID задачи из `backlog/`. Нет задачи — сначала завести.
- После merge ветка удаляется автоматически (`Settings → General → Pull Requests → Automatically delete head branches`).

### Direct push для admin

Admin репо добавлен в *Bypass list* ruleset-а master (Settings → Rules). `git push origin master` напрямую разрешён. PR остаётся опцией для рискованных изменений, не обязательным шагом.

Force-push в master заблокирован для всех, включая admin — намеренно, защита от случайного переписывания истории.

## Commits: Conventional Commits 1.0

Источник: [conventionalcommits.org/v1.0.0](https://www.conventionalcommits.org/en/v1.0.0/).

Формат:

```
<type>(<scope>): <description>

[optional body]

[optional trailers]
```

**Types:**

| Type | Когда |
|---|---|
| `feat` | новая функциональность продукта |
| `fix` | багфикс в коде продукта |
| `docs` | только документация (README, docs/, спека) |
| `refactor` | изменение кода без изменения поведения |
| `test` | тесты или фикстуры |
| `build` | build-система (CMake, зависимости) |
| `ci` | CI-конфиги (GitHub Actions) |
| `perf` | оптимизация |
| `chore` | рутина: инфраструктура репо, скилы, переименования, конфиги |

**Scopes** для archcheck (по подсистемам):

| Scope | Подсистема |
|---|---|
| `config` | YAML loader, Config struct |
| `graph` | компонентный граф, циклы, метрики |
| `scan` | include / clang scanners |
| `rules/sf` | Core Guidelines SF.* |
| `rules/lakos` | циклы, god-headers, CCD/ACD/NCCD |
| `rules/martin` | I/A/D метрики |
| `rules/custom` | пользовательские pattern-правила |
| `report` | text / json / sarif reporters |
| `cli` | main, аргументы, exit codes |
| `fixtures` | `fixtures/` |
| `build` | CMake, упаковка |
| `docs` | общие документы |
| `spec` | архитектурная спецификация |
| `claude` | `.claude/` (settings, skills) |
| `tasks` | `backlog/` |
| `process` | git workflow, CHANGELOG, релиз-процесс |

Если изменения охватывают несколько scope-ов — указать самый репрезентативный или опустить scope.

**Subject:** ≤ 72 символа, lowercase, императив (`add`, не `added`), без точки в конце. Можно ссылаться на задачу: `(#NNN)`.

**Trailers** (поверх Conventional Commits — парсерами игнорируются, нужны для AI-аудита):

```
AI-Assisted: Claude
Verified: <как проверял — autotest / manual / build / nothing>
Risk: low|med|high (причина)
Co-Authored-By: Claude <noreply@anthropic.com>
```

## Versioning: SemVer 2.0

Источник: [semver.org/spec/v2.0.0](https://semver.org/spec/v2.0.0.html).

- Pre-1.0 (текущая фаза): `0.x.y`. SemVer §4 разрешает breaking-изменения в MINOR; избегаем, но не запрещаем.
- v1.0 — когда стабилизированы CLI-флаги, exit codes, JSON-схема отчёта, формат YAML-конфига, формат baseline.
- Что считается breaking change — см. [`docs/architecture-spec.md`](../architecture-spec.md) секция «Stability contract».

## Tags: annotated `vX.Y.Z`

```bash
git tag -a v0.1.0 -m "Release 0.1.0"
git push origin master --follow-tags
```

Lightweight tags (`git tag v0.1.0` без `-a`) не использовать — теряется метаинформация (автор, дата, сообщение).

## Changelog: Keep a Changelog 1.1

Источник: [keepachangelog.com/v1.1.0](https://keepachangelog.com/en/1.1.0/).

[`CHANGELOG.md`](../../CHANGELOG.md) в корне репо. Секции: `Added / Changed / Deprecated / Removed / Fixed / Security`. Сверху всегда `[Unreleased]`, при релизе — фиксируется как `[X.Y.Z] - YYYY-MM-DD`, и заводится новая пустая `[Unreleased]`. Пишется руками, не авто-генерируется — курируемый changelog читается людьми лучше.

## Известные ограничения lint-gate

### clang-format: расхождение локальной и CI-сборки

CI использует `clang-format-18` из Ubuntu apt. Локальная среда (AstraLinux) содержит `clang-format-18.1.8 (9.astra6)` — другую сборку того же major-версии. Изредка они форматируют сложные конструкции (многострочные тернарные, длинные цепочки `<<`) по-разному.

**Принятое решение:** принять как риск. Если CI падает с `-Wclang-format-violations` на файле, который lint-gate принял локально — гнать `clang-format -i` и пушить отдельный `fix(ci)` коммит.

Полное устранение требует единого бинарника (Docker с Ubuntu), что слишком тяжело для локальной разработки.

## Release-процесс (черновик до v1.0)

1. Все коммиты для релиза влиты в master.
2. `CHANGELOG.md`: переименовать `[Unreleased]` → `[X.Y.Z] - YYYY-MM-DD`, добавить пустую `[Unreleased]`.
3. Bump версию в build-системе (когда появится CMakeLists.txt).
4. Коммит: `chore(release): bump to X.Y.Z`.
5. Тег: `git tag -a vX.Y.Z -m "Release X.Y.Z"`.
6. Push: `git push origin master --follow-tags`.
7. GitHub Release: через UI или `gh release create vX.Y.Z`.
