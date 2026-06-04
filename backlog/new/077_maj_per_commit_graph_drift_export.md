# [RESEARCH][DIFF] Per-commit export изменений include-графа для ручной верификации drift-кейсов

**Дата создания:** 2026-06-02
**Дата старта:** —
**Статус:** new
**Модуль:** EXPERIMENTS / DIFF / RESEARCH
**Приоритет:** major
**Сложность:** M
**Блокирует:** zero-config sibling-drift validation
**Заблокирован:** —
**Related:** #018 (git_diff_analysis), #054 (ai_repo_duplication_run), #075 (mvp_v1_trusted_diff_workflow), #076 (new_cross_area_dependency_drift)

## Цель

Сделать воспроизводимый export **по всем коммитам истории репозитория**, который
сохраняет изменения include-графа на уровне commit-to-parent diff. Нужен не новый
product surface, а исследовательский материал для ответа на вопрос:

> “Есть ли в реальных AI-heavy репах коммиты, которые протягивают новую прямую
> связь между соседними модулями / зонами?”

## Контекст

Сейчас `archcheck --diff` умеет сравнить **одну** ревизию с другой, но у нас нет
готового режима, который:

- проходит по истории;
- сохраняет summary по каждому коммиту;
- отдельно выделяет коммиты, где появились новые зависимости;
- даёт компактный файл для ручного eyeballing.

Аналогичный паттерн уже есть в duplication-research:

- `partial_history_drift.sh`
- `generate_per_commit.py`
- `per_commit_hits.{jsonl,md}`

Для graph drift нужен такой же слой, но поверх `archcheck --diff`.

## Что сделать

### 1. Добавить per-commit graph exporter

- [ ] Скрипт в `experiments/ai_repo_run/`, который идёт по `git rev-list --reverse --first-parent HEAD`.
- [ ] Для каждого коммита с родителем запускает `archcheck --diff <sha>~1..<sha> .`.
- [ ] Сохраняет summary-метрики:
      `added_edges`, `removed_edges`, `grown_cycles`, `new_area_deps`.

### 2. Сохранять кандидатов для ручной проверки

- [ ] Для коммитов с любым drift-сигналом сохранять отдельную запись.
- [ ] В запись включать:
      `sha`, `date`, `subject`, summary-метрики, список `added`-рёбер,
      секцию `new_cross_area_dependencies` если есть.
- [ ] Сделать человекочитаемый `.md` рядом с `.jsonl`.

### 3. Не смешивать с MVP CLI

- [ ] Не добавлять новый CLI-режим в `src/main.cpp`.
- [ ] Не менять exit-контракты продукта.
- [ ] Держать это в `experiments/` как инструмент исследования и валидации.

## Критерий приёмки

- [ ] Есть reproducible-скрипт, который экспортирует per-commit graph drift series.
- [ ] Есть machine-readable output (`jsonl`/`tsv`) и короткий human-readable digest (`md`).
- [ ] Можно быстро выделить коммиты, где появились новые зависимости/циклы/area-pairs.
- [ ] Скрипт проверен хотя бы на одной локальной репе.

## Как использовать результат

1. Прогнать exporter на AI-heavy локальной репе.
2. Отфильтровать коммиты с `new_area_deps > 0` или большим `added_edges`.
3. Руками открыть только эти кандидаты через `git show`.
4. По живым кейсам решить, какой zero-config sibling/module drift сигнал имеет смысл.

## Не делать в этой задаче

- не пытаться сразу угадать SOLID/Lakos violation автоматически;
- не тащить config rules;
- не менять продуктовый `--diff` surface;
- не смешивать с duplication export.
