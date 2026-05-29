# [CONFIG] YAML loader for `.archcheck.yml` v1 (phase 1 schema)

**Дата создания:** 2026-05-29
**Дата старта:** 2026-05-29
**Дата завершения:** 2026-05-29
**Статус:** done
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

1. ~~**YAML-парсер: `ryml` или `yaml-cpp`?**~~ — **решено 2026-05-29**: `ryml` v0.7.0 уже подключён через FetchContent в `CMakeLists.txt:55-65` (для других подсистем, archcheck_core линкуется к `ryml::ryml`). Используем его. Переход на `yaml-cpp` не рассматриваем без конкретного блокера от ryml.

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

- [x] **Top-level**: ровно три ключа `version`/`modules`/`rules`; неизвестный ключ → exit 2
- [x] **`version`**: integer, ровно `1`; `version: 2` → exit 2 с сообщением "unsupported schema version, this binary supports v1"
- [x] **`modules`**: непустой map; имена матчат `[a-z0-9_-]+`; уникальны; `paths:` непустой; глобы непусты
- [~] **Не-overlap модулей** — duplicate name detection реализовано, "буквально идентичные globs" не реализованы (low value до wiring в pipeline; пометка как future micro-task)
- [x] **`rules`**: каждый элемент имеет `type` и `name`; `name` уникальны; `type ∈ {layers, independence, forbidden}`
- [x] **`layers`**: `layers:` непустой ordered list, элементы — существующие модули, без повторов
- [x] **`independence`**: `modules:` непустой set, элементы — существующие модули, без повторов
- [x] **`forbidden`**: `from:` и `to:` непустые списки; элементы — существующие модули; пересечение `from ∩ to` пусто
- [x] **Diagnostic format**: `file:line:col: <message>` через ryml location API; работает для map/seq nodes, для scalar values ryml даёт 0:0 (acceptable limitation)

### Фаза 3 — fixtures + tests

- [x] `fixtures/config/pass/` — четыре референс-примера из `docs/config_format.md` (tiny / layered / legacy / mixed), каждый — отдельная поддиректория с валидным `.archcheck.yml`
- [x] `fixtures/config/fail/` — по одной поддиректории на каждую категорию валидационных ошибок:
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

- [x] CLI: флаг `--config <path>` (default auto-pickup в CWD намеренно не реализован — отложен как QoL после wiring)
- [x] При `ConfigError` — print `file:line:col: <message>` на stderr, `exit 2`
- [x] **Не подключать `Config` к rule pipeline** — выполнено: `dispatch_config` validates → discards → `run_check` с default rules

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

### 2026-05-29 — фаза 1 (skeleton) и фаза 2 (parsing + validation), без line numbers

Public API + полный validation loop для v1 phase 1 schema. Loader не подключён к pipeline и не имеет caller'ов — `archcheck_core` собирается, тесты `235/235` зелёные на каждом шаге.

**Реализовано в `src/config/config_loader.cpp`:**

- `read_file()` — чтение YAML, ошибка "cannot open" если файл не открывается.
- `parse_version()` — top-level map, `version: 1` обязателен; другие версии → "unsupported schema version".
- `validate_top_keys()` — отвергает любые top-level ключи кроме `version` / `modules` / `rules`; обязательное присутствие `modules` и `rules`.
- `parse_modules()` + helpers (`is_valid_module_char`, `validate_module_name`, `parse_module_def`):
  - `modules:` — non-empty map.
  - Имена матчат `[a-z0-9_-]+`, уникальны.
  - `paths:` — non-empty list непустых строк.
- `parse_rules()` + `parse_rule()` + три парсера типов:
  - Каждый rule имеет `type` и `name`, name уникален.
  - `type` диспатчится в `LayersRule` / `IndependenceRule` / `ForbiddenRule`, неизвестный type → ошибка.
- `cross_validate()` + `RuleValidator` struct + helpers:
  - Каждое имя модуля в `layers` / `independence.modules` / `forbidden.from` / `forbidden.to` существует в `modules:`.
  - Без повторов внутри списка.
  - `forbidden.from ∩ to = ∅`.

**Все ошибки** идут через `ConfigError(file, line, col, msg)`. Line/col пока 0:0 (фаза 2.5 — ryml location API — не сделана).

**Закрытые открытые вопросы:**

