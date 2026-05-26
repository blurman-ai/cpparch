# [GRAPH] Фундамент графа зависимостей и diff-примитивы

**Дата создания:** 2026-05-26
**Дата старта:** 2026-05-26
**Статус:** wip
**Модуль:** GRAPH
**Приоритет:** blocker
**Сложность:** M (2-4 дня)
**Блокирует:** #009 (ai_drift_regression_rules)
**Заблокирован:** —
**Related:** #006 (spec_refactor)

## Цель

Зафиксировать и реализовать канонический file-level dependency graph и
отдельный `graph-baseline` contract, без которых нельзя надёжно реализовывать
первый прототип `DRIFT.*`.

## Контекст

В спеке появился новый класс `drift-regression rules`, который смотрит не на
“плохой код вообще”, а на структурное ухудшение графа относительно baseline.
Первый прототип намеренно узкий: только file-level graph, только `DRIFT.1` и
`DRIFT.2`, без git history и без repo inference.

Для этого нужен не просто разовый include-graph, а устойчивая внутренняя
модель графа, пригодная для:

- построения из результатов scan-подсистемы;
- вычисления SCC и reachability;
- сравнения baseline-графа и графа после изменения;
- последующего хранения и загрузки минимального `graph-baseline`.

Отдельно проведено первичное исследование вопроса “можно ли взять extraction
слой готовым”. Вывод на текущий момент такой:

- **готовые dependency extractors есть**, но почти все они build-aware и
  требуют точной compile-конфигурации (`-I`, defines, compile database,
  toolchain);
- для `v0.1` fast backend нужен **zero-config repo-local extraction**, то есть
  детерминированный textual include resolver, который умеет работать без
  `compile_commands.json`;
- поэтому готовые инструменты разумно рассматривать как
  **optional adapters / validation sources**, а не как ядро v0.1.

Наиболее полезные кандидаты:

- **compiler depfiles** (`gcc` / `clang` с `-M`, `-MM`, `-MD`, `-MF`) — хороший
  источник build-resolved зависимостей, но только если уже известны include
  paths и compile flags;
- **`clang-scan-deps`** — сильный refined backend / validation source, но тоже
  завязан на compile-команды и toolchain;
- **Include-What-You-Use** — полезен для include hygiene, но не заменяет наш
  zero-config graph extractor;
- **`makedepend`** — исторический fallback-класс инструментов, но не лучший
  фундамент для продукта.

Отсюда рабочая гипотеза: канонический graph container и fast extraction core
делаем сами; готовые extractors позже подключаем как альтернативные источники
или cross-check режимы.

Эта задача блокирует реализацию первого прототипа `DRIFT.*` и часть будущих
Lakos/graph-based checks.

## План выполнения

- [ ] Зафиксировать канонический уровень графа для v0.1: `file-level`
- [ ] Зафиксировать, что module-level представления строятся как проекция поверх file graph, а не как отдельная первичная модель
- [ ] Определить, что считается dependency edge в fast-backend для v0.1
- [ ] Зафиксировать extraction pipeline для fast backend: `discover_files -> scan_includes -> resolve -> classify -> build_graph -> diagnostics`
- [ ] Описать формат внутренних идентификаторов узлов (`NodeId`) и нормализацию путей
- [ ] Зафиксировать политику include resolution для `#include "..."` и `#include <...>` в zero-config режиме
- [ ] Зафиксировать классификацию результатов extraction: `project`, `external`, `unresolved`, `ambiguous`
- [ ] Зафиксировать, где и как later подключаются optional adapters (`depfiles`, `clang-scan-deps`) без размывания fast-path семантики
- [ ] Спроектировать `component_graph` / `dependency_graph` API без лишней абстракции
- [ ] Реализовать операции: add node, add edge, adjacency, reverse adjacency, edge existence
- [ ] Реализовать graph algorithms, нужные как примитивы: SCC, reachable set, reverse reachable set, path existence
- [ ] Добавить diff-примитивы для первого прототипа: new edges, removed edges, grown SCC
- [ ] Зафиксировать `graph-baseline` contract: `format version + normalized nodes + normalized edges`
- [ ] Подготовить fixtures / integration samples для маленьких графов и их diff-сценариев

