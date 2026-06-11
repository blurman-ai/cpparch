# Большая сводка: аудит кода · гипотеза AI-дрейфа · внешние исследования · дешёвые гарды · clang-бэкенд

_2026-06-11. Метод: multi-agent прогон (5 аудиторов кода, инвентаризация правил, 2 рецензента
гипотезы, 5 веб-исследователей) + адверсариальная верификация ключевых утверждений по
первоисточникам. Утверждения из интернета помечены: ✅ — верификатор открыл первоисточник и
подтвердил; ◻ — источник найден исследователем, но верификация не прогонялась (лимиты сессии);
числа в ✅-утверждениях сверены с текстом источника._

---

## TL;DR — ответы на шесть вопросов

1. **Код в целом здоров** — слоистость scan→graph→rules→report без единого обратного ребра,
   two-backend split не протёк, собственный код dogfood-чист. Но: **2 high-находки по
   безопасности** (abort на кривом YAML вместо exit 2; stack overflow на глубокой include-цепочке
   в default-правиле), **собственный lizard-гейт сейчас красный** (5 функций в src/ за порогами),
   ~95 строк мёртвой LCS-механики в shipped-библиотеке, placeholder `evaluateAgainstCorpus`
   с вечным precision=1.0, и систематическая fork/exec-копипаста в git-подсистеме (~50 строк).
2. **Вывод «гипотеза не подтверждается» — неправомерен.** Правомерный вывод: «дизайн не способен
   различить эффект меньше ~3.5× на пороговых метриках при горизонте <12 мес и контаминированном
   контроле». Литература 2024–2026 говорит ровно то же: глобальные метрики (дупликация,
   графовые) молчат и в чужих работах, стойкий сигнал живёт в локальной сложности и
   микро-паттернах — там, где наш boolean-drift и дал 1.6–2.3×. **Прямых работ про
   boolean/config-bag drift не нашлось — это, по-видимому, оригинальный результат.**
3. **Сейчас проверяется:** 5 default-правил (SF.7/8/9 + GodHeader 50 + ChainLength 10),
   3 drift-правила (DRIFT.1/2 — gate, DRIFT.3 — advisory), diff-режим (6 типов регрессий),
   duplication advisory. Заявленного SF.21, чтения compile_commands.json, SARIF и exit 3 — нет.
4. **Боль C++ подтверждена числами:** №1 — управление зависимостями (~82% респондентов ISO
   2024 называют pain), №2 — build times (~80%); боль хроническая (−2–3 п.п. за 3 года).
   Физика: хедеры = 99.68% компилируемых строк Chromium. Chrome и Figma строили **самописные**
   include-граф чекеры в CI (Figma: блокирует 50–100 регрессий в день) — готового CLI-продукта
   на рынке нет; C++20 modules при 4.1% адопции библиотек нишу не закроют.
5. **Дешёвых гардов — два корпуса идей:** ~15 на include-графе (private-headers, entry-point,
   транзитивные контракты, SDP, include-weight budget…) и ~15 без include-графа (клоны в новом
   коде, change coupling, flag-argument, config-bag…), 8 из которых уже превращены в задачи
   backlog/new/093–100. Сквозное правило: блокирует только «дельта против baseline на новом
   коде», всё вероятностное — advisory.
6. **Clang-бэкенд:** единственное, что принципиально недоступно include-графу — символьный
   уровень (какие символы откуда используются). Топ-кандидаты: symbol-level layering, phantom
   edges, SF.2, метрика абстрактности A для Martin D. Цена — 2–3 порядка дороже include-скана
   (LLVM: directive-scan ~3 c vs полный парс ~15 мин) → строго opt-in, по compile_commands.json,
   инкрементально.

---

## 1. Аудит кодовой базы (~14 тыс. строк src/include/tests)

### 1.1 Что хорошо (подтверждено, не комплимент)

- **Слоистость соблюдена безупречно**: полная карта include-зависимостей src/ — ни одного
  ребра против направления scan→graph→rules→report, ни одного цикла между модулями.
- **Two-backend split чист**: libclang в дереве отсутствует, в include-only путь ничего не протекло.
- **Dogfooding**: бинарь собирается, прогон на самом репо — собственный код (src/include/tests)
  даёт **0 нарушений**; все 1067 находок — gitignored sandbox/ (1057) и нарочные fixtures/ (10).
- **git вызывается через fork+execvp без шелла** — классической shell-инъекции нет.
- **README почти полностью честен**: все флаги существуют, пример вывода воспроизводится
  побайтово, пороги и exit-коды совпадают.
- Заголовков-сирот нет, все CLI-флаги диспетчеризуются, закомментированного кода нет,
  premature-абстракций не найдено (IRule — 8 реализаций, FileSource — 2).

### 1.2 Безопасность (модель угроз: CI на недоверенном репо)

