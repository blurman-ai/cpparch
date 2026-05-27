# [DEVX][TOOLING] Установить и интегрировать Serena MCP для semantic-операций над C++ кодом

**Дата создания:** 2026-05-27
**Дата старта:** 2026-05-27
**Дата завершения:** 2026-05-27
**Статус:** completed
**Модуль:** DEVX, TOOLING
**Приоритет:** major
**Сложность:** S (0.5 дня)
**Блокирует:** #019 (step 3/3 — переименования методов lower_snake_case → lowerCamelCase)
**Заблокирован:** —
**Related:** #019 (cpp_style_realign), все будущие задачи требующие AST-rename / semantic refactor

## Цель

Установить и подключить **Serena MCP-сервер** к Claude Code, чтобы агенту
были доступны AST-aware операции над C++ кодом (rename symbol в первую
очередь). Это разблокирует #019 step 3/3 (массовые переименования
методов/функций) и всю последующую работу, требующую безопасных
именованных рефакторингов на уровне AST, а не текста.

## Контекст

При попытке доделать #019 step 3/3 (rename methods/functions
`lower_snake_case` → `lowerCamelCase`) выяснилось:

1. `clang-rename` (родной LLVM tool для AST-rename) **не пакуется** в
   AstraLinux/Debian репах. В пакете `clang-tools` его нет.
2. LLVM-prebuilt tarball с `clang-rename` весит **1.94 GB** — несоразмерно
   ради одного бинаря.
3. `clang-tidy 11` имеет `readability-identifier-naming --fix`, но это
   массовый rewrite по правилам стиля, а не точечный rename — риск
   задеть больше чем нужно.
4. **`clangd`** (LSP-сервер) есть в репах (clangd-11/13/15/19) и умеет
   AST-aware rename через `textDocument/rename`. Это правильный backend.

**Serena (oraios/serena)** — MCP-сервер, который оборачивает LSP в
agent-friendly tools. Поддерживает clangd для C++. Даёт агенту команды
`rename_symbol`, `find_references`, `goto_definition`, и т.п. на основе
LSP, без необходимости агенту самому говорить LSP-JSON-RPC.

Альтернативы рассматривались (Python-скрипт-обёртка над clangd, generic
lsp-mcp, clang-tidy identifier-naming auto-fix) и отвергнуты в пользу
Serena: меньше home-grown кода, переиспользуется на будущих задачах,
mainstream-выбор в MCP-экосистеме на момент написания.

## План выполнения

- [x] Установить **clangd-19** через apt (`sudo apt install clangd-19`). Сделать `update-alternatives` симлинк `clangd -> clangd-19`, чтобы CLI вызовы работали без версии
- [x] Сгенерировать `build/debug/compile_commands.json` (флаг `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` в cmake; уже включён в текущем checkout-е) — clangd его прочитает
- [x] **Добавлено сверх плана:** симлинк `compile_commands.json` → `build/debug/compile_commands.json` в корне репо. Без него clangd не находит флаги компиляции, references/rename не работают
- [x] Установить Serena через **uvx** (PyPI `serena` — не тот пакет; правильно через `uvx --from git+https://github.com/oraios/serena serena ...`)
- [x] Прописать Serena в MCP config Claude Code. Использован новый формат: `claude mcp add-json -s user serena '{...}'` → запись в `~/.claude.json`. Старого `~/.claude/mcp_servers.json` в текущей версии Claude Code больше нет
- [x] Перезапускать Claude Code не пришлось — Serena подцепилась в текущей сессии
- [x] **Добавлено сверх плана:** перерегистрировать с `--context ide-assistant` (вместо дефолтного `desktop-app`) и `--enable-web-dashboard false --enable-gui-log-window false` — оптимизация под CLI-агента
- [x] **Добавлено сверх плана:** `.serena/` в `.gitignore` — Serena сама создаёт `.serena/project.yml` при первой активации, версионировать не нужно
- [x] Smoke-test: rename одного identifier в `tests/`. **Частично:** rename переименовал только определение, не use-site. Откатил через Edit, файл чистый (`git diff` пуст). Не баг setup-а — это засада с clangd background-index, **зафиксирована в `docs/dev/serena_setup.md` → "Засады"**. Полный smoke-rename отложен до прогрева индекса (см. ниже)
- [x] Документировать в `docs/dev/serena_setup.md` — что поставлено, как настроено, ограничения, деинсталляция
- [x] **Smoke-test reroll (новая сессия 2026-05-27):** `rename_symbol DependencyGraph/successors → successorsSmoke` через Serena → 12 changes (declaration + definition + 10 use-sites в `src/graph/algorithms.cpp`, `baseline.cpp`, `diff.cpp`, `tests/unit/graph/dependency_graph_test.cpp`). Grep подтвердил: единственные оставшиеся `successors` — в строковых литералах `TEST_CASE("successors ...")`, что корректно (AST строк не трогает). **Revert** через обратный rename сломался (1 change — только в .h, clangd закэшировал старую карту location-ов после первого rename) — это **новая разновидность Засады 1**: повторный rename подряд в одной сессии до переиндексации. Откатил `git checkout -- src/ include/ tests/`, рабочая копия чистая. Дополнено в `docs/dev/serena_setup.md` — Засада 1, пункт 5 + рекомендация добивать недодеп через `replace_content` regex
- [x] **Permission:** добавлен `"mcp__serena__*"` в `~/.claude/settings.json` → `permissions.allow`, чтобы вызовы Serena не требовали подтверждения (user request 2026-05-27)

