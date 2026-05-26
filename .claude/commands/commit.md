Create git commit with auto-detected changes following Conventional Commits 1.0.

Канон процесса — [`docs/dev/git_workflow.md`](../../docs/dev/git_workflow.md). Скил приводит коммит в соответствие с ним.

Argument (optional): подсказка типа (например `/commit fix` или `/commit test`).

## Steps

1. `git status` — посмотреть изменённые/неотслеживаемые файлы.
2. `git diff` — посмотреть содержимое изменений.
3. Если есть тест-лог (`build/test_log.txt`, `build/Testing/Temporary/LastTest.log`) — прочитать, извлечь имя сьюта, список тестов, PASSED/FAILED. Нет лога — секция тестов пропускается.
4. Проанализировать изменения и собрать сообщение по схеме Conventional Commits:

   ```
   <type>(<scope>): <subject>

   [optional body]

   [optional trailers]
   ```

5. **Показать сообщение пользователю и ЖДАТЬ подтверждения.** Запрошены правки — переписать и показать снова.
6. Аккуратно застейджить только релевантные файлы:
   - Никаких `.env`, ключей, секретов.
   - Бинарники — только если пользователь явно попросил.
   - Связанные `.h` и `.cpp` — вместе.
7. Создать коммит через heredoc.
8. `git push origin master` (direct push разрешён admin-у; если работа на feature-ветке — `git push -u origin <branch>`).
9. `git status` после — убедиться, что прошло.

## Type — что выбирать

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

## Scope — где жить

| Scope | Подсистема |
|---|---|
| `config` | YAML loader, Config struct |
| `graph` | компонентный граф, циклы, метрики |
| `scan` | include / clang scanners |
| `rules/sf` | Core Guidelines SF.* правила |
| `rules/lakos` | циклы, god-headers, CCD/ACD/NCCD |
| `rules/martin` | I/A/D метрики |
| `rules/custom` | пользовательские pattern-правила |
| `report` | text / json / sarif reporters |
| `cli` | main, аргументы, exit codes |
| `fixtures` | `fixtures/`, test corpora |
| `build` | CMake, упаковка |
| `docs` | общие документы (README, docs/) |
| `spec` | архитектурная спецификация конкретно |
| `claude` | `.claude/` (settings, скилы) |
| `tasks` | `backlog/` (управление задачами) |
| `process` | git workflow, CHANGELOG, релиз-процесс |

Несколько scope-ов в одном коммите — выбрать самый репрезентативный или опустить scope. Не подходит ни один — придумать кратко.

## Subject (первая строка)

- ≤ 72 символа.
- Lowercase, без точки в конце.
- Императив: `add`, не `added`/`adds`.
- Можно ссылаться на задачу: `(#NNN)`.

## Body

После пустой строки. Что и почему, не как (как видно в diff). Опционально.

При наличии тестов — отдельный блок:

```
Tests-Run: test1, test2, test3
Tests-Result: X/Y PASSED (Z%)
```

## Trailers (поверх Conventional Commits, для AI-аудита)

```
AI-Assisted: Claude
Verified: <как проверял — autotest / manual / build / nothing>
Risk: low|med|high (причина)
Co-Authored-By: Claude <noreply@anthropic.com>
```

| Trailer | Когда обязателен |
|---|---|
| `AI-Assisted:` | Любое участие ИИ в коде |
| `Verified:` | Всегда при `AI-Assisted` |
| `Risk:` | Изменение правил, графа, графовых метрик; удаление кода |
| `Co-Authored-By:` | Если ИИ писал значительную часть |

**Risk:**
- `low` — документация, комментарии, мелкие правки.
- `med` — новый код, рефакторинг, новые правила без изменений семантики.
- `high` — изменения в графовой логике, дефолтных правилах, exit codes, формате baseline, формате конфига.

## Разделять коммиты

- Код отдельно от fixtures (другая аудитория ревью).
- Правила отдельно от инфраструктуры.
- Рефакторинг отдельно от фич.
- Documentation-only — отдельный коммит.

## Перед удалением legacy

1. Tag или baseline-commit.
2. Тесты, покрывающие функционал, должны быть.
3. В коммите — ссылка на tag для отката.

## Пример

```
fix(graph): correct cycle detection on self-loops (#015)

Self-loop A → A был не виден в DFS из-за raннего возврата на visited-узле.
Теперь self-loop детектится отдельной проверкой до DFS.

Tests-Run: graph_self_loop, graph_two_node_cycle, graph_dag
Tests-Result: 3/3 PASSED (100%)

AI-Assisted: Claude
Verified: autotest
Risk: med (поведение графовой логики)

Co-Authored-By: Claude <noreply@anthropic.com>
```

## IMPORTANT

- Heredoc для сохранения форматирования.
- Match existing code style.
- Только реально изменённые файлы.
- Если непонятно что включать — спросить.
- Force-push в master заблокирован — не пытаться обходить.
