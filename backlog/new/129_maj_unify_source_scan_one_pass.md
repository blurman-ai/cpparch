# [SCAN] один проход по дереву + общий authored/vendored/generated view для всех правил

**Дата создания:** 2026-06-18
**Дата старта:** —
**Статус:** new
**Модуль:** SCAN
**Приоритет:** major
**Сложность:** unknown
**Блокирует:** консистентность вердиктов правил; применимость отчёта (#124)
**Заблокирован:** —
**Related:** #127 (точность vendored/generated-предиката — вставляется сюда), #068 (graph vendor exclude), #069 (vendored file exclude), #081 (over-exclusion), #113 (apache-banner dominant-guard — переиспользовать), #124 (корпус-прогон, вскрывший расхождение)

## Цель

Сканировать дерево исходников **один раз** на снапшот: прочитать+классифицировать
файлы (authored / vendored / generated / test) в **общий view**, который потребляют
все правила (clone, complexity, graph), вместо того чтобы каждое правило само
обходило дерево и само фильтровало по своей копии логики.

## Контекст — три разошедшихся реализации одной идеи

Каждая проверка вендор-фильтрует **в своём методе**, разным подмножеством общих
предикатов → **несогласованные вердикты на одном файле**:

| правило | метод | что использует |
|---|---|---|
| clone | `collectNonVendoredSources` (project_files.cpp) | path + basename + license-header (`isVendoredFile`) |
| graph | `filterVendored` (graph_builder.cpp) | path + basename + **license-header c dominant-banner guard** (#113) |
| complexity | `collectFilePairs` (local_complexity_drift.cpp) | path + basename **БЕЗ license-header** (намеренно отключён) |

**Доказательства (корпусный FP-аудит #124, `docs/research/agent_drift_within_repo.md` §5):**
- rmm `cpp/benchmarks/utilities/rapidcsv.h` (vendored single-header вне vendor-каталога):
  «вендор» для clone/graph, но «авторский» для complexity → ложный **CCN 64**.
- MS-Teams `objectmodel_wrap.cpp` (SWIG-генерёнка): generated вообще не исключается
  ни одним правилом → ложный clone.

**Дог-фуд-ирония (зафиксировать в питче):** `archcheck src/ include/` → `No violations`.
Собственный клон-детектор **не видит** эту дупликацию — она **концептуальная**
(три переписанных врозь реализации), а не token-clone. Честная граница token-detection:
важнейшую дупликацию (расходящиеся реимплементации концепта) ловит архитектурный
взгляд, не «найди одинаковые токены». Сильное признание для отчёта, не слабость.

## Почему разошлись (не чистая лень — но и не оправдание)

License-header сигнал требует **обзора всего дерева** (dominant-banner guard #113/#109
foundationdb: если >50% файлов с баннером — это лицензия самого проекта, слой выключить).
Этот guard есть **только в graph** (он сканит всё дерево). complexity **дифф-скоупный**
(парсит только изменённые файлы) → не считает ratio по дереву → **отрубил license-header
целиком** (тупо-безопасно, ценой rmm-FP). НО `FileSource` (memory `GitObjectFileSource` /
disk worktree) умеет `list()`/`read()` по **всему** дереву — данные доступны, complexity
их просто не читает. Значит ratio **вычислим**; нужен один общий проход, который его
считает один раз и отдаёт всем.

## Дизайн

- Общая scan-стадия: `AuthoredSourceView` (или расширить `project_files`), строится один
  раз из `FileSource`: `list()` → `read()` каждого файла **однажды** → классификация
  (предикат из #127: .gitmodules / path-токены / nested-LICENSE+код / copyright-mismatch /
  generated-маркеры + dominant-banner guard #113).
- Правила потребляют view: clone — по authored-файлам; graph — по authored; complexity
  парсит изменённые **authored** файлы, но берёт классификацию и whole-tree banner-ratio
  из общего view.
- Это и есть `scan → rules` из спеки; убирает и тройной IO, и тройной фильтр, и
  per-rule дрейф (одно место для generated-паттернов и порогов).
- `#127` поставляет ТОЧНОСТЬ предиката; `#129` — то, что предикат считается ОДИН раз и
  ШАРИТСЯ. Делать в связке (или #127 первым — предикат, затем #129 — единый проход).

## План выполнения

- [ ] Вынести единый `classifySource()` / `AuthoredSourceView` с богатейшей логикой (из `filterVendored`)
- [ ] Прокинуть whole-tree banner-ratio в diff-скоупный complexity
- [ ] Перевести clone / graph / complexity на общий view (убрать 3 отдельных фильтра)
- [ ] Добавить generated-паттерны в одном месте (`*_wrap.cpp`, `*.pb.{h,cc}`, `moc_`/`ui_`/`qrc_`, `*.tab.*`/`*.yy.*`, `@generated`/`DO NOT EDIT`)
- [ ] Фикстуры (ниже) + дог-фуд `archcheck src/ include/ tests/` = 0 нарушений
- [ ] Прозрачность: `excluded N files (vendored/generated, reason: ...)` (как в #127)

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Сначала закрыть/учесть #127 (предикат), затем единый проход здесь.
2. Снять регресс на rmm (CCN 64 уходит) и MS-Teams (SWIG-clone уходит).

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Один проход + общий view, не per-rule фильтр | три реализации уже разошлись по поведению (rmm/MS-Teams) — корень в раздельных обходах |
| Базироваться на логике `filterVendored` (graph) | у неё единственной есть dominant-banner guard (#113), остальные — урезанные |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| src/scan/project_files.{h,cpp} | единый `AuthoredSourceView` / `classifySource` |
| src/scan/new_clone_drift.cpp | потреблять общий view |
| src/scan/local_complexity_drift.cpp | потреблять общий view + whole-tree banner-ratio |
| src/graph/graph_builder.cpp | потреблять общий view (убрать `filterVendored`-дубль) |

## Fixtures (обязательны)

- [ ] `fixtures/source_classification/pass_consistent/` — один и тот же vendored-файл даёт ОДИНАКОВЫЙ вердикт всем правилам
- [ ] `fixtures/source_classification/fail_rmm_style/` — single-header либа вне vendor-каталога НЕ фалит complexity
- [ ] `fixtures/source_classification/fail_generated_swig/` — `*_wrap.cpp` НЕ фалит clone
- [ ] `fixtures/source_classification/pass_self_licensed/` — проект со своим баннером в каждом файле НЕ исключён целиком (anti-over-exclude, #113)

## Самопроверка

Риск — OVER-exclude (съесть свой код) и регресс поведения правил. Проверять
перечислением: один и тот же набор файлов → один и тот же вердикт у всех правил;
сверить вывод clone/complexity/graph до и после на 2-3 реальных репах (rmm, foundationdb,
MS-Teams). «Стало меньше нарушений» — повод проверить, не выкинули ли лишнего.

## Сошедшийся дизайн (design-workflow, 2026-06-18)

5 линз Discover + 3 дизайна + судья. Выбран **Design 3 — `scan::AuthoredScope` + один
source на ref**; полный TreeSnapshot (read/token-дедуп) отвергнут как раздувание скоупа
(вынесен в отдельный follow-up). Корень шире, чем думали: AND-цепочка «код проекта?»
открыто закодирована в **7 местах с 5 формулами** — `collectNonVendoredSources`
(unguarded banner), `filterVendored` (>50%-dominant-guarded banner), `collectFilePairs`
(banner OFF, #109), `god_file_growth.cpp:36` (комментарий обещает generated-exclude,
**код не делает**), `defect_attractor`/satd/test (path-only). Плюс per-diff дерево
читается 2× на сторону тремя отдельными `cat-file`-детьми; в check хедеры до 3× с диска.

**API (чистый value-type на (path,content), backend-agnostic — two-backend split цел):**
`AuthoredScope::fromFiles(files)` (считает dominant-banner ratio один раз) /
`changedFilesMode()` (banner=no-op, сохраняет #109-safe поведение complexity) /
`excluded(path,content)`; + `scan::isGeneratedPath()` (lift из `duplication_scanner.cpp:173`
+ SWIG `_wrap.{cpp,cxx,c}`). НЕ мигрируют: god_file_growth/defect_attractor/satd/test —
они классифицируют commit-пути без контента, остаются на path-трио (документированный gap).

**Twist #109:** complexity до шага 6 — `changedFilesMode()` (rmm ещё палит CCN 64,
foundationdb 0); шаг 6 даёт полное дерево → `fromFiles(fullList)` → rmm чинится через
общий ratio БЕЗ регресса foundationdb. Наивно «включить баннер» — запрещено.

**План (~120–160 LOC, каждый шаг сам собирается+тестируется, ревертится по 1 файлу):**
0. Golden baseline (без кода): dogfood=0 + clone/complexity/graph на rmm/SWIG/foundationdb/Apache → сохранить пер-репо.
1. `isGeneratedPath`+SWIG → repoint `phasePathBasedFpSuppress`, удалить приватную копию. Фикстура `fixtures/generated/swig_wrap/`.
2. `AuthoredScope` (pure addition, 0 call-site) + equivalence-тест: `excluded()` == каждая из 5 формул. `tests/scan/authored_scope_test.cpp` + фикстуры.
3. graph `filterVendored` → AuthoredScope (delta: graph теперь дропает generated).
4. clone `collectNonVendoredSources` → AuthoredScope (delta: clone получает dominant-banner guard → recall ↑).
5. complexity `collectFilePairs` → `changedFilesMode()` (rmm ещё НЕ чинится — by design; foundationdb остаётся 0).
6. `diff_command` — один baseline+current source на ref, передать в graph+complexity+clone (borrow by ref, git-orphan hygiene); complexity → `fromFiles(fullList)` → **rmm fixed**.
7. Финальный dogfood + корпус пер-репо vs golden + lizard.

**Решения человека (флаг судьи):** (1) шаг 6 в скоупе — ДА (это и есть запрос «централизованный запуск»). (2) **шаги 3–5 сдвигают опубликованные числа отчёта** (clone recall ↑, graph −generated) → перегнать корпус + обновить `docs/research/agent_drift_within_repo.md`. (3) SWIG только точные суффиксы. (4) generated-removal глобально (меняет cycle/god/chain на репах с генерёнкой). (5) history-advisories остаются path-only.

**Риск:** med. Главный — реинтродукция #109 (mitigated: banner no-op до шага 6); пер-репо golden (не агрегат — агрегат врёт); git-orphan hygiene в шаге 6. Out of scope (follow-up TreeSnapshot): comment/literal-стрипперы, brace-walkers, расходящиеся lex-варианты — там include_scanner нужен byte-offset, который token-stream теряет.
