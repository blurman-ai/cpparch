# depends-on-less-stable (SDP)

- **Category:** C (graph metrics)
- **Authority:** high — Robert C. Martin
- **Source:** Martin. *OO Design Quality Metrics* (1994); Stable Dependencies Principle. [PDF](https://linux.ime.usp.br/~joaomm/mac499/arquivos/referencias/oodmetrics.pdf)

## Rule

> "Depend in the direction of stability. A package should only depend upon packages that are more stable than it is."

## Why for cpparch

Прямой граф-чек поверх уже считаемой *Instability* `I = Ce / (Ca + Ce)` (low I = stable, high I = unstable). Если модуль X с `I=0.2` зависит от модуля Y с `I=0.8`, X тянет за собой нестабильный (часто меняющийся) Y — любое изменение Y будет каскадно перепахивать X. Это базовая ошибка слоистой архитектуры.

Не требует новых данных: `I` уже вычисляется для Martin metrics.

## Detection

Для каждой direct dependency `X → Y`:
- Вычислить `I_X` и `I_Y`.
- Если `I_X < I_Y` — флагать «SDP violation: stable X depends on less stable Y».

Опциональный порог толерантности (`I_Y - I_X > 0.2`), чтобы не шуметь на пограничных случаях.

## Fixtures

- `pass_layered/` — domain (`I≈0`) ← application (`I≈0.5`) ← ui (`I≈1`). Каждый зависит вниз — в сторону стабильности.
- `fail_inverted/` — domain зависит от ui (классический инверс layering).
- `pass_tolerance/` — оба ≈0.5, разница ниже порога, не флагается.
