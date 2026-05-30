# [AI][CONFIG][V1] Правила для агента, который заполняет `.archcheck.yml.draft`

**Дата создания:** 2026-05-28
**Дата старта:** —
**Статус:** new
**Модуль:** DOCS, CONFIG
**Приоритет:** major
**Сложность:** M (контракт + примеры + anti-slop guardrails)
**Целевой релиз:** v1 phase 1 (post-MVP)
**Блокирует:** практическое использование AI synthesis без ручного написания конфига
**Заблокирован:** future/v1_maj_config_format_minimal_contract.md
**Related:** future/v1_maj_ai_config_synthesis_eval_protocol.md, future/010_maj_ai_rule_synthesis_contract.md, future/v1_maj_ai_config_iterative_loop.md (итеративный прогон поверх этих authoring-правил), #053 / #056 (dup-спайки — эмпирический источник правил по шум-файлам и exclude, см. раздел ниже), #054 (прогон по 50 AI-репам — источник правил по шум-СТРОКАМ, `experiments/ai_repo_run/`)

## Цель

Зафиксировать правила, по которым агент заполняет `.archcheck.yml.draft` — не выдумывая
архитектуру и не маскируя догадки под факты. Агент пишет только `.draft`, не финальный config.

## Целевой формат вывода агента

Агент заполняет `.archcheck.yml.draft` в схеме из `v1_maj_config_format_minimal_contract.md`.
Приоритет типов правил:
1. **`layers`** — если есть directional hierarchy — использовать его, не раскрывать в N×(N-1) пар `forbidden`.
2. **`independence`** — если модули одного уровня не должны знать друг о друге.
3. **`forbidden`** — точечные запреты которые не вписываются в layers.

Каждое правило и каждый модуль — YAML-комментарий с источником (`observed` / `inferred` / `speculative`).

## Разрешённые источники истины

- Реальная файловая структура (пути, имена директорий)
- Include-граф (кто реально включает кого)
- README / ARCHITECTURE.md если есть
- Явные naming conventions (префиксы, суффиксы)

## Запрещённое поведение агента

- Выдумывать слои которых нет в репо (нет `domain/` → не писать `domain` module)
- Переносить архитектурные паттерны из других проектов без evidence
- Делать `forbidden` на слабом основании (`speculative` → только `# TODO`, не правило)
- Скрывать неуверенность: все `speculative` поля должны быть явно помечены

## Уровни уверенности

| Уровень | Когда | В .draft |
|---------|-------|----------|
| `observed` | Прямо видно в файловой структуре или include-графе | Пишем как правило |
| `inferred` | Следует из naming/README, не подтверждено графом | Правило + `# inferred` |
| `speculative` | Common pattern, нет evidence в репо | `# TODO` комментарий, не правило |

## Что человек обязан проверить перед accept

- Каждый `modules.*.paths` — реально ли файлы существуют
- Каждое `layers` / `independence` правило — не нарушает ли существующий код
- Все `# inferred` и `# speculative` комментарии — подтвердить или удалить

## Пример хорошего draft

```yaml
version: 1

modules:
  core:
    paths: ["include/spdlog/**"]            # observed: directory exists
  sinks:
    paths: ["include/spdlog/sinks/**"]      # observed: directory exists
  details:
    paths: ["include/spdlog/details/**"]    # observed: directory exists

rules:
  - type: layers
    name: spdlog-layering
    layers: [sinks, core, details]          # inferred: sinks include core, details is low-level
    # TODO: verify sinks do not include details directly

  - type: independence
    name: sinks-independent
    modules: [sinks]                        # speculative: typical pattern, not verified
```

## Пример плохого draft (запрещён)

```yaml
# НЕ ДЕЛАТЬ ТАК — нет директорий в репо:
modules:
  presentation:
    paths: ["src/presentation/**"]    # BAD: не существует
  business_logic:
    paths: ["src/business/**"]        # BAD: выдумано

rules:
  - type: forbidden
    name: no-circular                 # BAD: слишком широко, без evidence
    from: [business_logic]
    to: [presentation]
```

## Дополнение из опыта dup-спайков #053/#056 (шум-файлы и exclude)

> Источник: прогоны fast-backend duplication-спайков на реальных деревьях
> (cpparch `src/`, и особенно `ttcg` — 15388 файлов, из них 315 МБ vendored
> `Src/packages/boost…`), 2026-05-30. Эмпирический фон по exclude см. также
> `experiments/line_duplication/FILTER_CLASSIFICATION_REPORT.md`.

### Почему это касается config-агента

Без отсечения шумовых файлов первый прогон **тонет в них**: на `ttcg` без
`--exclude /packages/` весь топ дублей — это boost `mem_fn_template.hpp`
template-твины, ноль сигнала о коде проекта. Тот же риск у агента: он увидит
`Src/packages/`, `build/`, `*_autogen/` как существующие директории и по
текущему правилу `observed: directory exists` запишет их как модули. **Это
ложная архитектура.** Существование директории — необходимое, но НЕ достаточное
условие: нужно ещё подтвердить, что она **first-party**, а не вендор/генерат.

