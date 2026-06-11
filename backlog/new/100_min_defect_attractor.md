# [CHEAP-DRIFT][HISTORY] Defect Attractor Signal

**Дата создания:** 2026-06-10
**Дата старта:** —
**Статус:** new
**Модуль:** GIT / HISTORY / REPORT
**Приоритет:** minor
**Сложность:** medium
**Блокирует:** —
**Заблокирован:** —
**Related:** #048 (drift_clean_checkout_methodology), #075 (trusted_diff_workflow), #098 (god_file_growth)

## Цель

Выявлять файлы, которые непропорционально часто попадают в fix-like коммиты и поэтому выглядят как defect attractors.

Это report-only / advisory analytics для review prioritization. Не архитектурный verdict и не bug predictor "из коробки".

## Контекст

- Источник постановки: `docs/codex_archcheck_cheap_drift_tasks.md`, Task 8.
- Задача близка к #098 по технике: history pass по `git log --numstat`, но классификация идёт не по росту файла, а по характеру коммит-сообщений.
- Это уже за пределами строгих authority-backed default rules. Значит, сигнал может жить только как отчёт/подсветка.
- Нельзя скатываться в SZZ, blame-цепочки, ownership maps и другие тяжёлые истории. В исходной постановке explicitly requested a cheap signal.

## План выполнения

### Detection contract

- Commit считается fix-like, если message матчится на паттерны `fix`, `bug`, `crash`, `regression`, `hotfix`, `segfault`, `assert` и т.п.
- Конкретика v1:
  - regex (case-insensitive, по subject-строке): `\b(fix(es|ed)?|bug|crash|regress(ion|ed)?|hotfix|segfault|fault|oops|leak)\b`;
    исключить `prefix|suffix|postfix|fixture` (negative lookbehind/ahead или
    word-boundary уже отсекает — зафиксировать тестом);
  - merge-коммиты скип (`--no-merges`); rename/format-коммиты отфильтровать
    эвристикой «fix-like И файлов затронуто > 30» → скип (массовые механические);
  - окно: последние 200 коммитов или 12 мес (общий `history_query` с #098);
  - аттрактор: файл в **топ-децили** по числу fix-like touches при
    `fix_touches >= 5` (абсолютный пол — иначе на молодых репо топ-дециль = 1 фикс);
  - «всплеск»: `>= 3` fix-like touches одного файла за 14 дней — отдельная
    пометка в message.
- Для каждого файла агрегируются:
  - число fix-like commits в окне;
  - позиция относительно percentile/top-N;
  - optional total-touch context.
- Finding:
  - `HIST.1.defect_attractor`
  - `file = <path>`
  - `line = 0`
  - message с количеством fix-like touches и размером окна.

### Runtime shape

- Один history pass по `git log --numstat --pretty=...`.
- Vendor/generated/test paths по возможности исключать тем же классификатором, что используется в остальном продукте.
- Message classification должна быть простой и объяснимой; сложную NLP-логику не добавлять.
- Если файл попадает в много fix-like commits из-за механических rename/format commits, это нужно либо фильтровать, либо честно задокументировать как ограничение.

### Scope discipline

- Report-only по умолчанию.
- Без blame, SZZ, ownership, bus-factor.
- Без "quality score" по всему проекту.
- Без попытки делать точное causal inference "этот файл дефектный потому что ...".

### Конкретный план в текущем коде

1. Не строить второй history parser:
   использовать тот же `include/archcheck/git/history_query.h` + `src/git/history_query.cpp`, который нужен #098.
2. В parser-е хранить как минимум:
   commit id, subject line, список `NumstatEntry`.
   Для первой версии этого достаточно; body parsing и blame не нужны.
3. Fix-like classification делать отдельной чистой функцией в detector-е:
   subject line lowercased, regex/substring rules на `fix`, `bug`, `crash`, `regression`, `hotfix`, `segfault`, `assert`.
4. File exclusions брать из уже существующей классификации:
   vendor/test/generated paths фильтровать теми же helpers из `file_classification.h`, чтобы сигнал не расходился с остальным runtime.
5. Как и #098, не пытаться встраивать это в `RegressionReport` или `IRule`:
   на выходе достаточно file-level `Violation` с `line = 0`.
6. Если нужен machine-readable history output, это тот же общий CLI-пререквизит:
   `dispatch_format()` сейчас snapshot-only и не умеет ни `--diff`, ни будущий history path.
7. Тесты привязать к уже имеющемуся git infrastructure:
   parser/unit level — новый `tests/unit/git/history_query_test.cpp`;
   classifier level — новый `tests/unit/scan/defect_attractor_test.cpp`;
   при необходимости end-to-end сценарии пока можно добавить в [tests/integration/diff/git_diff_test.cpp](~/projects/cpparch/tests/integration/diff/git_diff_test.cpp), чтобы не плодить вторую temp-repo harness раньше времени.

## Fixtures & Tests

- `fixtures/defect_attractor/fail/parser_fix_history.log`
- `fixtures/defect_attractor/pass/non_fix_history.log`
- `fixtures/defect_attractor/pass/vendor_ignored.log`
- `fixtures/defect_attractor/pass/test_files_ignored.log`

Обязательные проверки:

- fix-like commit messages учитываются корректно;
- repeated fixes по одному файлу дают finding;
- non-fix history не даёт false positive;
- vendor/test/generated файлы по возможности исключаются;
- output остаётся report/advisory и не меняет default exit code.

## Критерии готовности

- Один history pass, а не много ad-hoc shell-вызовов.
- Простая, объяснимая message classification.
- Нет попытки сделать SZZ-lite.
- Ясно описаны ограничения по false positives.

## Не делать

- SZZ.
- Blame graph.
- Ownership / knowledge map.
- Общий "health score" репозитория.
- Gate by default.

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Спроектировать совместимый parser history log.
2. Зафиксировать набор fix-like patterns для первого прохода.
3. Добавить fixtures на positive и negative histories.
4. Оценить сигнал на 1-2 живых репозиториях.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| History pass общий с #098, если возможно | Техника одна и та же, не нужно дублирование |
| Только простая regex/message classification | Cheap signal должен оставаться дешёвым и объяснимым |
| Report/advisory only | Сигнал вероятностный, не gate-grade |
| `line = 0` | У history aggregation нет точной строковой привязки |

## Планируемые файлы

| Область | Изменение |
|---------|-----------|
| `src/git/` или history analytics | Parser log + fix-like aggregation |
| `src/main.cpp` / optional report path | Подключение сигнала |
| `tests/unit/` | Commit-message classification |
| `tests/integration/` | End-to-end history scenarios |
| `fixtures/defect_attractor/` | Synthetic history fixtures |
