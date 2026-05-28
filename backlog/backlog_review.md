# Backlog Review — 2026-05-28 (обновлено 2026-05-28: MVP readiness + real project)

> **Фокус-вопрос:** что осталось до MVP и когда можно запустить на реальном OS-проекте?
> Оба режима: статический анализ кода (`archcheck [path]`) и анализ коммитов (`archcheck --diff`).

## TL;DR

**MVP функционально завершён.** `archcheck 0.1.0` собирается, 173/173 тестов, все 5 дефолтных правил работают, `--diff` для commit-анализа работает. **Запустить на реальном проекте можно сегодня.** До официального "MVP done" остаётся: закрыть #025 (smoke-test, ~1ч) + #027 (branch coverage → 90%, ~2-3 дня). Итого: **2-3 рабочих дня.**

---

## Режим 1 — статический анализ: `archcheck [path]`

**Что работает сегодня:**
- SF.7 (using namespace в заголовках), SF.8 (include guard), SF.9 (циклы в include-графе)
- Lakos.GodHeader (fan-in > 30), Lakos.ChainLength (глубина > 10)
- Text + JSON output, exit codes 0/1/2
- Include resolution через **suffix-matching** — без `compile_commands.json` покрывает ~80-90% реальных `#include`; системные хедеры помечаются `external` и не участвуют в графе (ложных позитивов нет)

**Главный пробел для legacy-кода:**
В CLI нет `--baseline <file>` для подавления уже существующих нарушений. Первый запуск на legacy-проекте покажет всё сразу — может быть overwhelming. *Обходной путь:* `--diff main..HEAD` (режим 2) показывает только новые нарушения — достаточно для CI.

**Попробовать прямо сейчас:**
```bash
archcheck --graph /path/to/oss-project   # nodes/edges/scc статистика
archcheck /path/to/oss-project           # полный check всех правил
archcheck --format json /path/to/project # для CI-интеграции
```

---

## Режим 2 — анализ коммитов: `archcheck --diff`

**Что работает сегодня:**
- Строит граф из git-объектов in-memory (без worktree на диске)
- Сравнивает до/после → показывает только **новые** dependency-рёбра
- `WORKTREE` ref — включает uncommitted changes
- Merge-commit поддержан, fast-path если нет C++ изменений

**Типовые use-cases:**
```bash
# Новые рёбра за последний коммит
archcheck --diff HEAD~1..HEAD /path/to/project

# Регрессии в PR (CI)
archcheck --diff main..feature-branch /path/to/project

# Найти какой коммит ухудшил архитектуру — shell-оркестровка поверх инструмента
git log --format=%H main..feature | while read sha; do
  echo "=== $sha ===" && archcheck --diff "${sha}~1..${sha}" /path
done
```

**Пробел:** `--diff` показывает новые рёбра, но не скаляр "насколько сложнее стало". Для ранжирования коммитов по сложности можно использовать `--graph` (выдаёт scc_cyclic, largest_scc) — нужна внешняя оркестровка. Это осознанный scope, не баг.

---

## Пробелы vs MVP.md acceptance criteria

| Пункт | Состояние |
|-------|-----------|
| compile_commands.json support | ❌ нет (suffix-match — работает для большинства проектов) |
| Module mapping + YAML rules | ❌ нет (→ v0.2, решение задокументировано) |
| `archcheck init` command | ❌ нет (не приоритет v0.1) |
| `--baseline <file>` для check-режима | ❌ нет в CLI (модуль в коде есть, CLI не подключён) |
| Cycle detection | ✅ SF.9 |
| Text + JSON output | ✅ |
| Exit codes 0/1/2/3 | ✅ |
| Fixtures для всех правил | ✅ (5 наборов pass/fail) |
| CI workflow | ✅ (example + PR comment) |

---

Всего задач: **2 WIP + 7 future** + **28 завершённых**.

---

## Протухшие (сделано, но осталось в wip/)

| Файл | Причина | Рекомендация |
|------|---------|--------------|
| `wip/018_crt_git_diff_analysis.md` | Все чекбоксы закрыты, «Сделано» полное, код в коммите `54911d0` | **Move to completed** — немедленно |
| `wip/022_min_diff_merge_commit_coverage.md` | Все чекбоксы закрыты, «Сделано» полное, коммит `f6771b3` | **Move to completed** — немедленно |
| `wip/023_maj_diff_skip_when_no_cpp_changes.md` | Все чекбоксы закрыты, `changedCppFiles` есть в коде (grep подтверждён), код в коммите `54911d0` | **Move to completed** — немедленно |

Примечание по #023: код был включён в коммит `54911d0` (#018, #024) — имя задачи не упомянуто явно, но функциональность подтверждена grep-ом.

---

## Быстрые победы (quick win)

| Файл | Цель | Модуль | Оценка |
|------|------|--------|--------|
| `wip/025_maj_pr_comment_integration.md` | Sticky PR-comment + Step Summary + merge_group в example workflow | CI, DOC | S (~1-2 часа) |

Задача хорошо спланирована, не требует изменений продакшен-кода (только YAML workflow + docs). Прямой следующий шаг.

---

## Средние задачи

_Нет активных средних задач — всё либо quick win, либо future._

---

## Сложные / заблокированные (future/)

| Файл | Блокер / Горизонт |
|------|-------------------|
| `future/005_maj_sarif_reporter_spec.md` | v0.2+; нет реализации violations/reporter ещё |
| `future/009_maj_ai_drift_regression_rules.md` | v0.3+; требует детального дизайна и rules-framework |
| `future/010_maj_ai_rule_synthesis_contract.md` | v0.3+; парная к #009 (synthesis ↔ drift) |

Все три корректно лежат в future/ — трогать не нужно.

---

## Без анализа (нужно исследование)

_Нет — все активные задачи имеют полноценный контекст и план._

---

## Дубли / связанные

| Файлы | Предложение |
|-------|-------------|
| `future/009` и `future/010` | Сознательная пара (synthesis ↔ drift regression), **не** дубли. Держать как есть, добавить `Related:` уже прописан в обоих. |

---

## Следующие действия (приоритет)

1. **Сейчас**: переместить `018`, `022`, `023` → `completed/` (5 мин).
2. **Далее**: взять `#025` (PR-comment UX) — готовый план, 1-2 часа.
3. **Future**: `005` SARIF spec разблокируется когда появится rules-framework (v0.2).
