# [SCAN][RULES][BUILD] libclang semantic backend (--with-clang)

**Дата создания:** 2026-05-29
**Дата старта:** —
**Статус:** new
**Модуль:** SCAN / RULES / BUILD
**Приоритет:** major
**Сложность:** L (multi-day, разбивается на фазы)
**Блокирует:** SF.21, SF.2, SF.5, SF.10, SF.11, Martin Abstractness (все семантические правила v0.2+)
**Заблокирован:** ~~#043 (spike_clang_perf)~~ — **разблокирован 2026-05-29**, спайк подтвердил двух-бекендную схему ([`docs/dev/spike_clang_perf.md`](../../docs/dev/spike_clang_perf.md)): libclang ×1350 медленнее regex, ~9 минут на 5000-TU монорепе single-thread. Скоуп этой задачи остаётся как написан, `--with-clang` сохраняется.
**Related:** #5 (gh — owner-таска), #6 (gh — Issue 6 audit), #006 (spec_refactor — заложил двух-бекендную схему), #028 (rules_engine_mvp — отложил SF.21 "требует libclang"), #035 (sf7_brace_depth — пометил точный парсинг как libclang/v0.2), #008 (dependency_graph_foundation), #043 (spike_clang_perf — подтвердил архитектуру)

## Цель

Завести опциональный libclang-бэкенд под флагом `--with-clang`, дающий правилам доступ к AST, и провести через него первое семантическое правило (SF.21) как вертикальный срез.

## Контекст

Двух-бекендная схема — принятое решение с #006: fast (препроцессорный скан) = default в v0.1, libclang = opt-in / полноценно в v0.2. Но task-owner'а у бэкенда нет — он живёт только в `docs/architecture-spec.md` (§«Двух-бекендная схема», запланированный `clang_scanner.h/.cpp`, пример `--with-clang --compile-commands`) и как «куда отложено» в закрытых тасках. Эта задача — owner v0.2-фазы.

Что разблокирует: семантические SF.2 / SF.5 / SF.10 / SF.11 / SF.21 и Martin's Abstractness — всё, что нельзя достоверно вытащить препроцессорным сканом.

Два инварианта, которые нельзя нарушить:

1. **Zero-setup не ломается.** Тул собирается и все v0.1-правила работают без установленного LLVM. libclang — guarded optional dependency. `--with-clang` без libclang в сборке → чистая ошибка, а не падение.
2. **Fast-backend правила не зависят от clang.** SF.7/8/9, Lakos.* не должны ничего знать про AST-типы.

## Открытый вопрос (решить в фазе 1)

**Как семантическое правило получает AST, не пачкая `IRule`?**

Текущий `IRule::check(graph, readFile)` = граф включений + текстовый `readFile`. Допиши в него AST — и все fast-backend правила начнут транзитивно зависеть от clang-типа, который не используют (ISP-нарушение).

Предлагаемое направление (подтвердить/опровергнуть на фазе 1): отдельный `ISemanticRule` с `check(SemanticContext&)`, где `SemanticContext` — тонкая обёртка над `clang::ASTContext` / cursor API. `rule_set` гоняет `ISemanticRule`-набор только при `--with-clang`. Fast-набор остаётся на `IRule` нетронутым. Никакого общего базового интерфейса «на всякий случай» — два набора, два прохода, явная развилка в `run_check`.

## План выполнения

Три вертикальных среза. Каждый — кандидат на спинофф в 042a/b/c при переезде в `wip/` (по образцу 008 → 008a–h). Не плодить файлы заранее.

### Фаза 1 — 042a: скелет бэкенда + CMake opt-in + плумбинг флага

- [ ] CMake: `option(ARCHCHECK_WITH_CLANG "..." OFF)`; `find_package(Clang CONFIG)` под guard; при OFF/absent — `clang_scanner.cpp` не компилируется, цель собирается без LLVM
- [ ] `include/archcheck/scan/clang_scanner.h` — публичная форма (типы + сигнатура парса TU), без логики
- [ ] `src/scan/clang_scanner.cpp` — заглушка под `#ifdef ARCHCHECK_WITH_CLANG`
- [ ] CLI: флаги `--with-clang` и `--compile-commands <path>`; без libclang в сборке `--with-clang` → exit с понятным «rebuild with `-DARCHCHECK_WITH_CLANG=ON`»
- [ ] Решить открытый вопрос: завести `ISemanticRule` / `SemanticContext` (или обосновать иной выбор) — зафиксировать в «Ключевые решения»
- [ ] Smoke-тест: сборка с `-DARCHCHECK_WITH_CLANG=ON` зелёная; сборка без флага зелёная и не тянет LLVM
- [ ] CI: добавить опциональную matrix-ногу с libclang (не ломать основную ногу без LLVM)

