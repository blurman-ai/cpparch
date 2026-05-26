# archcheck — Architecture rules for C++

> Версия документа: 2.1. Изменения от 2.0:
>
> - Headline переориентирован: «модульные границы и циклы зависимостей в CI» — ядро; Lakos / Core Guidelines / Martin понижены до «в дополнение, цитируемые источники» (#006).
> - AI-guardrail (EURECOM constraint decay paper) поднят в `## TL;DR` как третий абзац (#006).
> - Roadmap полностью перевёрстан: v0.1 — fast backend, без `compile_commands.json` и libclang; libclang только в v0.2; Martin метрики — v0.4 опционально; `--suggest-config` выпил (#006).
> - Двух-бекендный вопрос закрыт как принятое решение, секция переписана (#006).
> - Добавлена секция «Stability contract» — что считается breaking change после v1.0 (#007).
> - Имя продукта зафиксировано как `archcheck` (проверено на GitHub / PyPI / crates.io / Homebrew / npm) (#003).
> - Раздел «Дефолтный анализ» расширен колонкой «Фаза», SF.4 убран из дефолтов с обоснованием, Martin метрики помечены как v0.4 (#006).
> - Пример конфига переориентирован: ядро — `modules` + `forbidden_deps`, defaults — вторичная секция; добавлен минимальный конфиг для legacy (#006).
> - Риски обновлены: лицензионный риск убран (решён, Apache 2.0); риски libclang/compile_commands помечены как v0.2+; Martin's A — v0.4 (#006).
>
> Версия 2.0 → 1.0: подтверждены источники спроса, перепозиционирование вокруг Lakos + Core Guidelines, переработан раздел дефолтных правил (3 уровня), обновлена сравнительная таблица и MVP-скоуп.

---

## TL;DR

Open source CLI для проверки архитектурных инвариантов в C++ проектах. **Ядро — модульные границы и циклы зависимостей в CI:** пользователь описывает в YAML, какие модули какие могут импортировать, archcheck строит граф зависимостей и ломает сборку при нарушении. Нарушения репортятся с `file:line:column`, exit code — non-zero. Для legacy-проектов — `--baseline`: фриз текущих нарушений, новые ломают CI.

Это закрывает многолетнюю дыру в экосистеме: для Java, C#, TypeScript, Python, Go, Rust, Dart такие инструменты есть, в C++ — ни одного живого open source.

**AI-guardrail.** Свежий paper из EURECOM (Dente, Satriani, Papotti, май 2026) измерил эффект *constraint decay* — ~30 п.п. просадки assertion pass rate в LLM-кодинге при добавлении структурных ограничений. Лечится выносом архитектурных проверок за пределы промпта — в CI. Промпт деградирует с контекстом, CI — нет.

В дополнение к user-defined правилам — набор проверок с явной атрибуцией: циклы и god-headers по [Lakos](https://www.amazon.com/Large-Scale-Software-Design-John-Lakos/dp/0201633620), правила из секции SF [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-source) (Stroustrup, Sutter), опционально — метрики Мартина (I/A/D). Каждое дефолтное правило цитирует признанный источник — это снимает аргумент «это твоё мнение».

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

- Принимает YAML-конфиг с описанием модулей и разрешённых / запрещённых зависимостей между ними.
- Сканирует исходники: по умолчанию — препроцессорный скан включений (без `compile_commands.json` и без libclang); libclang добавляется через `--with-clang` для семантических проверок (см. §«Двух-бекендная схема»).
- Проверяет:
  - **Запрещённые межмодульные зависимости** (ядро продукта).
  - **Циклические зависимости** в include-графе (SF.9).
  - God-headers, длину include-цепочек, метрики Lakos (CCD/ACD/NCCD в отчёте).
  - Правила из C++ Core Guidelines секции SF — каждое с атрибуцией.
  - Метрики Мартина (Ce, Ca, I, A, D) на уровне namespace — опционально.
  - Кастомные паттерны (raw SQL вне data layer, и т.п.).
- Поддерживает `--baseline` с первого релиза: фриз текущих нарушений, новые ломают CI.
- Выдаёт отчёт в человеко-читаемом виде, плюс JSON и SARIF для CI-интеграции.
- Возвращает non-zero exit code при нарушениях (см. §Exit codes).
- Работает без конфига с консервативными дефолтами (см. §«Дефолтный анализ»).

### НЕ делает

- Не ищет баги, UB, утечки памяти — это PVS-Studio, Coverity, CppCheck.
- Не форматирует код — это clang-format.
- Не проверяет стиль и идиомы — это clang-tidy.
- Не минимизирует includes — это IWYU.
- Не заменяет code review.
- Не GUI, не VS Code extension, не web dashboard. **CLI и CI.**

---

## Дефолтный анализ (без конфига)

Когда archcheck запущен без `--config`, применяется набор универсальных правил с явной атрибуцией. Каждое правило цитирует признанный источник — это снимает аргумент «это твоё мнение». При этом любое правило можно выключить: через CLI-флаг, через конфиг, или через комментарий в коде. Атрибуция — это не догмат, а защита от обвинения в вкусовщине.

Структура дефолтов — по источнику авторитета и по фазе roadmap (v0.1 / v0.2 / опционально).

### Уровень 1. C++ Core Guidelines секция SF (Stroustrup, Sutter)

Правила работы с source-файлами от создателя языка и председателя комитета — официальный источник. Реализуются только статически проверяемые из них (часть руководства — про дизайн, не про статический анализ).

| Rule ID | Что | Тип проверки | Фаза |
|---|---|---|---|
| **SF.7** | Нет `using namespace` в глобальном scope заголовка | text-scan / AST | **v0.1** (approx), v0.2 (precise) |
| **SF.8** | Каждый `.h` имеет include guards или `#pragma once` | препроцессор | **v0.1** |
| **SF.9** | **Нет циклических зависимостей между source-файлами** | граф | **v0.1** |
| **SF.21** | Нет анонимных namespace в заголовках | text-scan / AST | **v0.1** (approx), v0.2 (precise) |
| **SF.2** | `.h` не содержит определений объектов / non-inline функций | AST | v0.2 |
| **SF.5** | `.cpp` обязан включать свой `.h` | имена + AST | v0.2 |
| **SF.10** | Нет зависимостей на implicitly-included names | AST + includes | v0.2 |
| **SF.11** | Заголовки self-contained (можно включить первым в TU без падения) | компиляция | v0.2 |

**SF.4 (`#include` идут перед другими декларациями)** — намеренно не дефолт: проверяется тривиально, но продуктовой ценности почти нет — это ещё один style-check, а не архитектурный инвариант. Можно включить руками через `--enable=SF.4`. Оценку правил см. в [`docs/research/`](../research/).

Маркетинговая фраза: *"implements 8 statically-checkable rules from the C++ Core Guidelines SF section across v0.1 and v0.2"*.

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

**Опционально**, по флагу `--martin-metrics`. Не входят в дефолты по двум причинам: (1) для C++ требуют эвристик (что считать абстрактным типом — pure abstract class? любой с virtual? template?), (2) не двигают ранний adoption — пользователь покупает «запрет domain → infrastructure», а не NCCD. Появляются в v0.4 (см. Roadmap).

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

### Классы правил по источнику архитектурной истины

Для продукта эпохи AI-кодинга важно разделять правила не только по теме
(Core Guidelines / Lakos / Martin / custom), но и по тому, **откуда берётся
архитектурная истина**, необходимая для проверки.

#### Intrinsic rules

Правило вычисляется из кода, графа зависимостей и diff-а без знания домена.
Оно не требует ручной параметризации пользователем.

Свойства:

- можно запускать на любом репозитории сразу;
- хорошо подходит для zero-config CI;
- false positive rate должен быть низким;
- правило должно быть объяснимо одной фразой.

#### Repo-inferred rules

Правило требует архитектурной гипотезы, но эта гипотеза может быть выведена
автоматически по структуре репозитория, истории и фактическому графу
зависимостей.

Свойства:

- AI-агент может предложить параметризацию после чтения репозитория;
- до подтверждения пользователя правило работает как `report-only` или
  `warning`;
- после подтверждения inferred-конфиг становится обычной частью проекта.

Типичные источники вывода:

- структура каталогов (`include/`, `src/`, `public/`, `private/`, `detail/`);
- фактические entrypoints между подсистемами;
- исторически сложившиеся фасады;
- baseline текущего графа.

#### User-declared rules

Правило выражает намеренную архитектуру команды, которую нельзя надёжно
угадать из кода.

Свойства:

- без явной конфигурации не включается;
- именно этот класс даёт максимальную бизнес-ценность;
- но именно он имеет самый высокий порог внедрения.

Типичные примеры:

- `domain` не зависит от `infrastructure`;
- `ui` зависит только от `application`;
- raw SQL запрещён вне data layer;
- конкретные third-party зависимости запрещены в части системы.

### Drift-regression rules

**Snapshot-анализ (intrinsic + user-declared rules на текущем состоянии репозитория) остаётся ядром продукта.** На нём держится первый запуск без конфига, дог-фудинг на топ-N OSS проектов из roadmap v0.5, и весь объём пользовательских архитектурных контрактов. DRIFT — это **дополнительный слой** для сценария, когда есть baseline графа и есть смысл сравнивать "до" и "после".

Отдельный продуктовый инсайт для эпохи AI-assisted development: существует
полезный класс правил, которые ловят не “плохой код вообще”, а
**структурное ухудшение относительно baseline**.

Это не замена обычному baseline нарушений.

- `--baseline` замораживает нарушения правил (`SF.*`, `Lakos`, custom).
- `--graph-baseline` хранит снимок графа зависимостей для `DRIFT.*`.

То есть обычный baseline отвечает на вопрос:
“появились ли новые нарушения известных правил?”

А `DRIFT.*` отвечает на вопрос:
“ухудшилась ли сама структура графа, даже если ни одно из других правил не
сработало?”

#### Первый прототип

Первый прототип drift-анализа должен быть намеренно узким:

- только `intrinsic rules`;
- только file-level dependency graph;
- только `warning` по умолчанию;
- без git history;
- без repo inference;
- без автоматической параметризации агентом.

Его задача — доказать, что diff-based structural analysis вообще даёт полезный
сигнал.

#### DRIFT.1 `no_new_shortcut_edge`

**Класс:** `intrinsic`

**Что ловит:** новое прямое ребро зависимости, которое сокращает уже
существующий многошаговый путь.

**Формализация:**

- строится baseline dependency graph проекта;
- для каждого нового ребра `u -> v`, появившегося после изменения,
  проверяется, существовал ли до изменения путь `u => v` длиной `>= 2`;
- если такой путь существовал, новое ребро помечается как `shortcut edge`.

**Замечание:** правило может давать честные false positives в случае
осознанного рефакторинга. Поэтому в первой реализации оно должно быть
`warning` и поддерживать suppression / allow-механизм.

#### DRIFT.2 `no_new_cycle_or_cycle_growth`

**Класс:** `intrinsic`

**Что ловит:** появление нового цикла или увеличение существующего strongly
connected component.

**Формализация:**

- baseline и post-change графы раскладываются на SCC;
- правило срабатывает, если появился новый SCC размером `> 1`,
  появился self-loop, или существующий SCC вырос по числу узлов / рёбер.

**Замечание:** это самый сильный и наименее спорный кандидат для первой
реализации.

#### Правила следующей волны

Следующие drift-правила признаются перспективными, но не входят в первый
прототип:

- `DRIFT.3 public_surface_growth`
- `DRIFT.4 blast_radius_growth`
- `DRIFT.5 hub_reinforcement`
- `DRIFT.6 boundary_bypass_of_existing_entrypoint`
- `DRIFT.7 scattered_new_boundary_access`
- `DRIFT.8 hotspot_inflow`

Причины переноса:

- `DRIFT.3`, `DRIFT.6`, `DRIFT.7` требуют repo inference;
- `DRIFT.4`, `DRIFT.5` требуют настройки порогов и сначала должны жить как
  `report-only`;
- `DRIFT.8` требует git history и должен быть opt-in
  (`--with-git-history` или аналог).

#### Graph baseline

Для `DRIFT.*` ядро должно опираться на отдельный baseline графа:

- `--graph-baseline <file>` — загрузить baseline graph;
- `--write-graph-baseline <file>` — записать baseline graph.

Git-based режим допустим как удобный adapter поверх этого механизма, но не как
единственная семантика:

- `--graph-baseline-from-git <rev>` — опционально, позже.

#### Минимальный baseline contract

В первой реализации `graph-baseline` должен хранить канонический snapshot,
а не derived metrics:

- format version;
- нормализованный список узлов;
- нормализованный список рёбер.

Все производные величины (`SCC`, `reachability`, `blast radius`, `hubness`)
должны вычисляться при загрузке baseline.

Это делает baseline проще, устойчивее и не привязывает формат хранения к
текущему набору drift-правил.

---

## Анализ по конфигу

**Ядро конфига — `modules` + `forbidden_deps` / `allowed_deps` между ними.** Это то, ради чего пользователь ставит archcheck. Дефолтные правила (SF.*, Lakos-метрики) включены, но не headline — их можно отключить блоком `defaults`.

### Формат

```yaml
# .archcheck.yml
version: 1

# 1. Модули — описываем структуру проекта.
modules:
  domain:
    paths: ["src/domain/**"]
  application:
    paths: ["src/application/**"]
  infrastructure:
    paths: ["src/infrastructure/**"]
  ui:
    paths: ["src/ui/**"]

# 2. Правила между модулями — главная ценность.
rules:
  - name: "domain is independent"
    module: domain
    forbidden_deps: [application, infrastructure, ui]

  - name: "ui depends only on application"
    module: ui
    allowed_deps: [application]

  # Pattern-правила — опционально.
  - name: "no raw SQL outside infrastructure"
    pattern: '\b(SELECT|INSERT|UPDATE|DELETE)\s'
    forbidden_in: [domain, application, ui]

# 3. Дефолтные правила — включить/выключить группы.
defaults:
  core_guidelines: true       # все SF.* правила, доступные на текущей фазе
  lakos_metrics: true         # CCD/ACD/NCCD в отчёт
  martin_metrics: false       # отключено (heuristic-tier, v0.4+)
  disable: [SF.10]            # не проверять конкретное правило

# 4. Пороги Lakos-метрик.
thresholds:
  max_include_depth: 10
  god_header_fan_in: 30
  nccd_warning: 2.0
```

**Минимально-полезный конфиг** (типичный first-run для legacy-проекта):

```yaml
version: 1
modules:
  core: { paths: ["src/core/**"] }
  ui:   { paths: ["src/ui/**"] }
rules:
  - module: core
    forbidden_deps: [ui]
```

Запустить, заморозить нарушения baseline-ом, поставить в CI — всё.

### Команды

```bash
# Анализ с дефолтами, без конфига и без compile_commands.json.
# Fast backend — препроцессорный скан, работает на любом проекте.
archcheck

# С кастомным конфигом.
archcheck --config .archcheck.yml

# Семантические правила (SF.2/5/10/11, правила из C/I) — нужен libclang.
archcheck --config .archcheck.yml --with-clang --compile-commands build/compile_commands.json

# JSON-вывод для CI.
archcheck --format json --output report.json

# SARIF для GitHub Code Scanning (v0.2+).
archcheck --format sarif --output sarif.json

# Заморозить текущие нарушения как baseline (day-one feature).
archcheck --baseline .archcheck-baseline.json

# Только метрики, без жёстких правил.
archcheck --metrics
```

### Exit codes

- `0` — нарушений нет.
- `1` — найдены нарушения.
- `2` — ошибка конфигурации или парсинга.
- `3` — внутренняя ошибка инструмента.

---

## Stability contract

После релиза `v1.0` следующие интерфейсы — публичные и подчиняются [SemVer 2.0](https://semver.org/spec/v2.0.0.html). Любое breaking-изменение → MAJOR bump.

| Интерфейс | Breaking change (MAJOR) | Non-breaking (MINOR) |
|---|---|---|
| Exit codes | удаление кода, изменение семантики существующего | добавление нового кода |
| CLI флаги | удаление флага, изменение поведения, переименование | добавление флага с default-значением |
| JSON-схема отчёта | удаление поля, изменение типа | добавление поля |
| SARIF выход | изменение схемы | добавление полей (в рамках SARIF spec) |
| Формат YAML-конфига | удаление ключа, изменение типа/семантики | добавление ключа с default |
| Формат baseline-файла | изменение схемы (ломает существующие baselines) | расширение схемы с обратной совместимостью чтения |
| Набор дефолтных правил | добавление правила, ломающее ранее зелёные проекты | добавление правила, выключаемого через `--disable` |

**До v1.0** (`0.x.y`) — SemVer §4 разрешает breaking-изменения в MINOR. Стремимся избегать, но не запрещено. Любое breaking-изменение фиксируется в [`CHANGELOG.md`](../CHANGELOG.md) секции `Changed` с пометкой `**Breaking:**`.

---

## Архитектура инструмента

### Технологический стек

- **Язык:** C++20.
- **Парсинг:** два бекенда (см. ниже). Fast — препроцессорное сканирование (свой парсер `#include`-директив и комментариев), без AST-зависимостей. Slow — libclang / libtooling (LLVM) для AST-семантики. tree-sitter рассматривался — даёт только синтаксическое дерево без семантики, недостаточно для семантических правил.
- **Конфиг:** YAML через `ryml` (header-only, быстрый, без зависимостей) или `yaml-cpp`.
- **Build:** CMake.
- **CI:** GitHub Actions.
- **Тесты:** Catch2 или GoogleTest.

### Двух-бекендная схема (решение для v0.1)

Для include-only правил (SF.7, SF.8, SF.9, циклы, god-headers, длина цепочек) libclang избыточен — эти правила решаются препроцессорным сканированием. libclang нужен только для семантических правил (SF.2, SF.5, SF.10, SF.21, Martin abstractness, правила из C/I секций Core Guidelines).

**Принято: два бекенда, fast по умолчанию.**

- **Fast backend (preprocessor-only)** — default в v0.1. Работает без `compile_commands.json`, мгновенно, на любой build-системе. Покрывает critical-path правила v0.1: `forbidden_deps`/`allowed_deps`, циклы (SF.9), god-headers, длина include-цепочек, SF.7/SF.8/SF.21.
- **libclang backend** — opt-in в v0.1 через флаг (`--with-clang`), полноценно появляется в v0.2. Нужен для AST-проверок: SF.2 / SF.5 / SF.10 / SF.11, правила из секций C и I Core Guidelines, Martin's *Abstractness* (A).

Преимущества решения:
- **Низкий порог входа.** Первый запуск на новом проекте — без настройки CMake и `compile_commands.json`.
- **Производительность.** Большие проекты получают быструю проверку основного набора правил без libclang-overhead.
- **Уменьшает MVP-скоуп.** v0.1 не зависит от libclang для основной ценности.

Trade-off: семантические правила Уровня 1 (SF.2, SF.5, SF.10, SF.11) сдвигаются из v0.1 в v0.2. Это сознательный выбор — лучше отложить семантику, чем требовать `compile_commands.json` на первом запуске.

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

### v0.1 — MVP: module boundaries + cycles в CI (2–3 недели)

**Cover-story:** запуск на чужом проекте, без `compile_commands.json` и libclang, даёт сразу полезный результат за 10 минут.

- **Fast backend** (preprocessor-only) — единственный.
- YAML-конфиг: `forbidden_deps` / `allowed_deps` между модулями (по path-glob).
- **Core правила:**
  - Циклы зависимостей (SF.9).
  - God-headers (Lakos): in-degree > threshold (default 30).
  - Длина include-цепочек (Lakos): > threshold (default 10).
  - SF.7 (using namespace в `.h` — text-scan, approximate).
  - SF.8 (include guards / `#pragma once`).
  - SF.21 (anonymous namespace в `.h`).
- **`--baseline` с day one**: фриз текущих нарушений, новые ломают CI.
- Text-репорт с цветным выводом в TTY + JSON-репорт.
- Exit codes (см. §Exit codes).
- Базовая CI на GitHub Actions для самого archcheck.

**Цель:** пользователь может склонить репо, запустить `archcheck`, получить осмысленные нарушения, заморозить их baseline-ом и встроить в CI. Без CMake-плясок, без libclang.

### v0.2 — libclang backend + остальные SF (3–4 недели)

- **libclang backend** становится opt-in mainline (через `--with-clang`).
- Остальные SF-правила: SF.2 (no defs in headers), SF.5 (`.cpp` includes its `.h`), SF.10 (no implicit includes), SF.11 (self-contained headers).
- Точная версия SF.7 (через AST вместо text-scan), точная SF.21.
- **SARIF output** для GitHub Code Scanning.

### v0.3 — Правила из C / I / NL секций CCG + BDE (3–4 недели)

См. [docs/research/rules/](../research/rules/).

- **C:** C.121 (interface pure abstract), C.133 (no protected data), C.134 (uniform access level).
- **I:** I.2 (no mutable globals), I.3 (no singletons), I.22 (no complex global init).
- **NL:** NL.27 (file suffix).
- **Bloomberg BDE:** no-inter-component-friendship, external-linkage-declared-in-header.
- Прочие: forward-decl-of-std, deep-nested-namespace.

### v0.4 — Martin metrics + distribution polish (3–4 недели)

- **Martin metrics:** Ce, Ca, I, A, D на уровне namespace — опционально, по флагу. Включаем, **только если пользователи попросят**: для ранних adopter-ов archcheck-а более ценны жёсткие правила, чем дашборд-метрики.
- Кастомные pattern-правила: regex по тексту (raw SQL outside data layer и т.п.).
- Pre-commit hook из коробки.
- Docker image.
- Static binary в release-артефактах под Linux x86_64/arm64, macOS arm64, Windows x64.
- GitHub Actions workflow example.

### v0.5 — Шаблоны и community (1–2 месяца)

- **Templates** под clean / hexagonal / onion / layered архитектуры (готовые `arch.yaml`).
- Регрессионная проверка на топ-N OSS проектов (fmt, Catch2, spdlog, abseil, folly и т.п.).
- Полная документация: getting started, configuration reference, all rules, comparison with alternatives.
- Гайд по миграции с CppDepend и Tomtom/cpp-dependencies.

### Long-term (6–12 месяцев, опционально)

- Plugin API для кастомных правил.
- Опциональная визуализация графа (graphviz output, не GUI).
- Поддержка C (если будет спрос).
- Метрики Lakos hierarchical reuse.
- Опциональный bridge к clangd index для скорости на больших проектах.

### Что не делаем

- **`--suggest-config`** — выпил из roadmap. Авто-вывод модульной структуры либо тривиален (по каталогам — пользователь напишет за 5 минут), либо магия, которой не поверят. Если позже возникнет конкретный запрос — пересмотрим.

---

## Ключевые риски и open questions

1. **Libclang перформанс (v0.2+).** На больших проектах libclang-бекенд может быть медленным. Митигация: fast backend default-ный, libclang только opt-in для семантических правил; кэширование AST; clangd index bridge — long-term опция.
2. **Compile commands availability (v0.2+).** Некоторые проекты не генерируют `compile_commands.json` из коробки (особенно MSBuild). Митигация: fast-бекенд работает без него, поэтому риск касается только пользователей, которые включили `--with-clang`. Документация по генерации для CMake / Bazel / Meson.
3. **Юзабилити дефолтных правил.** Слишком строгие → пользователь увидит 5000 нарушений и закроет вкладку. Митигация: дефолтные правила консервативные (только SF.7/8/9/21 + циклы + god-headers + длина цепочек), плюс `--baseline` с day one.
4. **Шаблоны C++ (v0.3+).** Шаблонные классы при инстанциации создают неочевидные межмодульные зависимости. План: на v0.1 правила работают по includes (этого хватает для cycles / god-headers / SF.7/8/21); на v0.3+, когда придут AST-правила, анализировать декларации шаблона; инстанциации — long-term.
5. **«Абстрактный тип» для Martin's A (v0.4).** Pure abstract? Любой с virtual? Template? Документировать эвристики, дать настройки. Не блокер — Martin metrics опциональны.

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

1. Поднять минимальную CMake-сборку (fast-бекенд без libclang в v0.1, см. §«Двух-бекендная схема»).
2. Реализовать `compile_commands.json` reader + базовый include-graph extractor (опциональный вход — fast-бекенд работает и без него).
3. Реализовать YAML config loader с тремя простыми правилами (allowed_deps, forbidden_deps, forbidden_pattern).
4. Реализовать первые правила: **SF.9 (циклы), SF.7 (using namespace в .h), SF.8 (guards), god-headers**. Они дают видимый результат на первом же реальном проекте.
5. Написать первый integration test на маленьком C++-проекте с известными нарушениями.
6. После работающего MVP — комментарий в TNG/ArchUnit#242.

---

## Источники

- [C++ Core Guidelines, секция SF](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-source) — Stroustrup, Sutter.
- John Lakos. *Large-Scale C++ Software Design.* Addison-Wesley, 1996.
- John Lakos. *Large-Scale C++ Volume I: Process and Architecture.* Addison-Wesley, 2019.
- Robert C. Martin. *OO Design Quality Metrics: An Analysis of Dependencies.* 1994. [PDF](https://linux.ime.usp.br/~joaomm/mac499/arquivos/referencias/oodmetrics.pdf)
- [ArchUnit issue #242 — "Does an archunit exist for c/c++"](https://github.com/TNG/ArchUnit/issues/242)
- Dente, Satriani, Papotti (EURECOM). *Constraint Decay in LLM-Assisted Code Generation.* Май 2026.

---

*Версия 2.1. Документ перерабатывается по ходу обсуждения; конкретные изменения от 2.0 — см. список в шапке.*