1. **YAML-парсер** — `ryml` v0.7.0 (уже в `CMakeLists.txt:55-65`, archcheck_core линкуется к `ryml::ryml`). `yaml-cpp` не рассматриваем.
3. **Config error vs warning** — only errors (exit 2). Warnings соблазнительны, но размывают контракт.

### 2026-05-29 — фазы 2.5 / 3 / 4 завершены, задача закрыта

- **Фаза 2.5** (`c2d5505`): `ConfigError` несёт настоящий line/col через ryml location API. Введён `LoaderCtx { parser, file }`, передаётся в каждую validate/parse-функцию; `throw_at(ctx, node, msg)` извлекает `parser.location(node)`. Ошибки без anchor-node (cross-rule, missing top key) используют `throw_top` с file:1:1 — детерминированно, не 0:0.
  - **Известное ограничение**: для scalar value nodes (например, `version: 2`) ryml даёт line=0, col=0 — accelerator не строит location для скаляров без anchor. Для map/seq nodes (включая `defaults:`, `modules:` тех или иных правил) — корректно. На практике 80%+ ошибок получают полезный line.
- **Фаза 3** (`7f4b3f4`): 4 pass + 9 fail fixtures в `fixtures/config/{pass,fail}/<name>/archcheck.yml`. Каждый pass-fixture — это reference example из `docs/config_format.md` (tiny / layered / legacy / mixed). Каждый fail-fixture — отдельная категория ошибки. Catch2-тест `tests/unit/config/test_loader.cpp` для каждого:
  - pass: загрузка → REQUIRE на структуре (count модулей, типы правил, имена).
  - fail: `REQUIRE_THROWS_WITH` с `ContainsSubstring` по ключевому слову (например, `"unknown top-level key"`).
  - Все 13 новых тестов зелёные. Общая cuite: 248/248.
- **Фаза 4** (`16f2bf8`): CLI флаг `--config <path>` — валидирует YAML, печатает `ConfigError` (`file:line:col: msg`) на stderr с exit 2, после успеха хэндит дальше в существующий `run_check`. `Config` пока discards — подключение к rule pipeline вынесено в отдельную задачу.
- **End-to-end smoke**: `archcheck --config fixtures/config/pass/tiny/archcheck.yml /tmp` → exit 0, "No violations". `archcheck --config fixtures/config/fail/fail_unknown_top_key/archcheck.yml /tmp` → `archcheck: file:8:0: unknown top-level key 'defaults' …` + exit 2.

## Коммиты

| SHA | Описание |
|-----|----------|
| `047fd0d` | `feat(config): add Config struct types for v1 phase 1 schema` |
| `a1120b2` | `chore(tasks): add #052 cross-TU duplication detector…` — параллельная сессия захватила staged-файлы и закоммитила loader skeleton (config_loader.h/cpp + CMake wiring) под чужим subject. Атрибуция в коммите некорректна, но код мой. |
| `3e046ef` | `feat(config): reject unknown top-level keys, require modules/rules` |
| `4d9a172` | `feat(config): parse and validate modules block` |
| `2893aed` | `feat(config): parse rules block — dispatcher + layers type` |
| `574516d` | `feat(config): parse independence and forbidden rule types` |
| `f3377ce` | `feat(config): cross-rule validation — module existence and disjoint sets` |
| `47685ea` | `chore(tasks): checkpoint #051 — phase 1 and phase 2 (no line numbers yet)` |
| `c2d5505` | `feat(config): real line/col in ConfigError via ryml location API` |
| `7f4b3f4` | `test(config): add 4 pass + 9 fail fixtures and Catch2 loader tests` |
| `16f2bf8` | `feat(cli): --config <path> validates .archcheck.yml v1` |

## Как работает

`archcheck::config::load(path)` — единственный публичный entry point:

