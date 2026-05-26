# [BUILD] Dogfood: cppcheck + clang-tidy + lizard в CI

**Дата создания:** 2026-05-26
**Дата старта:** —
**Статус:** new
**Модуль:** BUILD
**Приоритет:** major
**Сложность:** S (день)
**Блокирует:** —
**Заблокирован:** github_actions_ci
**Related:** sarif_reporter_spec

## Цель

Не дать коду cpparch гнить — в каждом PR прогонять три дешёвые внешние тулзы поверх clang-tidy: **cppcheck**, **lizard**, и второй проход clang-tidy с расширенным набором. Полный отчёт от cpparch на самом себе — отдельной таской, когда правила появятся.

## Контекст

Из исследования 2026-05-26: cppcheck ловит то, что clang-tidy пропускает (UB, утечки на другом анализаторе), lizard — пороги сложности (CCN ≤ 15, функция ≤ 30 строк — совпадает с `docs/code_quality.md`). Оба бесплатные, быстрые, низкий шум. scan-build и `-fanalyzer` — позже, в nightly.

## План выполнения

- [ ] Отдельный job `static-analysis` в `ci.yml` (или отдельный workflow), параллельно сборке
- [ ] **cppcheck:** `cppcheck --enable=warning,performance,portability --inline-suppr --error-exitcode=1 src/` — падать на находках
- [ ] **lizard:** `lizard src/ --CCN 15 --length 30 --arguments 5 --warnings_only` — падать на находках (пороги взять из `docs/code_quality.md`)
- [ ] **clang-tidy второй проход** с расширенным `.clang-tidy-strict` (cert-*, misc-*) — пока **warning-only**, не падать; копить базлайн
- [ ] Сохранять отчёты как artifacts на 14 дней
- [ ] **Не** включать IWYU — мы сами в этой нише
- [ ] **Не** включать flawfinder — устаревает, для нас почти всё мимо

## Сделано
- (пусто)

## В работе
- (пусто)

## Следующие шаги

1. cppcheck job — самый дешёвый, начать с него
2. lizard job
3. clang-tidy strict как warning-only

## Ключевые решения

| Решение | Причина |
|---------|---------|
| cppcheck + lizard как gate, clang-tidy strict как warning | Минимум шума на старте, иначе бросим |
| scan-build/-fanalyzer — позже | Дороже по времени, в nightly когда стабилизируется CI |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| .github/workflows/ci.yml | job static-analysis |
| .clang-tidy-strict | новый |
