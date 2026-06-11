# [CHEAP-DRIFT][DIFF] SATD Delta

**Дата создания:** 2026-06-10
**Дата старта:** 2026-06-11
**Статус:** wip
**Модуль:** GIT / DIFF / REPORT
**Приоритет:** major
**Сложность:** small
**Блокирует:** —
**Заблокирован:** —
**Related:** #018 (git_diff_analysis), #024 (in_memory_fs_for_diff), #075 (trusted_diff_workflow), #093 (flag_argument)

## Цель

Предупреждать, что PR добавляет новые self-admitted technical debt markers:

- `TODO`
- `FIXME`
- `HACK`
- `XXX`
- `TEMP`
- `temporary`
- `workaround`
- `quick fix`
- `dirty`

Это review-signal, а не архитектурное нарушение.

## Контекст

- Источник постановки: `docs/codex_archcheck_cheap_drift_tasks.md`, Task 4.
- Правило должно жить **только** в delta-контексте. Полный scan по legacy-дереву здесь почти бесполезен и конфликтует с принципом "gate only on new drift".
- Текущий `archcheck --diff` уже умеет structural comparison между git refs. Эту задачу нужно вписать рядом с существующим diff pipeline, а не городить отдельный полу-инструмент.
- По продуктовой рамке это не default architectural rule. Значит: advisory-first, opt-in gate максимум для узкого кейса `FIXME/HACK без issue id`.

## План выполнения

### Detection contract

- `SATD.1.new_satd_marker`:
  - любой новый marker на added line;
  - advisory по умолчанию.
- `SATD.2.untracked_fixme_or_hack`:
  - `FIXME` или `HACK` на added line без issue id;
  - остаётся gate-candidate, но не включается default gate без явной конфигурации в будущем.

### Алгоритм (тривиальный, фиксируем чтобы не разъехался)

1. `collectAddedLines()` (см. «Конкретный план», п.2) → `{file, line, text}`.
2. Маркер ищется **только в комментарной части** added line: после `//` или
   внутри `/* */` (грубый детект: позиция `//` вне строковых литералов; строки
   без комментария — скип). Иначе `dirty` в идентификаторе кода даст FP.
3. `SATD.1` regex (case-insensitive, на границах слов):
   `\b(TODO|FIXME|HACK|XXX|TEMP)\b|\btemporary\b|\bworkaround\b|\bquick.?fix\b|\bdirty\b`.
4. `SATD.2`: маркер `FIXME|HACK` и в той же строке НЕТ issue-id:
   `([A-Z][A-Z0-9]+-\d+)|(#\d+)|(\b(gh|issue)[-/]\d+)` (JIRA / GitHub / generic).
5. Case-фиксация: `TODO/FIXME/HACK/XXX/TEMP` — только UPPERCASE (lowercase `todo`
   в прозе комментария — не маркер); словесные (`workaround`, `temporary`,
   `dirty`, `quick fix`) — case-insensitive.
6. Один finding на строку (не на маркер); `Violation{ruleId, file, line, message=текст строки усечённый до 120}`.

### Runtime shape

- Работать только по added lines из unified diff.
- Full-tree scan вне scope.
- Issue-pattern для первого прохода можно захардкодить, если отдельная config surface требует schema work.
- Если line mapping уже даёт точный путь и номер строки, finding должен жить в текущем `Violation` contract.

### Noise control

- Не считать удалённые или контекстные строки.
- Игнорировать marker inside removed code blocks.
- Case sensitivity должна быть зафиксирована тестами, а не оставлена "как получится".

### Конкретный план в текущем коде

1. В текущем коде нет reusable git command helper:
   `runGit()` живёт в anon namespace в [src/git/git_state.cpp](/home/localadm/projects/cpparch/src/git/git_state.cpp).
   Первый шаг этой задачи — вынести его в `include/archcheck/git/git_exec.h` + `src/git/git_exec.cpp`, иначе #096/#097/#098/#100 будут дублировать process-launch код.