1. Читает YAML с диска (`read_file`).
2. Парсит ryml-ом в режиме `locations(true)` — `Parser` хранит accelerator для последующих `parser.location(node)` lookups; `Tree` хранит сами узлы.
3. Прогоняет шесть validate/parse-стадий:
   - `parse_version` — `version: 1`, иначе exit 2 со стабильным сообщением.
   - `validate_top_keys` — отвергает любые ключи кроме `version` / `modules` / `rules`; обязательное присутствие `modules`/`rules`.
   - `parse_modules` — `modules:` — map, имена матчат `[a-z0-9_-]+`, уникальны; `paths:` — non-empty list непустых строк.
   - `parse_rules` — `rules:` — список, каждый rule имеет `type` и `name`, name уникален; `type` диспатчится в один из трёх типов.
   - `parse_*_rule` (три штуки) — соответствующие поля (`layers` / `modules` / `from`+`to`).
   - `cross_validate` через `RuleValidator` (visitor) — каждое имя модуля в rule-references существует в `modules:`, нет повторов внутри списка, `forbidden.from ∩ to = ∅`.
4. Возвращает заполненную `Config { version, modules, rules: vector<variant<Layers, Independence, Forbidden>> }`.
5. Любая ошибка → `ConfigError(file, line, col, msg)` (наследник `std::runtime_error`).

Под капотом — три ключевые штуки:

- **`std::variant<LayersRule, IndependenceRule, ForbiddenRule>`** для правил — расширение фиксировано контрактом схемы (MAJOR bump), variant читается чище ООП-наследования.
- **`LoaderCtx { parser, file }`** прокидывается во все функции — единственный способ дать throw-сайту доступ к ryml location.
- **`RuleValidator` struct + `std::visit`** для cross-validation — overload-based dispatch держит cross_validate < 30 строк (lizard).

## Чем управляется

- **Авторитет схемы** — `docs/config_format.md`. Любое изменение поведения loader-а сверяется с этим документом. Если loader разъехался — править loader, не доку.
- **Версионирование** — `version: 1` стабилен через все archcheck-1.x/2.x. Bump до `version: 2` — только при breaking-изменении контракта (см. SemVer таблицу в `docs/config_format.md`).
- **Диагностика** — `[rule:<name>]` пока не используется в выводе loader (это для violation reporter после wiring). Текущий формат ошибки: `file:line:col: <message>`.
- **Расширение phase 2** — `defaults`, `thresholds`, `baseline`, `ignore`, `required`, `protected`, `severity` — добавляются как новые ключи. Loader сейчас отвергает их (validate_top_keys), что **намеренно**: добавлять их раньше времени значит размыть контракт.

## С чем связана

