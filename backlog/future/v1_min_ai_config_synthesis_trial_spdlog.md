# [AI][EVAL][V1] Пилотный прогон AI-генерации конфига на `spdlog`

**Дата создания:** 2026-05-28
**Дата старта:** —
**Статус:** new
**Модуль:** DOCS
**Приоритет:** minor
**Сложность:** S (один практический эксперимент)
**Целевой релиз:** v1 phase 1 (post-MVP)
**Блокирует:** —
**Заблокирован:** future/v1_maj_agent_config_authoring_rules.md, future/v1_maj_ai_config_synthesis_eval_protocol.md
**Related:** future/v1_maj_ai_config_synthesis_eval_protocol.md

## Цель

Проверить на одном репо: может ли агент выдать полезный `.archcheck.yml.draft` который человек
не переписывает с нуля.

## Репозиторий: spdlog

Repo: https://github.com/gabime/spdlog

Причины выбора: C++, известный, обозримый, нетривиальная структура.

Ожидаемая структура модулей для draft (observed, проверить на актуальном коммите):

```
include/spdlog/          → core (основные заголовки)
include/spdlog/sinks/    → sinks (output backends)
include/spdlog/details/  → details (low-level, не для публичного include)
include/spdlog/fmt/      → fmt (embedded fmt library)
```

Ожидаемые правила (inferred, без запуска):
- `details` — самый низкий уровень, sinks и core могут его включать
- `sinks` и `core` — не должны зависеть друг от друга (independence?)
- `fmt` — изолированная embedded lib, никто её не должен включать напрямую кроме details

Это гипотезы — агент должен их подтвердить или опровергнуть по include-графу.

## Входные артефакты для прогона

1. `find include/ -type f -name "*.h" | sort` — файловая структура
2. Include-граф (запустить archcheck или python include-scanner)
3. `head -100 README.md`
4. Системный prompt из `docs/ai_config_authoring_rules.md`

## Что зафиксировать по результату

- Конкретный `git rev-parse HEAD` spdlog на котором шёл прогон
- `.draft` от Claude — `docs/research/ai_eval/spdlog_claude_draft.yml`
- `.draft` от Codex — `docs/research/ai_eval/spdlog_codex_draft.yml`
- Заполненный human review sheet — `docs/research/ai_eval/spdlog_review.md`
- Финальный usable config после правок — `docs/research/ai_eval/spdlog_final.yml`

## Вывод по итогу (заполнить после)

```
Verdict Claude: ___
Verdict Codex: ___
Edit distance Claude: ___  Codex: ___
Главные ошибки: ___
Hypothesis: synthesis useful / not useful / useful with heavy review
```

## План выполнения

- [ ] Зафиксировать конкретный commit spdlog
- [ ] Собрать входные артефакты
- [ ] Запустить Claude по протоколу, сохранить `.draft`
- [ ] Запустить Codex по тому же протоколу, сохранить `.draft`
- [ ] Заполнить review sheet по метрикам из eval_protocol
- [ ] Написать вывод: synthesis useful / not useful / useful with heavy review

## Сделано

- (пусто)

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| docs/research/ai_eval/spdlog_claude_draft.yml | артефакт прогона Claude |
| docs/research/ai_eval/spdlog_codex_draft.yml | артефакт прогона Codex |
| docs/research/ai_eval/spdlog_review.md | сравнение и выводы |
| docs/research/ai_eval/spdlog_final.yml | финальный config после правок |
