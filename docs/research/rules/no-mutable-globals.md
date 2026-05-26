# no-mutable-globals

- **Category:** B (AST / semantic)
- **Authority:** high — C++ Core Guidelines
- **Source:** [CCG I.2 — Avoid non-`const` global variables](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#i2-avoid-non-const-global-variables)
- **Equivalent:** clang-tidy `cppcoreguidelines-avoid-non-const-global-variables`

## Rule

> "Non-`const` global variables hide dependencies and make them subject to unpredictable changes."

## Why for cpparch

Глобальное мутабельное состояние ломает модульность: любой модуль может его читать/писать, и dependency-граф врёт — формально зависимости нет, фактически она есть через глобал. Прямой архитектурный дефект, не стиль.

## Detection

AST-проход: `VarDecl` в namespace или file scope (не `function-local`, не `class member`), тип не `const`-qualified, не `constexpr`/`constinit`. Игнорировать `inline constexpr` константы — они допустимы.

## Fixtures

- `pass/` — только `const` / `constexpr` / `constinit` глобалы.
- `fail_mutable/` — `int counter = 0;` в namespace scope.
- `fail_static_storage/` — `static int counter;` в `.cpp` (тоже флагать или нет — решить при реализации).
