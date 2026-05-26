# stable-but-concrete (SAP)

- **Category:** C (graph metrics)
- **Authority:** high — Robert C. Martin
- **Source:** Martin. *OO Design Quality Metrics* (1994); Stable Abstractions Principle. [PDF](https://linux.ime.usp.br/~joaomm/mac499/arquivos/referencias/oodmetrics.pdf)

## Rule

> "A package should be as abstract as it is stable. Stable packages should be abstract (so they don't lock in concrete implementation choices)."

## Why for cpparch

«Зона боли»: модуль низко-`I` (стабильный) + низко-`A` (конкретный). Стабильный модуль уже трудно менять, а если он ещё и конкретный (полно implementation classes, мало pure abstract) — он буквально цементирует реализацию, от которой все зависят. Это самая дорогая ошибка из всех Martin metrics.

Не требует новых данных: `I` и `A` уже считаются.

## Detection

Для каждого модуля:
- `pain = (1 - I) * (1 - A)`.
- Если `pain > 0.6` (настраиваемо) — флагать «SAP violation: stable + concrete (zone of pain)».

Дополнительно: рассчитывать `D = |A + I - 1|` (distance from main sequence) и репортить топ-N модулей с худшим D — это уже стандартная Martin-метрика, но stable-but-concrete даёт более понятную в репорте формулировку причины.

## Fixtures

- `pass_main_sequence/` — модули с `D < 0.3`.
- `fail_zone_of_pain/` — модуль `I=0.1`, `A=0.1` — стабильный и конкретный.
- `pass_zone_of_uselessness/` — модуль `I=0.9`, `A=0.9` — нестабильный и абстрактный. Тоже не идеал, но другая боль; репортится отдельным сообщением, без `fail`.
