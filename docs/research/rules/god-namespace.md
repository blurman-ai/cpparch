# god-namespace

- **Category:** C (graph metrics)
- **Authority:** medium — расширение Lakos fan-in + Martin Ce/Ca
- **Source:** Lakos *LSCSD* §3.10 (fan-in/fan-out), Martin OODQM (Ce/Ca). Конкретное правило — наша агрегация.

## Rule

> "Namespace с afferent coupling (Ca) > 50 ИЛИ efferent coupling (Ce) > 30 — кандидат на расщепление."

## Why for cpparch

god-headers уже в спеке (in-degree > 30 — split). Это правило поднимает идею на уровень неймспейса: «бог-namespace» — это пакет, от которого зависят все (`Ca` зашкаливает) или который зависит от всех (`Ce` зашкаливает). Первое означает «слишком много чужого кода привязано к этому слою» (хрупкий стабильный модуль), второе — «слой не умеет работать без всех остальных» (модуль-агрегатор). Оба — архитектурные smell-ы, ловимые с помощью уже считаемых метрик.

Не требует новых данных: Ce/Ca уже считаются для Martin metrics.

## Detection

После построения namespace-уровневого графа:
- Для каждого namespace вычислить `Ce` и `Ca`.
- Флагать если `Ca > threshold_ca` (default 50) **или** `Ce > threshold_ce` (default 30).
- Оба порога настраиваемы.

## Fixtures

- `pass_balanced/` — namespace с `Ca=10, Ce=5`.
- `fail_high_ca/` — `utils::` с `Ca=80` (все зависят).
- `fail_high_ce/` — `application::` с `Ce=40` (зависит от всего).
