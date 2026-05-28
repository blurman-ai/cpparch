# [AI][CONFIG][V1] Правила для агента, который заполняет `.archcheck.yml.draft`

**Дата создания:** 2026-05-28
**Дата старта:** —
**Статус:** new
**Модуль:** DOCS, CONFIG
**Приоритет:** major
**Сложность:** M (контракт + примеры + anti-slop guardrails)
**Целевой релиз:** v1 phase 1 (post-MVP)
**Блокирует:** практическое использование AI synthesis без ручного написания конфига
**Заблокирован:** future/v1_maj_config_format_minimal_contract.md
**Related:** future/v1_maj_ai_config_synthesis_eval_protocol.md, future/010_maj_ai_rule_synthesis_contract.md

## Цель

Зафиксировать правила, по которым агент заполняет `.archcheck.yml.draft` — не выдумывая
архитектуру и не маскируя догадки под факты. Агент пишет только `.draft`, не финальный config.

## Целевой формат вывода агента

Агент заполняет `.archcheck.yml.draft` в схеме из `v1_maj_config_format_minimal_contract.md`.
Приоритет типов правил:
1. **`layers`** — если есть directional hierarchy — использовать его, не раскрывать в N×(N-1) пар `forbidden`.
2. **`independence`** — если модули одного уровня не должны знать друг о друге.
3. **`forbidden`** — точечные запреты которые не вписываются в layers.

Каждое правило и каждый модуль — YAML-комментарий с источником (`observed` / `inferred` / `speculative`).

## Разрешённые источники истины

- Реальная файловая структура (пути, имена директорий)
- Include-граф (кто реально включает кого)
- README / ARCHITECTURE.md если есть
- Явные naming conventions (префиксы, суффиксы)

## Запрещённое поведение агента

- Выдумывать слои которых нет в репо (нет `domain/` → не писать `domain` module)
- Переносить архитектурные паттерны из других проектов без evidence
- Делать `forbidden` на слабом основании (`speculative` → только `# TODO`, не правило)
- Скрывать неуверенность: все `speculative` поля должны быть явно помечены

## Уровни уверенности

| Уровень | Когда | В .draft |
|---------|-------|----------|
| `observed` | Прямо видно в файловой структуре или include-графе | Пишем как правило |
| `inferred` | Следует из naming/README, не подтверждено графом | Правило + `# inferred` |
| `speculative` | Common pattern, нет evidence в репо | `# TODO` комментарий, не правило |

## Что человек обязан проверить перед accept

- Каждый `modules.*.paths` — реально ли файлы существуют
- Каждое `layers` / `independence` правило — не нарушает ли существующий код
- Все `# inferred` и `# speculative` комментарии — подтвердить или удалить

## Пример хорошего draft

```yaml
version: 1

modules:
  core:
    paths: ["include/spdlog/**"]            # observed: directory exists
  sinks:
    paths: ["include/spdlog/sinks/**"]      # observed: directory exists
  details:
    paths: ["include/spdlog/details/**"]    # observed: directory exists

rules:
  - type: layers
    name: spdlog-layering
    layers: [sinks, core, details]          # inferred: sinks include core, details is low-level
    # TODO: verify sinks do not include details directly

  - type: independence
    name: sinks-independent
    modules: [sinks]                        # speculative: typical pattern, not verified
```

## Пример плохого draft (запрещён)

```yaml
# НЕ ДЕЛАТЬ ТАК — нет директорий в репо:
modules:
  presentation:
    paths: ["src/presentation/**"]    # BAD: не существует
  business_logic:
    paths: ["src/business/**"]        # BAD: выдумано

rules:
  - type: forbidden
    name: no-circular                 # BAD: слишком широко, без evidence
    from: [business_logic]
    to: [presentation]
```

## План выполнения

- [ ] Написать `docs/ai_config_authoring_rules.md`: allowed sources, forbidden behavior, уровни уверенности
- [ ] Добавить 2 полных примера: good draft и bad draft с аннотациями
- [ ] Зафиксировать минимальный output contract: что должно быть в каждом поле `.draft`
- [ ] Описать checklist для человека перед accept
- [ ] Связать с `synthesize`-command design когда он появится

## Сделано

- (пусто)

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| docs/ai_config_authoring_rules.md | контракт для агента |
| README.md | краткое объяснение `.draft` workflow после стабилизации |
