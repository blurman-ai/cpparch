# #071: Правила FP Classification для дупликатов

**Дата создания:** 2026-06-03  
**Статус:** wip  
**Дата старта:** 2026-06-03  
**Приоритет:** high  

---

## Цель

Систематизировать и зафиксировать **объективные правила** различия между:
- **TP (True Positive)** — реальный copy-paste, нужен рефактор
- **FP (False Positive)** — легитимный паттерн, гарды должны его отфильтровать

Цель: в следующих сессиях при анализе дубликатов сразу применять правила и не выдумывать FP там, где их нет.

---

## Сделано

- [x] **P0.2 whole-file гард** — файл-агрегация (≥80% фрагментов меньшего файла совпали → whole-file clone), считается в `ScanResult.wholeFileClones`, пары убираются. Тесты зелёные.
- [x] **P0.9 generated гард** — `isGeneratedPath` (`.pb.cc`, `moc_`, `ui_`, `.tab.c`, `lex.yy`, `/generated/`). Единственный выживший path-гард.
- [x] **Вывод дубликатов структурирован** (`src/main.cpp`): `(TYPE, weighted, line)`, whole-file счётчик, пересортировка по weighted, поле `Pair::type` (EXACT/RENAMED/LITERAL/MIXED/STRUCTURAL) через `cloneType()`.
- ❌ **P0.7 (platform) и P0.8 (perf-variant) УДАЛЕНЫ** (2026-06-03): давили реальный TP. Идентичные POSIX-функции (`OS::sleep`/`truncateFile`, w=1.0) между os_macos/os_linux — выносимый копипаст, а path-гард не отличал их от platform-специфики. «identical = report, regardless of path».
- ⚠️ **НЕ гард:** status_bar `addText/Icon/ColorIndicator` — **TP высокого приоритета** (worst-kind: copy + сменили один тип). Репортить, не давить.
- 📊 **Эмпирика:** LibreSprite, все 16 коммитов → 10 уникальных пар, **все TP, 0 FP** (подтверждено). Корпусные «58% FP» — артефакт дженерик-детектора, наша архитектура их не производит.
- [ ] Прогнать триаж ([triage_dup_commits.py](../../experiments/triage_dup_commits.py)) на др. репах (vmecpp, corpus-репы) — проверить 0-FP на ширине
- [ ] ⚠️ Секции «Правила (Draft)» и «Внешняя опора» ниже всё ещё описывают P0.7/P0.8 как FP-гарды — переписать под факт (удалены)
- [ ] Написать правила в `docs/duplication_architecture.md` § **FP Classification Rules**

---

## Внешняя опора (authority, не мнение)

Наш extractability-тест совпадает с эмпирическим каталогом
**Kapser & Godfrey, «"Cloning considered harmful" considered harmful»** (Empirical
Software Engineering, 2008, 13(6):645–692) — это снимает упрёк «вкусовщина»
(принцип проекта «authority over opinion»):

- их **parameterized code** (копия, сводимая в одну параметризованную функцию) —
  именно наш **worst-kind TP** (скопировал блок, поменял один токен);
- их **platform variations** / **replicate & specialize** — были нашими гардами
  P0.7-P0.8 (удалены 2026-06-03; ныне действуют только P0.9 generated и P0.2 whole-file);
