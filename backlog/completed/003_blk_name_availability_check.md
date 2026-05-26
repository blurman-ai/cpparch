# [DOCS] Проверить доступность имени archcheck

**Дата создания:** 2026-05-26
**Дата старта:** 2026-05-26
**Дата завершения:** 2026-05-26
**Статус:** done
**Модуль:** DOCS
**Приоритет:** blocker
**Сложность:** S (час-два)
**Блокирует:** #004 (project_skeleton), #002 (github_actions_ci)
**Заблокирован:** —
**Related:** —

## Цель

Подтвердить или отвергнуть имя `archcheck` (бинарь) / `cpparch` (рабочее имя репо) до начала кода — переименование после первого релиза дорого.

## Контекст

Из `docs/architecture-spec.md` §"Ключевые риски", item 4 (на момент задачи): name availability на GitHub/PyPI/crates.io/Homebrew не проверена. README использовал `cpparch`, спека — `archcheck`. Нужно было сойтись на одном.

## План выполнения

- [x] GitHub: проверить, есть ли `github.com/*/archcheck` с C++ контекстом
- [x] PyPI: `pip index versions archcheck`
- [x] crates.io: проверить, занят ли крейт `archcheck`
- [x] Homebrew: проверить formula
- [x] npm — для полноты (некоторые dev-tools публикуются и туда)
- [x] Trademarks: беглый поиск
- [x] Если `archcheck` занят — кандидаты-замены (`archlint`, `cpparch`, `archdep`, `lakos`)
- [x] Зафиксировать финальное имя в README.md и architecture-spec.md

## Сделано

- **2026-05-26** — Проверка обоих priority-имён прошла через WebFetch / WebSearch. Результат:

  | Имя | GitHub | PyPI | crates.io | Homebrew | npm | Trademark/brand | Итог |
  |---|---|---|---|---|---|---|---|
  | **archcheck** | ✅ org/user свободен; ~10 одноимённых репо у частных юзеров, все 0–2 stars, разные языки (Python/Java/JS/Go/HTML), C++ нет | ✅ 404 | ✅ 404 | ✅ 404 | ✅ 404 | ✅ конфликтов не найдено | ✅ **взять** |
  | **cpparch** | ✅ свободно; ~10 одноимённых репо, личные C++-архивы; `willjoseph/cpparch` 2015 — заброшен, 0 stars | ✅ 404 | ✅ 404 | ✅ 404 | ✅ 404 | ⚠️ нет прямого конфликта, но созвучие с `cppcheck` создаёт фоновый шум в поиске и наборе | ⚠️ риск |

  Запасные кандидаты (`archlint`, `archguard-cpp`, `cpp-arch`, `archdep`, `lakos`) проверять не понадобилось — `archcheck` чист.

- **2026-05-26** — Принято: **`archcheck`** как имя продукта.

- **2026-05-26** — Bulk-замена `cpparch` → `archcheck` (с вариантами `Cpparch` → `Archcheck`, `CPPARCH` → `ARCHCHECK`) во всех `.md` репо, кроме:
  - `.claude/commands/findings.md` — там путь к memory `~/.claude/projects/-home-localadm-projects-cpparch/...` соответствует имени локальной директории, не имени продукта.
  - `backlog/completed/007_*.md` — историческая запись, не редактируется задним числом.

- **2026-05-26** — Зачищено в `docs/architecture-spec.md`:
  - Preamble «Рабочее название. Альтернативы…» убран.
  - «Ключевые риски» item 4 (про имя) удалён; пункты 5/6/7 перенумерованы в 4/5/6.
  - «Следующие шаги»: пункт 1 (проверить имя) и пункт 3 (решить вопрос двух-бекендной схемы) удалены — оба закрыты; остальные перенумерованы.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| `archcheck`, не `cpparch` | Семантика прозрачнее; нет коллизии с известным `cppcheck` (легко перепутать в поиске и при наборе); совпадает с авторитетным написанием в спеке. `cpparch` тоже технически свободен, но создаёт фоновый шум. |