| # | Severity | Находка | Где |
|---|---|---|---|
| S1 | **high** | Кривой `.archcheck.yml` → ryml дефолтным обработчиком зовёт **abort** → SIGABRT (134) вместо контрактного exit 2. В `baseline.cpp` правильный паттерн (set_callbacks→exception→restore) уже есть, в `config_loader.cpp` — не применён | `src/config/config_loader.cpp:341-358` |
| S2 | **high** | **Stack overflow на глубокой include-цепочке**: `computeSccDepths` — рекурсивная лямбда, глубина = длине цепочки; правило Lakos.ChainLength в default-наборе → злонамеренный репо с цепочкой из десятков тысяч заголовков роняет обычный запуск | `src/graph/algorithms.cpp:213-230` |
| S3 | medium | Обход дерева **следует по симлинкам-файлам наружу корня** (`is_regular_file` разыменовывает): `evil.h → /etc/passwd` читается и частично утекает в отчёт | `src/scan/project_files.cpp:39-67` |
| S4 | medium | Чтения файлов и git-блобов **без лимита размера** + **нет top-level try/catch в main** → OOM / bad_alloc → std::terminate (134) вместо exit 3 | `src/git/git_object_file_source.cpp:227-268`, `main.cpp:614-619` |
| S5 | medium | `jsonEscape` экранирует только `"` `\` `\n` — управляющие символы U+0000..001F из имён файлов/кода дают **невалидный JSON** (breakout-инъекции нет, но downstream-потребители валятся) | `include/archcheck/report/json_escape.h:11-27` |
| S6 | low | git исполняется в недоверенном репо **без харднинга**: локальный `.git/config` (`core.fsmonitor`, `diff.external`, `core.pager`) и хуки (`post-checkout` при worktree add) исполняют произвольные команды | `src/git/git_state.cpp:310` |

Рекомендации (по находкам): S1 — переиспользовать паттерн из baseline.cpp; S2 — итеративный
DFS (конденсация — DAG, топологический проход с мемоизацией); S3 — `symlink_status` +
проверка `weakly_canonical` внутри root; S4 — лимит N МБ + top-level catch → exit 3;
S5 — `\uXXXX` для всего <0x20 + U+FFFD для невалидного UTF-8; S6 — `GIT_CONFIG_NOSYSTEM=1`,
`-c core.hooksPath=/dev/null -c core.fsmonitor= -c diff.external= -c core.pager=cat`, `--`
перед ref-аргументами.

Path-traversal через резолв include и shell-инъекций **не обнаружено**.

### 1.3 Мёртвый код (каждый кандидат проверен grep'ом)

Главное:

- **~95 строк полного DP-LCS мертвы в shipped-библиотеке**: `diffTokens`/`DiffOp` +
  `buildLcsTable`/`backtrackLcs`/`collapseDelInsPairs` вызываются только тестами; продакшен
  `cloneType` их не зовёт ([clone_classifier.cpp:10-106](../../src/scan/duplication/clone_classifier.cpp#L10-L106)).
- `reverseReachableFrom` и `hasPath` — публичный API графа, живущий только в тестах
  ([algorithms.h:15-16](../../include/archcheck/graph/algorithms.h#L15-L16)).
- **`evaluateAgainstCorpus` — заглушка, всегда precision=1.0** («assume all reported pairs are
  TP»), ground truth игнорируется — создаёт ложное ощущение precision-контроля
  ([fp_corpus_eval.cpp:44-74](../../src/scan/duplication/fp_corpus_eval.cpp#L44-L74)).
- Write-only/никогда не читаемые: `MetricThresholds::chainLengthLimit`, `Pair::sharedRare`,
  `BaselineLoadError::line` (всегда 0), `Config::version`.
- Мёртвые аксессоры: `ConfigError::file()/line()/column()`, `Worktree::valid()`,
  `DiskFileSource::root()`.
- `tests/duplication_vmecpp_test.cpp` и `duplication_all_projects_test.cpp` **никогда не
  компилируются** (не включены в CMakeLists).
- Осознанные задела (не мусор, но зафиксировано): `Config::modules/rules` парсятся без
  enforcement; `extraExcludes` у GodHeaders недостижим из CLI/конфига.

### 1.4 Качество / соответствие собственным стандартам

- **Собственный lizard-гейт красный**: 5 функций в src/ (3 в duplication_scanner.cpp,
  main.cpp:117, +1) и 13 TEST_CASE за порогами CCN15/len30 → exit 1. Памятка проекта требует
  гнать lizard перед пушем — прямо сейчас он бы не прошёл.
- **Систематическая копипаста против собственного порога «>5 строк»**: ~50 строк fork/exec
  между `git_state.cpp` и `git_object_file_source.cpp`; **три копии `toLower`** при заявленном
  single source of truth; дубль JSON-сериализации violations + хрупкий ручной substring-парсер
  baseline; дубль vendor/test-фильтра между graph_builder и project_files; `plainJaccard` —
  25-строчная копия `weightedJaccard`.
- **Нейминг расколот**: 8 файлов на snake_case против требуемого lowerCamelCase, а центральный
  интерфейс **`IRule` нарушает собственный запрет I-префикса** из code_style.md («используйте
  просто Rule»).
- `duplication_scanner.cpp` — самый проблемный файл: опечатки в именах (`Loclal`, `filer`),
  **две функции «phase8»**, мёртвые ветки, россыпь неименованных магических порогов,
  комментарии-пересказы (forbidden pattern).
- Классов >300 строк нет; clang-format чист.

### 1.5 Соответствие собственной архитектуре

- **«Static table» регистрации — на деле императивная фабрика**: добавление правила требует
  правки `rule_set.cpp` (OCP из спеки в строгом смысле нарушен; чужие rule-файлы трогать
  не надо — это смягчает).
- **NCCD считается не по Лакосу**: `ACD/log2(N+1)` вместо канонического отношения к ACD
  сбалансированного бинарного дерева — калиброванные пороги Лакоса к нашему NCCD неприменимы
  ([algorithms.cpp:275](../../src/graph/algorithms.cpp#L275)).
- **Коллизия ID DRIFT.3**: в спеке зарезервирован за public_surface_growth, в коде —
  bidirectional coupling. Одно из двух надо переименовать, пока не вышло наружу.
- CCD/ACD/NCCD считаются, но **не выводятся** ни в check, ни в `--graph` — только NCCD-дельта
  в `--diff` (CLAUDE.md обещает «in report»).
- **Dogfood-гейта в CI нет** (только smoke `--version/--help` + diff-режим в PR-workflow),
  хотя CLAUDE.md называет его обязательным.

### 1.6 Документы vs реальность (главное)

- **CLAUDE.md содержит ложные утверждения**: чтение compile_commands.json (в коде 0 вхождений),
  `file:line:column` (колонок нет), обязательный dogfooding в CI (нет), SF.21 в MVP-наборе
  (нет ни кода, ни фикстур), «CCD/ACD/NCCD in report» (не выводятся).
- **MVP.md acceptance criteria не выполнены** (dependency rules не enforced, compile_commands
  не читается), при этом README объявляет «MVP functionally complete» — противоречие.
- CHANGELOG: не дописаны #088/#091/vendored-классификация; ссылка на несуществующий репозиторий.
- Backlog: TASK_TRACKER устарел (P0-блокеры стоят 8 дней при работе в research), #079 завершена
  но в wip/, дубликаты ID #071/#072, зомби-файл `dup_band_70_80.md` в корне.
- architecture-spec.md разошлась по флагам (`--graph-baseline` vs `--drift-baseline`),
  структуре каталогов и roadmap (DRIFT.1/2 отгружены в v0.1, спека говорит v0.3).

---

## 2. Что archcheck проверяет сейчас (инвентаризация)

Полная версия — в выводе агента-инвентаризатора; здесь сжатая таблица.

| Правило/режим | Что ловит | Источник | Порог | Гейт? | Фикстуры |
|---|---|---|---|---|---|
| SF.7 | `using namespace` в глобальной области заголовка | Core Guidelines | — | да | ✅ |
| SF.8 | нет include guard (первые 60 непустых строк; искл.: .inc, ObjC) | Core Guidelines | — | да | ✅ |
| SF.9 | циклы include-графа (Tarjan, SCC≥2; искл.: inline-split pair #088, all-conditional) | CG/Lakos | — | да | ✅ |
| Lakos.GodHeader | fan-in > порога (искл. PCH-имена) | Lakos | 50 | да | ✅ |
| Lakos.ChainLength | глубина include-цепочки > порога | Lakos | 10 | да | ✅ |
| DRIFT.1 | новое ребро между двумя файлами, существовавшими в baseline («shortcut») | собств. корпус | — | **gate** | ✅ + real-world |
| DRIFT.2 | новый/выросший SCC против baseline | Lakos | — | **gate** | ✅ |
| DRIFT.3 | новая взаимная area-связность A↔B через разные файлы (без file-cycle) | собств. корпус | — | advisory | ✅ |
| `--diff` | added/removed edges, grown cycles, new cross-area, chain growth, new god-headers (порог тут **30**, не 50!), NCCD-дельта | — | — | exit 1 при регрессии | интеграционные |
| `--duplication` | токеновый клон-детектор (нормализация, фрагментация, rare-token + winnowing #092, 10 precision-фильтров, классификатор EXACT/RENAMED/LITERAL/MIXED/STRUCTURAL) | NiCad-класс | weighted ≥0.6 | **нет, всегда exit 0** | ❌ нет fixtures |
| `--baseline` | подавление по триплету (rule, file, line) | — | — | — | — |

**Заявлено, но в коде НЕТ**: SF.21; SARIF; clang_scanner; compile_commands.json;
module-правила layers/independence/forbidden (парсятся, не enforced — help честно
предупреждает); exit 3 (`return 3` отсутствует, catch-all в main нет); Martin Ce/Ca/I/A/D;
группировка `rules/core_guidelines|lakos|martin/`.

Несостыковка порогов: god-header fan-in **50** в правиле, но **30** в diff-режиме
(`regression_report.h:52`) — два разных дефолта на одну сущность.

---

## 3. Гипотеза «ИИ увеличивает архитектурный дрейф»

### 3.1 Что сделано и что показало (реконструкция по docs/research + experiments)

Эволюция: (1) детекторная фаза — DRIFT.1/2 на 33 AI-PR; (2) популяционная — агентские vs
контроль по циклам/копипасту; (3) сужение #089 — boolean-state; (4) кросс-язык velocity.

**Сработало:**
- **DRIFT.1**: 7 из 33 AI-PR (21%), 12 находок, 4 архетипа (UI→widgets, generic→features,
  system→component, ui-config→core-data); 0 шума на 26 чистых PR, включая +9762 LOC. LibreSprite
  #581 верифицирован тройной git-проверкой → hermetic-фикстура.
- **DRIFT.2 как gate**: 72 из 135 092 коммитов корпуса (0.05%) — редкий, объективный.
- **Boolean per-struct drift (единственный положительный популяционный сигнал)**: AGENTIC
  9.8% структур vs NON-AGENTIC 6.0% → **1.6×**; на репо 36% vs 17% → **2.1×**; конфаунд
  активности снят (медианы коммитов 1013 vs 940 = 1.08×); после eye-check обеих групп
  (FP-доля у non-agentic выше) коррекция **усиливает** ratio до **~2.3×** (5.6% vs 2.4%).
- Velocity: event-study 61 vs 31 — коммиты 1.61× vs 1.00×, строк/коммит 190→306 при плоском
  контроле.

**Не сработало:** циклы (3% vs 6% реп — 2 репы против 2), authored-копипаст (M-W p=0.144),
AI%↔долг (ρ≈±0.06), статический bool-счётчик (78% конфиг-мешки), нейминг-правило (0/1, revert),
file-level history drift (45% FP), velocity-ramp как прокси агентности (топ-ramp репы AI%=0),
сырая cross-area связность (~50% false-alarm).

### 3.2 Почему «не подтверждается» — неправильный вывод (методологическая рецензия)

Полный разбор — отдельный материал рецензента; ядро:

1. **Контроль заражён по построению**: невидимый ИИ (автокомплиты, снятые трейлеры) →
   non-differential misclassification → смещение к нулю. При правдоподобных 20% ложных
   агентских + 30% скрытого ИИ в контроле истинный ratio 2.0 наблюдается как **1.38** —
   ниже порога различимости дизайна. Хуже: в boolean-сравнении «non-agentic» группа взята из
   корпуса, прошедшего агентский гейт (msglen≥100 — один из OR-критериев) — что сигнал выжил
   на таком разбавленном контрасте, скорее аргумент **за** гипотезу.
2. **Метрики уровня (stock) вместо скорости (flow)**: репа может утроить связность, не
   пересекши ни один порог. Собственный кейс mako: cross-module рёбра ×3.5 за год при
   «циклах без тренда» — сигнал был в производной.
3. **Floor effect + мощность**: циклы p≈0.06, n=61 vs 31 → MDE ≈ ratio 3.5×; копипаст →
   MDE ≈ 29 п.п. «38% vs 32%» неинформативно — дизайн не отличил бы 32% от 55%. Ни одного
   CI/p-value/power-расчёта в доках нет; boolean per-repo (36% vs 17%, z≈3.2) — номинально
   значим, но без кластер-поправки и со слепотой разметки = 0.
4. **Survivorship по построению**: гейт «>300 коммитов с мая 2025» отбирает выживших; если
   дрейф убивает проекты — жертвы исключены критерием включения; born-agentic (максимальная
   доза) выброшены.
5. **Time-to-effect**: эрозия — процесс лет (Lehman; Eick et al.); адопция почти у всех
   H2 2025+, окно «после» 90 дней. Даже человеческий зрелый код (newton-dynamics) держит
   инварианты 1.5 года плоско. Для пороговых метрик горизонт 2–4 года; для производных —
   6–12 мес (уже доступно).
6. **Прецедент «правдоподобного нуля»**: grown_cycles был нулём корпус-wide из-за трёх багов
   пайплайна — каждый новый null-вывод требует positive control (инъекция известного дрейфа).

**12 альтернативных дизайнов** (рецензент, по убыванию ценность/стоимость):
1. ⭐ **Commit-level attribution внутри смешанных реп** — AI- vs human-коммиты **той же репы**
   (repo fixed effects убивают все конфаундеры разом; 135k per-commit записей уже собраны,
   нужен join с атрибуцией; ~1 неделя).
2. PR-level case-control: те же 10 реп, сматченные human-PR (helper готов; 150–200 PR на плечо).
3. Within-repo interrupted time series по **скорости роста** связности (излом наклона в дате
   адопции, mixed-effects).
4. Staggered diff-in-diff (Callaway–Sant'Anna) по датам адопции.
5. Survival analysis нарушений («AI-нарушения чинятся реже/позже»).
6. Семейство микро-аккреционных метрик по образцу #089 (параметры функций, ветки, размер
   заголовка, config-ключи; 3–5 дней на метрику).
7. Abstraction-bypass rate — обобщение DRIFT.1 без порога (доля новых include в impl-заголовки
   чужого модуля vs фасад; работает на молодых репах).
8. Per-commit duplication introduction rate (vendored-FP снимается автоматически).
9. Churn/rework half-life (GitClear-style, чистый git).
10. Matched-pair корпус (возраст/LOC/домен/контрибуторы; CEM/propensity).
11. Полноисторический реклон ~100 реп (снять shallow-цензуру: 54% boolean-кандидатов обрезаны).
12. Negative-control outcomes + **слепая разметка** (без неё «2.3×» не выдержит рецензию).

### 3.3 Что говорит литература (адверсариально проверенные источники)

**Самое близкое к нашему дизайну** — CMU, MSR 2026 (arXiv 2511.04427), ✅ проверено по полному
тексту: diff-in-differences, **806 реп**, принявших Cursor (прокси адопции — первый коммит
`.cursorrules`), против **2418 контролей** (1:3 nearest-neighbor propensity matching, тот же
язык, AUC 0.83–0.91), помесячно 01.2024–08.2025, ≥6 мес до/после, panel GMM. Результаты (ATT,
log): **velocity транзиентна** (+281% строк в месяц 1, исчезает к 3-му), **cognitive complexity
(SonarQube, project-level) +41.6% ±7.6 — стойко**, static-warnings **+30.3% ±6.7 — стойко**,
**duplicated lines density +7.0% ±4.8 — НЕзначимо**. Языки: TS 366 / Python 118 / JS 60 / Go 36 /
Rust 24 — **C++ практически нет**. Их threats to validity — дословно наши: видимая адопция,
неизвестная интенсивность, контаминация контроля другими AI-инструментами.

Остальное по слоям:

- **Строчный уровень (дупликация/churn)**: GitClear 2025 (211M строк): copy/paste 8.3%→12.3%
  изменённых строк (2021→2024), дублированные блоки ≥5 строк ×8, refactored/moved 25%→<10%,
  copy/paste впервые > moved ◻; churn-удвоение к 2024 ✅. Академическое подтверждение клоновой
  линии: FSE 2025 — Type-1/2 клоны у коммерческих генераторов до 7.5% ◻.
- **Поле AI-кода**: ~19.8k AI-файлов — у AI **меньше** кросс-файловой дупликации (17.2% vs
  24.5%), но «deeper, not broader» call-графы и более глубокая вложенность ◻ (arXiv 2603.27130).
  AI-файлы после мержа реже обслуживаются — деградация копится молча ◻ (arXiv 2605.06464).
- **Архитектурный уровень — только synthetic**: LoC↔arch-smells ρ=0.94, специфичность промпта
  не влияет (p>0.8), «Modular Mirage» — формальная модульность без семантической связности ◻
  (arXiv 2605.02741); SlopCodeBench: structural erosion в 77% траекторий агентов ◻
  (arXiv 2603.24755); LLM систематически нарушают модульность — лезут в private-члены ◻
  (Harvard, LMPL 2025); constraint decay −30 п.п. при L0→L3, Clean Architecture −9.1 п.п. ✅
  (Dente et al., arXiv 2605.06445 — наш constraint_decay.md).
- **Макро**: DORA 2024 — на +25% AI-adoption: throughput −1.5%, stability −7.2% ✅; DORA 2025 —
  throughput развернулся в плюс, **негатив по stability сохранился**, фрейминг «AI —
  амплификатор» ✅.
- **Контр-сторона (честно)**: GitHub RCT — +13.6% строк без readability-ошибок ◻; Google RCT —
  −21% времени ◻; ANZ ~1000 инженеров — +42% скорость без значимого ухудшения качества ◻;
  METR RCT — опытные OSS-мейнтейнеры с AI на **19% медленнее** при субъективных −20% ◻;
  Uplevel — bug rate +41% без роста throughput ◻; Stanford ~100k devs — ~50% валового прироста
  съедает rework, в brownfield чистый эффект 5–10% ◻.
- Смежное: опрос практиков — мелкие AI-сниппеты НЕ эродируют cohesion/coupling, эрозия
  начинается на крупных мульти-файловых генерациях ◻ (arXiv 2506.17833).

### 3.4 Синтез: что это значит для нас

1. **Наш «null» на глобальных метриках — не аномалия, а воспроизведение мирового результата.**
   Ни одна работа не показала сигнал на уровне глобальных графовых метрик реальных реп.
   CMU прямо получил «duplication n.s.» при стойком росте сложности — наша пара
   «копипаст p=0.144 / boolean-drift 2.3×» структурно та же картина.
2. **Сигнал живёт в локальной концентрации**: cognitive complexity (CMU), вложенность и глубина
   (полевое 2603.27130), private-access (Harvard), boolean/config-bag (мы). Это обосновывает
   уже начатый эксперимент `local_complexity_drift` (6/6 TP на ручной разметке) и задачу #099
   (indentation-complexity-drift — прокси Hindle ICPC 2008: дисперсия отступов коррелирует
   с McCabe/Halstead ◻).
3. **C++ — лакуна литературы**: CMU-корпус почти без C++, полевые работы тоже. Наш C++-корпус —
   сам по себе вклад; «глобальные метрики плоские на C++» может быть language-specific
   находкой (старше проекты, жёстче ревью). Boolean/config-bag drift — кандидат на публикацию
   (MSR/TechDebt), при условии слепой разметки и кластер-поправки (§3.2 п.12).
4. **Позиционирование продукта дословно из DORA 2025**: условие безопасной AI-скорости —
   «robust control systems»; archcheck — ровно такой control system для архитектуры.
   Метрики транзиентности velocity (+281% месяц 1 → 0 к месяцу 3) — готовый маркетинговый
   аргумент «скорость уходит, сложность остаётся».

### 3.5 Рекомендованный минимальный пакет следующих шагов

1. Commit-level attribution на готовых 135k записях (дизайн №1) — самый дешёвый шанс получить
   каузально-чистый результат.
2. Слепая пере-разметка boolean-выборок вторым проходом + кластер-робастный тест — сделать
   «2.3×» публикуемым.
3. Local complexity drift (уже идёт) + indentation-вариант как масс-метрика без парсера.
4. Повтор event-study через 6 мес на замороженной выборке (скрипты есть).

---

## 4. Боль C++-разработчиков

### 4.1 Опросы (все числа сверены с первоисточниками)

- **ISO C++ Developer Survey 2024** (Q6 о фрустрациях, 1261 ответивших) ✅: боль №1 —
  «Managing libraries my application depends on»: **45.4% major + 36.4% minor pain (~82%)**;
  №2 — **build times: 42.9% + 37.4% (~80%)**; CI с нуля — 30.4% major; CMake — 30.4%.
  Классический memory-safety заметно ниже (~20% major).
- **Тренд 2021→2024** ✅: зависимости −3.0 п.п., build times −2.4 п.п. — боль ослабевает
  медленно, остаётся топ-1/топ-2. **Хроническая.**
- **ISO 2025 (1036) и 2026 (1434 респондента, +38%)** ✅: свободная форма «волшебной палочки» —
  №1 стандартный пакетный менеджер/метаданные сборки, №2 **«make modules/header replacement
  actually work»** (headers/includes/macros названы painful дословно), в топе же — сократить
  время сборки.
- **JetBrains C++ Ecosystem 2023** (2627 C++-разработчиков) ✅: библиотеки — главная боль;
  **30% вообще не используют анализ кода**, анализ в CI — меньшинство.
- Смежно про ИИ (ISO 2025, 937 ответивших) ◻: 75.1% сталкивались с некорректным ИИ-выводом,
  70.8% не доверяют — поддерживает позиционирование «control system для AI-кода»; ISO 2026:
  ИИ чаще всего используют для понимания незнакомого/легаси кода, агентам не хватает
  dependency/context maps.

### 4.2 Физика проблемы и прецеденты ниши

- **Хедеры = 99.68% компилируемых строк Chromium** ✅: 30 137 шагов компиляции обрабатывали
  3 611.6 млн строк зависимостей при 11.86 млн строк исходников; полный билд 21.45 CPU-часов;
  рост O(N²).
- Include-граф Chromium ◻: 141 248 узлов, 1.31 млн рёбер, **7.8 млн простых циклов, 99%
  которых сидят в одной SCC из 92 файлов** — аргумент репортить SCC, а не циклы.
- **Chrome include-дашборд** ◻: per-edge анализ стоимости (`windows.h` = 287 GB ввода);
  за недели снято 5.13% ввода компилятора и 6.1% CPU.
- **Figma** ◻: сборка +50% за 12 мес при +10% кода (транзитивные include); самописный
  include-граф чекер в CI **блокирует 50–100 регрессий в день**, билд ускорен вдвое.
  Это дословно «archcheck как CI-gate», только самописный — готового продукта не было.
- Академия ◻: cyclic dependency — среди трёх частейших симптомов эрозии в код-ревью
  (arXiv 2201.01184); hub-циклы растут и дорожают (2306.10599); замена косвенных include
  прямыми даёт до 9% времени компиляции (ACM 2022).

### 4.3 C++20 modules нишу не закроют (горизонт archcheck)

Адопция ничтожна: **106 из 2587 популярных библиотек (4.1%)** дают модули (срез 11.2025,
arewemodulesyet.org ◻); Ropert (04.2026) не рекомендует большинству проектов ◻; конвертация
deal.II (800k LOC) — у downstream «no clear trend» по сборке ◻ (arXiv 2506.21654); Szalay 2025:
модуляризация легаси невозможна без предварительной чистки внутренней архитектуры ◻ →
**позиционирование: «archcheck — шаг 0 перед миграцией на modules»** (modules-readiness режим,
§4.4 п.7).

### 4.4 Идеи, прямо закрывающие найденные боли

1. **include-weight budget + ratchet** («не хуже чем вчера» по транзитивному весу) — прецеденты
   Figma/Chrome; закрывает боль №2 напрямую, без компиляции.
2. **Отчёт «top include offenders»** (cost = fan-in × транзитивный размер) — ответ на жалобу
   из HN-треда про IWYU («жалуется на всё, прячет 2–3 корневых виновников»).
3. **SCC-репорт с минимальным разрезом** вместо перечисления циклов (Chromium: 7.8M циклов =
   1 SCC × 92 файла; arXiv 2306.10599 — подсказывать паттерн распутывания).
4. **Ratchet по CCD/ACD/весу** как режим diff/drift (предотвращение регрессий ценнее разовой
   чистки — урок Figma).
5. **zero-code TU**: TU, у которых ~100% препроцессорного ввода — чужие хедеры (Chromium: 680
   сгенерённых файлов жгли 20 CPU-минут впустую).
6. **forward-declarability hint**: эвристика на include-скане + токенах; точная версия — в
   clang-бэкенд (§6).
7. **modules-readiness**: кластеризация include-графа в кандидаты-модули + отчёт блокеров
   (циклы между кластерами, макро-зависимости).
8. **Vendored из коробки** (уже есть): 56.5–60.6% собирают зависимости в своём билде (ISO
   2025/2026) — фильтр third_party обязателен против «5000 violations on first run».
9. **Маркетинговые числа в README**: «зависимости и build times — major pain для 45% и 43%
   (ISO 2024); хедеры — 99.7% компилируемых строк Chromium; archcheck находит причину за
   секунды без компиляции».
10. **JSON-карта зависимостей для ИИ-агентов** (граф компонентов + уровни + метрики) — прямой
    запрос из ISO 2026; расширяет аудиторию за пределы CI.

---

## 5. Дешёвые архитектурные гарды (без компиляции)

Контекст рынка: в каждой крупной экосистеме, кроме C++, есть дешёвый OSS-инструмент
архитектурных правил поверх графа импортов (ArchUnit, import-linter, dependency-cruiser,
eslint-plugin-boundaries, Tach, deptrac). В C++ это закрыто только коммерческими инструментами
с полным парсингом (CppDepend ~120 CQLinq-правил, Sonargraph, Lattix, Structure101).
**Рыночная дыра подтверждается** и опросом 87 практиков (arXiv 2108.01018 ◻): связь
архитектуры с кодом считают важной, выделенных инструментов не используют. Прецедент
производительности: cpp-dependencies (TomTom) строит include-граф 1.5 ГБ кода за ~2.1 с ◻.

Наука: reflexion models (Murphy–Notkin, FSE 1995) — convergence/divergence/**absence**;
Arcan (Fontana et al.) — 4 архитектурных смелла детектируются на одном графе зависимостей
(Cyclic, Hub-Like, Unstable Dependency, God Component); Pruijt et al. (SPE 2017) — 10 зрелых
ACC-инструментов массово теряют **непрямые** зависимости → честная транзитивность = конкурентное
преимущество.

### 5.1 На include-графе (уже есть вся инфраструктура)

Ранжировано по соотношению ценность/стоимость; авторитет в скобках:

1. **Транзитивные layers/forbidden/independence** с печатью полной цепочки (import-linter,
   ArchUnit; закрывает уже распарсенные, но не enforced module-правила конфига!).
2. **no-private / protected headers**: `detail/`, `internal/`, `*_impl.h`, `*-inl.h` нельзя
   включать извне модуля (boundaries no-private, Lakos-инсуляция). Сложность: низкая.
3. **entry-point**: чужой модуль — только через его публичные заголовки (include/ или umbrella);
   включение чужого src/-заголовка — нарушение (boundaries entry-point, Tach, Bazel visibility).
4. **external/vendor isolation**: сторонние заголовки (`<boost/…>`, `<windows.h>`, `<QtCore/…>`)
   — только из объявленных модулей-адаптеров (deptrac, go-arch-lint vendors).
5. **orphan headers**: fan-in 0, не парный, не entry-point → кандидат на удаление
   (dependency-cruiser no-orphans). Тривиально — fan-in уже есть.
6. **no-upward-include**: запрет `#include "../…"` вверх за корень модуля (ArchUnit
   no-classes-depend-upper). Тривиально.
