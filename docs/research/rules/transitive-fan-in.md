# transitive-fan-in

- **Category:** C (graph metrics)
- **Authority:** high — Lakos
- **Source:** Lakos. *Large-Scale C++ Software Design* §3.10; Component Dependency (CD).

## Rule

> "Component с transitive fan-in > 50% от размера системы — флаг."

## Why for cpparch

Дополняет god-headers (direct fan-in) и СD-метрику (own CD). Если компонент входит транзитивно в > половину всех компонентов проекта, любая его правка запускает rebuild половины системы — и любой баг в нём задевает половину системы. Это раннее предупреждение про rebuild-storm и про over-coupled common code. Прямо вычисляется по уже построенному графу (transitive closure → in-degree).

## Detection

После построения component-графа:
1. Вычислить transitive closure (reverse reachability) для каждого узла.
2. Для каждого компонента `C` посчитать `transitive_fan_in(C) = |{X : X транзитивно зависит от C}|`.
3. Если `transitive_fan_in(C) / N > 0.5` (где N — общее число компонентов) — флагать. Порог настраиваемый.

Стоит репортить топ-5 по transitive fan-in даже без преодоления порога — это полезная метрика для understanding.

## Fixtures

- `pass_balanced/` — 20 компонентов, максимум transitive_fan_in = 6 (30%).
- `fail_god_component/` — 20 компонентов, `core.h` транзитивно тянут 15 (75%).
- `pass_small_project/` — 5 компонентов: 50% — это 2-3 компонента, что нормально для маленького проекта (правило отключается при `N < min_N`, default 10).