## Сделано

- **2026-05-26** — задача переведена в `wip`.
- **2026-05-26** — зафиксирован scope первого исследования по extraction layer:
  проблема не в структуре хранения графа, а в устойчивом построении
  file-level include/dependency graph по большому C++-репозиторию.
- **2026-05-26** — проведён первичный обзор готовых источников зависимостей:
  - compiler depfiles (`-M/-MM/-MD/-MF`) годятся как build-aware adapter;
  - `clang-scan-deps` выглядит сильным refined source, но требует compile context;
  - IWYU полезен рядом, но не решает задачу zero-config extraction;
  - следовательно, v0.1 fast backend всё равно требует собственного
    repo-local include resolver.

## В работе

- Уточнение архитектуры extraction layer для `v0.1`: что сканируем, как
  резолвим include-пути, как помечаем `external/unresolved/ambiguous`, и где
  пройдут границы между собственным fast extractor и optional adapters.

## Mini-design: fast extraction v0.1

Ниже — рабочий draft extraction semantics для первого прототипа. Это не
финальный код, а целевая форма поведения fast backend.

### 1. Что сканируем

В graph pipeline попадают **все project files подходящих расширений**, а не
только `.cpp`.

Стартовый набор расширений:

- translation units: `.c`, `.cc`, `.cpp`, `.cxx`
- headers / include-like: `.h`, `.hh`, `.hpp`, `.hxx`, `.ipp`, `.tpp`, `.inl`, `.inc`

По умолчанию из discovery исключаются только явно нецелевые директории:

- `.git/`
- `build/`
- `cmake-build-*/`
- `.cache/`
- `.idea/`
- `.vscode/`
- `out/`

То есть v0.1 **не** пытается автоматически отбрасывать `third_party/`,
`vendor/` или `external/`: это отдельное решение, которое нельзя тихо
зашивать эвристикой.

### 2. Как сканируем include-директивы

Fast backend в v0.1 — это **textual include scanner**, а не полноценный
препроцессор и не AST.

Сканер должен:

- читать файл как текст;
- понимать logical lines с `\\`-continuation;
- игнорировать `#include` внутри line comments, block comments, string literals,
  char literals и raw string literals;
- распознавать директиву только когда `#` стоит как первый значимый символ
  logical line;
- извлекать только literal includes:
  - `#include "path"`
  - `#include <path>`

Что сознательно **не** делаем в v0.1:

- не раскрываем macro-based includes (`#include SOME_MACRO`);
- не интерпретируем include через token pasting и другие macro tricks;
- не пытаемся эмулировать компиляторный preprocessor state.

Macro-based include должен попадать в diagnostics как отдельный случай
(`dynamic_include` / `macro_include`), но **не** порождать ребро графа.

### 3. Как относимся к conditional compilation

Для zero-config режима у нас нет надёжного набора `-D` и compile target
context-а. Поэтому v0.1 принимает честное ограничение:

- **literal `#include` в любой ветке `#if/#ifdef/#ifndef` считается candidate include**;
- fast graph therefore является over-approximation;
- branch-sensitive уточнение — это задача optional adapters
  (`depfiles`, `clang-scan-deps`) или более позднего refined backend.

Это важно зафиксировать заранее: v0.1 строит **структурно полезный**, но не
компиляторно-идеальный граф.

### 4. Как резолвим include token в project file

Ключевая задача extraction layer — не хранение графа, а **repo-local include
resolution**.

Для этого заранее строятся индексы project files:

- `exact_path_index`: `repo-relative path -> file`
- `suffix_index`: `include token suffix -> [candidate files]`

Стартовый алгоритм resolution:

#### Для `#include "x"`

1. Проверить путь относительно директории текущего файла.
2. Если не найдено — проверить exact repo-relative match `x`.
3. Если не найдено — проверить unique suffix match среди project files.
4. Если найден ровно один кандидат — это `project` include.
5. Если найдено больше одного кандидата — это `ambiguous`.
6. Если не найдено ни одного кандидата — это `unresolved`.

#### Для `#include <x>`

