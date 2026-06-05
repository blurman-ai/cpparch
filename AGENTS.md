# AGENTS.md

Руководство для coding agents, работающих в репозитории `archcheck`.

> Для Claude Code основной источник инструкций — [CLAUDE.md](CLAUDE.md).
> Этот файл — краткая агент-нейтральная выжимка; при расхождении деталей
> приоритет у канонических docs, на которые он ссылается.

## Язык и стиль работы

- Отвечай пользователю по-русски, тепло и по-человечески.
- Пиши коротко, предметно, без лишней болтовни.
- Приоритет источников при конфликте:
  - **что реально shipped** → [CHANGELOG.md](CHANGELOG.md) (authoritative по версиям);
  - **продуктовая рамка и архитектура** → [docs/architecture-spec.md](docs/architecture-spec.md);
  - затем [docs/MVP.md](docs/MVP.md), затем [README.md](README.md).

## Состояние проекта

- **v0.1 в активной разработке.** Репозиторий реализован и собирается: есть
  `src/`, `include/archcheck/`, `tests/`, `CMakeLists.txt`, CI на GitHub Actions.
- Ядро-пайплайн работает: fast preprocessor scan → include graph → default rules
  → reporters. Бинарь ships SF.7/8/9 + Lakos (cycles, god-headers, chain length),
  baseline, drift и diff. Точный shipped-набор — в [CHANGELOG.md](CHANGELOG.md).
- Имя продукта **зафиксировано**: `archcheck` (бинарь `archcheck`). Рабочая
  директория остаётся `cpparch` ради стабильности путей.

## Что это за продукт

`archcheck` — **CI-first CLI** для проверки архитектурных инвариантов C++ проектов.

Инструмент:

- читает `compile_commands.json` (для семантических правил) либо работает
  preprocessor-only без него (для include-правил);
- строит граф зависимостей по include (по AST — позже, v0.2);
- применяет YAML-описанные модульные правила и набор default-правил;
- репортит нарушения как `file:line` (column — будущий semantic-backend);
- завершается с ненулевым кодом при нарушениях.

Позиционирование принципиально:

- **не** "ArchUnit для C++";
- а **Lakos physical design + C++ Core Guidelines SF.\* checks в CI**.

Каждое default-правило несёт атрибуцию (Core Guidelines / Lakos / Martin).
Сохраняй эту рамку в docs, сообщениях и описаниях фич.

## Что это НЕ делает

- не линтер (clang-tidy);
- не bug finder (PVS/Coverity/cppcheck);
- не formatter (clang-format);
- не include optimizer (IWYU);
- не GUI, не web dashboard, не IDE extension.

Если новая идея противоречит списку — остановись и проверь, нужна ли она вообще.

## Обязательное чтение перед нетривиальной работой

- [CLAUDE.md](CLAUDE.md) — рабочие принципы и команды;
- [docs/architecture-spec.md](docs/architecture-spec.md) — главный источник по продукту;
- [docs/MVP.md](docs/MVP.md) — границы MVP;
- [docs/code_style.md](docs/code_style.md) — стиль C++20 (**единственный источник истины по стилю**);
- [docs/code_quality.md](docs/code_quality.md) — анти-slop ограничения и пороги;
- [docs/dev/git_workflow.md](docs/dev/git_workflow.md) — git-процесс;
- [backlog/README.md](backlog/README.md) — жизненный цикл задач.

Перед редактированием существующего файла прочитай его целиком.

## Архитектура

Пайплайн:

```text
scan → graph → rules → report
```

Подсистемы под `src/` (существуют):

- `config/` — YAML loader → `Config`;
- `scan/` — `include_scanner` (fast, preprocessor-only, shipped) и
  `clang_scanner` (libclang, семантические правила — v0.2);
- `graph/` — DAG, циклы, levelization, CCD/ACD/NCCD;
- `rules/` — одно правило = один класс = один файл, сгруппированы по источнику;
- `report/` — text/json reporters (sarif — позже).

Ключевые инварианты:

- Разделение `include_scanner` / `clang_scanner` — **осознанное**. Не схлопывай без обсуждения.
- **Одно правило = один класс = один файл.** Регистрация через статическую таблицу;
  добавление правила не должно трогать существующие файлы правил (OCP).
- Не строй "универсальный rule engine".

## Технологические ограничения

- Язык **C++20**; сборка **CMake (Ninja)**; зависимости через FetchContent.
- YAML: **`ryml`**. Тесты: **Catch2 v3**. CI: GitHub Actions.
- libclang/libtooling — для будущего семантического backend-а.
- **Зависимости — минимум.** Без Boost, без тяжёлых graph-библиотек:
  `unordered_map<NodeId, vector<NodeId>>` достаточно.
