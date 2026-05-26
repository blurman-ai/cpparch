Create git commit with auto-detected changes and (если есть) результатами тестов.

Argument (optional): commit type prefix (e.g. `/commit fix` или `/commit test`).

Steps:
1. Run `git status` to see modified/untracked files.
2. Run `git diff` to see changes.
3. If a test log exists at `build/test_log.txt` или `build/Testing/Temporary/LastTest.log` — прочитать через Read и извлечь:
   - Имя сьюта.
   - Список тестов и счётчик PASSED/FAILED.
   Если лога нет — пункт пропускается, никакого Tests-Result в коммит.
4. Проанализировать изменения и собрать сообщение:
   - **Type**: `feat` / `fix` / `refactor` / `docs` / `test` / `build` / `chore` — по типу файлов.
   - **Module tag** в квадратных скобках после типа:
     - `[CONFIG]` — `config/`, YAML loader
     - `[GRAPH]` — `graph/`, cycle detection, metrics
     - `[SCAN]` — `scan/`, include / clang scanners
     - `[RULES][SF]` — Core Guidelines SF.* rules
     - `[RULES][LAKOS]` — cycles, god-headers, depth, CCD/ACD/NCCD
     - `[RULES][MARTIN]` — I/A/D метрики
     - `[RULES][CUSTOM]` — пользовательские pattern-правила
     - `[REPORT]` — text / json / sarif reporters
     - `[CLI]` — main, аргументы, exit codes
     - `[FIXTURES]` — `fixtures/`, test corpora
     - `[BUILD]` — CMakeLists, CI, упаковка
     - `[DOCS]` — `docs/`, README
     - `[DOCS][CLAUDE]` — `.claude/`, CLAUDE.md, скилы
     - `[DOCS][TASKS]` — `backlog/`, управление задачами
     - Если ни один тег не подходит — придумать (`[YAML]`, `[CACHE]` и т.п.).
     - Пропускать тег только если изменения охватывают несколько несвязанных модулей.
   - Concise summary (1-2 предложения).
   - При наличии тестов — секция с результатами.
5. **Показать сообщение пользователю и ЖДАТЬ подтверждения.**
   - Если запрошены правки — переписать и показать снова.
6. Аккуратно застейджить только релевантные файлы:
   - Никаких `.env`, ключей, секретов.
   - Бинарники — только если пользователь явно попросил.
   - Связанные `.h` и `.cpp` — вместе.
7. Создать коммит в формате:
   ```
   <type>: [TAG] краткое описание

   [подробности если нужны]

   [если были тесты:]
   Tests-Run: test1, test2, test3
   Tests-Result: X/Y PASSED (Z%)

   AI-Assisted: Claude
   Verified: <как проверял — autotest / manual / build / nothing>
   Risk: low|med|high (причина)

   Co-Authored-By: Claude <noreply@anthropic.com>
   ```
8. `git status` после коммита — убедиться, что прошло.

## AI-трейлеры

| Trailer | When required |
|---------|---------------|
| `AI-Assisted:` | Любое участие ИИ в коде |
| `Verified:` | Всегда при AI-Assisted |
| `Risk:` | Изменение правил, графа, графовых метрик; удаление кода |
| `Co-Authored-By:` | Если ИИ писал значительную часть |

**Risk:**
- `low` — документация, комментарии, мелкие правки.
- `med` — новый код, рефакторинг, новые правила без изменений семантики.
- `high` — изменения в графовой логике, дефолтных правилах, exit codes, формате baseline.

## Разделять коммиты

- Код отдельно от fixtures (другая аудитория ревью).
- Правила отдельно от инфраструктуры.
- Рефакторинг отдельно от фич.
- Documentation-only — отдельный коммит.

## Перед удалением legacy

1. Tag или baseline-commit.
2. Тесты, покрывающие функционал, должны быть.
3. В коммите — ссылка на tag для отката.

IMPORTANT:
- Heredoc для сохранения форматирования.
- Match existing code style.
- Только реально изменённые файлы.
- Если непонятно что включать — спросить.
