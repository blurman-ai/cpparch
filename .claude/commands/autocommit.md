Create git commit WITHOUT asking for confirmation — fire-and-forget вариант `/commit`.

Канон процесса — [`docs/dev/git_workflow.md`](../../docs/dev/git_workflow.md). Скил приводит коммит в соответствие с ним.

**Назначение.** Пользователь вводит команду и уходит. Скил сам собирает сообщение, стейджит, коммитит и пушит — **без показа и без ожидания подтверждения**. Отличие от `/commit` ровно в этом: шаг «показать и ждать» убран.

Argument (optional): подсказка типа (например `/autocommit fix` или `/autocommit test`).

## Жёсткое правило автономности

Тебя не будет рядом, чтобы ответить на вопрос. Поэтому:

- **Упавший гейт — сначала ПОЧИНИТЬ.** lint упал или coverage FAIL → попытаться исправить причину (отформатировать, поправить cppcheck/lizard-замечание, добить покрытие тестом). Починилось и гейт прошёл — **коммитить**. После фикса гейт прогнать заново.
- **Не чинится автономно — СТОП.** Если фикс требует решения человека, рискован или непонятен — не коммитить, оставить вывод ошибок и описать, что пробовал. Лучше нет коммита, чем мусор в истории.
- **Никаких интерактивных вопросов.** Развилка, требующая решения человека (что включать, секреты в диффе, неоднозначный скоуп) — остановиться и описать проблему, не гадать.

## Steps

1. `git status` — изменённые/неотслеживаемые файлы.
2. `git diff` — содержимое изменений.
3. **Lint-gate** — на изменённых `.h`/`.cpp`:

   ```bash
   { git diff --name-only HEAD; git ls-files --others --exclude-standard; } \
     | grep -E '\.(h|cpp)$' | xargs -r clang-format --dry-run --Werror

   cppcheck --enable=warning,performance,portability \
            --inline-suppr --error-exitcode=1 \
            --suppress=missingIncludeSystem --quiet \
            -I include src/ include/

   lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/
   ```

   Любая упала — **СТОП**, вывести ошибки, не продолжать.

4. **Coverage gate**:

   ```bash
   bash scripts/check_coverage.sh
   ```

   Пороги: lines ≥ 70%, functions ≥ 60%, branches ≥ 40%.
   Результат **FAIL** — **СТОП**, показать вывод скрипта, не коммитить.
   (В автономном режиме не продолжаем «сознательно принимая низкое покрытие» — некому принять.)
   При успехе — `Verified: build+coverage`.

5. Если есть тест-лог (`build/test_log.txt`, `build/Testing/Temporary/LastTest.log`) — прочитать, извлечь сьют, тесты, PASSED/FAILED. Нет лога — секция тестов пропускается.
6. Собрать сообщение по Conventional Commits (схема, типы, scope, subject, body, trailers — **идентично `/commit`**, см. [`commit.md`](commit.md)).
7. **БЕЗ показа и подтверждения.** Сразу аккуратно застейджить только релевантные файлы:
   - Никаких `.env`, ключей, секретов. Заметил секрет в диффе — **СТОП**.
   - Бинарники — не стейджить (в автономном режиме не у кого спросить).
   - Связанные `.h` и `.cpp` — вместе.
8. Создать коммит через heredoc.
9. Push: `git push origin master` (admin direct push); на feature-ветке — `git push -u origin <branch>`.
10. `git status` после — убедиться, что прошло. Кратко отчитаться: хэш, ветка, push-результат.

## Сообщение коммита

Полностью как в [`commit.md`](commit.md): Conventional Commits 1.0, таблицы type/scope, subject ≤ 72, body «что и почему», блок `Tests-Run`/`Tests-Result` при наличии лога, и трейлеры:

```
AI-Assisted: Claude
Verified: <build+coverage / autotest / manual / build>
Risk: low|med|high (причина)
Co-Authored-By: Claude <noreply@anthropic.com>
```

## IMPORTANT

- Heredoc для форматирования.
- Только реально изменённые файлы.
- Force-push в master заблокирован — не обходить.
- Непонятно что включать / секрет в диффе / неоднозначный скоуп → **СТОП и описать**, не гадать.
