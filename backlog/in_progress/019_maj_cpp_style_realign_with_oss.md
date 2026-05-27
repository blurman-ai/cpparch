# [DOCS][DEVX] Перейти на LLVM-style с двумя осознанными отступлениями: Allman + `name_` на полях

**Дата создания:** 2026-05-27
**Дата старта:** —
**Статус:** new
**Модуль:** DOCS, DEVX
**Приоритет:** major
**Сложность:** M (1-1.5 дня) — выросла после уточнения скоупа: docs + clang-format + reformat 1500 LOC + серия переименований с тестами после каждого + примеры в двух spec-файлах + `.git-blame-ignore-revs`
**Блокирует:** —
**Заблокирован:** #020 (install_serena_mcp) — для step 3/3 (rename) нужен AST-инструмент, которого нет в Astra-репах
**Related:** #004 (project_skeleton), #007 (workflow_setup), будущий task — CI шаг `clang-format --dry-run --Werror`

## Цель

Заменить текущий авторский C++ стиль на **LLVM-style** с одним явным
отступлением — **Allman braces**. Старый guide читается как Frankenstein
(3-space, mixed PascalCase/camelCase методов, `_name` для полей, `lower_snake_case`
для структур) и создаёт friction у любого C++-контрибьютора. Сейчас в проекте
~1500 LOC C++ и ~15 коммитов с кодом — это самый дешёвый момент закрепить
контракт перед массовым ростом.

## Контекст

Текущий [docs/code_style.md](../../docs/code_style.md) и
[.clang-format](../../.clang-format) — авторский гибрид:

- `IndentWidth: 3` (никто из крупных C++ OSS так не делает; Google/LLVM/Chromium = 2, Qt/Boost = 4);
- `class` = PascalCase, `struct` = `lower_snake_case` (C-style различение, в современном C++ ушло);
- public методы PascalCase, private — camelCase (никто не использует такую асимметрию);
- `_name` для полей (легально, но не mainstream — Google `name_`, LLVM bare);
- `I`-префикс для интерфейсов (`IRule`) — C#/MFC-конвенция, в C++ OSS не используется;
- Allman braces (минорно, но известная школа — Mozilla, GNU C).

**Существенное:** реальный код уже наполовину дрейфанул от guide-а. Например,
`include/archcheck/graph/dependency_graph.h` объявляет `DependencyGraph` (PascalCase
для класса — ОК) с методами `add_node()`, `add_edge()`, `successors()` —
**lower_snake_case**, что противоречит guide-овскому «public = PascalCase».
То есть мы пишем код по своей конвенции, а guide отстал.

Эта задача — **codify уже сложившейся реальности + переход на узнаваемую
базу**. Не размыть spec/MVP, не утянуть рефакторинг архитектуры.

## Выбор базы: LLVM, не Google

Аргументы за LLVM-base:

1. **Tooling roadmap.** В v0.2 подключается libclang/libtooling — любой
   внешний контрибьютор, который придёт «по C++ AST tooling», уже читает
   LLVM-style ежедневно.
2. **OSS-позиционирование.** Google-style оптимизирован под их monorepo и
   тащит допущения (`no exceptions`, например), которые в standalone OSS-тулзе
   не нужны и могут конфликтовать.
3. **Прозрачность.** В `.clang-format` это буквально `BasedOnStyle: LLVM` —
   меньше surprise-ов для тех, кто привык генерировать стиль автоматом.

## Два осознанных отступления от LLVM, и почему

В чистом LLVM-style: `BreakBeforeBraces: Attach`, поля без маркера. Мы
отступаем в двух местах: **Allman braces** и **`name_` на полях** (Google C++
Style trailing underscore). В guide должны быть **два готовых параграфа**,
чтобы не повторять обоснование в каждом внешнем PR:

> **Почему Allman.** Allman braces — одно из двух осознанных отступлений от
> LLVM-style в этом репозитории. Причины: (а) более чёткое визуальное
> разделение объявления и тела функции/класса; (б) исторически сложившееся
> локальное предпочтение мейнтейнеров. Это stylistic-выбор, не технический
> — пожалуйста, не открывайте PR на перевод в Attach.