### Что агент ОБЯЗАН учитывать (добавить в промпт)

- **Не делать модулей из вендора/генерата/билда.** Жёсткие классы шума
  (всегда вне архитектуры, кандидаты в `exclude`, не в `modules`):
  `packages/`, `third_party/`, `3rdparty/`, `vendor/`, `bundled/`, `extern/`,
  `deps/`, `_deps/`, `single_include/`, `amalgamate*`, `dist/`,
  `*_autogen/`, `Generated/`, `moc_*`, `qrc_*`, `ui_*`, `*.g.cpp`,
  `build*/`, `cmake-build-*/`, `CMakeFiles/`.
- **Мягкие классы — по контексту, не вслепую:** `test/`, `tests/`, `examples/`,
  `sandbox/`, `legacy/`, `upgrade/`. На `ttcg` `Src/NTPROSurfaceSplitSandbox/`
  содержал реальный код (туда дословно скопирован `mesh_splitter.h`), так что
  «sandbox» нельзя зачищать автоматически. Помечать `# inferred` / `# TODO`, а
  не выкидывать молча.
- **Параллельные near-duplicate деревья — это сигнал дублирования, НЕ модули.**
  На `ttcg`: `Triangulation` vs `Triangulation_int`, `PolyPolygon` vs
  `PolyPolygon_int` — float/int-копии (токен-в-токен идентичны, `plain=1.0`).
  Агент **не должен** заводить `*_int` как отдельный слой/модуль: это missing
  reuse edge. Правильная реакция — `# TODO: likely duplicated tree (see
  duplication pass), not a separate layer`, а не два симметричных модуля.
- **Уточнить уровень `observed`:** «directory exists» — слишком слабо для
  vendored-ловушки. `observed` = директория существует **И** first-party **И**
  участвует в include-графе (есть входящие/исходящие рёбра внутри проекта).
  Иначе — максимум `inferred`.
- **`observed` по include-графу надёжен только на ОЧИЩЕННЫХ строках.** Граф,
  построенный по сырому тексту, ловит ложные рёбра из лицензионных шапок,
  закомментированных инклюдов, `R"(...)"`-данных и `#if 0`-блоков (см. раздел
  про шум-строки ниже). Прежде чем поднимать находку до `observed`, граф обязан
  идти по тем же очищенным строкам, что и дедуп.

### Классы шум-СТРОК (не только файлов) — из прогона #054

> Источник: прогон fast-backend duplication-спайка по 50 реальным C++ AI-репам
> (AIDev, `experiments/ai_repo_run/`, baseline `runs/v5_if0/`), 2026-05-30.
> Все 6 классов подтверждены на исходниках корпуса. Это расширение раздела про
> шум-ФАЙЛЫ выше на уровень отдельных строк: даже в first-party файле часть
> строк — не код и даёт ложные cross-file «дубли».

- **Лицензионные шапки `/* ... */`** (текст MIT/Apache в начале файла) — дают
  ложный cross-file блок «дубля». Пример: `mqtt_client` — 19-строчный «дубль»
  оказался текстом MIT-лицензии, не кодом. → дедуп обязан **stateful**-стрипать
  блок-комменты (как боевой SF.7, `src/rules/sf7_using_namespace.cpp`), а не
  только построчные `//`.
- **Регистр вендор-папок.** В корпусе встречаются ВСЕ варианты:
  `thirdParty / ThirdParty / 3rdParty / External / Submodules`. Матчинг
  exclude/skip обязан быть **регистронезависимым** (alpaka: 62%→20% после фикса).
- **Embedded raw-string данные `R"( ... )"`** — шейдеры/SQL/скрипты, зашитые в
  C++ строкой. Пример: Effekseer, GL ShaderHeader. Содержимое = данные, не код,
  стрипать тело raw-string.
- **`#if 0` / `#if false` мёртвые блоки** — напр. fxc disassembly-листинги под
  `#if 0` (Effekseer DX): 33%→28% после пропуска. Пропускать с учётом вложенности.
- **Числовые data-массивы** `const BYTE g_main[] = {69,70,88,...}` — байт-код
  шейдеров числами, НЕ под `#if 0`. line-based стрип это **не лечит** → передано
  в **#056** (partial-duplication pass).
- **`#include`-блоки и header↔impl шапки** — одинаковые списки `#include` между
  файлами и между `X.h`/`X.cpp` = общие зависимости, не reuse-edge. `#include`
  пропускать. Остаток header↔impl (тела/параметры конструктора) — это
  cross-**component** сигнал, его адресует **#053 P0-B** (cross-component map),
  не line-level.

### Пример плохого draft — vendored dir как модуль (добавить в док)

```yaml
# НЕ ДЕЛАТЬ ТАК — директории существуют, но это не архитектура проекта:
modules:
  boost:
    paths: ["Src/packages/boost.1.76.0.0/**"]   # BAD: vendored, в exclude, не module
  autogen:
    paths: ["**/*_autogen/**"]                   # BAD: генерат
  triangulation_int:
    paths: ["**/Triangulation_int/**"]           # BAD: дубль-копия float-ветки, не слой
```

