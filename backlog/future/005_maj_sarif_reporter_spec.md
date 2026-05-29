# [REPORT] Спецификация SARIF reporter

**Дата создания:** 2026-05-26
**Дата старта:** —
**Статус:** new
**Модуль:** REPORT
**Приоритет:** major
**Сложность:** S (полдня на спеку, реализация отдельной таской)
**Целевой релиз:** v0.2+ (SARIF output появляется в v0.2 per roadmap)
**Блокирует:** реализация sarif_reporter (отдельной таской позже)
**Заблокирован:** —
**Related:** #001 (dogfood_static_analyzers — текущий strict report тоже может перейти на SARIF), #6 (gh — audit Issue 7: фаза неоднозначна)

## Цель

Зафиксировать **до написания кода reporter'а** маппинг внутренних понятий archcheck (правила, нарушения, источники: Core Guidelines / Lakos / Martin) в SARIF 2.1.0, чтобы не переделывать формат после первого релиза.

## Контекст

SARIF 2.1.0 — OASIS-стандарт, JSON, понимают GitHub Code Scanning, SonarQube, GitLab, VS Code расширение `MS-SarifVSCode.sarif-viewer`. Это «лингва-франка» для интеграции с экосистемой партнёров (clang-tidy, cppcheck, GCC `-fanalyzer` уже умеют SARIF). Заложить с v0.1.

Спека: https://docs.oasis-open.org/sarif/sarif/v2.1.0/sarif-v2.1.0.html

## План выполнения

- [ ] **Решить целевую фазу.** Аудит #6 Issue 7: roadmap (`architecture-spec.md` line 656) кладёт SARIF в v0.2, а эта задача лежит в `future/` (по конвенции = v0.3+). Параллельно `tests` декларация «SARIF с v0.1» в «Ключевых решениях» ниже — третий вариант. Решение принять явно: v0.2 → переместить файл в `new/` и завести парную implementation-таску; v0.3+ → обновить roadmap в спеке. Должна остаться ровно одна формулировка.
- [ ] Решить формат `ruleId`: предложение — `archcheck/core-guidelines/SF.7`, `archcheck/lakos/no-cycles`, `archcheck/martin/instability`. Авторитет = namespace, имя правила = последний сегмент
- [ ] `tool.driver.name` = `archcheck`, `version` = из CMake
- [ ] `rules[]` секция: для каждого правила — `id`, `name`, `shortDescription`, `fullDescription`, `helpUri` (ссылка на Core Guidelines / Lakos главу), `defaultConfiguration.level`
- [ ] `results[].level`: маппинг наших severity → SARIF (`error` / `warning` / `note`)
- [ ] `results[].locations[]`: file/line/column из `file:line:column` контракта в спеке
- [ ] Граф-нарушения (cycle) — как `relatedLocations[]`, все участники цикла
- [ ] Метрики (CCD/ACD/NCCD) — **не через results**, а через `properties` на уровне run (или отдельный `invocations[].properties`) — обсудить
- [ ] Baseline режим — маппинг на SARIF `baselineState` (`unchanged` / `new` / `absent`)
- [ ] Положить в `docs/sarif-mapping.md` финальную спеку с примерами JSON
- [ ] Подготовить 2-3 reference-файла (`tests/sarif/expected_*.json`) до реализации

## Сделано
- (пусто)

## В работе
- (пусто)

## Следующие шаги

1. Прочитать SARIF 2.1.0 spec по разделам (rules, results, locations, baseline)
2. Посмотреть как clang-tidy и cppcheck сериализуют — взять как образец
3. Написать `docs/sarif-mapping.md`

## Ключевые решения

| Решение | Причина |
|---------|---------|
| SARIF с v0.1 | бесплатный билет в GitHub Code Scanning + SonarQube |
| ruleId с namespace по авторитету | сохраняет позиционирование "authority-cited rules" в выходе |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| docs/sarif-mapping.md | новая спека |
| tests/sarif/expected_*.json | референс-выходы |
