Create a new task document in `backlog/new/`.

Argument: short task name in snake_case (e.g. `/create-task sf9_cycle_detection`).

## Имя файла

```
backlog/new/NNN_<priority>_<argument>.md
```

- **NNN** — 3-значный ID, `max(существующих) + 1` по всем подкаталогам (`new/`, `wip/`, `completed/`).
- **priority** — 3-буквенный код: `blk` / `crt` / `maj` / `min`.

Пример: `006_maj_spec_refactor.md`.

## Steps

1. **Найти следующий ID:**
   ```
   Glob(pattern="???_*.md", path="backlog/new")
   Glob(pattern="???_*.md", path="backlog/wip")
   Glob(pattern="???_*.md", path="backlog/completed")
   ```
   Из всех собранных имён извлечь первые 3 цифры, найти max, прибавить 1. Pad to 3 digits (`007`, не `7`).

2. **Проверить дубли** — найти похожие имена:
   ```
   Glob(pattern="*{argument}*.md", path="backlog/new")
   Glob(pattern="*{argument}*.md", path="backlog/wip")
   Glob(pattern="*{argument}*.md", path="backlog/completed")
   ```
   Если найдено — показать пути, спросить: продолжать с существующим или создать новый.

3. **Найти связанные задачи** — Grep по ключевым словам из имени по `backlog/new/`, `backlog/wip/`, `backlog/completed/`. Если есть связанные — предложить указать ID как `Related: #NNN` в шапке.

4. **Спросить пользователя**, если не очевидно из имени:
   - **Модуль / тег**: `CONFIG` / `GRAPH` / `SCAN` / `RULES` / `REPORT` / `CLI` / `FIXTURES` / `BUILD` / `DOCS`. Можно комбинировать (`RULES][SF`).
   - **Приоритет**: `blocker` / `critical` / `major` / `minor`. Если пользователь уже указал — не переспрашивать. Без указаний — по умолчанию `minor`.
     - Mapping в код: `blocker → blk`, `critical → crt`, `major → maj`, `minor → min`.
   - **Цель** — одно предложение.

5. **Создать файл** `backlog/new/NNN_<code>_{argument}.md`:
   ```markdown
   # [MODULE] Task title

   **Дата создания:** {today YYYY-MM-DD}
   **Дата старта:** —
   **Статус:** new
   **Модуль:** {module}
   **Приоритет:** {priority}
   **Сложность:** unknown
   **Блокирует:** —
   **Заблокирован:** —
   **Related:** —

   ## Цель

   {one sentence goal}

   ## Контекст

   {description или "TODO: описать контекст"}

   ## План выполнения

   - [ ] Шаг 1
   - [ ] Шаг 2

   ## Сделано

   - (пусто)

   ## В работе

   - (пусто)

   ## Следующие шаги

   1. ...

   ## Ключевые решения

   | Решение | Причина |
   |---------|---------|
   | ... | ... |

   ## Изменённые файлы

   | Файл | Изменение |
   |------|-----------|
   | ... | ... |

   ## Fixtures (если правило)

   - [ ] `fixtures/{rule}/pass/`
   - [ ] `fixtures/{rule}/fail_*/`
   ```

6. Сообщить путь созданного файла и присвоенный ID.

## Когда начинаешь работу — выбор формата

Канон процесса — [`docs/dev/git_workflow.md`](../../docs/dev/git_workflow.md).

- **Direct push в master** — для рутинных задач (мелкие фиксы, одиночные коммиты, документация). Admin репо в bypass list, PR не нужен.
- **Feature-ветка** `<type>/<NNN>-<slug>` — для значимой работы (новый модуль, refactor, многоэтапная фича). Type из Conventional Commits (`feat / fix / docs / refactor / chore / test / build / perf / ci`). Слияние — через PR в UI или прямой merge.

Пример: задача `#015` с приоритетом `crt`, модуль `RULES/SF`, тип `feat` → ветка `feat/015-sf9-cycle-detection`.

## Переезды между состояниями

- **new → wip:** `git mv backlog/new/NNN_... backlog/wip/NNN_...`, проставить `**Дата старта:** YYYY-MM-DD`, статус → `wip`.
- **wip → completed:** `git mv backlog/wip/NNN_... backlog/completed/NNN_...`, статус → `done`, дописать в конец:
  - **Дата завершения:** YYYY-MM-DD
  - **Как работает** (принцип / алгоритм / поток данных)
  - **Чем управляется** (конфиг, флаги, env vars)
  - **С чем связана** (зависимости, модули)
  - **Диагностика** (логи, метрики, ключевые места для отладки)

При смене приоритета — переименовать файл (новый 3-буквенный код), ID не меняется.

## Ссылки между задачами

В `**Блокирует:**`, `**Заблокирован:**`, `**Related:**` использовать формат:
```
**Блокирует:** #004 (project_skeleton), #002 (github_actions_ci)
```

Имя в скобках — для удобства чтения, ID — устойчивый якорь.
