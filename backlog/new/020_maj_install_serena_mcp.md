# [DEVX][TOOLING] Установить и интегрировать Serena MCP для semantic-операций над C++ кодом

**Дата создания:** 2026-05-27
**Дата старта:** —
**Статус:** new
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

- [ ] Установить **clangd-19** через apt (`sudo apt install clangd-19`). Сделать `update-alternatives` симлинк `clangd -> clangd-19`, чтобы CLI вызовы работали без версии
- [ ] Сгенерировать `build/debug/compile_commands.json` (флаг `-DCMAKE_EXPORT_COMPILE_COMMANDS=ON` в cmake; уже включён в текущем checkout-е) — clangd его прочитает
- [ ] Установить Serena через **uvx** или pipx: `uvx serena-agent` для разового запуска или зарегистрировать как persistent MCP-сервер
- [ ] Прописать Serena в MCP config Claude Code (`~/.claude/mcp_servers.json` или эквивалент текущей версии Claude Code). Указать `cwd` = корень репо, `language_server` = clangd
- [ ] Перезапустить Claude Code session, убедиться что Serena tools видны (`mcp__serena__rename_symbol` или похожее)
- [ ] Smoke-test: переименовать один малозначимый identifier в `tests/` через Serena, убедиться что (а) меняются все use-site, (б) комменты не задеты, (в) build+тесты зелёные. Откатить smoke-rename
- [ ] Документировать в `docs/dev/serena_setup.md` (или внутри `docs/dev/`) короткую инструкцию: что поставлено, как настроено, как использовать
- [ ] Сразу после закрытия — вернуться к **#019 step 3/3**: прогнать через Serena все переименования из инвентаризации (5 функций scan, 9 функций graph, 5 методов DependencyGraph, ~7 struct-полей)

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Установка clangd-19 + Serena
2. Smoke-test
3. Документация setup-а
4. Возврат к #019 step 3/3

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Использовать Serena, не свой Python LSP-driver | Меньше home-grown кода; переиспользуется на будущих semantic-refactor задачах; mainstream-выбор в MCP-экосистеме |
| clangd-19, не более старые версии | Чем новее — тем лучше rename работает на C++20 templates / concepts. 19 — последняя из доступных в Astra-репах |
| Не качать LLVM-prebuilt-tarball (2 ГБ) | Несоразмерно ради одного бинаря; clangd-19 deb-пакет на порядок легче |
| Не использовать clang-tidy identifier-naming --fix | Это массовый rewrite по правилам, не точечный rename — риск задеть больше чем нужно |
| Smoke-test в `tests/` перед боевым прогоном | Дёшево проверить что Serena действительно понимает scope/AST, а не делает text-based replace |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `~/.claude/mcp_servers.json` (или эквивалент) | new entry — Serena MCP |
| `docs/dev/serena_setup.md` | new — короткая инструкция установки |
| (системные) `clangd-19` через apt | new |
| (системные) Serena через uvx/pipx | new |