## Сделано

- clangd-19 19.1.7 (1.astra2) установлен из Astra-репов, симлинк через `update-alternatives` (priority 190)
- Symlink `compile_commands.json` → `build/debug/compile_commands.json` в корне репо (требование clangd для cross-file references; не очевидно из README Serena)
- Serena MCP зарегистрирована в `~/.claude.json` (user scope) с флагами `--project-from-cwd --context ide-assistant --enable-web-dashboard false --enable-gui-log-window false`
- Serena подцепилась в текущей сессии без рестарта Claude Code, `Status: ✓ Connected`
- В текущей сессии Serena уже работает: `get_symbols_overview`, `find_symbol`, `get_diagnostics_for_file` возвращают корректные данные; clang-tidy warnings проходят через LSP
- `.serena/` добавлена в `.gitignore`
- `docs/dev/serena_setup.md` — полная инструкция (установка с нуля, флаги, две засады, использование, деинсталляция)

## В работе

- (пусто; задача закрыта)

## Следующие шаги

1. Возврат к **#019 step 3/3** — массовый rename `lower_snake_case` → `lowerCamelCase` через Serena. Workflow на каждый rename: `find_symbol` → (опционально `find_referencing_symbols` для прогрева) → `rename_symbol` → `grep`-валидация → если недодеп, `replace_content` regex `\bold\b → new` → build + ctest + lizard. Каждая логическая группа (scan-функции / graph-функции / DependencyGraph-методы / struct-поля) — один коммит, SHA в `.git-blame-ignore-revs`

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Использовать Serena, не свой Python LSP-driver | Меньше home-grown кода; переиспользуется на будущих semantic-refactor задачах; mainstream-выбор в MCP-экосистеме |
| clangd-19, не более старые версии | Чем новее — тем лучше rename работает на C++20 templates / concepts. 19 — последняя из доступных в Astra-репах |
| Не качать LLVM-prebuilt-tarball (2 ГБ) | Несоразмерно ради одного бинаря; clangd-19 deb-пакет на порядок легче |
| Не использовать clang-tidy identifier-naming --fix | Это массовый rewrite по правилам, не точечный rename — риск задеть больше чем нужно |
| Smoke-test в `tests/` перед боевым прогоном | Дёшево проверить что Serena действительно понимает scope/AST, а не делает text-based replace |
| Symlink `compile_commands.json` в корне (не сторонний симлинк в `build/`) | clangd ищет в `<project>/` и `<project>/build/`. В нашей раскладке файл живёт в `build/debug/` — нужен мост. Симлинк в корне — минимум магии, понятно любому разработчику |
| Scope MCP-регистрации = `user`, не `project` | Чтобы не плодить `.mcp.json` в репо. `--project-from-cwd` снимает проблему "Serena не знает про какой проект речь" — она находит его по CWD текущей сессии Claude Code |
| `--context ide-assistant` вместо дефолтного `desktop-app` | desktop-app подразумевает GUI-чат и расширенное summarization. `ide-assistant` — корректный context для CLI-агентов (Claude Code, Codex, Gemini) согласно докам Serena |
| `--enable-web-dashboard false` | Claude Code — CLI; браузерный дашборд только пытался бы открыться в фоне и захламлять процесс |
| Smoke-test провален как rename, но засчитан как diagnostic-валидация | clangd-warnings проходят через Serena → инструмент работает. Полный rename — после прогрева индекса, не блокер для setup-задачи |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `~/.claude.json` | new entry — Serena MCP (user scope) |
| `docs/dev/serena_setup.md` | new — полная инструкция |
| `.gitignore` | + `.serena/` |
| `compile_commands.json` (корень репо) | new симлинк → `build/debug/compile_commands.json` (в gitignore) |
| `.serena/` (автогенерирована Serena) | new локально, не коммитится |
| (системные) `clangd-19` через apt | new |
| (системные) Serena кэш в `~/.cache/uv/` | new |
| (системные) `/etc/alternatives/clangd` → `clangd-19` | new (update-alternatives priority 190) |