- Дистрибутив: один статический бинарь на платформу + Docker image.

## Границы scope

Без явного запроса НЕ добавляй: AST-based rules сверх плана, plugin system,
visualization, auto architecture inference, IDE integrations. Это явно вне продукта.

## Правила качества и проектирования

Принципы: **YAGNI**, **Authority over opinion**, **Zero-config first**,
**Deterministic output**, **Boring tech**.

Практика:

- сначала проверь, нельзя ли решить задачу удалением кода;
- сначала поищи существующую сущность, прежде чем создавать новую;
- не добавляй абстракции "на вырост"; не рефактори код, который не понимаешь;
- не делай большой рефакторинг посреди фичи без отдельного согласования.

Жёсткие пороги — в [docs/code_quality.md](docs/code_quality.md) (функция ≤ 30 строк,
класс ≤ 300, параметров ≤ 4, вложенность ≤ 3, ≤ 50 новых строк на изменение без
тестов/fixtures, ≤ 2 новых файла, ≤ 1 новый класс, 0 абстракций без запроса).
Перед пушем гонять `lizard --CCN 15 --length 30 --arguments 5 --warnings_only src/ include/ tests/`.

## Стиль C++ кода

**Единственный источник истины — [docs/code_style.md](docs/code_style.md). Читай его, не дублируй.**

Самое частое (точные формулировки и обоснование — в code_style.md):

- Allman braces, 2 пробела, строки ≤ 120, UTF-8 без BOM, Unix newlines.
- Namespaces `lower_snake_case`; типы `PascalCase`; **методы и свободные функции
  `lowerCamelCase`**; локальные/параметры `lowerCamelCase`.
- **Поля класса — trailing underscore: `name_`** (не `_name`, не `m_name`).
  Поля `struct`-data-carrier-ов — без подчёркивания.
- `constexpr`-константы `kPascalCase`; макросы `UPPER_SNAKE_CASE`.
- **Для нового кода не префиксуем интерфейсы `I`** — чисто-виртуальная база это
  обычный `class` (`Rule`, не `IRule`). (В коде есть legacy `IRule` — это не цель
  для нового кода; массовый rename вне scope обычной задачи.)
- Без `using namespace` в заголовках; заголовки self-contained; `#pragma once`;
  без анонимных namespace в заголовках.
- Предпочитай `[[nodiscard]]`, `noexcept`, `string_view`/`span`, `optional`/`variant`,
  `ranges`, `concepts`, RAII и стандартную библиотеку.

## Правила и fixtures

Правило без fixtures не существует. Каждое новое правило обязано иметь
`fixtures/<rule>/pass/` и `fixtures/<rule>/fail_*/`. Если фичу нельзя проверить
через fixtures — её не реализуют.

Каждое default-правило обязано иметь атрибуцию (Core Guidelines / Lakos / Martin).
Без источника — это не default, а опциональный lower-priority check.

## Dogfooding

`archcheck` обязан проходить собственные правила в CI (no cycles, SF.7/8/9/21,
без god-headers и т.д.). Любой merge, ломающий собственные правила, недопустим.

## Контракты CLI

Коды возврата (контракт — не менять без версионирования):

- `0` — OK
- `1` — violations found
- `2` — config / parsing error
- `3` — internal error

## Workflow

Задачи живут в `backlog/`, по одному `.md` на задачу: `new/` — заведено,
`wip/` — в работе, `completed/` — завершено и превращено в документацию
(секции «как работает / чем управляется / с чем связана / диагностика»).
Не смешивай несколько задач в один файл. См. [backlog/README.md](backlog/README.md).

## Ограничения на действия агента

- **Не коммить без явной просьбы** (`/commit` или «сделай коммит»). Завершил — жди.
- **Сборку запускать можно свободно** — это инструмент-проект, верификация через
  реальную компиляцию здесь норма. По умолчанию собирать **Debug**, не Release.
- Не придумывай лишнюю структуру или абстракции без подтверждённой необходимости.

## Короткая проверка перед тем как считать задачу выполненной

- не расширил ли scope сверх запрошенного;
- не добавил ли абстракцию без явной пользы;
- можно ли удалить часть нового кода;
- есть ли проверяемый путь в fixtures/тестах;
- не противоречит ли изменение Lakos/Core Guidelines framing;
- не конфликтует ли изменение с [docs/architecture-spec.md](docs/architecture-spec.md).