### Фаза 2 — 042b: загрузка compile_commands + первый AST-факт

- [ ] Загрузка `compile_commands.json` (clang `CompilationDatabase`); ошибка при отсутствии — внятная
- [ ] Парс одной TU в AST через `SemanticContext`
- [ ] Вытащить один проверяемый факт (например, список namespace-деклараций в TU) — доказать, что AST-данные текут до места, где будут жить правила
- [ ] Fixtures: минимальный проект с `compile_commands.json` (1–2 TU)
- [ ] Тест: факт извлекается детерминированно

### Фаза 3 — 042c: первое семантическое правило SF.21 (вертикальный срез)

- [ ] `include/archcheck/rules/sf21_anon_namespace_in_header.h` + `src/rules/...cpp` на `ISemanticRule`
- [ ] Правило: anonymous namespace в файле-заголовке (по `isHeaderFile()`) → violation с `file:line`
- [ ] Регистрация в семантическом наборе `rule_set`, гоняется только при `--with-clang`
- [ ] Fixtures: `fixtures/sf21/pass/` (anon ns только в .cpp), `fail_in_header/` (anon ns в .h)
- [ ] Тест: pass = 0 нарушений, fail = 1 нарушение с верным line
- [ ] Снять с #028 пометку «SF.21 → v0.2» (закрыт)

## Альтернативы (отклонены / отложены)

- **Первое правило SF.2 вместо SF.21** (использовать namespaces для логической структуры) — отклонено как первый срез: размытый критерий, спорная дефолтность, больше про стиль чем про однозначный AST-факт. SF.21 — бинарный, общепризнанный (Core Guidelines), уже помечен как libclang-кандидат в #028. SF.2 заводится отдельной задачей после того, как бэкенд устаканится.
- **tree-sitter вместо libclang** — даёт синтаксическое дерево без семантики (резолв имён, namespaces, ODR). Для SF.2/5/10/11 недостаточно. Зафиксировано в `architecture-spec` §528.
- **Семантические рёбра в общий `DependencyGraph`** (clang-резолв уточняет существующий граф для SF.9/Lakos) — заманчиво, но это отдельная ценность и отдельный скоуп. Не смешивать с «доступ к AST для новых правил». Кандидат в отдельную задачу v0.2+, не сюда. YAGNI.
- **Единый базовый `IRule` с optional AST** — отклонено: ISP-нарушение, fast-набор не должен зависеть от clang. Два набора, явная развилка.

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. ~~Дождаться итогов спайка #043~~ — **готово 2026-05-29.** Двух-бекендная схема подтверждена замером.
2. Начать с фазы 1 (CMake opt-in + скелет `clang_scanner.h/.cpp` + `--with-clang` + решение по `ISemanticRule`).
3. Учесть в фазе 1: версия libclang **≥18** (на спайке мерилась 19; 11 устаревшая, не цель). Зафиксировать в CMake `find_package`-логике и в CI matrix.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| libclang — opt-in, не дефолт | zero-setup не ломаем, новые пользователи не должны устанавливать LLVM |
| Отдельный `ISemanticRule` (предв.) | ISP — fast-набор не должен транзитивно тащить clang-типы |
| Первое правило — SF.21, не SF.2 | бинарный AST-факт против размытого стиля |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `CMakeLists.txt` / `src/CMakeLists.txt` | `ARCHCHECK_WITH_CLANG` option, guarded `find_package(Clang)` |
| `include/archcheck/scan/clang_scanner.h` | new (фаза 1) |
| `src/scan/clang_scanner.cpp` | new, под `#ifdef` (фазы 1–2) |
| `include/archcheck/rules/i_semantic_rule.h` | new (фаза 1, если подтверждён `ISemanticRule`) |
| `include/archcheck/rules/sf21_anon_namespace_in_header.h` + `.cpp` | new (фаза 3) |
| `src/rules/rule_set.cpp` | семантический набор под `--with-clang` |
| CLI (entry point) | флаги `--with-clang`, `--compile-commands` |
| `fixtures/sf21/{pass,fail_in_header}/` | new (фаза 3) |
| `tests/...` | clang-скан + SF.21 |
| `.github/workflows/ci.yml` | опциональная libclang matrix-нога |

## Fixtures

- [ ] `fixtures/sf21/pass/` — anonymous namespace только в `.cpp`
- [ ] `fixtures/sf21/fail_in_header/` — anonymous namespace в `.h`
