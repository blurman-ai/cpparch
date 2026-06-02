Create git commit with auto-detected changes following Conventional Commits 1.0.

Канон процесса — [`docs/dev/git_workflow.md`](../../docs/dev/git_workflow.md). Скил приводит коммит в соответствие с ним.

Argument (optional): подсказка типа (например `/commit fix` или `/commit test`).

## Steps

1. `git status` — посмотреть изменённые/неотслеживаемые файлы.
2. `git diff` — посмотреть содержимое изменений.
3. **Auto-format** — переформатировать все изменённые/новые `.h`/`.cpp` файлы:

   ```bash
   { git diff --name-only HEAD; git ls-files --others --exclude-standard; } \
     | grep -E '\.(h|cpp)$' | xargs -r clang-format -i
   ```

4. **Lint-gate** — запустить на изменённых `.h`/`.cpp` файлах:

   ```bash
   # clang-format: ВСЁ дерево src include tests — как format-check джоба CI.
   # Не только изменённые файлы: ловим дрейф форматирования в нетронутых
   # файлах, иначе CI упадёт на том, что локально пропустили.
   find src include tests \( -name '*.h' -o -name '*.cpp' \) -print \
     | xargs -r clang-format --dry-run --Werror --style=file

   # cppcheck: всегда на src/ include/ (дёшево, ~1 сек) — как static-analysis CI
   cppcheck --enable=warning,performance,portability \
            --inline-suppr --error-exitcode=1 \
            --suppress=missingIncludeSystem --quiet \
            -I include src/ include/

   # lizard: complexity/length пороги (как в CI)
   lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/
   ```

   Если хотя бы одна проверка упала — **остановиться**, вывести ошибки и не продолжать до исправления.

5. **Build + test + smoke gate** — реальная сборка и прогон, как `build`-джоба CI.
   Это главный «не падать на CI» гейт: coverage-гейт собирает только
   инструментированный gcc-билд, а тут проверяем, что код честно компилируется,
   тесты зелёные и бинарь запускается.

   ```bash
   # Configure + build Debug (build/debug переиспользуется между запусками).
   cmake -B build/debug -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug
   cmake --build build/debug

   # Тесты (создаёт build/debug/Testing/Temporary/LastTest.log для блока ниже):
   ( cd build/debug && ctest --output-on-failure )

   # Smoke-тест бинаря — ровно как на CI (форму не ужесточать: CI принимает
   # как exit 2, так и exit 0 — `unknown` трактуется как путь сканирования):
   ./build/debug/src/archcheck --version
   ./build/debug/src/archcheck --help
   ./build/debug/src/archcheck unknown || test $? -eq 2
   ```

   Любой шаг упал (не собралось / тест красный / smoke не прошёл) —
   **остановиться**, вывести вывод и не продолжать до исправления.

   > CI дополнительно собирает на `clang-18` и `clang-18-libc++`. Это тяжело
   > гонять перед каждым коммитом, поэтому в гейт не входит. Если правка
   > рискует по переносимости (новые `#include`, шаблоны, `std::`-фичи) —
   > прогнать вручную:
   > `CXX=clang++-18 cmake -B build/clang -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug && cmake --build build/clang`.

6. **Coverage gate** — запустить скрипт покрытия:

   ```bash
   bash scripts/check_coverage.sh
   ```

   Скрипт использует отдельный `build/coverage` (с `ARCHCHECK_ENABLE_COVERAGE=ON`),
   прогоняет все тесты, собирает lcov с branch-coverage и проверяет пороги:
   - lines ≥ 70%, functions ≥ 60%, branches ≥ 40%

   Если результат **FAIL** — показать вывод скрипта пользователю и спросить:
   продолжить (пользователь сознательно принимает низкое покрытие) или остановиться.
   При `Verified:` trailer писать `build+coverage` если прошло, `build+coverage(warn)` если пользователь продолжил несмотря на FAIL.

   Пороги можно переопределить: `MIN_LINES=50 bash scripts/check_coverage.sh`.

7. Тест-лог уже создан шагом 5 (`build/debug/Testing/Temporary/LastTest.log`) — прочитать, извлечь имя сьюта, список тестов, PASSED/FAILED.
8. Проанализировать изменения и собрать сообщение по схеме Conventional Commits:

   ```
   <type>(<scope>): <subject>

   [optional body]

   [optional trailers]
   ```

9. **Показать сообщение пользователю и ЖДАТЬ подтверждения.** Запрошены правки — переписать и показать снова.
10. Аккуратно застейджить только релевантные файлы:
   - Никаких `.env`, ключей, секретов.
   - Бинарники — только если пользователь явно попросил.
   - Связанные `.h` и `.cpp` — вместе.
11. Создать коммит через heredoc.
12. `git push origin master` (direct push разрешён admin-у; если работа на feature-ветке — `git push -u origin <branch>`).
13. `git status` после — убедиться, что прошло.

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