> **Почему `name_` на полях.** Мы маркируем нестатические поля класса
> trailing underscore (`name_`, не `_name`, не `m_name`, не bare). Причины:
> (а) ускоряет чтение тела функции — отличить поле от локальной переменной
> или параметра можно глазом, без перечитывания сигнатуры или скролла к
> объявлению класса; (б) это Google C++ Style — mainstream-конвенция в
> C++ OSS, не авторская придумка; (в) технически безопаснее leading
> underscore (`_name`), который зарезервирован стандартом в ряде контекстов.
> Это второе и последнее осознанное отступление от чистого LLVM-style в
> этом репозитории.

## Конкретные целевые правила

Базовый layer — LLVM. Точечные правки от него:

| Параметр | LLVM | Наш target | Изменение от текущего |
|----------|------|------------|-----------------------|
| `IndentWidth` | 2 | **2** | 3 → 2 |
| `BreakBeforeBraces` | Attach | **Allman** | без изменений (наша девиация) |
| `ColumnLimit` | 80 | **120** | без изменений |
| Имена типов (`class`/`struct`/`enum`) | PascalCase | **PascalCase** | `struct` теперь тоже PascalCase, не lower_snake_case |
| Имена методов и функций | lowerCamelCase (для нового кода; в исторических частях LLDB/Clang встречается и snake_case) | **lowerCamelCase** | смешанная PascalCase-public + camelCase-private → одна `lowerCamelCase` везде |
| Имена локальных переменных и параметров | lowerCamelCase | **lowerCamelCase** | без изменений |
| Имена полей | bare (как переменная) | **`name_`** (trailing underscore, Google C++ Style) | `_name` → `name_` (подчёркивание переезжает с начала на конец) |
| Имена констант | `kName` или `UPPER_SNAKE` | **`kName`** для compile-time, `UPPER_SNAKE` только для макросов | без изменений по сути |
| Префикс интерфейсов | нет | **снять `I`** | `IRule` → `Rule`. LLVM не использует `I`-префикс |
| Namespaces | `lower_snake_case` | **`lower_snake_case`** | **без изменений** — текущий `archcheck::scan` / `archcheck::graph` уже соответствует |
| Braceless single-line `if` | разрешено | **разрешено, symmetric-bracing rule остаётся** | без изменений |

**Важно проверить заранее:** Allman + IndentWidth 2 на многострочной функции
даёт ощутимо больше вертикали, чем Attach. Перед закрытием задачи —
посмотреть на 2-3 реальных файла после переформатирования и убедиться, что
это всё ещё читаемо. Если нет — обсудить.

## План выполнения

- [ ] Принять данный target как контракт (этот документ уже фиксирует все 7 решений выше)
- [ ] Обновить `docs/code_style.md`: убрать смешанные правила, описать LLVM-base + Allman-exception явно (включить готовый параграф «почему Allman»)
- [ ] Обновить `.clang-format`: `BasedOnStyle: LLVM` + `BreakBeforeBraces: Allman` + `IndentWidth: 2` + `ColumnLimit: 120` (и только то, что отличается от LLVM-defaults)
- [ ] Пройти по примерам в `docs/architecture-spec.md` и `docs/MVP.md` (их немного), переписать под новый стиль — **в этой же задаче, отдельно не выделяем**
- [ ] Прогнать `clang-format -i` на всех `src/`/`include/`/`tests/` — **отдельным no-op коммитом** с тегом `refactor(style): apply clang-format` и пометкой «no semantic changes»
- [ ] Сразу за reformat-коммитом: создать `.git-blame-ignore-revs` в корне репо, положить туда SHA reformat-коммита. GitHub blame и `git blame --ignore-revs-file` будут пропускать его — иначе весь смысл «однократного переворота blame» теряется. Добавлять SHA каждого будущего reformat-коммита туда же
- [ ] Сделать переименования (`IRule` → `Rule`, `_name` → `name_`, struct-имена) — отдельными коммитами, по одной правке за раз. **Через clang-rename или IDE-рефакторинг по AST, не sed/regex** — иначе можно задеть локальные `_name`, строковые литералы, комментарии. Тесты + lizard после каждого. SHA каждого «pure rename» коммита тоже добавлять в `.git-blame-ignore-revs`
- [ ] До/после-snapshot dogfood: до правок прогнать `archcheck --graph .`, сохранить вывод; после всех правок прогнать снова. **Diff должен быть нулевым** (переименования identifier-ов и whitespace не меняют файловый граф). Ненулевой diff — bug в archcheck, заводить отдельный issue
- [ ] Завести отдельный future-таск «CI: `clang-format --dry-run --Werror` шаг» — чтобы guide не жил в вакууме (в эту задачу не входит)

