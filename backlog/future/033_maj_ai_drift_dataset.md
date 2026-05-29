# [RESEARCH/FIXTURES] AI-assisted C++ repos as architectural drift test cases

**Дата создания:** 2026-05-28
**Дата старта:** 2026-05-29
**Статус:** in-progress
**Модуль:** RESEARCH/FIXTURES
**Приоритет:** major
**Сложность:** M (исследование ~2 дня, интеграция в fixtures — отдельно)
**Целевой релиз:** v0.2+
**Блокирует:** —
**Заблокирован:** ~~#009 (ai_drift_regression_rules)~~ ✅ DRIFT.1/DRIFT.2 готовы
**Related:** #009 (ai_drift_regression_rules), #030 (baseline_file_flag)

## Цель

Собрать и задокументировать набор реальных C++ репозиториев с верифицированными
AI-assisted коммитами как reference test cases для DRIFT.1/DRIFT.2 и валидации
archcheck в условиях реального архитектурного дрейфа.

## Контекст

AI-assisted development оставляет следы в git истории: `Co-Authored-By: Claude`,
`Generated with Claude Code`, CLAUDE.md, AGENTS.md, bot-авторы PR. Это открывает
возможность найти публичные репозитории, где конкретные изменения сделаны AI,
и запустить archcheck до/после — сравнить граф зависимостей.

Ключевая гипотеза: AI-агент часто делает правдоподобный shortcut через границу
модуля (добавляет удобный helper не в том слое, тащит include из "неправильного"
места) не создавая очевидного мусора, но накапливая структурный дрейф.

Цель задачи — **не** доказать, что AI плохой. Формулировка:
> Given an AI-assisted change set, did the dependency graph move further away
> from the project's existing architectural structure?

Это превращает реальные AI-коммиты в честный стресс-тест для DRIFT правил.

## Кандидаты (исследование 2026-05-28)

### Tier 1 — лучшие первые кандидаты

| Repo | Почему подходит | Что проверять |
|------|-----------------|---------------|
| **LibreSprite/LibreSprite** | C++ GUI app, PR с пачкой Claude Code коммитов: macOS gestures, menu search, toolbar badges, Makefile, docs | UI не лезет в app/core; macOS-specific логика не расползается за пределы platform layer; fan-in у новых helpers |
| **proximafusion/vmecpp** | C++/Python scientific, PR #359 явно помечен Claude Code, затрагивает C++ bindings, Python API, outputs, build | core numerical code не зависит от Python bindings; bindings не тащат test/demo deps; output validation не расползается в solver core |
| **PrusaSlicer AI fork PR** | Большой C++/CMake, PR с многими Claude co-authored commits: geometry, G-code, UI dialogs, CI, docs | UI dialogs не зависят от low-level geometry impl; printer-specific logic не расползается по общему G-code path |

### Tier 2 — интересные, но с оговорками

| Repo | Почему интересен | Оговорка |
|------|-----------------|----------|
| **BambuStudio PR #10794** | Большой C++/CMake, в PR явная инструкция всегда добавлять Co-Authored-By: Claude Opus 4 | Один PR — надо проверить систематичность AI workflow во всём проекте |
| **PX4/PX4-Autopilot** | Огромный safety-critical C/C++, есть CLAUDE.md с правилами для Claude | Явно запрещают AI attribution — по trailer'ам не найти. Хорош для проверки "архитектурные правила записаны, enforcement нужен" |
| **nodos-dev/sys-device** | Свежий CI run с `Co-Authored-By: Claude Opus 4.6`, CMake changes | Нишевый, надо проверить размер и структуру |
| **OpenMS/contrib** | CMake/build scripts, release с co-authored Claude | Скорее build/dependency repo, не application architecture |

### Метрики для измерения дрейфа

```
git diff <before-ai>..<after-ai> --name-only   # какие файлы тронуты
```

Метрики archcheck для before/after сравнения:
- новые include edges между top-level модулями
- новые обратные зависимости (нарушение направлений)
- рост fan-in у новых "utility" заголовков
- platform-specific includes в generic layers
- test/build/demo deps в production code
- "один коммит пересекает слишком много архитектурных зон"

