Завершить задачу из бэклога: проверить что сделано, зафиксировать коммитом если нужно, перенести файл в completed/ с заполненными итогами.

Argument: task ID (NNN) или паттерн имени (e.g. `/fix-issue 042`, `/fix-issue sf9`).

## Steps

1. **Найти задачу** — в `backlog/wip/` по ID (3-значный, ноль-паддинг) или паттерну:
   ```
   Glob(pattern="{NNN}_*.md", path="backlog/wip")
   ```
   Если не найдено в wip/ — проверить `backlog/new/` и `backlog/future/`. Если нашлось там — предупредить пользователя: «Задача не взята в работу; перевести в wip и стартовать?» (см. `/issue`).

2. **Прочитать файл задачи.** Вывести:
   - Заголовок + путь.
   - **Цель** — что должно быть достигнуто.
   - **Сделано** — что уже закрыто.
   - **Следующие шаги** — что осталось.

3. **Проверить git-статус** по отношению к задаче:
   ```bash
   git log --oneline -10
   git status --short
   git diff --stat HEAD
   ```
   - Есть ли незакоммиченные изменения, связанные с задачей?
   - Есть ли уже коммиты с упоминанием `#NNN` или темы задачи?

4. **Оценить полноту:**
   - Все пункты из **Следующие шаги** закрыты?
   - Есть ли `fixtures/<rule>/pass/` и `fixtures/<rule>/fail_*/` для всех затронутых правил? (требование из CLAUDE.md: без фикстур правило не считается реализованным)
   - Тесты проходят? Запустить:
     ```bash
     /home/localadm/projects/cpparch/build/debug/tests/archcheck_tests
     ```
     Если бинарь не собран — предупредить, не блокировать.

5. **Коммит, если нужен:**
   Если есть незакоммиченные изменения, относящиеся к задаче — коммитить в соответствии со скиллом `/commit`:

   a. Прогнать lint-gate (как в `/commit` шаг 3):
      ```bash
      { git diff --name-only HEAD; git ls-files --others --exclude-standard; } \
        | grep -E '\.(h|cpp)$' | xargs -r clang-format --dry-run --Werror

      cppcheck --enable=warning,performance,portability \
               --inline-suppr --error-exitcode=1 \
               --suppress=missingIncludeSystem --quiet \
               -I include src/ include/

      lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/
      ```
      Если хотя бы одна проверка упала — **стоп**: вывести ошибки и не двигаться дальше до исправления.

   b. Прогнать coverage gate:
      ```bash
      bash scripts/check_coverage.sh
      ```
      FAIL → показать вывод, спросить: продолжить (принять warn) или остановиться.

   c. Собрать сообщение по Conventional Commits (тип/scope по таблицам из `/commit`):
      ```
      fix(<scope>): <summary> (#NNN)

      [optional body: что и почему]

      AI-Assisted: Claude
      Verified: <build / build+tests / manual>
      Risk: low|med|high (<причина>)
      Co-Authored-By: Claude <noreply@anthropic.com>
      ```

   d. **Показать сообщение пользователю и ждать подтверждения.** Запрошены правки — переделать и показать снова.

   e. Застейджить только файлы, относящиеся к задаче, и создать коммит через heredoc:
      ```bash
      git add <relevant files>
      git commit -m "$(cat <<'EOF'
      <commit message>
      EOF
      )"
      ```

   f. `git push origin master`.

6. **Заполнить файл задачи итогами** (секции, которых ещё нет или неполны):

   ```markdown
   ## Как работает
   <одно-два предложения о механике реализации — для будущего читателя>

   ## Ключевые решения
   - <решение 1>: <почему>

   ## Изменённые файлы
   - `src/...` — <что сделано> (commit <SHA>)

   ## Итог
   **Статус:** completed
   **Дата завершения:** <YYYY-MM-DD>
   ```

7. **Переместить файл в `backlog/completed/`:**
   ```bash
   git mv backlog/wip/{NNN}_*.md backlog/completed/
   ```
   Закоммитить этот перенос отдельным `chore`-коммитом:
   ```
   chore(tasks): close #NNN — <task title>
   ```
   Push.

8. **Проверить GitHub issue** (если репо имеет GH remote):
   ```bash
   gh issue view {N} 2>/dev/null
   ```
   Если issue существует и открыт — спросить пользователя: закрыть через `gh issue close {N} --comment "<summary>"`?
   Это опциональный шаг; не блокировать если gh недоступен.

9. **Отчёт:**
   - Путь к completed-файлу.
   - Список коммитов, относящихся к задаче (SHA + однострочник).
   - Статус тестов (если запускались).
   - Статус GitHub issue (если проверялся).