1. Проверить exact repo-relative match `x`.
2. Если не найдено — проверить unique suffix match среди project files.
3. Если найден ровно один кандидат — это `project` include.
4. Если найдено больше одного кандидата — это `ambiguous`.
5. Если не найдено ни одного кандидата — это `external`.

Эта политика даёт простой и объяснимый zero-config contract:

- quote include сначала трактуется как локальный/project include;
- angle include по умолчанию трактуется как external, если репозиторий не даёт
  однозначного project-match;
- resolver не “угадывает” молча, если кандидатов несколько.

### 5. Что попадает в канонический graph

В file-level graph попадают:

- **узлы**: все discovered project files;
- **рёбра**: только **успешно resolved project includes**.

Не попадают в graph edges:

- `external`
- `unresolved`
- `ambiguous`
- `macro_include`

Они должны храниться как diagnostics / extraction findings, а не как рёбра
канонического графа.

Это важно для baseline semantics:

- `graph-baseline` хранит только nodes + resolved project edges;
- extraction warnings живут отдельно и не размывают смысл `DRIFT.1` / `DRIFT.2`.

### 6. Какие diagnostics нужны сразу

Extraction layer должен уметь объяснить, почему edge не появился.

Минимальный diagnostic record:

- source file
- line
- raw include token
- include kind: `quote` / `angle`
- resolution kind: `project` / `external` / `unresolved` / `ambiguous` / `macro_include`
- target file, если include resolved
- candidate files, если include ambiguous

Повторные одинаковые include-ы в одном файле должны dedup-иться на уровне
graph edge, но locations надо сохранять для отчёта.

### 7. Где место готовых инструментов

После фиксации zero-config semantics можно добавлять adapters:

- **depfile adapter** — импорт build-resolved зависимостей из `gcc/clang`
  dependency output;
- **clang-scan-deps adapter** — refined dependency source для проектов, где
  есть compile context;
- **cross-check mode** — сравнение fast graph и refined graph для измерения
  расхождений и настройки эвристик.

Но эти adapters не меняют каноническое определение fast graph для v0.1.

### 8. Что остаётся открытым

До реализации ещё нужно отдельно решить:

- хранить ли metadata о “include найден внутри conditional block”;
- нужен ли отдельный `vendored/generated` classification layer;
- как именно индексировать suffix matches, чтобы не тащить лишнюю память;
- как оформлять suppression для хронически ambiguous includes.

## Следующие шаги

1. Сначала зафиксировать extraction semantics: как именно из repo получается file-level graph
2. Затем зафиксировать baseline contract и минимальный API графа
3. После этого перейти к реализации `DRIFT.1` / `DRIFT.2`

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Сначала graph primitives, потом rules | Иначе `DRIFT.*` и Lakos checks будут опираться на неустойчивую модель |
| Канонический граф — file-level | Это упрощает семантику и устраняет дублирование первичных моделей |
| Fast extraction core делаем сами | Zero-config v0.1 требует repo-local include resolver без compile database |
| Готовые dependency extractors считаем adapters, а не ядром | `depfiles` и `clang-scan-deps` полезны, но завязаны на build context |
| Нужен именно graph diff, а не только snapshot | AI-drift rules по смыслу сравнивают состояние “до/после” |
| baseline хранит snapshot, а не derived metrics | Это делает формат устойчивее и не привязывает его к текущему набору правил |
| Минимум зависимостей, без внешней graph-библиотеки | Совпадает с архитектурными ограничениями проекта |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `src/scan/include_scanner.*` | будущий fast extractor / include scanner |
| `src/scan/include_resolver.*` | будущий repo-local include resolver |
| src/graph/* | будущая реализация графа и алгоритмов |
| tests/unit/graph/* | будущие unit tests |
| tests/unit/scan/* | будущие unit tests для extraction / resolver |
| tests/integration/graph/* | будущие integration fixtures |
| docs/architecture-spec.md | уточнение `graph-baseline` и file-level contracts |

## Fixtures

- [ ] Минимальный DAG без циклов
- [ ] Граф с одним SCC
- [ ] Сценарий “появилось новое ребро”
- [ ] Сценарий “shortcut edge”
- [ ] Сценарий “cycle growth”
- [ ] Сценарий “unresolved include”
- [ ] Сценарий “ambiguous include”