2. Добавить `include/archcheck/git/diff_query.h` + `src/git/diff_query.cpp` с двумя публичными примитивами:
   `collectAddedLines(...)` для unified diff `--unified=0`;
   `collectNumstat(...)` для `--numstat` (этот же API сразу переиспользует #097).
3. `collectAddedLines(...)` должен поддерживать оба текущих diff-режима:
   `a..b` и `a..WORKTREE`, с теми же WORKTREE semantics, что уже зафиксированы в `changedCppFiles()`.
4. Сам SATD detector держать отдельным маленьким файлом (`src/scan/satd_scan.cpp` либо соседний flat helper), который принимает `AddedLine` записи и отдаёт `rules::ViolationList`.
5. Интеграцию делать рядом с [src/main.cpp](/home/localadm/projects/cpparch/src/main.cpp)::`run_diff()`:
   не расширять `RegressionReport`,
   не менять `archcheck::diff::writeTextReport(...)`,
   а печатать SATD-блок после structural diff summary.
6. Если в этой же задаче нужен machine-readable diff output, придётся сначала рефакторить `dispatch_format()` / `dispatch_diff()` в [src/main.cpp](/home/localadm/projects/cpparch/src/main.cpp):
   сейчас `--format` работает только для `run_check()`, а `--diff` JSON вообще не умеет.
7. Тесты привязать к текущей структуре:
   parser-уровень — новый `tests/unit/git/diff_query_test.cpp`;
   marker logic — новый `tests/unit/scan/satd_scan_test.cpp`;
   repo-level diff — расширение [tests/integration/diff/git_diff_test.cpp](/home/localadm/projects/cpparch/tests/integration/diff/git_diff_test.cpp).

## Fixtures & Tests

- `fixtures/satd_delta/pass/hack_with_issue.diff`
- `fixtures/satd_delta/fail/todo_added.diff`
- `fixtures/satd_delta/fail/fixme_without_issue.diff`
- `fixtures/satd_delta/pass/context_only.diff`

Обязательные проверки:

- marker ловится только на added lines;
- `HACK ABC-123: ...` не триггерит `SATD.2`;
- `FIXME: ...` без issue id триггерит `SATD.2`;
- text/json output остаётся совместимым с текущим contract;
- правило не сканирует весь legacy tree.

## Критерии готовности

- Diff-only.
- Advisory-first.
- Без schema change.
- Без нового отдельного CLI-инструмента вне `archcheck`.

## Не делать

- Full repository grep по SATD.
- Ownership / blame analysis.
- Автоматическое распознавание "правильных" issue tracker форматов beyond simple regex.
- Gate by default для любого `TODO`.

## Сделано

1. Вынести `runGit()` из anon namespace git_state.cpp в общий хелпер:
   - `include/archcheck/git/git_exec.h` + `src/git/git_exec.cpp` (82 строк)
   - Обновлена git_state.cpp, чтобы использовать общий хелпер
2. Создать `include/archcheck/git/diff_query.h` + `src/git/diff_query.cpp`:
   - `collectAddedLines()` — парсинг unified diff --unified=0
   - `collectNumstat()` — парсинг git diff --numstat
   - Поддержка обоих режимов: a..b и a..WORKTREE
3. Создать SATD-детектор:
   - `include/archcheck/scan/satd_scan.h` + `src/scan/satd_scan.cpp` (156 строк)
   - `detectSatdMarkers()` реализует SATD.1 (любой маркер) и SATD.2 (FIXME/HACK без issue-id)
   - Поддержка всех регулярных маркеров из спеки
   - Маркер ловится только в комментариях (// или /* */)
   - Один violation на строку, усечение до 120 символов
4. Интеграция в src/main.cpp:
   - Добавлено собирание added lines через `collectAddedLines()`
   - Добавлен вызов `detectSatdMarkers()` в `runDiffFullPath()`
   - SATD-блок печатается после structural report (advisory-only, не гейтит)
5. Фикстуры в fixtures/satd_delta/:
   - pass/: hack_with_issue.diff, context_only.diff, dirty_in_code.diff
   - fail/: todo_added.diff, fixme_without_issue.diff, hack_without_issue.diff, temporary_marker.diff, workaround_marker.diff
6. Тесты:
   - tests/unit/git/diff_query_test.cpp (2 базовых теста, 21 строка)
   - tests/unit/scan/satd_scan_test.cpp (22 теста, 184 строки)
   - tests/integration/diff/git_diff_test.cpp (добавлен 1 интеграционный тест)
   - Все 381 тест проходят

## В работе

- (нет)

## Следующие шаги

- Готово к ревью и мерджу

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Только added lines | Это новый debt, а не перепись всего наследия |
| `SATD.2` уже, чем `SATD.1` | Не всякий `TODO` должен даже теоретически ломать CI |
| Hardcoded defaults acceptable in v1 | Текущий schema contract не готов к cheap-drift knobs |
| Рядом с existing diff pipeline | Не плодить второй способ понимать git diff |

## Планируемые файлы

| Область | Изменение |
|---------|-----------|
| `src/git/` или diff orchestration | Доступ к added lines |
| `src/main.cpp` / cheap-drift pass | Подключение SATD detector |
| `tests/unit/` | Marker / regex logic |
| `tests/integration/` | Diff fixtures |
| `fixtures/satd_delta/` | `pass/` и `fail/` сценарии |
