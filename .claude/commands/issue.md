Continue working on a task from the local backlog.

Argument: task ID (NNN) or filename pattern (e.g. `/issue 008`, `/issue 8`, `/issue dependency_graph`).

Steps:

1. Parse argument:
   - If matches `^\d+[a-z]?$` (digits, optionally followed by a single letter like `008a`): task ID.
   - Otherwise: filename substring pattern (snake_case).

2. Find matching file in **all** backlog dirs — `new/`, `wip/`, `future/`, `completed/`:
   ```
   Glob(pattern="{NNN}_*.md", path="backlog/new")
   Glob(pattern="{NNN}_*.md", path="backlog/wip")
   Glob(pattern="{NNN}_*.md", path="backlog/future")
   Glob(pattern="{NNN}_*.md", path="backlog/completed")
   ```
   For ID: `{NNN}` is the argument zero-padded to 3 digits (e.g. `8` → `008`). If no match, retry without padding (e.g. `8_*.md`) — some legacy IDs may exist.

   For filename pattern: `*{pattern}*.md` in the same four dirs.

   IMPORTANT: Use `path` parameter for directory, `pattern` for filename only. Never put `backlog/` into `pattern`.

3. If multiple matches across dirs: show the list (path + first line) and ask the user to clarify.

4. Read the matched file.

5. **If file was found in `backlog/completed/`**:
   - Treat as read-only documentation. Tell the user: «Задача завершена (`backlog/completed/`), показываю как справку».
   - Summarize sections **Цель**, **Как работает**, **Ключевые решения**, **Изменённые файлы** and stop. Do not propose continuation unless the user asks.
   - Exception: if the user explicitly says «переоткрыть» / «вернуть в wip», move the file to `backlog/wip/`, set `**Статус:** wip` and `**Дата старта:** <today>`.

6. **If file is in `backlog/new/` or `backlog/future/`**:
   - Ask the user: «Перевести в wip и стартовать?» — if yes, `git mv` to `backlog/wip/` and add `**Дата старта:** <today>`, set status to `wip`.
   - If no, treat as read-only briefing.

7. **Optionally** try GitHub issue with the same number (only if argument is a numeric ID and the repo has a GH remote):
   ```bash
   gh issue view {N} --comments 2>/dev/null
   ```
   - If the issue exists and has comments not reflected in the local file under `## Комментарии из GitHub` — append them in format `**[YYYY-MM-DD] @user:** текст`, skip empty/bot noise, tell the user how many were added.
   - If `gh` fails (no remote, no issue, gh not installed) — silently skip. GitHub integration is best-effort; the backlog file is the source of truth.

8. Extract and display key context from the local file:
   - Заголовок (первая строка `# ...`) + путь к файлу.
   - **Цель** (зачем задача).
   - **Сделано** (текущий статус).
   - **В работе** / **Следующие шаги**.
   - **Ключевые решения**.
   - **Изменённые файлы** (с SHA коммитов, если есть).
   - **Блокирует / Заблокирован / Related** — если есть, прорезолвить ID в имена соседних задач.

9. Resume work based on **В работе** / **Следующие шаги**.

10. Update progress via `/checkpoint` as you go.

Example:
```
/issue 8
→ Found: backlog/wip/008_blk_dependency_graph_foundation.md
→ Status: wip (started 2026-05-12)
→ Цель: построить in-memory компонент-граф из include-scanner вывода
→ Сделано:
  - 008a: API skeleton (commit f2182bd)
  - 008b: naive extraction (commit b5f9a1b)
→ Следующие шаги:
  1. Подключить 008c (skip comments) к скан-пайплайну
  2. Покрыть фикстурой fixtures/include_scanner/comments/
→ Continuing from step 1…
```
