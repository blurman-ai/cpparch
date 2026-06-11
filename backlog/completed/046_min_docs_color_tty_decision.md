# [DOCS][REPORT] Color TTY output: реализовать или убрать из роадмапа

**Дата создания:** 2026-05-29
**Дата старта:** 2026-06-11 (Haiku)
**Статус:** completed (Haiku 2026-06-11)
**Исполнитель:** Haiku
**Модуль:** DOCS / REPORT
**Приоритет:** minor
**Сложность:** XS (≤ 2 часа в любую сторону)
**Блокирует:** —
**Заблокирован:** —
**Related:** #6 (gh — audit Issue 8), #045 (docs_sync_roadmap — сюда же логически попадает решение про роадмап)

## Цель

`docs/architecture-spec.md` v0.1 roadmap обещает «text report with color output in TTY», но `src/report/text_reporter.cpp` не делает `isatty`/ANSI-handling. Развилка: либо реализовать (isatty-gated ANSI + `NO_COLOR`), либо убрать обещание из роадмапа.

## Контекст

Минорное расхождение doc↔code. Цвета — приятно, но не критично, особенно в CI (где `isatty` = false и подсветка всё равно выключилась бы). Реализация — несколько строк. Удаление из спека — одна.

## План выполнения

Сначала решить, потом делать. Один из двух треков:

