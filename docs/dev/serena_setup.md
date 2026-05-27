# Serena MCP setup для archcheck

Serena ([oraios/serena](https://github.com/oraios/serena)) — MCP-сервер, который
оборачивает LSP в agent-friendly tools. Для archcheck используется как
**AST-aware refactoring backend** (rename symbol, find references, find
implementations) поверх **clangd-19**.

Задача #020 в `backlog/`. Урок дня: `rename_symbol` через LSP корректно
переименовывает все use-site **только после прогрева background-index у clangd**
— см. раздел "Засады" ниже.

## Что должно быть установлено

| Компонент | Источник | Версия на момент setup |
|-----------|----------|------------------------|
| `clangd-19` | `apt install clangd-19` (Astra / Debian репы) | 19.1.7 (1.astra2) |
| `clangd` симлинк → `clangd-19` | `update-alternatives` | — |
| `uvx` (uv) | пользовательский, `~/.local/bin/uvx` | 0.11.7 |
| Serena | через `uvx --from git+...`, **не** через PyPI | git HEAD |
| Claude Code MCP entry `serena` | `~/.claude.json`, user scope | — |
| Симлинк `compile_commands.json` → `build/debug/compile_commands.json` | в корне репо | — |

## Установка с нуля

### 1. clangd-19

```bash
sudo apt-get install -y clangd-19
sudo update-alternatives --install /usr/bin/clangd clangd /usr/bin/clangd-19 190
clangd --version   # ожидаем "clangd version 19.x"
```

В Astra/Debian репах есть пакеты clangd-11, 13, 15, 19. Выбран 19 как самый
свежий — лучше работает с C++20 templates/concepts, на которых построен archcheck.

`update-alternatives` нужен потому, что пакет `clangd-19` ставит только
`/usr/bin/clangd-19`, без unsuffixed-симлинка. Serena (через solidlsp) вызывает
бинарь `clangd` без версии.

### 2. compile_commands.json в корне репо

Без этого clangd видит файлы, но ничего не знает про флаги компиляции —
references/rename работать не будут.

```bash
# CMake уже генерирует build/debug/compile_commands.json
# (флаг -DCMAKE_EXPORT_COMPILE_COMMANDS=ON в CMakeLists.txt).
# clangd ищет файл в корне репо или в build/ — делаем симлинк:
ln -sfn build/debug/compile_commands.json compile_commands.json
```

Симлинк в `.gitignore` (строка `compile_commands.json`). `build/debug/` — это
Debug-конфигурация, которая всегда собирается локально для разработки.

### 3. Serena через uvx

```bash
uvx --from git+https://github.com/oraios/serena serena --help
```

Первый запуск качает зависимости в кэш `uv`. Последующие — мгновенные. PyPI-пакет
`serena` существует, но это другой проект и не содержит executable; нужен
именно git-source `oraios/serena`.

### 4. Регистрация в Claude Code (user scope)

```bash
claude mcp add-json -s user serena '{
  "command": "uvx",
  "args": [
    "--from", "git+https://github.com/oraios/serena",
    "serena", "start-mcp-server",
    "--project-from-cwd",
    "--context", "ide-assistant",
    "--enable-web-dashboard", "false",
    "--enable-gui-log-window", "false"
  ]
}'
```

Запись попадает в `~/.claude.json`. Ключевые флаги:

- `--project-from-cwd` — Serena сама находит проект из CWD текущей сессии
  Claude Code (ищет `.serena/project.yml`, `.git`, fallback на CWD). Прямо в
  доках Serena помечен как "Intended for CLI-based agents like Claude Code".
  Без этого пришлось бы либо регистрировать MCP per-project (`-s project`),
  либо в каждой сессии вручную вызывать `activate_project`.
- `--context ide-assistant` — корректирует system-prompt Serena под CLI-агента
  (вместо дефолтного `desktop-app`, который предполагает GUI-чат).
- `--enable-web-dashboard false`, `--enable-gui-log-window false` —
  Claude Code это CLI, GUI-окна и браузерный дашборд только мешают.

Проверка:

```bash
claude mcp get serena
# ожидаем: Status: ✓ Connected
```

### 5. Первая активация в проекте

При первом запросе к Serena из cpparch (или ручном вызове `activate_project`)
Serena создаст `.serena/` в корне проекта:

```
.serena/
├── cache/                 # внутренний кэш Serena
├── memories/              # project-specific memories Serena (отдельно от Claude memory)
├── project.yml            # авто-сгенерированная конфигурация
└── project.local.yml      # template для локальных переопределений (пустой)
```

Вся папка `.serena/` в `.gitignore` — генерируется каждым разработчиком локально.

## Засады

### Засада 1 — references/rename не работают сразу после старта

**Симптом:** `find_referencing_symbols` возвращает `{}` для public методов,
`rename_symbol` переименовывает только определение (1 change), а use-site
остаются со старым именем → сборка ломается.

**Причина:** clangd работает в два этапа. На AST-уровне (текущий файл) всё
готово сразу. Cross-file references требуют **background index**, который
clangd строит в фоне, складывая в `.cache/clangd/index/`. Для свежего
project до завершения индексации `textDocument/references` возвращает пусто, и
LSP-rename ограничивается локальным scope-ом.

**Что делать:**

1. **Всегда валидировать rename grep-ом** до сборки. Шаблон:
   ```bash
   grep -rn '<old_name>' src/ include/ tests/   # должно быть 0 совпадений
   grep -rn '<new_name>' src/ include/ tests/   # совпадает с ожидаемым
   ```
2. Если rename переименовал < ожидаемого числа референсов — откатить
   (`git checkout -- <file>`) и подождать, пока clangd закончит индексацию.
   Прогресс можно увидеть в `.cache/clangd/index/` (растёт количество
   `.idx` файлов).
3. Прогрев индекса переживает сессии — `.cache/clangd/` сохраняется между
   запусками Claude Code. Холодный rename — только проблема первого старта на
   новом checkout-е.
4. Для критичных массовых rename (как в #019 step 3/3) — сначала прогреть
   индекс (например, открыть пару файлов через `find_referencing_symbols` на
   известных public-символах и дождаться непустого ответа), потом rename.
5. **Повторные rename подряд в одной сессии** тоже триггерят этот эффект:
   после первого `rename_symbol` файлы на диске обновились, но clangd ещё
   не перечитал их и не знает новых location-ов. Следующий rename того же
   символа (например, revert через переименование обратно) увидит только
   declaration в `.h`. Workflow: после каждого `rename_symbol` валидировать
   grep-ом; если остались недопереименованные use-site вне строк/комментов
   — **не дожидаться переиндексации, добить через `mcp__serena__replace_content`
   regex `\bold_name\b → new_name`** (идентификаторы в проекте уникальные,
   `\b` безопасен). Это быстрее ждать индекс и не блокирует следующий rename.

### Засада 2 — анонимные namespace в tests/

`find_referencing_symbols` на функциях из анонимного namespace в `.cpp`-файле
тестов (Catch2) часто возвращает пусто, даже когда индекс прогрет. Helper-rename
внутри тестового файла — делать через `Edit` / `replace_content` (regex), не
через `rename_symbol`. Это ограничение clangd при работе с translation-unit-local
символами.

## Использование

После setup в любой сессии Claude Code, открытой из cpparch, доступны MCP-tools:

- `mcp__serena__rename_symbol` — AST-rename public-символа (см. засады выше)
- `mcp__serena__find_symbol` / `mcp__serena__find_referencing_symbols` —
  навигация по AST
- `mcp__serena__find_implementations` — найти реализации виртуальных методов
- `mcp__serena__get_symbols_overview` — иерархия символов в файле без чтения
  всего тела
- `mcp__serena__replace_symbol_body` — заменить тело символа
- `mcp__serena__safe_delete_symbol` — удалить символ с проверкой ссылок

Полный список — `mcp__serena__get_current_config`.

## Деинсталляция

```bash
claude mcp remove serena -s user
sudo apt remove clangd-19
sudo update-alternatives --remove clangd /usr/bin/clangd-19
rm compile_commands.json   # симлинк, не файл
rm -rf .serena/ .cache/clangd/
```

Serena сама нигде в `/usr/` не лежит — кэш `uv` в `~/.cache/uv/` чистится
обычным `uv cache clean`.
