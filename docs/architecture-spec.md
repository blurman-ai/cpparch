# archcheck — Architecture rules for C++

> Рабочее название. Перед публикацией проверить занятость на GitHub / PyPI / crates.io / Homebrew.
> Альтернативы: `cpparch`, `cpp-arch`, `archlint`, `archguard-cpp`.
>
> Версия документа: 2.0. Изменения от 1.0: подтверждены источники спроса, перепозиционирование вокруг Lakos + Core Guidelines, переработан раздел дефолтных правил (3 уровня), обновлена сравнительная таблица и MVP-скоуп.

---

## TL;DR

Open source CLI-инструмент для проверки архитектурных инвариантов в C++ проектах. Закрывает многолетнюю дыру в экосистеме: для Java, C#, F#, TypeScript, Python, PHP, Go, Rust, Dart такие инструменты есть; в C++ — нет ни одного живого open source.

Инструмент строится не как порт ArchUnit (это контекст, но не позиционирование), а как **CI-обвязка над двумя авторитетными источниками C++ архитектуры**: правилами секции SF из C++ Core Guidelines (Stroustrup, Sutter) и метриками физического дизайна из Lakos "Large-Scale C++ Software Design". Плюс классические метрики связности Мартина (Instability / Abstractness / Distance) на уровне namespace.

YAML-конфиг описывает модули и разрешённые зависимости. Парсинг — libclang. Граф зависимостей строится на основе include и AST. Нарушения репортятся с file:line:column, инструмент возвращает non-zero exit code, CI ломается, архитектурный дрейф не доходит до master.

Главное отличие от текущего ландшафта: **набор дефолтных правил со ссылками на признанные источники**. Запускается без конфига и сразу даёт результат, который нельзя оспорить как "вкусовщину".

---

## Проблема

### Архитектурный дрейф

Любая нетривиальная C++ кодовая база со временем размывается. Слои перестают быть слоями. Модули, задуманные независимыми, начинают друг друга тянуть. Появляются циклы в include-графе. God-headers нарастают. Это не баг конкретной команды — это статистическая неизбежность для проекта старше двух-трёх лет.

Code review это не ловит — ревьюер видит дельту, а не структуру всего проекта. Линтеры (clang-tidy, cppcheck) про другое — стиль, baby's first bug. Bug-hunters (PVS-Studio, Coverity) — про корректность операций, не структуры. Между ними остаётся дыра: никто не проверяет, что код соответствует заявленной архитектуре.

### AI-кодинг ускоряет дрейф

Агенты (Claude Code, Codex, Cursor) уверенно генерируют рабочий код, но плохо удерживают архитектурные ограничения. Свежий paper из EURECOM (Dente, Satriani, Papotti, май 2026) измерил эффект: ~30 процентных пунктов просадки в assertion pass rate при переходе от чисто функциональной задачи к полностью специфицированной структурно. Авторы назвали это **constraint decay**.

Лечится не количеством правил в CLAUDE.md, а выносом архитектурных проверок за пределы промпта — в CI. Промпт деградирует с контекстом. CI — нет.

---

## Доказательства спроса

### 1. Публично зафиксированная дыра шестилетней давности