### Возможные доп. поля (для дедупликатора и шире)

Эти поля выходят за минимальный контракт (`version/modules/rules`) и относятся
к config-формату + проходам #053/#056 — здесь фиксирую как вход для их дизайна:

- **Топ-левел `exclude:`** (список glob) — общий для scanner, duplication-pass и
  rules. Самое высокоэффективное поле: без него первый прогон = сплошной шум.
  Агент заполняет его обнаруженными жёсткими классами выше (с комментарием-
  источником), это его прямой выхлоп наравне с `modules`.
- **Блок дедупликатора** (черновой, на #053/#056): `duplication.exclude`
  (поверх общего), `similarity_threshold`, `min_tokens`; пометка намеренных
  твинов уезжает в baseline-модель #053, не в agent-draft.
- Агент НЕ выставляет пороги дедупа сам (`speculative`) — только предлагает
  `# TODO` с обоснованием, решение за человеком.
- **Стрип-правила шум-строк — поведение дедупликатора ПО УМОЛЧАНИЮ, не конфиг
  агента.** Стрип block-comment / raw-string / `#if 0` / `#include` и
  numeric-array (последнее — #056) встроены в проход, агент их **не** выставляет
  и не предлагает: это не архитектурное решение, а гигиена детектора. В draft
  они не попадают вовсе.
- **`exclude`-матчинг обязан быть регистронезависимым** — зафиксировано как
  требование к формату (корпус #054 содержит `thirdParty/ThirdParty/3rdParty`
  вперемешку). Агент пишет класс шума одним вариантом, движок матчит регистр-
  независимо.
- Кросс-реф конкретных классов: numeric data-массивы → **#056**;
  header↔impl / cross-component остаток → **#053 P0-B**.

### Эмпирика OSS-sweep: third-party детектится НЕ по имени

> Доказательная база: `experiments/partial_duplication/OSS_SWEEP_REPORT.md` —
> dup-проход по 19 OSS-репо. Главный урок — именно про third-party / генерат.

- **Вендор часто без маркера `third_party/`/`vendor/`** — лежит обычными
  подпапками внутри `src/`. BambuStudio: `src/mcut/`, `src/minilzo/`, embedded
  `src/nlohmann/`. Substring-exclude по `third_party`/`vendor`/`packages` их НЕ
  ловит → они всплыли как «находки» (Bambu: 180 пар, почти все внутри этих
  библиотек) и всплыли бы у агента как ложные `modules`.
- **Генерат проектно-специфичен.** grpc: `src/core/ext/upb-gen/**/*.upb.h` дал
  **7712** «дублей» (генерат идентичен by construction). Маска `.pb.` его
  пропускает — у upb своё расширение `.upb.h`. token-LCS confirm тут НЕ спасает:
  код реально идентичен, лечится только exclude.
- **Вывод для агента:** фиксированному exclude-листу доверять нельзя. Вендор и
  генерат определять по **сигналам**, не по имени каталога: license-заголовок
  чужого проекта, баннеры `// DO NOT EDIT` / `@generated` / `generated by …`,
  отсутствие входящих include-рёбер из app-кода (изолированный кластер / сток),
  чужой стиль/нейминг. Найденное → в `exclude` с комментарием-источником,
  неуверенное → `# TODO`. Прямое расширение `observed`: директория существует
  ≠ она твоя ≠ она рукописная.

## План выполнения

- [ ] Написать `docs/ai_config_authoring_rules.md`: allowed sources, forbidden behavior, уровни уверенности
- [ ] Внести в док раздел про шум-файлы: жёсткие/мягкие exclude-классы, vendored-ловушку, параллельные дубль-деревья (из #053/#056)
- [ ] Внести в док раздел про шум-СТРОКИ (из #054): лиц-шапки `/* */`, регистр вендор-папок, raw-string данные, `#if 0`, numeric-массивы (→#056), `#include`/header↔impl (→#053 P0-B)
- [ ] Зафиксировать требования: регистронезависимый `exclude`-матчинг; стрип-правила = дефолт движка, не agent-draft; граф для `observed` — на очищенных строках
- [ ] Уточнить определение `observed` (first-party + include-граф, не просто «директория существует»)
- [ ] Согласовать с config-форматом топ-левел `exclude:` и черновой `duplication`-блок (cross-ref #053/#056)
- [ ] Добавить 2 полных примера: good draft и bad draft с аннотациями
- [ ] Зафиксировать минимальный output contract: что должно быть в каждом поле `.draft`
- [ ] Описать checklist для человека перед accept
- [ ] Связать с `synthesize`-command design когда он появится

## Сделано

- (пусто)

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| docs/ai_config_authoring_rules.md | контракт для агента (вкл. раздел про шум-файлы / exclude из #053/#056) |
| README.md | краткое объяснение `.draft` workflow после стабилизации |
| (config-формат) | топ-левел `exclude:` + черновой `duplication`-блок — согласовать с #053/#056 |
