# [CONFIG] YAML loader for `.archcheck.yml` v1 (phase 1 schema)

**Дата создания:** 2026-05-29
**Дата старта:** —
**Статус:** new
**Модуль:** CONFIG
**Приоритет:** major
**Сложность:** M (один скоуп: YAML → Config struct + валидация + fixtures, без подключения к rule pipeline)
**Целевой релиз:** v1 phase 1 (post-MVP)
**Блокирует:** future/v1_maj_agent_config_authoring_rules.md, future/v1_maj_ai_config_synthesis_eval_protocol.md, future/v1_min_ai_config_synthesis_trial_spdlog.md (вся цепочка AI synthesis — без рабочего loader-а агенту некуда писать)
**Заблокирован:** —
**Related:** wip/v1_maj_config_format_minimal_contract.md (спека), docs/config_format.md (контракт), future/010_maj_ai_rule_synthesis_contract.md

## Цель

Реализовать `src/config/` — чтение `.archcheck.yml` v1 phase 1 в типизированный `Config`-struct по контракту [`docs/config_format.md`](../../docs/config_format.md). Loader-only: rule pipeline пока не трогаем, hooks-ов в `run_check` не добавляем. Закрытие задачи = "файл читается, валидируется, ошибки внятные, fixtures зелёные".

## Контекст