7. **SDP-правило**: I = Ce/(Ce+Ca) per component; ребро в сторону более нестабильного —
   warning (Martin SDP; прецеденты дешёвой реализации — dependency-cruiser `moreUnstable`,
   Arcan Unstable Dependency, peer-reviewed).
8. **hub-like dependency**: fan-in И fan-out одновременно выше порогов — расширение god-header
   (Arcan, peer-reviewed). Тривиально.
9. **include-weight budget** («Header Hero в CI»): cost = lines × транзитивные включения;
   diff-режим «PR утяжелил compile cost на N%» (мотивация: Chromium 0.32% первичных строк).
10. **self-include-first** — дешёвый прокси SF.5: X.cpp включает X.h первым substantive-инклудом
    (BDE дословно, Google cpplint build/include_order).
11. **include-guard convention** + детект коллизий гардов (cpplint, bde_verify).
12. **no-include-of-impl**: запрет `#include "x.cpp"` (clang-tidy bugprone-suspicious-include —
    у нас бесплатно).
13. **SF.12-форма**: `""` для своих, `<>` для внешних; флаг на расхождение резолва с формой
    (Core Guidelines SF.12 — никем полноценно не покрыт).
14. **reflexion-absence**: объявленное в конфиге разрешённое ребро, отсутствующее в коде →
    «unused declared dependency» (Murphy–Notkin absence, deptrac debug:unused).