- их тезис: вред определяется **контекстом и коэволюцией**, не сходством текста —
  ровно поэтому severity выносим в отдельный сигнал (см. #078, co-change).

Сводка типов клонов, эмпирика вреда и методы детекта со ссылками:
[../../docs/research/code_clones.md](../../docs/research/code_clones.md).

## Правила (зафиксированы в docs/duplication_architecture.md §5.x)

Формализованные критерии различия TP/FP перенесены в основной архитектурный документ: [docs/duplication_architecture.md](../../docs/duplication_architecture.md) §5.x **FP Classification Rules (#071)**.

Там же — extractability-тест (порядок применения до сигналов), 4 верных TP-сигнала, действующие гарды (P0.2, P0.9), история удаления P0.7/P0.8 и контекст severity.

### Историческая справка: удалённые гарды

**P0.7 (platform-twins)** и **P0.8 (perf-variants)** были path-гардами, которые привили к idea "разные платформы → разные реализации → не дублирование". Удалены 2026-06-03, потому что на практике идентичный код (например, `OS::sleep()` одинаков на POSIX) может быть как действительной платформо-специфичностью (разные syscalls), так и копипастом простых helper-функций. Гард не может различить — давит и TP, и FP. Решение: **identical = report, regardless of path**; тогда human reviewer видит пару и принимает решение.

---

## Ключевые решения

- **«identical = report, regardless of path»** — path-based FP-гарды неверны: идентичный код выносим = TP, даже между платформами. P0.7/P0.8 удалены.
- **similarity-score ≠ refactor-priority** — низкий weighted (разные типы разбавляют bag-of-words) ≠ FP; такие пары рискуют быть ПРОПУЩЕННЫМИ, не over-reported.
- **worst-kind TP** = copy block + сменить один токен (status_bar indicator-триплет) — высокий приоритет, не «средний».
- **Наша архитектура ≠ дженерик-детектор** — function-фрагменты ≥30 токенов + P0.6 не производят большинство корпусных «FP» (decl-хидеры, coincidental). 42% precision к нам неприменимо.
- **Generated (P0.9)** — единственный честный path-FP; но маркеры ловят мало реального, нужен banner-scan сырого файла.

## Изменённые файлы (закоммичены — проверено 2026-06-11)

- `src/scan/duplication/duplication_scanner.cpp` — P0.2 whole-file (`phase8WholeFileSuppress`, строка 336), P0.9 generated (`isGeneratedPath`, строка 173); P0.7/P0.8 удалены. В master (история: `1329d06`, `be56245`).
- `include/archcheck/scan/duplication/duplication_scanner.h` — `ScanResult.wholeFileClones`, опции гардов.
- `src/main.cpp` — структурированный вывод дубликатов (TYPE/weighted/line, счётчик, сортировка).
- `tests/duplication_path_guards_test.cpp` — P0.9 + whole-file кейсы.
- `experiments/triage_dup_commits.py`, `experiments/dup_triage_libresprite.md` — триаж-инструмент + отчёт.
- mem: `fp_classification_rules`.

## В работе / Следующие шаги

1. Прогнать триаж на vmecpp / corpus-репах — проверить 0-FP на ширине (ждёт go; **research, вне Haiku-скоупа**).
2. Переписать устаревшие секции «Правила (Draft)» / «Внешняя опора» — план для Haiku ниже.
3. (опц.) generated banner-scan; data-table гард (риск прибить biquad-TP) — **вне скоупа**, P1.3 закрыт данными в #083 (не реализуем до v0.2 semantic).

## План для Haiku (2026-06-11) — только документация

Перед стартом ОБЯЗАН прочитать целиком: эту задачу, [docs/dev/haiku_task_guide.md](../../docs/dev/haiku_task_guide.md) §2, [docs/duplication_architecture.md](../../docs/duplication_architecture.md) (весь — это контекст подсистемы).

**Это docs-only задача. C++ код, тесты, эксперименты НЕ трогать.**

### Главная ловушка (прочитай дважды)

Секция «Правила (Draft)» этой задачи **частично неверна**: пункты FP-2 (platform) и FP-3 (perf-variants) описывают гарды P0.7/P0.8 как действующие, но они **УДАЛЕНЫ 2026-06-03** (см. «Сделано» и «Ключевые решения»). Авторитетная семантика — секция «Ключевые решения», главный принцип: **«identical = report, regardless of path»**. НЕ переноси Draft в доку как есть.

### Действующие гарды (факты, проверены 2026-06-11 по коду)

В `src/scan/duplication/duplication_scanner.cpp` живы: P0.2 whole-file (`phase8WholeFileSuppress:336`), P0.9 generated (`isGeneratedPath:173`). P0.7/P0.8 — удалены. P1.3 header↔impl — закрыт данными (#083), не реализован. Перед написанием доки ОБЯЗАН перепроверить grep'ом — если набор гардов изменился, остановись и доложи.

### Шаг 1 — секция в `docs/duplication_architecture.md`

В §5 «Классы ложных срабатываний и как лечим» (строка ~182) добавить подсекцию `### 5.x FP Classification Rules (#071)`:

1. **Порядок применения: extractability-тест ПЕРВЫМ** — «можно ли вынести в одну функцию с параметрами?» да → TP, нет (разные семантики) → FP.
2. **TP-сигналы** — 4 пункта из секции «Правила (Draft) → TP» (они верны, переносить можно): идентичный код в разных файлах (w≥0.95); полный скопированный блок (w=1.0 cross-file); 3+ повторения в одном файле; extractable.
3. **Действующие FP-гарды** — только те, что есть в коде (см. факты выше): P0.2, P0.9.
4. **История P0.7/P0.8** — абзац: были path-гардами platform/perf-variants, удалены 2026-06-03, причина — давили реальный TP (идентичные POSIX-функции между os_macos/os_linux — выносимый копипаст); принцип «identical = report».
5. **Авторитет** — ссылка на Kapser & Godfrey 2008 и [docs/research/code_clones.md](../../docs/research/code_clones.md) (формулировка из секции «Внешняя опора», вычистив упоминание P0.7/P0.8 как наших гардов).
6. Severity по co-change — отдельный сигнал, см. #078 (одна строка, не расписывать).

### Шаг 2 — починить секции этой задачи

- «Правила (Draft)» → переименовать в «Правила (зафиксированы в docs/duplication_architecture.md §5.x)», FP-пункты 2-3 переписать как историю удаления, FP-пункт 4 (if-elif цепь) пометить честно: гардом не реализован, остаётся ручным критерием триажа.
- «Внешняя опора» — убрать «наши гарды P0.7-P0.9» → «наш P0.9 и удалённые P0.7/P0.8».

### Definition of done

- `grep -n "FP Classification" docs/duplication_architecture.md` → ≥1 строка.
- В новой подсекции extractability-тест идёт ДО списков сигналов.
- `grep -n "P0.7\|P0.8" docs/duplication_architecture.md backlog/wip/071_fp_classification_rules.md` — каждое вхождение только в контексте «удалены».
- `grep -cn "uncommitted\|Коммитов пока нет" backlog/wip/071_fp_classification_rules.md` → 0.
- `git status` — изменены только `docs/duplication_architecture.md` и этот файл.
- Никаких «outdated»-баннеров — устаревшее переписывается, не помечается.

### Эскалация (когда остановиться и передать старшей модели)

Остановись, запиши сюда «Заблокировано: <что/почему>» и доложи, если: grep по коду показал набор гардов, отличный от заявленного выше; нашёл противоречие между «Ключевые решения» и docs/duplication_architecture.md, которое нельзя разрешить принципом «identical = report»; кажется, что надо менять код. Дальше — Sonnet, затем Opus.

---

## Related

- mem:`fp_classification_rules` — буду обновлять по ходу работы

