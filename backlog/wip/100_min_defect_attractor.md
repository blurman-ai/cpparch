# [CHEAP-DRIFT][HISTORY] Defect Attractor Signal

**Дата создания:** 2026-06-10
**Дата старта:** 2026-06-11
**Статус:** wip
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
    word-boundary отсекает `prefix|suffix|postfix|fixture` (зафиксировано тестом);
  - merge-коммиты скип; rename/format-коммиты отфильтрованы эвристикой «fix-like И файлов затронуто > 30» (механические);
  - окно: последние 200 коммитов (общий `history_query` с #098);
  - аттрактор: файл в **топ-децили** по числу fix-like touches при `fix_touches >= 5`;
- Для каждого файла агрегируются:
  - число fix-like commits в окне;
  - позиция относительно percentile/top-N;
- Finding:
  - `HIST.1.defect_attractor`
  - `file = <path>`
  - `line = 0`
  - message с количеством fix-like touches и размером окна.

### Runtime shape

- Один history pass по `git log --numstat --pretty=...`, переиспользуется `history_query.h` (#098).
- Vendor/generated/test paths исключаются тем же классификатором из `file_classification.h`.
- Message classification простая: regex-матчинг, никаких NLP.
- Механические commits (>30 файлов + fix-like) фильтруются автоматически.

### Scope discipline

- Report-only по умолчанию, advisory, не гейт.
- Без blame, SZZ, ownership, bus-factor.

## Сделано

- ✅ **Детектор** (include/archcheck/scan/defect_attractor.h + src/scan/defect_attractor.cpp):
  - `isFixLikeCommit()` — regex-матчинг на subject-строке
  - `shouldSkipCommit()` — отсев merge/пустых/механических коммитов (>30 файлов)
  - `aggregateFixTouches()` — агрегирование по production-файлам (исключены vendor/test)
  - `calculateTopDecileThreshold()` — статистика: if <10 файлов то max, иначе 90-й перцентиль
  - `detect()` — вернуть violations в порядке убывания fix_touches
  - **Качество кода**: 
    - ✅ NLOC ≤30 (refactored: `filterCandidates()` helper)
    - ✅ CCN ≤15 (макс 12)
    - ✅ Аргументы ≤5
    - ✅ clang-format
    - ✅ cppcheck (нет warnings)

- ✅ **Подключение в CLI** (src/main.cpp::run_history):
  - Добавлены включения для defect_attractor.h
  - run_history() теперь печатает сначала SIZE.1 блок, затем HIST.1 блок
  - HIST.1 остаётся advisory (exit 0 всегда)

- ✅ **Тесты** (tests/unit/scan/defect_attractor_test.cpp):
  - 11 unit-тестов покрывают:
    - fix-like pattern matching (word boundaries: "fix" matches, "prefix/suffix/fixture" не match)
    - 6 фиксов в 1 файл (12 файлов total) → 1 finding (топ-дециль)
    - 4 фикса < floor (5) → no finding
    - merge commits skipped
    - mechanical fix (>30 файлов) skipped
    - vendor paths excluded
    - test files excluded
    - результаты отсортированы по убыванию fix_touches
    - разные fix-like паттерны recognized
    - empty history → no findings
    - <10 файлов → top-1 threshold

- ✅ **CMakeLists.txt**:
  - src/scan/defect_attractor.cpp добавлен в archcheck_core library
  - tests/unit/scan/defect_attractor_test.cpp добавлен в archcheck_tests

- ✅ **Гейты**:
  - clang-format: ✅ exit 0
  - cppcheck: ✅ exit 0
  - lizard: ✅ exit 0 (NLOC ≤30, CCN ≤15, args ≤5)
  - сборка Debug: ✅ exit 0
  - тесты: ✅ 413/413 passed (11 defect_attractor тестов + все остальные)
  - dogfooding: ✅ src/, include/, tests/ — no violations

## В работе

- (пусто)

## Следующие шаги

- (нет, задача закрыта)

## Ограничения (documented)

- Spike на 200 последних коммитов (окно может быть меньше на молодых репо).
- Нет temporal анализа (всплеск за 14 дней не реализован в v1, но структура позволит добавить).
- Нет blame/ownership graph (по дизайну: cheap signal only).

## Ключевые решения

| Решение | Причина |
|---------|---------|
| History pass общий с #098 | Техника одна и та же, переиспользуется `history_query.h` |
| Простая regex classification | Cheap signal, объяснимо, нет NLP |
| Report/advisory only | Сигнал вероятностный, не gate-grade |
| `line = 0` | История агрегирует на файл-уровне, точной строки нет |
| Top-decile if <10 files → max | Моделирует то же как 90-й перцентиль при полной популяции |
| >30 файлов в fix-коммите → skip | Отсев механических (lint, format, rename) по эвристике |

## Материалы

- **Реализация**: 2 файла (h+cpp), 206 строк кода
- **Тесты**: 11 unit-тестов
- **Качество**: CCN≤15, NLOC≤30, все гейты зелёные
- **Переиспользование**: history_query, file_classification, violation-структуры