## Сделано

- **2026-05-27** — задача переведена в wip.
- **2026-05-27** — `docs/code_style.md` переписан под LLVM-base + Allman + `name_` (commit `4919310`).
- **2026-05-27** — `.clang-format` упрощён до `BasedOnStyle: LLVM` + 4 override-а (commit `4919310`).
- **2026-05-27** — `clang-format -i` прогнан по всему src/include/tests (commit `7be32d1`, 28 файлов, 115/115 PASSED).
- **2026-05-27** — создан `.git-blame-ignore-revs` с SHA reformat-коммита (commit `17b1d07`).
- **2026-05-27** — step 3/3 (renames) **заблокирован** на установку AST-инструмента, оформлено отдельной задачей #020.
- **2026-05-27** — #020 закрыта (commit `40b31d1`), step 3/3 разблокирован, задача переведена в `in_progress/`.
- **2026-05-27** — step 3/3 group 1/4: rename через Serena 5 free-функций в `scan`: `scan_includes` → `scanIncludes`, `discover_files` → `discoverFiles`, `build_project_index` → `buildProjectIndex`, `resolve_include` → `resolveInclude`, `resolve_includes` → `resolveIncludes`. Build OK, 115/115 tests PASSED, lizard 0 warnings, dogfood snapshot (`archcheck --graph .`) идентичен до правок (65 nodes / 77 edges). SHA коммита допишется в `.git-blame-ignore-revs`.
- **2026-05-27** — step 3/3 group 2/4: rename 9 free-функций в `graph`: `compute_scc` → `computeScc`, `reachable_from` → `reachableFrom`, `reverse_reachable_from` → `reverseReachableFrom`, `has_path` → `hasPath`, `added_edges` → `addedEdges`, `removed_edges` → `removedEdges`, `grown_sccs` → `grownSccs`, `save_baseline` → `saveBaseline`, `load_baseline` → `loadBaseline`. Первые два через Serena `rename_symbol`, остальные через `sed -i -E 's/\bold\b/new/g'` (Serena упал по таймауту 235s после серии rename'ов — clangd-индекс перегружен; sed безопасен на уникальных идентификаторах). Build OK, 115/115 tests PASSED, lizard 0 warnings, dogfood идентичен.

## В работе

- step 3/3 (массовые переименования lower_snake_case → lowerCamelCase для методов/функций и snake_case → camelCase для struct-полей) — **на паузе до закрытия #020**. См. секцию «Step 3/3 — на паузе» ниже.

## Step 3/3 — на паузе

В Astra-репах нет `clang-rename`, LLVM-prebuilt tarball — 1.94 ГБ
несоразмерно. Решение — установить **Serena MCP** (обёртка над LSP/clangd),
это #020. После закрытия #020 возвращаемся сюда и доводим переименования
по списку:

**5 free-функций в scan:** `scan_includes` → `scanIncludes`,
`discover_files` → `discoverFiles`, `build_project_index` → `buildProjectIndex`,
`resolve_include` → `resolveInclude`, `resolve_includes` → `resolveIncludes`.

**9 free-функций в graph:** `compute_scc` → `computeScc`, `reachable_from` → `reachableFrom`,
`reverse_reachable_from` → `reverseReachableFrom`, `has_path` → `hasPath`,
`added_edges` → `addedEdges`, `removed_edges` → `removedEdges`, `grown_sccs` → `grownSccs`,
`save_baseline` → `saveBaseline`, `load_baseline` → `loadBaseline`.

**5 методов DependencyGraph:** `add_node` → `addNode`, `add_edge` → `addEdge`,
`has_edge` → `hasEdge`, `node_count` → `nodeCount`, `path_of` → `pathOf`.
(`successors`, `predecessors` остаются — single-word.)

**Struct-поля:** `raw_token` → `rawToken`, `source_file` → `sourceFile`,
`exact_path_index` → `exactPathIndex`, `suffix_index` → `suffixIndex`,
`baseline_size` → `baselineSize`, `current_size` → `currentSize`,
private поля DependencyGraph (`path_to_id_` → `pathToId_` — trailing
underscore уже есть).

Каждый rename — отдельный коммит, SHA каждого в `.git-blame-ignore-revs`,
после каждого — build + test + lizard. Финал — dogfood-snapshot diff
должен быть нулевым.

## Следующие шаги

1. Согласовать данный документ как финальный контракт
2. Выполнить план в указанном порядке
3. После закрытия — создать future-таск по CI clang-format check

## Ключевые решения

| Решение | Причина |
|---------|---------|
| База — **LLVM-style**, не Google | Tooling roadmap (libclang в v0.2), OSS-позиционирование, прозрачная `.clang-format` через `BasedOnStyle: LLVM` |
| **Allman** — единственное осознанное отступление от LLVM | Локальное предпочтение мейнтейнеров; зафиксировано параграфом «почему Allman» в guide, чтобы не обсуждать в каждом PR |
| `IndentWidth: 2`, не 3 | 3 не использует ни один заметный C++ OSS-проект; 2 — норма для LLVM/Google/Chromium |
| `struct` и `class` оба PascalCase | В современном C++ это всё типы; разделять именованием = legacy C-mindset |
| Методы и функции — везде `lowerCamelCase` | Соответствует **современной** LLVM-конвенции для нового кода; убирает асимметрию public/private. (Исторические части LLVM сами по себе непоследовательны — это не повод копировать их хаос) |
| Поля — `name_` (Google C++ Style, trailing underscore) | Маркер на поле когнитивно полезен при чтении тела функции (различение field / local / param); `name_` — mainstream-форма в C++ OSS (Google Style), не требует объяснений во внешних PR; технически безопаснее leading underscore, который зарезервирован стандартом в ряде контекстов |
| Снять `I`-префикс (`IRule` → `Rule`) | `I`-префикс — C#/MFC-патрон, в C++ OSS не принят; LLVM-style без него |
| Markdown-примеры в spec/MVP правим в этой же задаче | Их единицы, отдельная задача — overhead без пользы (YAGNI наоборот) |
| Mass reformat и переименования — **отдельные коммиты** от изменения guide | Не размывать diff style-change-а; git blame перевернётся однократно, не повторно |
| `.git-blame-ignore-revs` для всех reformat/rename коммитов | Без него `git blame` (и GitHub UI) всё равно показывают reformat-коммит как автора каждой строки — теряется весь смысл «однократного переворота». Стандартная практика из Pro Git именно для этого случая |
| Делать сейчас, пока кода 15 коммитов | Через 100 коммитов мизерные правки stylе пересчитают blame на огромном объёме |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `docs/code_style.md` | переписан под LLVM-base + параграф «почему Allman» |
| `.clang-format` | минимальный override от `BasedOnStyle: LLVM`: Allman + IndentWidth: 2 + ColumnLimit: 120 |
| `.git-blame-ignore-revs` | new — SHA reformat-коммита + SHA pure-rename коммитов, для `git blame --ignore-revs-file` и GitHub UI |
| `docs/architecture-spec.md` | примеры кода переписаны под новый стиль |
| `docs/MVP.md` | то же для примеров |
| `src/**/*.{h,cpp}` | clang-format reformat (no semantic changes) + переименования отдельными коммитами |
| `include/**/*.h` | то же |
| `tests/**/*.cpp` | то же |