[TNG/ArchUnit issue #242](https://github.com/TNG/ArchUnit/issues/242) от октября 2019: "Does an archunit exist for C/C++?". Закрыт мейнтейнером ArchUnit (`codecholeric`) фразой: *"I'm really sorry, but I don't know any alternative for C/C++. There are some solutions for .NET/C#, but for C/C++ I don't know of anything."* Это шесть лет публично зафиксированной дыры от человека, который мониторит экосистему профессионально.

### 2. Экосистема ArchUnit-style тулзов активно растёт, но не в C++

| Язык | Инструмент | Статус |
|---|---|---|
| Java | ArchUnit | v1.4.2 — апрель 2026, активный |
| C# | ArchUnitNET | активный |
| F# | FsArchUnit | активный |
| TypeScript | ts-archunit | активный |
| Python | ArchUnitPython | **появился 3 недели назад** (май 2026) |
| PHP | PHPat, Deptrac, PestPHP | активные |
| Go | go-cleanarch, fresh-onion | активные |
| Dart | lakos | активный, реализует именно Lakos-метрики |
| Rust | dependency-constraints | активный |
| **C++** | — | **нет** |

Тренд однозначный: C++ — последняя крупная незанятая ниша. Появление ArchUnitPython в мае 2026 — свежее доказательство, что парадигма продолжает расширяться на новые языки.

### 3. Существующие "конкуренты" не закрывают нишу

| Инструмент | Класс | OSS? | Что не так |
|---|---|---|---|
| **CppDepend** | architecture + metrics | нет (paid) | Платный, enterprise, GUI-first, LINQ-based, не вписывается в CI-культуру open source |
| **tomtom/cpp-dependencies** | include-graph | да | **Мёртв** — последний коммит 2021, заброшен Tomtom |
| **lukedodd/include-wrangler** | include-graph | да | Заброшен, прототип |
| **jeremy-rifkin/cpp-dependency-analyzer** | include-graph | да | Single-dev игрушка, только adjacency matrix, без CI |
| **DeepEnds** | dependency viz | да | Visual Studio плагин, не CLI |
| **IWYU** | includes optimization | да | Минимизирует includes, **не** проверяет архитектуру |
| **CppCheck / clang-tidy** | linter / bug-hunter | да | Другая задача — баги и стиль |
| **SonarQube Server 2025 R2** | "Design & Architecture" | нет (paid serious) | Закрытый, дорогой для opensource |
| **Sotograph** | enterprise | нет | Закрытый, ентерпрайз |
| **archcheck** | **architecture in CI** | **да** | **Эта ниша** |

### 4. Культурный риск и как его снимать

C++ community консервативнее джавовского. "Clean architecture" и "hexagonal architecture" в C++ не религия. Зато есть собственная авторитетная традиция: **Lakos** ("Large-Scale C++ Software Design") и **C++ Core Guidelines** (Stroustrup, Sutter).

Поэтому инструмент позиционируется не как "ArchUnit для C++", а как **"Lakos physical design + Core Guidelines SF.* checks в CI"**. Это говорит на родном языке C++ комьюнити и убирает аргумент "нам не нужны ваши джавовские концепции".

---

## Почему именно C++

В Java, C#, TypeScript, Python архитектурные тесты — мейнстрим. В C++ — дыра. Документально:

- TNG/ArchUnit issue #242 (см. выше), шесть лет без решения.
- Microsoft пытался закрыть нишу через Visual Studio 2010 Layer Diagrams для C/C++. Фича умерла, в современной VS её нет.
- Существующее в C++ либо платное (CppDepend), либо мёртвое (Tomtom/cpp-dependencies), либо в смежной задаче (IWYU про минимизацию includes).

---

## Целевая аудитория

В порядке убывания приоритета:

1. **C++ команды, активно использующие AI-кодинг.** Им нужен внешний guardrail, который не зависит от того, помнит ли модель правила.
2. **Open source проекты средне-крупного размера (10k–500k LOC)**, где у мейнтейнеров нет ресурса вручную проверять архитектуру в каждом PR.
3. **Команды с legacy C++ кодом**, которые хотят остановить дальнейший дрейф через `--baseline` (текущие нарушения фризятся, новые ломают CI).
4. **Образовательная аудитория** — преподаватели, авторы курсов по физическому дизайну Lakos, Core Guidelines, clean / hexagonal / onion в C++.

### Чего НЕ целевая аудитория

- Embedded команды под жёсткие сертификации (MISRA, CERT) — для них есть PVS-Studio, Coverity. Они платят за каталог чужих правил, а не за DSL для своих.
- Команды без CMake/Bazel/любой системы, генерирующей `compile_commands.json` — без него парсить C++ корректно невозможно.

---

## Что делает (и что не делает)

### Делает

- Парсит `compile_commands.json`, использует libclang для построения include-графа и AST.
- Принимает YAML-конфиг с описанием модулей и правил между ними.
- Проверяет:
  - Запрещённые межмодульные зависимости.
  - Циклические зависимости.
  - Правила из C++ Core Guidelines секции SF (источник официальный).
  - Метрики физического дизайна Lakos (CCD, ACD, NCCD, levelizability).
  - Метрики Мартина (Ce, Ca, I, A, D) на уровне namespace.
  - Кастомные паттерны (raw SQL вне data layer, и т.п.).
- Выдаёт отчёт в человеко-читаемом виде, плюс JSON / SARIF для CI-интеграции.
- Возвращает non-zero exit code при нарушениях.
- Работает без конфига с разумными дефолтами (см. ниже).

### НЕ делает

- Не ищет баги, UB, утечки памяти — это PVS-Studio, Coverity, CppCheck.
- Не форматирует код — это clang-format.
- Не проверяет стиль и идиомы — это clang-tidy.
- Не минимизирует includes — это IWYU.
- Не заменяет code review.
- Не GUI, не VS Code extension, не web dashboard. **CLI и CI.**

---

## Дефолтный анализ (без конфига)

Когда инструмент запущен без `--config`, применяется набор универсальных правил, опирающихся на признанные источники. Каждое правило сопровождается атрибуцией, чтобы пользователь не мог сказать "это твоё личное мнение".

Структура — три уровня по силе авторитета.

### Уровень 1. C++ Core Guidelines секция SF (Stroustrup, Sutter)

Это **официальные** правила работы с source-файлами от создателя языка и председателя комитета. Не подлежат обсуждению. Реализуются только статически проверяемые из них (часть руководства — про дизайн, не про статический анализ).

| Rule ID | Что | Тип проверки |
|---|---|---|
| **SF.2** | `.h` не содержит определений объектов / non-inline функций | AST |
| **SF.4** | `#include` идут перед другими декларациями в файле | препроцессор |
| **SF.5** | `.cpp` обязан включать свой `.h` (тот, что определяет его интерфейс) | имена + AST |
| **SF.7** | Нет `using namespace` в глобальном scope заголовка | AST |
| **SF.8** | Каждый `.h` имеет include guards или `#pragma once` | препроцессор |
| **SF.9** | **Нет циклических зависимостей между source-файлами** | граф |
| **SF.10** | Нет зависимостей на implicitly-included names | AST + includes |
| **SF.11** | Заголовки self-contained (можно включить первым в TU без падения) | компиляция |
| **SF.21** | Нет анонимных namespace в заголовках | AST |

Девять правил из одного авторитетного источника. Маркетинговая фраза: *"implements 9 statically-checkable rules from the official C++ Core Guidelines SF section"*.

### Уровень 2. Lakos physical design (Large-Scale C++ Software Design)

Книга Lakos — де-факто Библия физического дизайна C++ с 1996 года. Цитируется в C++ комьюнити как Knuth в алгоритмах. Метрики уже реализованы в Dart-пакете `lakos`, в Sw!ftalyzer, в XDepend — **но не в open-source инструменте для C++**.

#### Метрики

- **CCD** (Cumulative Component Dependency) — сумма по всем компонентам числа компонентов, нужных для инкрементального тестирования каждого из них. Прямой показатель связности всей системы.
- **ACD** (Average Component Dependency) = `CCD / N`. Среднее число компонентов, которые могут поломаться при изменении одного.
- **NCCD** (Normalized CCD) = `CCD / CCD_balanced_binary_tree(N)`. **>2.0 → почти гарантированно есть циклы**; ≈1.0 → здоровая иерархия; <1.0 → "горизонтальный" граф.
- **Component Dependency (CD)** для каждого компонента — число компонентов, от которых он зависит транзитивно.
- **In-degree / Out-degree** — для каждого компонента.

#### Правила

- **Acyclic physical dependencies** — главное правило книги. Граф зависимостей компонентов должен быть DAG ("levelizable" в терминологии Lakos).
- **Levelizability** — каждой ноде графа можно присвоить уникальный уровень (длину самой длинной цепочки до листьев).
- **God-headers** — компоненты с in-degree выше порога (по умолчанию 30) репортятся как кандидаты на расщепление. Прямое следствие максимизации **fan-in/fan-out** Lakos.
- **Длина include-цепочек** — если самая длинная цепочка `#include` превышает порог (по умолчанию 10) — предупреждение. Ломает время сборки и тестируемость.

CCD/ACD/NCCD сами по себе не пороговые правила (не "проходит/не проходит"), но публикуются в отчёте и могут использоваться в `--baseline` режиме: "NCCD не должен расти от коммита к коммиту".

### Уровень 3. Martin's package metrics (R.C.Martin, 1994)

Классические метрики из "OO Design Quality Metrics" (Robert C. Martin). На уровне namespace или каталога.

| Метрика | Формула | Смысл |
|---|---|---|
| **Ca** | количество модулей, зависящих от этого | afferent coupling |
| **Ce** | количество модулей, от которых зависит этот | efferent coupling |
| **I** | `Ce / (Ca + Ce)` | instability, 0 = стабильный, 1 = нестабильный |
| **A** | `abstract types / all types` | abstractness |
| **D** | `\|A + I − 1\|` | distance from main sequence |

Используются в PHP Depend, Lattix, NDepend, ArchUnit. Универсальный язык для разговоров об архитектуре с людьми, прошедшими через Java/C# школу.

Опционально, по флагу `--martin-metrics`. Не дефолт по умолчанию, потому что для C++ требует чуть больше эвристик (что считать "абстрактным типом" — pure abstract class? любой класс с virtual? template?).

### Уровень 4. Несомненные практики без явного авторитета

Очевидные вещи, которые никто не оспорит, даже без цитаты:

- `.cpp` включает другой `.cpp` (почти всегда ошибка, кроме редких unity-build кейсов; помечается через `// archcheck: allow`).
- Включение из `src/` в `third_party/` (если структура распознаваема по пути).
- Forward declaration типов из `std::` (UB по стандарту, регламентируется `[namespace.std]`).
- Циклические зависимости между namespace (а-ля "namespace A использует типы из B, который использует типы из A").

### Управление правилами

Каждое дефолтное правило можно отключить:
- через флаг CLI: `--disable=SF.9,SF.21`
- через конфиг
- через комментарий в коде: `// archcheck: allow SF.7 (using namespace std for transition)`

---

## Анализ по конфигу

### Формат

```yaml
# .archcheck.yml
version: 1

modules:
  domain:
    paths: ["src/domain/**"]
  application:
    paths: ["src/application/**"]
  infrastructure:
    paths: ["src/infrastructure/**"]
  ui:
    paths: ["src/ui/**"]

rules:
  - name: "domain is independent"
    module: domain
    forbidden_deps: [application, infrastructure, ui]

  - name: "ui depends only on application"
    module: ui
    allowed_deps: [application]

  - name: "no raw SQL outside infrastructure"
    pattern: '\b(SELECT|INSERT|UPDATE|DELETE)\s'
    forbidden_in: [domain, application, ui]

# Включить/выключить дефолтные правила
defaults:
  core_guidelines: true       # все SF.* правила
  lakos_metrics: true         # CCD/ACD/NCCD в отчёт
  martin_metrics: false       # отключено (нужно явное согласие на эвристики)
  disable: [SF.10]            # не проверять конкретное правило

thresholds:
  max_include_depth: 10
  god_header_fan_in: 30
  nccd_warning: 2.0
```

### Команды

```bash
# Анализ с дефолтами, без конфига
archcheck --compile-commands build/compile_commands.json

# С кастомным конфигом
archcheck --config .archcheck.yml --compile-commands build/compile_commands.json

# JSON-вывод для CI
archcheck --format json --output report.json ...

# SARIF для GitHub Code Scanning
archcheck --format sarif --output sarif.json ...

# Заморозить текущие нарушения как baseline
archcheck --baseline .archcheck-baseline.json ...

# Сгенерировать стартовый конфиг по существующему коду
archcheck --suggest-config > .archcheck.yml

# Только метрики, без правил
archcheck --metrics ...
```

### Exit codes

- `0` — нарушений нет.
- `1` — найдены нарушения.
- `2` — ошибка конфигурации или парсинга.
- `3` — внутренняя ошибка инструмента.

---

## Архитектура инструмента

### Технологический стек

- **Язык:** C++20.
- **Парсинг:** libclang / libtooling (LLVM). Стандартный путь для AST-инструментов на C++. tree-sitter рассматривался — даёт только синтаксическое дерево без семантики, недостаточно для архитектурного анализа.
- **Конфиг:** YAML через `ryml` (header-only, быстрый, без зависимостей) или `yaml-cpp`.
- **Build:** CMake.
- **CI:** GitHub Actions.
- **Тесты:** Catch2 или GoogleTest.

### Открытый архитектурный вопрос: двух-бекендная схема

Для include-only правил (SF.7, SF.8, SF.9, циклы, god-headers, длина цепочек) **libclang избыточен**: эти правила решаются простым препроцессорным сканированием. libclang нужен только для семантических правил (SF.2, SF.5, SF.10, SF.21, Martin abstractness).

**Опция:** разделить на два бекенда — fast preprocessor-only (работает без `compile_commands.json`, мгновенно, на любой build-системе) и slow libclang-based (для семантики). На `--fast` запускается только первый.

Преимущества:
- Снимает риск производительности libclang на больших проектах.
- Снижает порог входа: проект без `compile_commands.json` получит хотя бы базовую проверку.
- Уменьшает MVP-скоуп.

**Решение откладывается до проектного спайка на v0.1.**

### Зависимости — минимум

Только libclang и YAML-парсер. Никакого boost, никаких тяжёлых graph-библиотек (граф — `unordered_map<NodeId, vector<NodeId>>`, большего не нужно).

### Распространение

- Single static binary через GitHub Releases.
- Docker image для CI.
- Pre-built под Linux x86_64, Linux arm64, macOS arm64, Windows x64.
- Опционально позже: Homebrew formula, vcpkg port, AUR package.

### Высокоуровневая структура кода

```
src/
  main.cpp                   # entry point, CLI parsing
  config/
    config_loader.h/.cpp     # YAML -> internal Config
    config.h                 # Config struct
  graph/
    component_graph.h/.cpp   # DAG, cycle detection, levelization
    metrics.h/.cpp           # CCD, ACD, NCCD calculation
  scan/
    include_scanner.h/.cpp   # fast preprocessor-only backend
    clang_scanner.h/.cpp     # libclang-based semantic backend
  rules/
    core_guidelines/         # SF.* rules, one file per rule
    lakos/                   # cycles, god-headers, depth
    martin/                  # I, A, D (optional)
    custom/                  # user-defined pattern rules
  report/
    text_reporter.h/.cpp
    json_reporter.h/.cpp
    sarif_reporter.h/.cpp
tests/
  integration/               # сэмпл-проекты с известными нарушениями
  unit/
```

Одно правило = один класс, реализующий интерфейс `IRule`. Регистрация через статическую таблицу. SOLID: добавление правила = новый файл, ничего существующего не правится (OCP).

---

## Стратегия dogfooding и маркетинга

### Регрессионная проверка на чужих репах

В CI добавляется job, который раз в неделю запускает инструмент на топ-20 популярных C++ open source проектах и публикует отчёт. Кандидаты:

- `fmt` — небольшой, чистый, baseline где мало нарушений.
- `Catch2` — header-only.
- `nlohmann/json` — тяжёлая шаблонная иерархия.
- `folly` — большой, корпоративный.
- `abseil-cpp` — корпоративный, чисто написанный.
- `spdlog` — средний, чистый.
- `grpc`, `protobuf` — большие, с кодогеном.
- `opencv` — огромный, легаси.
- `llvm-project` — анализатор анализирует себя.
- `boost` — отдельная вселенная.

#### Что это даёт

- Регрессионное тестирование. Если инструмент сломался на знакомом проекте — это сигнал.
- Бенчмарк скорости. В README будут реальные цифры по реальным проектам.
- PR-материал. Найденные циклы / нарушения → issue в их репах → у автора в резюме "contributed to LLVM/folly/fmt".
- **Готовый HN/Reddit пост.** Таблица NCCD по топ-20 C++ проектам — это самостоятельная новость, прокидывает инструмент попутно.

### План публичного запуска

1. **MVP в private**, 2-4 недели.
2. **Soft launch.** Репо публичный, README готов, без маркетинга. Тестирование на 3-5 знакомых проектах.
3. **Комментарий в TNG/ArchUnit issue #242:** "FYI, started this, here's the repo". Issue закрыт, но именно поэтому в нём редко комментируют, и любой пользователь, гуглящий "ArchUnit C++", попадает туда. SEO-вес шесть лет копится — бесплатный канал adoption.
4. **Hacker News пост.** Заголовок: "Show HN: archcheck — Lakos metrics and Core Guidelines checks for C++". Скриншот с метриками по llvm-project / boost. Подзаголовок: первая open source ArchUnit-style тулза для C++ за 6 лет.
5. **Reddit `/r/cpp`** — параллельно.
6. **Twitter/X:** теги к Anastasia Kazakova (CLion PM), к TNG (создатели ArchUnit), к Bartek Filipek (`bfilipek.com`, активно пишет про Core Guidelines), к John Lakos лично (он активен в C++ комьюнити).
7. **CppCast / cpp.chat.** Подготовить pitch на 5 минут.

---

## Roadmap

### v0.1 — MVP (2–3 недели)

- Чтение `compile_commands.json`.
- Парсинг через libclang, извлечение `#include`-графа.
- Загрузка YAML-конфига с модулями и базовыми правилами `allowed/forbidden`.
- **Дефолтные правила Уровня 1 (минимум):** SF.7, SF.8, SF.9, SF.21.
- **Дефолтные правила Уровня 2:** циклы, длина цепочек, god-headers, CCD/ACD/NCCD в отчёте.
- Простой текстовый репорт с цветным выводом в TTY.
- Exit codes.
- Базовая CI на GitHub Actions.
- README с quickstart и упоминанием Lakos / Core Guidelines в первом абзаце.

**Цель:** инструмент уже даёт ценность. Конкретный пользователь может скачать, написать конфиг (или запустить без него) и поставить в CI.

### v0.2 — Полный набор Core Guidelines SF и QoL (2 недели)

- Остальные SF-правила (SF.2, SF.4, SF.5, SF.10, SF.11).
- `--suggest-config` — анализирует существующий код, предлагает стартовый YAML.
- `--baseline` — фриз текущих нарушений.
- JSON-выход.

### v0.3 — Martin metrics + AST-правила (3–4 недели)

- Martin metrics (Ce, Ca, I, A, D) на уровне namespace.
- libtooling MatchFinder для семантических кастомных правил.
- DSL для кастомных правил на AST-уровне.
- Pattern matching по regex для текстовых правил.

### v0.4 — Интеграция и distribution (2 недели)

- Pre-commit hook из коробки.
- GitHub Actions workflow example.
- SARIF output для GitHub Code Scanning.
- Docker image.
- Static binary в release-артефактах под все 4 платформы.

### v0.5 — Шаблоны и community (1–2 месяца)

- Готовые конфиги для clean / hexagonal / onion / layered архитектур.
- Регрессионная проверка на топ-N OSS проектов.
- Полная документация: getting started, configuration reference, all rules, comparison with alternatives.
- Гайд по миграции с CppDepend и Tomtom/cpp-dependencies.

### Long-term (6–12 месяцев, опционально)

- Plugin API для кастомных правил.
- Опциональная визуализация графа (graphviz output, не GUI).
- Поддержка C (если будет спрос).
- Метрики Lakos hierarchical reuse, instability/abstractness Мартина для C++ специфики.
- Опциональный bridge к clangd index для скорости.

---

## Ключевые риски и open questions

1. **Libclang перформанс.** На больших проектах может быть медленным. Митигация: двух-бекендная схема (см. выше), кэширование AST.
2. **Compile commands availability.** Некоторые проекты не генерируют `compile_commands.json` из коробки (особенно MSBuild). Митигация: документация по генерации для CMake / Bazel / Meson; fast-бекенд работает и без него.
3. **Юзабилити дефолтных правил.** Слишком строгие → пользователь увидит 5000 нарушений и закроет вкладку. Митигация: дефолтные правила консервативные (только SF.7/8/9/21 + циклы), плюс встроенный `--baseline` сразу.
4. **Имя.** `archcheck` может быть занято. Проверить:
   - github.com/archcheck
   - pypi.org/project/archcheck
   - crates.io/crates/archcheck
   - homebrew formula
   - Если занято — `cpparch`, `archlint`, `archguard-cpp`.
5. **Лицензия.** Apache 2.0 предпочтительнее MIT: ArchUnit под ней, явный patent grant. Совместима со всеми C++ OSS лицензиями.
6. **Шаблоны C++.** Шаблонные классы при инстанциации создают неочевидные межмодульные зависимости. План: на v0.1 анализировать только декларацию шаблона, на v0.3+ — инстанциации.
7. **Что считать "абстрактным типом" для Martin's A.** Pure abstract? Любой с virtual? Template? Документировать эвристики, дать настройки.

---

## Метрики успеха

### Через 3 месяца после публичного релиза

- 50+ stars на GitHub.
- 5+ внешних issues или PRs (не от автора).
- 1+ упоминание в blog post или подкасте от третьей стороны.
- Документация покрывает все базовые сценарии.

### Через 6 месяцев

- 200+ stars.
- Активная регрессионная проверка на 10+ известных C++ проектах в CI.
- 2-3 регулярных contributor'а.

### Через 12 месяцев

- 500+ stars.
- Стабильный 1.0 релиз.
- Используется минимум в 5 публичных C++ проектах для CI.

---

## Принципы разработки

- **YAGNI.** Не делаем фичу, пока её не попросил конкретный пользователь.
- **Single responsibility.** Инструмент проверяет архитектуру. Не визуализирует, не форматирует, не ищет баги.
- **CLI-first.** Никакого GUI, никакого веб-дашборда. Запуск из шелла, выход — текст или JSON.
- **Zero-config first.** Запуск без аргументов даёт осмысленный результат.
- **Single binary.** Никаких runtime-зависимостей кроме libclang (статически слинкованной).
- **Authority over opinion.** Все дефолтные правила имеют атрибуцию (Core Guidelines / Lakos / Martin). Без "по моему мнению".
- **Скучные технологии.** C++20, libclang, YAML, GitHub Actions.
- **Documentation as code.** Документация в репо, генерируется в gh-pages. Без отдельного сайта.

---

## Следующие шаги

1. Проверить занятость имени `archcheck` на github / pypi / crates / homebrew.
2. Создать репозиторий, добавить LICENSE (Apache 2.0) и пустой README.
3. Решить вопрос двух-бекендной схемы (один спайк на день).
4. Поднять минимальную CMake-сборку с libclang.
5. Реализовать `compile_commands.json` reader + базовый include-graph extractor.
6. Реализовать YAML config loader с тремя простыми правилами (allowed_deps, forbidden_deps, forbidden_pattern).
7. Реализовать первые правила: **SF.9 (циклы), SF.7 (using namespace в .h), SF.8 (guards), god-headers**. Они дают видимый результат на первом же реальном проекте.
8. Написать первый integration test на маленьком C++-проекте с известными нарушениями.
9. После работающего MVP — комментарий в TNG/ArchUnit#242.

---

## Источники

- [C++ Core Guidelines, секция SF](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-source) — Stroustrup, Sutter.
- John Lakos. *Large-Scale C++ Software Design.* Addison-Wesley, 1996.
- John Lakos. *Large-Scale C++ Volume I: Process and Architecture.* Addison-Wesley, 2019.
- Robert C. Martin. *OO Design Quality Metrics: An Analysis of Dependencies.* 1994. [PDF](https://linux.ime.usp.br/~joaomm/mac499/arquivos/referencias/oodmetrics.pdf)
- [ArchUnit issue #242 — "Does an archunit exist for c/c++"](https://github.com/TNG/ArchUnit/issues/242)
- Dente, Satriani, Papotti (EURECOM). *Constraint Decay in LLM-Assisted Code Generation.* Май 2026.

---

*Версия 2.0. Документ для нового сеанса разработки. Может быть переработан по ходу обсуждения.*