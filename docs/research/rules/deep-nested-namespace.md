# deep-nested-namespace

- **Category:** A (preprocessor scan) или D (text pattern)
- **Authority:** medium — Google C++ Style Guide + общая практика
- **Source:** [Google C++ Style Guide — Namespaces](https://google.github.io/styleguide/cppguide.html#Namespaces); Lakos *LSCSD* §2.4 (физический пакеджинг).

## Rule

> "Глубина вложенности namespace > 4 уровней — флаг."

## Why for cpparch

Простой sprawl-детектор. `cpparch::rules::core_guidelines::sf::detail::` — пять уровней — это сигнал, что иерархия пакетов вышла из-под контроля или что вместо физического разделения (через директории / компоненты) пытаются делать логическое через nested namespace. Не глубокий архитектурный смысл, но дёшево и ловит реальный анти-паттерн.

## Detection

Preprocessor scan: для каждого файла отследить максимальную глубину вложенности `namespace X { namespace Y { ... } }`. Учитывать compact form C++17 (`namespace X::Y::Z {`) как глубину 3. Если > порога (default 4) — флагать. Порог настраиваемый.

Игнорировать anonymous namespace при подсчёте (он структурный, не семантический).

## Fixtures

- `pass_shallow/` — `namespace cpparch::rules { ... }`.
- `pass_compact_form/` — `namespace cpparch::rules::lakos { ... }` (3 уровня).
- `fail_deep/` — `namespace a::b::c::d::e::f { ... }` (6 уровней).
- `fail_traditional/` — пять вложенных `namespace X {` блоков.