15. **Visibility-метрики Довалила** RV/ARV/GRV per module в отчёт (ArchUnit Visibility Metrics).

### 5.2 Без include-графа: токены / git-история / поля структур

Доказательная база этого слоя местами сильнее классической статики:
- **Клоны**: Juergens et al., ICSE 2009 ✅ (открыт PDF постранично) — 52% клон-групп меняются
  неконсистентно, ~каждая 2–3-я непреднамеренная неконсистентная правка = дефект (107
  подтверждённых багов); Wagner 2016 — 17% type-3 клонов содержат дефекты ◻. Но Kapser &
  Godfrey ◻: клонирование часто осознанно → гейтить **только новый код** (Sonar way: ≤3%
  дублей on new code ✅-консенсус доков).
- **Git-process-метрики бьют code-метрики**: Rahman & Devanbu ICSE 2013 ◻; относительный churn
  → 89% точность fault-prone (Nagappan & Ball 2005 ◻); FixCache 73–95% (Kim ICSE 2007 ◻);
  20% файлов = 83% дефектов (Ostrand & Weyuker TSE 2005 ◻); change coupling коррелирует с
  дефектами сильнее сложности (D'Ambros WCRE 2009 ◻); minor-контрибуторы → больше сбоев
  (Bird FSE 2011 ◻). Вся линейка Tornhill реализуема CLI поверх `git log --numstat`
  (code-maat — proof of concept ✅); бизнес-валидация: Code Red (TechDebt 2022) — нездоровый
  код = 15× дефектов, +124% времени ✅-линия; нелинейность отдачи (Borg 2024 ◻).
- **Микро-структурные**: Core Guidelines I.4 (flags-enum вместо набора bool, антипример
  `set_settings(true, false, 42)`) и I.23 (<4 параметров) ✅; Fowler FlagArgument ◻; config-bag —
  Xu et al. FSE 2015: Hadoop 17→173 knobs за 7.5 лет, большинство не трогают ◻. **Прямых работ
  про field/state accretion как темпоральный smell нет — boolean-drift archcheck тут первый.**
- **Прививка от карго-культа**: Lenarduzzi 2019 ◻ — «чистые» и «грязные» по SonarQube классы
  не различаются по fault-proneness → блокировать только узкое и высокоточное.

Идеи → **задачи уже созданы** (backlog/new/093–100): flag-argument (I.4), param-count+accretion
(I.23), config-bag-growth (Xu 2015), satd-delta (Potdar/Shihab; Wehaibi: SATD ↔ изменяемость,
не дефекты → advisory), test-co-evolution (Zaidman), god-file-growth, indentation-complexity-drift
(Hindle 2008), defect-attractor (FixCache/SZZ). Сверх 093–100 остаются нереализованными идеями:

- **new-clone-gate** — клон ≥N токенов, у которого ≥1 инстанс целиком в diff против baseline
  (готовые токен-детектор + baseline; вероятно, самый ценный недостающий gate);
- **inconsistent-clone-drift** — пара, идентичная на baseline, правка коснулась одного
  инстанса (сильнейший сигнал по Juergens: ~50% таких правок = баг); advisory high-prio;
- **copy-instead-of-refactor** — PR добавляет клон существующего кода и модифицирует оригинал;
- **change-coupling drift** — новая пара совместно-меняющихся файлов выше порога относительно
  baseline (родная drift-механика);
- **hotspot-report** (churn × сложность/отступы) — только report, не gate;
- **knowledge-risk map** (Bird) — advisory, в соло-OSS шум.

### 5.3 Что НЕ гейтить (по литературе)

Процент дублей по всему репо (Kapser–Godfrey + «5000 violations» анти-паттерн); smell-гейты
без калибровки (Lenarduzzi); hotspots/churn/ownership/defect-attractors как gate (наказывают
за работу в нужном месте — только приоритизация); SATD (связан с изменяемостью, не дефектами);
change coupling как абсолют (легитимные пары header/impl, код+тест).

**Сквозной паттерн: блокирует только «дельта против baseline на новом коде» — это уже родная
механика archcheck; всё вероятностное — advisory/report.** (Полностью совпадает с продуктовым
правилом из docs/codex_archcheck_cheap_drift_tasks.md.)

---

## 6. Второй бэкенд (libclang): что класть и почём

### 6.1 Что даёт ТОЛЬКО семантика

Единственный принципиально новый примитив — **знание, какие символы откуда используются**.
На нём стоят три класса проверок:
1. **Точность рёбер**: include-без-использования (phantom edge) и
   использование-без-include через транзитивный заголовок. clangd include-cleaner помечает
   used всё, включая macro expansions, type deduction, template instantiation ✅ — на
   препроцессоре это нерешаемо (IWYU: ~90% кода — различение full use vs forward-declare ✅).
2. **Layering по символам** (аналог Bazel `-fmodules-decluse`/`-Wprivate-header` ◻): модуль A
   использует символ из B через легальный транзитивный C — include-граф видит A→C и слеп к A→B.
3. **ODR/инкапсуляция**: SF.2 (misc-definitions-in-headers), non-const глобалы (I.2),
   cross-module friend, метрика абстрактности A.

Валидация two-backend дизайна из первоисточника: clang-tidy `misc-header-include-cycle` сам
работает **на уровне препроцессора** ✅ — циклам семантика не нужна; не переносить в clang-бэкенд
ничего, что уже работает на include-графе.

### 6.2 Кандидаты (ранжированы ценность/стоимость)

1. **Symbol-level enforcement** существующих layers/forbidden-контрактов (max ценность).
2. **Phantom-edge detection** — IWYU-lite только для межмодульных рёбер (меньше шума, чем IWYU);
   двойная польза: advisory «ребро можно удалить» + уточнение CCD/ACD/циклов.
3. **SF.2** (misc-definitions-in-headers с архитектурной атрибуцией + baseline) — дёшево,
   уже в roadmap.
4. **Martin A и D=|A+I−1|** per component — I уже есть из графа, A требует AST; «Zone of Pain»
   отчёт с цитатой Martin (authority over opinion).
5. **Namespace↔module consistency** — физическая (каталог) vs логическая (namespace) структура.
6. **Cross-module friend** (English et al.: friend-классы = coupling hotspots ◻) — только
   через границу модуля, иначе шум.
7. **Mutable global state в публичных заголовках** (extern non-const = архитектурный контракт).
8. Concrete-types-in-interface (DIP) — дорого, v0.4+.

**Anti-ideas**: не строить свой ODR-линкер-детектор (gold --detect-odr-violations слаб ✅-линия,
рабочие варианты — Clang ODR hash только для modules и ASan в рантайме); не дублировать IWYU
целиком (сам «beta quality», version-locked к clang ✅) — брать эвристики через библиотеку
clang-include-cleaner из апстрима LLVM.

### 6.3 Цена (числа из первоисточников)

- LLVM (~7k файлов, 3.8M LoC): directive-scan ~3 с → полный препроцессинг ~28 с (9×) → полная
  сборка ~15 мин ◻ (clang-scan-deps тред cfe-dev).
- Chromium: полный clangd-индекс 2–3 часа на 48 ядрах/64 GB, 550 MB диск, 2.7 GB RAM ✅
  (числа 2019 г. — порядок, не точная цена сегодня).
- Отдельные clang-tidy чеки патологически дороги (misc-const-correctness >10× reparse ◻).

→ Архитектурные следствия: clang-бэкенд **(а)** строго opt-in, **(б)** требует
compile_commands.json (как clang-uml/IWYU/clang-tidy — это норма ниши), **(в)** пер-TU
инкрементальный с кэшем по хэшу compile command, **(г)** режим `--changed-only` для PR-CI.

---

## 6. Результаты: Corpus-wide Drift Analysis (2026-06-10..11)

### Постановка

Для проверки гипотезы на пул агентских C++ проектов собран **корпус из 1000 реп** по критериям:
- **agentic** (AI-трейлер, bot-авторство ≥10%, медиана длины коммита ≥100 симв)
- **> 300 коммитов с мая 2025** (наблюдаемое окно, short-since)
- **≥2 года истории** (перед/после baseline)
- **≤50 MB C++** после чистки (per-repo)

**Итого:** 815 (base) → **+185 new** (процесс: grow_corpus.py, идемпотентен, audit в grow_corpus_ledger.tsv).

### Четыре независимых слоя дрейфа

Каждый слой — отдельный скан, несводимый к другим:

| # | Слой | Формат | Результат |
|---|---|---|---|
| **1** | **Pokloommitny Graph-Drift** (архитектурные зависимости) | `*_graph_drift.jsonl/.md` | Per-commit добавления include-зависимостей. Артефакт `*_graph_drift.jsonl` пересчитан для 43 реп с протухшим/отсутствующим датасетом; общий волюм — 9700+ per-commit записей покоммитно |
| **2** | **Copypaste Clones** (дублирование кода) | `*_dup.txt` | Снимок HEAD: EXACT/RENAMED/LITERAL пары 3+ строк. 25 примеров в EXAMPLES_50.md с кликабельными ссылками (libvirt 53 строки, fireStormViewer 94, postgresql 150, apache-arrow 39). Top repos: AlchemyViewer (1288 пар), psryland/rylogic (1296), facebook/rocksdb (759) |
| **3** | **Boolean-Drift Per-Commit** (прирост bool-полей) | `bool_history_new185.csv` | 5514 коммитов, добавивших `bool`-поля в существующие заголовки (не в новые файлы). **Top repos**: OloEngineBase (207), FastLED (129), llama.cpp (113), Serial-Studio (102), AlchemyViewer (86). Окно: shallow-since=2025-05-01 |
| **4** | **Boolean-Drift Per-Struct** (constraint decay) | `perstruct_drift_new185.csv` | 455 структур, в которые були налились через ≥4 разных коммита (критерий MIN_COMMITS). **Top по span (давность)**: llvm/circt LoweringOptions (22 поля, 5 коммитов, 735 дней), qt/qtmultimedia QWasmVideoOutput (11 полей, 6 коммитов, 733 дня), intel/compute-runtime MockDebugSession (18 полей, 4 коммита, 732 дня) |

### Ключевые находки

1. **Graph-drift + Bool-drift = ортогональные сигналы.** Репа может лить архитектуру (график падает, новые cross-module зависимости) **и одновременно** фильтровать boolean constraints (структуры меняются исподволь). Пример: facebook/react-native топит с 278 graph-errors и **одновременно** 0.3% AI-коммитов (старый базовый). Алгоритм:
   - Если **graph-errors > Δbaseline** → ребята добавляют зависимости (архитектурный дрейф).
   - Если **per-struct drift > Δbaseline** → ребята накапливают состояние в структурах (constraint decay).
   - Если **obе** → kombinированный сигнал (контроль vs agentic).

2. **Окно shallow-since=2025-05-01 универсально.** Все три слоя (graph, boolean per-commit, per-struct) меряются в одном окне → данные несомненно сравнимы. Per-struct git-blame видит только in-window коммиты → поля ДО мая схлопываются в граничный коммит (conservative estimate constraint-decay, но правомерно).

3. **OOM на больших репах решён стримингом.** Исходный `bool_history_scan.py` жрал весь `git log -p` в RAM → падал на FastLED (130 MB, 7369 коммитов). Переписал на `Popen` построчный стриминг → **11.85 MB пик памяти на FastLED, 334 записи за 1.5 мин** (вместо OOM). Фикс перенесён.

### Файлы результатов

- **ai_repo_run/**: `corpus_summary.tsv` (492 строки, merged old+new), `EXAMPLES_50.md` (50 кликабельных примеров), `grow_corpus_ledger.tsv` (KEEP/TOOBIG/GIANT audit trail), `NEW_REPOS_DRIFT_REPORT.md` (топ по graph-errors)
- **boolean_state/**: `bool_history_new185.csv` (5514), `perstruct_drift_new185.csv` (455), `BOOL_NEW185_DONE.marker` (2026-06-10 18:14:22)

---

## 7. Реестр источников

### Верифицированные адверсариально (✅ — первоисточник открыт, числа сверены)

| Источник | Что подтверждено |
|---|---|
| [arXiv 2511.04427](https://arxiv.org/abs/2511.04427) (MSR 2026, CMU) | 806 реп/2418 контролей, `.cursorrules`-прокси, SonarQube cognitive complexity +41.64%±7.62 стойко, warnings +30.26%±6.66, duplication +7.03%±4.79 n.s., velocity транзиентна (+281% м1); проверено по полному тексту HTML |
| [GitClear Coding on Copilot](https://www.gitclear.com/coding_on_copilot_data_shows_ais_downward_pressure_on_code_quality) | churn-удвоение к 2024 — дословно в отчёте |
| [DORA 2024](https://dora.dev/research/2024/dora-report/) | +25% AI → throughput −1.5%, stability −7.2% (регрессионная модель отчёта) |
| [DORA 2025](https://dora.dev/dora-report-2025/) | разворот throughput в плюс, негатив stability сохранился, «AI-амплификатор» |
| [arXiv 2605.06445](https://arxiv.org/abs/2605.06445) (Dente et al., EURECOM) | constraint decay −30 п.п. L0→L3 (8 сильных конфигураций), подан 2026-05-07 |
| [Juergens et al., ICSE 2009](https://teamscale.com/hubfs/Publications/2009-do-code-clones-matter.pdf) | 52% клонов неконсистентны, |F|/|UIC|≈0.5, 107 багов — PDF открыт постранично |
| [clangd include-cleaner design](https://clangd.llvm.org/design/include-cleaner) | used-семантика: macro/deduction/instantiation |
| [misc-header-include-cycle](https://clang.llvm.org/extra/clang-tidy/checks/misc/header-include-cycle.html) | «operates at the preprocessor level» |
| [Chromium clangd docs](https://chromium.googlesource.com/chromium/src/+/lkgr/docs/clangd.md) | индекс 2–3 ч/48 ядер/64GB; 550MB/2.7GB (данные 2019) |
| [IWYU WhyIWYU](https://github.com/include-what-you-use/include-what-you-use/blob/master/docs/WhyIWYU.md), [iwyu.org](https://include-what-you-use.org/) | «~90% кода ради forward-declare»; beta quality, version-lock |
| [code-maat](https://github.com/adamtornhill/code-maat) | coupling/churn/ownership/age — standalone CLI из git log |
| [Sonar quality gates](https://docs.sonarsource.com/sonarqube-server/quality-standards-administration/managing-quality-gates/introduction-to-quality-gates) | «Sonar way»: дубли ≤3% только в новом коде |
| [Core Guidelines I.4/I.23](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#i4-make-interfaces-precisely-and-strongly-typed) | flags-enum vs bool; «fewer than four» |
| [ISO C++ Survey 2024](https://isocpp.org/files/papers/CppDevSurvey-2024-summary.pdf) | зависимости 45.4%+36.4%, build times 42.9%+37.4% (Q6, n=1261) |
| [ISO C++ Survey 2025](https://isocpp.org/files/papers/CppDevSurvey-2025-summary.pdf) / [2026](https://isocpp.org/files/papers/CppDevSurvey-2026-summary.pdf) | magic-wand: пакетный менеджер №1, «make modules/headers work», build times (n=1036/1434) |
| [moderncppdevops 2024](https://moderncppdevops.com/2024-survey-results/) | тренд 2021→2024: −3.0/−2.4 п.п. |
| [JetBrains C++ 2023](https://blog.jetbrains.com/clion/2024/01/the-cpp-ecosystem-in-2023/) | библиотеки — боль №1; 30% без анализа кода (n=2627) |
| [randomascii: Chromium builds](https://randomascii.wordpress.com/2020/03/30/big-project-build-times-chromium/) | хедеры 99.68% компилируемых строк; 3.6B vs 11.86M; 21.45 CPU-ч |

### Найденные, но без отдельной верификации (◻ — использовать с проверкой перед цитированием наружу)

AI-качество: GitClear 2025 PDF (8.3→12.3%, ×8 блоки, рефакторинг <10%); arXiv 2603.27130
(19.8k AI-файлов, 17.2% vs 24.5%, deeper-not-broader); 2605.02741 (ρ=0.94, Modular Mirage);
2603.24755 (SlopCodeBench, 77%); 2605.06464 (AI-файлы реже обслуживаются); 2601.18341 (Robbes,
adoption 22–29%); 2510.03029 (+63% smells); Harvard LMPL 2025 (private-access); 2604.04990
(Architecture Without Architects); FSE 2025 (клоны 7.5%); контр-сторона: GitHub RCT, Google RCT
(2410.12944), ANZ (2402.05636), Peng (2302.06590), METR, Uplevel, Stanford; 2506.17833 (опрос).
Дешёвые гарды: Wagner 2016; Kapser–Godfrey EMSE 2008; Code Red (2203.04374); Borg 2024
(2401.13407); Nagappan–Ball 2005; Rahman–Devanbu 2013; Kim FixCache 2007; Ostrand–Weyuker 2005;
D'Ambros 2009; Bird 2011; Xu FSE 2015; Lenarduzzi (1908.11590); Hindle 2008; Potdar–Shihab 2014;
Maldonado 2017; Wehaibi 2016; Zaidman 2011; Murphy–Notkin 1995; Pruijt SPE 2017; Li–Liang
(2103.11392); Fontana/Arcan; cpp-dependencies; randomascii (Chromium 3.6B строк); maskray
(Bazel layering_check); BDE wiki; cpplint; PMD CPD; jscpd; NiCad; SourcererCC.

---

## Приложение: связь с текущим бэклогом

- §5.2 идеи 1–8 = задачи **093–100** (созданы 2026-06-10 из codex_archcheck_cheap_drift_tasks.md).
- §3.4 п.2 = эксперимент `local_complexity_drift` (docs/research/local_complexity_drift_examples.md,
  6/6 TP) — литературное обоснование теперь есть (CMU complexity, Hindle proxy). **Но сам скорер
  прототипа (#102) меряет сложность некорректно** — разбор с репро и рекомендацией перейти на
  шкалу Sonar Cognitive Complexity: [local_complexity_drift_scorer_review.md](local_complexity_drift_scorer_review.md).
- §1.2 S1–S6, §1.3 (мёртвый код), §1.4 (lizard-гейт, копипаста git/) — кандидаты в новые задачи;
  S1/S2 стоит чинить до любого публичного релиза.
- §2 «заявлено, но нет» — список правок CLAUDE.md/README/MVP.md (см. также находки docs-аудита).
- §5.1 п.1 (транзитивные контракты) закрывает уже распарсенные module-правила конфига —
  естественный кандидат на следующую крупную фичу v0.2.