Спека `docs/config_format.md` зафиксировала формат: `version: 1`, `modules:` (map → `paths: [glob]`), `rules:` (list, `type ∈ {layers, independence, forbidden}`, обязательное `name`). Стейк-холдеры спеки — AI synthesis pipeline (#10, v1 #2-#4): без работающего loader-а проверить вывод агента нечем, и вся цепочка AI-генерации конфига блокируется.

Текущая кодовая база MVP читает только дефолтные правила, конфига нет. Эта задача добавляет subsystem `config/`, не подключая его к запуску — отдельная под-задача потом плумбит в `run_check`.

## Открытые вопросы (решить в фазе 1, до кода)

1. **YAML-парсер: `ryml` (header-only, по спеке) или `yaml-cpp` (более распространённый)?**
   Спека (`docs/architecture-spec.md` §«Технологический стек») допускает оба. Подтвердить `ryml` — он header-only, добавляется одним FetchContent-блоком, нет внешней зависимости. Если найдётся существенный недостаток (например, нечитаемые line numbers в ошибках) — обосновать переход на `yaml-cpp` в этой же задаче.

2. **Glob-движок: своя реализация или встроенный?**
   Глобы из `paths:` (`src/**`, `include/myproj/**`) нужны для membership-резолва (отдельная задача), но **проверка непустоты и валидность синтаксиса** делается уже в loader-е. Минимальная проверка: список непустой, каждая строка непустая, нет `*..` или `?:` (грубый sanity check). Полный glob-match — не в этой задаче.

3. **Что считается "config error" (exit 2) vs "validation warning"?**
   Контракт: всё, что нарушает грамматику или семантические инварианты из `docs/config_format.md`, — exit 2 с line-numbered сообщением. Warnings не вводим в phase 1 (соблазн "soft validation" → не делаем, чтобы не размывать контракт).

## План выполнения

### Фаза 1 — скелет subsystem + `Config` struct

- [ ] Решить YAML-парсер (см. открытый вопрос 1); добавить FetchContent в `CMakeLists.txt` (offline-кеш в `build/_deps/`)
- [ ] `include/archcheck/config/config.h` — публичные типы:
  - `struct ModuleDef { std::string name; std::vector<std::string> paths; }`
  - `enum class RuleType { Layers, Independence, Forbidden }`
  - `struct LayersRule { std::string name; std::vector<std::string> layers; }`
  - `struct IndependenceRule { std::string name; std::vector<std::string> modules; }`
  - `struct ForbiddenRule { std::string name; std::vector<std::string> from; std::vector<std::string> to; }`
  - `using Rule = std::variant<LayersRule, IndependenceRule, ForbiddenRule>;`
  - `struct Config { int version; std::vector<ModuleDef> modules; std::vector<Rule> rules; }`
- [ ] `include/archcheck/config/config_loader.h` — точка входа:
  - `Config load(const std::filesystem::path&)` — кидает `ConfigError` с line/col из YAML-парсера
- [ ] `src/config/config_loader.cpp` — реализация

### Фаза 2 — валидация (по контракту `docs/config_format.md`)

- [ ] **Top-level**: ровно три ключа `version`/`modules`/`rules`; неизвестный ключ → exit 2
- [ ] **`version`**: integer, ровно `1`; `version: 2` → exit 2 с сообщением "unsupported schema version, this binary supports v1"
- [ ] **`modules`**: непустой map; имена матчат `[a-z0-9_-]+`; уникальны; `paths:` непустой; глобы непусты (см. открытый вопрос 2)
- [ ] **Не-overlap модулей**: один файл не должен матчиться двумя модулями. *Заметка:* полная проверка нужна на real codebase, в loader-е — только проверка, что глобы не идентичны буквально (детект очевидных опечаток)
- [ ] **`rules`**: каждый элемент имеет `type` и `name`; `name` уникальны; `type ∈ {layers, independence, forbidden}`
- [ ] **`layers`**: `layers:` непустой ordered list, элементы — существующие модули, без повторов
- [ ] **`independence`**: `modules:` непустой set, элементы — существующие модули, без повторов
- [ ] **`forbidden`**: `from:` и `to:` непустые списки; элементы — существующие модули; пересечение `from ∩ to` пусто
- [ ] **Diagnostic format**: каждая ошибка имеет `file:line:col: <message>` (line/col из YAML-парсера); неизвестное поле — указывает на конкретный YAML-узел

### Фаза 3 — fixtures + tests

- [ ] `fixtures/config/pass/` — четыре референс-примера из `docs/config_format.md` (tiny / layered / legacy / mixed), каждый — отдельная поддиректория с валидным `.archcheck.yml`
- [ ] `fixtures/config/fail/` — по одной поддиректории на каждую категорию валидационных ошибок:
  - `fail_unknown_top_key/` — лишний `defaults:` (phase 2 feature)
  - `fail_wrong_version/` — `version: 2`
  - `fail_duplicate_module/` — два модуля с одинаковым именем
  - `fail_duplicate_rule_name/` — два правила с одинаковым `name`
  - `fail_unknown_rule_type/` — `type: required` (phase 2+)
  - `fail_unknown_module_in_rule/` — `from: [ghost]`
  - `fail_from_to_overlap/` — `from: [a], to: [a, b]`
  - `fail_empty_layers/` — `layers: []`
  - `fail_missing_name/` — правило без `name:`
- [ ] Catch2 тесты:
  - `tests/config/test_loader_pass.cpp` — каждый pass-fixture парсится в ожидаемую структуру
  - `tests/config/test_loader_fail.cpp` — каждый fail-fixture бросает `ConfigError`, сообщение содержит ключевое слово (например, `"duplicate rule name"`)

### Фаза 4 — диагностика и CLI плумбинг (минимум)

- [ ] CLI: флаг `--config <path>`, default `.archcheck.yml` в CWD; отсутствие → запуск с дефолтными правилами (текущее поведение), не ошибка
- [ ] При `ConfigError` — print `file:line:col: <message>` на stderr, `exit 2`
- [ ] **Не подключать `Config` к rule pipeline** в этой задаче — пайплайн остаётся как есть, loader работает "вхолостую": прочитал, провалидировал, доложил, что было прочитано (debug-print под `--verbose` или просто молча). Подключение — отдельная задача (см. "Следующие шаги")

## Что **не** в этой задаче

- Glob-резолв файлов в модули (membership) — отдельная задача после loader-а
- Подключение `Config` к rule pipeline / реальное применение `layers`/`independence`/`forbidden` правил — отдельная задача (это уже про rules/, не config/)
- v1 phase 2 фичи: `defaults`, `thresholds`, `baseline`, `ignore`, `required`, `protected`, `severity` — отдельная спека потом
- Pattern-rules (`forbidden_pattern`) — dropped из v1, не имплементируем

## Альтернативы (отклонены / отложены)

- **Один файл `config.cpp` без public header** — отклонено: `Config` struct нужен публично для rules/ и тестов
- **`std::variant<>` vs `IRule`-стиль наследования** — выбираем `variant`: правил всего три, типы фиксированы по контракту, добавление нового типа = breaking schema change (MAJOR bump), сравнимо по сложности с добавлением branch в visitor
- **Без line numbers в ошибках, простой "wrong key X"** — отклонено: для AI-synthesis pipeline критично, чтобы агент видел *где* в его сгенерированном `.draft` ошибка
- **Реализовать сразу с применением правил к pipeline** — отклонено: split scope, loader сам по себе закрываемый и проверяемый, пайплайн-интеграция — отдельная задача с другой acceptance criteria

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Подтвердить YAML-парсер (открытый вопрос 1) и завести FetchContent.
2. Пройти фазы 1→4 последовательно. Кандидат на спинофф в 051a/b/c при переезде в `wip/`, если фазы расползутся.
3. После закрытия — отдельная задача "config → rule pipeline": резолв `paths:` в файлы, применение `layers`/`independence`/`forbidden` к графу включений, генерация violations. Разблокирует practical use конфига.
4. После практической задачи — снять блок с `future/v1_maj_agent_config_authoring_rules.md` (агенту есть куда писать, формат подтверждён реальным loader-ом).

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Loader без подключения к pipeline | Закрываемый скоуп, проверяется fixtures-ами без изменения rules/. Подключение — отдельная задача с другой acceptance criteria |
| `std::variant<Rule>` вместо ООП | Правил три, типы фиксированы по контракту (расширение → MAJOR schema bump) — variant читается чище |
| Line/col в каждой ошибке | AI-synthesis pipeline (#2-#4) зависит от того, что агент видит точку ошибки в своём `.draft` |
| Warnings не вводим в phase 1 | Контракт `docs/config_format.md` чёрно-белый: либо валидно (exit 0), либо нет (exit 2). Soft-режим — путь к размыванию контракта |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `CMakeLists.txt` | FetchContent блок для YAML-парсера (ryml/yaml-cpp), offline-кеш `build/_deps/` |
| `include/archcheck/config/config.h` | new — `Config`, `ModuleDef`, `Rule` variant |
| `include/archcheck/config/config_loader.h` | new — `load()`, `ConfigError` |
| `src/config/config_loader.cpp` | new — реализация |
| `src/cli/main.cpp` (или аналог) | флаг `--config`, exit 2 на `ConfigError` |
| `fixtures/config/pass/{tiny,layered,legacy,mixed}/.archcheck.yml` | 4 reference fixtures из спеки |
| `fixtures/config/fail/*/...` | 9 negative fixtures (см. фазу 3) |
| `tests/config/test_loader_pass.cpp` | new |
| `tests/config/test_loader_fail.cpp` | new |

## Fixtures

- [ ] `fixtures/config/pass/tiny/` — 2 модуля + 1 forbidden
- [ ] `fixtures/config/pass/layered/` — 3 модуля + layers
- [ ] `fixtures/config/pass/legacy/` — 3 модуля + 2 forbidden
- [ ] `fixtures/config/pass/mixed/` — include/ + src/ + layers + independence + forbidden
- [ ] `fixtures/config/fail/fail_unknown_top_key/`
- [ ] `fixtures/config/fail/fail_wrong_version/`
- [ ] `fixtures/config/fail/fail_duplicate_module/`
- [ ] `fixtures/config/fail/fail_duplicate_rule_name/`
- [ ] `fixtures/config/fail/fail_unknown_rule_type/`
- [ ] `fixtures/config/fail/fail_unknown_module_in_rule/`
- [ ] `fixtures/config/fail/fail_from_to_overlap/`
- [ ] `fixtures/config/fail/fail_empty_layers/`
- [ ] `fixtures/config/fail/fail_missing_name/`