| Не делать защитную регистрацию squatting-аккаунтов на PyPI/crates/npm | Pre-1.0 OSS, без публикации — нет смысла. Если кто-то займёт пока мы строим v0.1 — пересмотрим. |
| Имя локальной директории `cpparch` оставляем | Переименование сломало бы memory-путь Claude и все hard-coded paths в shell-конфиге. Это косметика, не влияет на продукт. |
| GitHub-репо рекомендуется переименовать в `archcheck` | Settings → General → Repository name. GitHub автоматически redirect-ит старый URL. Это ручное действие пользователя — не делается через git. |

## Изменённые файлы

| Файл | Изменение | Commit |
|------|-----------|--------|
| `README.md` | `# cpparch` → `# archcheck`, все упоминания | (текущий) |
| `docs/architecture-spec.md` | preamble очищен, item 4 рисков удалён, «Следующие шаги» обновлены | (текущий) |
| `CHANGELOG.md` | `archcheck` в шапке; URL `[Unreleased]` ссылается на новый репо-URL | (текущий) |
| `CLAUDE.md` | заменены все упоминания | (текущий) |
| `AGENTS.md` | заменены все упоминания | (текущий) |
| `docs/MVP.md`, `docs/code_style.md`, `docs/code_quality.md`, `docs/dev/git_workflow.md` | заменены все упоминания | (текущий) |
| `docs/research/README.md` и `docs/research/rules/*.md` | заменены все упоминания | (текущий) |
| `backlog/README.md`, `backlog/new/001*`, `backlog/new/005*` | заменены все упоминания | (текущий) |

## Как работает

После #003 все внутренние документы используют одно имя — `archcheck`. Принцип такой:

1. **Имя продукта (бинаря и проекта)** — `archcheck`. Используется в README, спеке, CHANGELOG, документации, commit messages.
2. **Имя локальной директории** — остаётся `cpparch` для совместимости с уже настроенными tool-путями (Claude memory, IDE-конфиг). Это не влияет на пользователя продукта.
3. **GitHub-репо** — пока называется `cpparch`. Рекомендация: пользователю переименовать через Settings → General → Repository name → `archcheck`. GitHub автоматически настроит redirect со старого URL.
4. **Зарезервированные namespace-ы** на PyPI/crates.io/npm/Homebrew — не занимаем. Если кто-то нас опередит до публикации — пересмотрим (см. backup-имена в плане).

## Чем управляется

- Согласованность имени в репо — проверяется через `grep -rn cpparch --include="*.md"`. Должно возвращать только:
  - `.claude/commands/findings.md` (memory-путь, ожидаемо).
  - `backlog/completed/007_crt_workflow_setup.md` (исторические референсы, ожидаемо).
  - `backlog/completed/003_blk_name_availability_check.md` — этот файл (документация процесса).
- При повторном sed-проходе в будущем — добавлять новые исключения через `! -path` в команде `find`.

## С чем связана

- **#004 (project_skeleton)** — теперь разблокирована. Когда создадим `CMakeLists.txt`, имя бинаря будет `archcheck`. Использовать как target name.
- **#002 (github_actions_ci)** — разблокирована. CI-workflow будет ссылаться на `archcheck` бинарь.
- **#006 (spec_refactor)** — синхронизация имени с README/спекой попадает в одну из его подзадач (общий рефакторинг); часть этой работы уже выполнена тут.

## Диагностика

Команды для верификации:

```bash
# нет рудиментов имени в кодовой базе (кроме ожидаемых исключений):
grep -rn "cpparch" --include="*.md" | grep -v findings.md | grep -v completed/007 | grep -v completed/003

# имя в шапке README — archcheck:
head -1 README.md

# имя в шапке спеки — archcheck:
head -1 docs/architecture-spec.md

# в CHANGELOG.md URL ссылается на правильный репо (после ручного переименования на GitHub):
grep blurman-ai/ CHANGELOG.md
```