### Целевой вид демонстрации (для README)

```
Before AI PR:
  ui → app → core
  platform → she

After AI PR:
  core → ui        ❌  DRIFT.1: new shortcut edge
  app → macOS      ❌  DRIFT.1: platform leak into generic layer
  gcode → dialog   ❌  DRIFT.1: upward dependency
```

### GitHub search queries (для автоматизации)

```bash
gh search commits '"Generated with Claude Code" "C++"' --limit 100
gh search commits '"Co-Authored-By: Claude" "CMakeLists"' --limit 100
gh search prs '"Generated with Claude Code" "CMakeLists.txt"' --limit 100
gh search code 'filename:CLAUDE.md "C++"' --limit 100
gh search code 'filename:AGENTS.md "C++"' --limit 100
```

Для каждого кандидата:
```bash
git log --all --grep='Claude\|Generated with\|Co-Authored-By' \
    --regexp-ignore-case --stat
```

## План выполнения

- [ ] Для каждого Tier 1 репо: клонировать, найти AI-коммиты, зафиксировать commit range
- [ ] Запустить archcheck include-scan до/после AI-коммитов (когда DRIFT.1 готов)
- [ ] Задокументировать найденные нарушения в `docs/research/ai_drift_cases.md`
- [ ] Выбрать 2-3 сценария для integration fixtures в `fixtures/drift_*/`
- [ ] Обновить README с демонстрацией на реальных данных
- [ ] Расширить поиск автоматическими GitHub queries для новых кандидатов

## Сделано

- Первичное исследование кандидатов (2026-05-28): 8 репо оценено, Tier 1/Tier 2 выделены
- **2026-05-29: первый прогон DRIFT на реальных репо.** Результаты в [docs/research/ai_drift_cases.md](../../docs/research/ai_drift_cases.md):
  - **LibreSprite PR #581** (60eed0f → 276fdbd): **1 DRIFT.1 hit** — `app/ui/toolbar.cpp -> app/pref/preferences.h` (коммит `0aa57ad` "toolbar shortcut badges"). Эталонный shortcut edge, агент протащил include из preferences-слоя в UI-виджет, который раньше с ним не общался.
  - **vmecpp PR #360** (df63271 → 5eabd51, asymmetric infra): 0 DRIFT — крупный рефактор без дрейфа.
  - **vmecpp PR #340** (b44fb7f → a7797dc, consolidate constants): 0 DRIFT — рефакторинг сделан корректно.
- Подтверждено: DRIFT-правила не дают false-positive шума на нормальном AI-коде; срабатывают именно на тех находках, под которые проектировались.

## В работе

- (пусто)

## Следующие шаги

1. ~~Подождать реализации DRIFT.1/DRIFT.2 в #009~~ ✅
2. ~~Начать с LibreSprite — наименьший из Tier 1, понятная архитектура~~ ✅ (PR #581 hit)
3. ~~Зафиксировать конкретные commit SHA для before/after в этом файле~~ ✅ (в docs/research/ai_drift_cases.md)
4. Превратить LibreSprite-hit в минимальный fixture `fixtures/drift_real_world/libresprite_pr581/`
5. Прогнать PrusaSlicer AI fork PR — третий Tier 1 кейс
6. Добавить 2-3 Tier 2 кейса (BambuStudio #10794, nodos-dev/sys-device)
7. Когда корпус ≥ 5 PR — обновить README демонстрацией на реальных данных

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Нейтральная формулировка ("stress test", не "AI плохой") | Научная честность; задача — валидация инструмента, не обвинение |
| Tier 1 — проекты с явным AI attribution в коммитах | Чёткий before/after по SHA без ambiguity |
| PX4 в Tier 2, не Tier 1 | Запрещают attribution — AI-коммиты не идентифицируемы по git |
| Сначала Tier 1 как fixtures, Tier 2 как open-ended research | Fixtures нужны для CI; Tier 2 лучше для paper/blog |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| docs/research/ai_drift_cases.md | будущая документация findings |
| fixtures/drift_*/real_world/ | будущие integration fixtures из реальных реп |