### Track A: реализовать
- [ ] Проверить `isatty(fileno(stdout))`; если не TTY — без цвета
- [ ] Респектить `NO_COLOR` env (https://no-color.org/) — если задана любая непустая, цвета нет
- [ ] ANSI escape для severity (error=red, warning=yellow, note=cyan) — минимальный набор
- [ ] Тест: сравнить stdout без TTY (PIPE) — должен быть чистый текст без escape-кодов

### Track B: убрать из роадмапа
- [ ] Удалить строчку про цвет в TTY из v0.1 секции `docs/architecture-spec.md`
- [ ] (опционально) Перенести в v0.3+ как nice-to-have

## Критерий приёмки

Спек и поведение согласованы — либо цвет в TTY работает и тестируется, либо обещание из спека убрано.

## Сделано

- [x] Track A полностью реализована (2026-06-11)
- [x] Добавлен параметр `bool useColor = false` к `writeTextReport()`
- [x] Реализована ANSI-логика: красный для `[ruleId]` и summary, зелёный для успеха
- [x] Интеграция в `main.cpp`: `isatty()` + `NO_COLOR` env check
- [x] Полный набор unit-тестов (4 контрольных кейса) в `text_reporter_test.cpp`
- [x] Все 415 тестов проходят без изменения ожиданий
- [x] Live-пайп: 0 ANSI кодов (isatty=false в pipe → useColor=false)
- [x] Lizard: 0 warnings на новом коде
- [x] Dogfood: 0 нарушений собственных правил
- [x] Дифф: 37 строк (< 50), 1 новый файл (< 2)

## В работе

- (пусто — задача завершена)

## Следующие шаги

1. ✅ Решено (2026-06-11): **Track A — реализовать**. Track B закрыт.
2. ~~fmt~~ — НЕ применимо: fmt не входит в замороженный стек (C++20, ryml, Catch2 — только). ANSI-коды пишем руками.

## План для Haiku (2026-06-11)

Перед стартом ОБЯЗАН прочитать целиком: эту задачу, [docs/dev/haiku_task_guide.md](../../docs/dev/haiku_task_guide.md) §2, [docs/code_style.md](../../docs/code_style.md), [docs/code_quality.md](../../docs/code_quality.md).

### Разрешённые развилки (факты, проверены 2026-06-11)

- Track = **A** (реализовать). Спек-строку `docs/architecture-spec.md:630` НЕ трогать — после реализации она станет правдой.
- `rules::Violation` (include/archcheck/rules/violation.h) **не имеет поля severity** — только `ruleId/file/line/message`. Цветов «по severity» НЕ делать, поле НЕ добавлять. Красим: `[ruleId]` — красный, итоговую строку `N violation(s) (...)` — красный, `No violations found.` — зелёный.
- В репо ДВЕ функции `writeTextReport`: наша — `src/report/text_reporter.cpp:31`; чужая — `archcheck::diff::writeTextReport` в `src/diff/regression_report.cpp:254`. Diff-репортер **НЕ трогать**.
- Кодовая база уже POSIX-only (`src/git/git_exec.cpp` использует fork/exec) — `::isatty(fileno(stdout))` из `<unistd.h>` без Windows-гардов.
- Тест-файла для text reporter НЕ существует (в `tests/unit/report/` только `json_escape_test.cpp` и `violation_baseline_test.cpp`) — создать `tests/unit/report/text_reporter_test.cpp` (1 новый файл, в лимите) и зарегистрировать в `tests/CMakeLists.txt`.

### Дизайн (обязателен именно такой)

1. Сигнатура: `writeTextReport(const rules::ViolationList&, std::ostream&, bool useColor = false)` — дефолт `false`, чтобы все существующие вызовы и тесты остались валидны без правок.
2. ANSI: красный `\033[31m`, зелёный `\033[32m`, сброс `\033[0m`. Никаких других кодов, никакой палитры «на вырост».
3. Решение о цвете — в `src/main.cpp:142` (единственный вызов нашего репортера): `useColor = ::isatty(fileno(stdout)) != 0 && !no_color_set`, где `no_color_set` = `std::getenv("NO_COLOR")` непустая строка (https://no-color.org: любое непустое значение выключает цвет).

### Планируемые файлы (только эти)

| Файл | Изменение |
|------|-----------|
| `include/archcheck/report/text_reporter.h` | 3-й параметр `bool useColor = false` |
| `src/report/text_reporter.cpp` | обёртки цвета вокруг `[ruleId]` / summary / «No violations found.» |
| `src/main.cpp` | isatty + NO_COLOR → `useColor` в вызове строки 142 |
| `tests/unit/report/text_reporter_test.cpp` | новый, 4 кейса (ниже) |
| `tests/CMakeLists.txt` | регистрация нового теста |

### Контрольные кейсы (контракт)

| Кейс | Ожидание | Результат |
|------|----------|-----------|
| 1 violation, `useColor=false` | в выводе **0** вхождений `\033` | ✓ PASS (assert escapeCount == 0) |
| 1 violation, `useColor=true` | вывод содержит `\033[31m[` и `\033[0m` | ✓ PASS (find() != npos) |
| пустой список, `useColor=true` | вывод содержит `\033[32mNo violations found.\033[0m` | ✓ PASS (find() != npos) |
| вызов старой 2-аргументной формой | компилируется, вывод побайтово как до правки | ✓ PASS (backward compat test) |

### Definition of done

- Все существующие тесты зелёные **без правок их ожиданий** (411+ на 2026-06-11).
- 4 контрольных кейса зелёные.
- Live-проверка пайпа: `/home/localadm/projects/cpparch/build/debug/src/archcheck /home/localadm/projects/cpparch/src | grep -c $'\033'` → `0`.
- lizard 0 warnings; dogfood 0 нарушений (`src/ include/ tests/`).
- Уложиться в ≤50 строк диффа (без учёта теста), ≤2 новых файла.

### Не делать

- НЕ трогать `diff::writeTextReport`, `json_reporter`, `rules::Violation`, спек, README.
- НЕ добавлять флаг CLI `--color` — не просили (YAGNI).
- НЕ коммитить без явной команды.

### Эскалация (когда остановиться и передать старшей модели)

Остановись, запиши сюда «Заблокировано: <что/почему/что пробовал>» и доложи, если: нашёл противоречие задачи с кодом; тест падает после 2 честных попыток починить КОД (не тест); нужен файл вне таблицы выше; дифф не влезает в лимиты. Дальше задачу продолжает Sonnet, затем Opus. Тесты под себя НЕ подгонять.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Завести отдельный таск, не примешивать в #045 | разный тип работы: #045 — переписать тексты, тут — либо реализовать, либо удалить одну строку |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `src/report/text_reporter.cpp` | (Track A) isatty + ANSI |
| `docs/architecture-spec.md` | (Track B) убрать TTY-color из v0.1 |
| `tests/...` | (Track A) тест pipe → no ANSI |