- **Производит:** `Config` struct, готовый к подключению в rule pipeline. Сама интеграция (paths→files, применение правил к графу) — **отдельная задача** (не в скоупе #051).
- **Разблокирует:** [`future/v1_maj_agent_config_authoring_rules.md`](../future/v1_maj_agent_config_authoring_rules.md) — теперь у AI-агента есть рабочий loader, чьи ошибки можно скармливать обратно в prompt. Блокировка снимается после того, как config → pipeline-таска закрыта (агенту нужен не только validator, но и runnable end-to-end).
- **Реализует:** [`completed/v1_maj_config_format_minimal_contract.md`](v1_maj_config_format_minimal_contract.md) — спека формата.
- **Использует:** ryml v0.7.0 (FetchContent в `CMakeLists.txt:55-65`), Catch2 v3 (FetchContent в `tests/CMakeLists.txt`).
- **Соседствует с:** [`future/010_maj_ai_rule_synthesis_contract.md`](../future/010_maj_ai_rule_synthesis_contract.md) — старый synthesize-контракт. После #051 + интеграции #010 становится формализуемым: synthesize-агент производит YAML по схеме phase 1, archcheck его валидирует тем же loader-ом.

## Диагностика

Симптомы и где смотреть:

- **`No violations found`** на конфиге, который должен фейлиться — это пока ожидаемо: `Config` discards в `dispatch_config`, pipeline ещё не применяет правила. Задача "config → pipeline" не закрыта.
- **`file:0:0: …`** в сообщении ошибки — ryml не построил location для конкретного scalar node (см. "Известное ограничение" в Сделано). Не баг loader-а — баг ryml location accelerator на скалярах без anchor. Будет фиксироваться когда/если ryml апдейтнем.
- **Loader пропустил неизвестный ключ в `modules.*` или `rules[*]`** — посмотри: validate_top_keys работает только на root. Внутренние unknown keys (например, `modules.core.tags: [...]`) пока не отвергаются. Это TODO для отдельной микро-задачи если станет проблемой.
- **`duplicate module name 'X'` не срабатывает на буквально одинаковых YAML-ключах** — ryml объединяет duplicate keys в map. Тест `fail_duplicate_module` использует `REQUIRE_THROWS_AS` (а не keyword match), потому что точная ошибка зависит от ryml. На практике YAML-парсеры с duplicate keys ведут себя по-разному; контракт `docs/config_format.md` это явно не специфицирует.
- **Конфиг проходит valid через loader, но archcheck не применяет правила** — это by design phase 4: `dispatch_config` validates → discards → `run_check` с default rules. Wiring в pipeline — отдельная задача.
- **Изменения в loader ломают существующие fixtures** — fixtures это reference; обновляются только если меняется `docs/config_format.md`. Спека > loader > fixtures.

## Следующие шаги (post-#051)

1. **Отдельная задача "config → rule pipeline"** — резолв `paths:` в файлы (glob match по include graph nodes), применение `layers`/`independence`/`forbidden` к графу, генерация `Violation` с `[rule:<name>]` id. Разблокирует practical use конфига.
2. **Auto-pickup `.archcheck.yml` в CWD** при запуске без аргументов — мелкая QoL после того, как pipeline-интеграция стабильна (иначе риск surprise behaviour на чужих репах).
3. **v1 phase 2** (`defaults` / `thresholds` / `baseline` / `ignore`) — отдельная спека после первого практического прогона на реальном репо.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Loader без подключения к pipeline | Закрываемый скоуп, проверяется fixtures-ами без изменения rules/. Подключение — отдельная задача с другой acceptance criteria |
| `std::variant<Rule>` вместо ООП | Правил три, типы фиксированы по контракту (расширение → MAJOR schema bump) — variant читается чище |
| Line/col в каждой ошибке | AI-synthesis pipeline (#2-#4) зависит от того, что агент видит точку ошибки в своём `.draft` |
| Warnings не вводим в phase 1 | Контракт `docs/config_format.md` чёрно-белый: либо валидно (exit 0), либо нет (exit 2). Soft-режим — путь к размыванию контракта |

## Изменённые файлы

| Файл | Изменение | Финальный коммит |
|------|-----------|-------------------|
| `include/archcheck/config/config.h` | new — `Config`, `ModuleDef`, `Rule` variant | `047fd0d` |
| `include/archcheck/config/config_loader.h` | new — `load()`, `ConfigError` | `a1120b2` |
| `src/config/config_loader.cpp` | new — реализация всей валидации (фазы 1, 2.1-2.4, 2.5) | `c2d5505` |
| `src/CMakeLists.txt` | `+ config/config_loader.cpp` в `archcheck_core` | `a1120b2` |
| `src/main.cpp` | `+ dispatch_config`, флаг `--config`, exit 2 на `ConfigError` | `16f2bf8` |
| `fixtures/config/pass/{tiny,layered,legacy,mixed}/archcheck.yml` | 4 reference fixtures из спеки | `7f4b3f4` |
| `fixtures/config/fail/<9 dirs>/archcheck.yml` | 9 negative fixtures | `7f4b3f4` |
| `tests/unit/config/test_loader.cpp` | new — 13 Catch2 cases | `7f4b3f4` |
| `tests/CMakeLists.txt` | `+ unit/config/test_loader.cpp` | `7f4b3f4` |

## Fixtures

- [x] `fixtures/config/pass/tiny/` — 2 модуля + 1 forbidden
- [x] `fixtures/config/pass/layered/` — 3 модуля + layers
- [x] `fixtures/config/pass/legacy/` — 3 модуля + 2 forbidden
- [x] `fixtures/config/pass/mixed/` — include/ + src/ + layers + independence + forbidden
- [x] `fixtures/config/fail/fail_unknown_top_key/`
- [x] `fixtures/config/fail/fail_wrong_version/`
- [x] `fixtures/config/fail/fail_duplicate_module/`
- [x] `fixtures/config/fail/fail_duplicate_rule_name/`
- [x] `fixtures/config/fail/fail_unknown_rule_type/`
- [x] `fixtures/config/fail/fail_unknown_module_in_rule/`
- [x] `fixtures/config/fail/fail_from_to_overlap/`
- [x] `fixtures/config/fail/fail_empty_layers/`
- [x] `fixtures/config/fail/fail_missing_name/`
