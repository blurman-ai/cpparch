# [AI][EVAL][V1] Протокол оценки AI-генерации конфига на Claude и Codex

**Дата создания:** 2026-05-28
**Дата старта:** —
**Статус:** new
**Модуль:** DOCS
**Приоритет:** major
**Сложность:** M (методика, критерии, шаблоны отчёта)
**Целевой релиз:** v1 phase 1 (post-MVP)
**Блокирует:** осмысленную проверку гипотезы "AI снижает барьер входа в config mode"
**Заблокирован:** future/v1_maj_agent_config_authoring_rules.md
**Related:** future/v1_min_ai_config_synthesis_trial_spdlog.md, future/010_maj_ai_rule_synthesis_contract.md

## Цель

Воспроизводимый протокол сравнения Claude и Codex в задаче создания `.archcheck.yml.draft`.
Мерим практическую полезность draft, не "красоту YAML".

## Единый входной набор для обеих моделей

1. Файловая структура репо (`find src/ include/ -type f | head -200`)
2. Include-граф в текстовом виде (archcheck --format json или аналог)
3. README / ARCHITECTURE.md если есть (первые 100 строк)
4. Системный prompt из `docs/ai_config_authoring_rules.md` (одинаковый для обеих)

Контекстный бюджет: ≤ 8k токенов входа. Если репо больше — обрезать по правилу `head -200`.

## Метрики (считать для каждого прогона)

| Метрика | Как считать |
|---------|-------------|
| **Structural correctness** | Все `modules.*.paths` — реально существующие директории (0/1 per module) |
| **Rule type quality** | Использованы ли `layers`/`independence` вместо flat `forbidden`? (`layers_used`: yes/no) |
| **False boundaries** | Количество правил `forbidden`/`layers` которые нарушает существующий код |
| **Edit distance** | Количество изменённых строк до первого usable конфига (считать git diff) |
| **Confidence marking** | Все `speculative` помечены? (yes / partial / no) |
| **Stale entries** | Количество `# TODO` которые человек удалил как бессмысленные |

Pass threshold: `structural correctness` = 100%, `false boundaries` ≤ 2, `edit distance` ≤ 30%.

## Правила fair comparison

- Один и тот же репозиторий, один и тот же git commit
- Один и тот же системный prompt (из `docs/ai_config_authoring_rules.md`)
- Один и тот же входной набор артефактов
- Прогон сохраняется как-есть, без доработки prompt-а под конкретную модель

## Human review sheet (заполнять для каждого прогона)

```
Repo: ___________  Commit: ___________  Model: ___________  Date: ___________

Modules declared: ___   Modules correct: ___   False modules: ___
Rules declared: ___   Rule types used: [ ] layers  [ ] independence  [ ] forbidden only
False boundaries (violations in existing code): ___
Edit distance to usable (lines changed): ___
Confidence marking: [ ] full  [ ] partial  [ ] missing
Stale TODO entries removed: ___

Verdict: [ ] useful as-is  [ ] useful after minor edits  [ ] heavy rewrite needed  [ ] useless
```

## Что считается "хорошим" draft (критерии по референсам)

- Модули соответствуют реальным директориям (как Deptrac `paths:`)
- Hierarchy выражена через `layers`, а не flat `forbidden` (как Import Linter `layers` type)
- Нет выдуманных слоёв (anti-pattern из study ArchUnit vs flat config tools)
- Каждая неочевидная граница помечена как `inferred` или `speculative`

## План выполнения

- [ ] Принять `docs/ai_config_authoring_rules.md` как prerequisite
- [ ] Зафиксировать единый prompt contract в `docs/ai_config_eval_protocol.md`
- [ ] Зафиксировать human review sheet в `docs/ai_config_eval_template.md`
- [ ] Выбрать первый репозиторий (кандидат: spdlog) и провести пилот
- [ ] После пилота: упростить или ужесточить метрики по результату

## Сделано

- (пусто)

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| docs/ai_config_eval_protocol.md | методика оценки |
| docs/ai_config_eval_template.md | шаблон отчёта по прогону |
