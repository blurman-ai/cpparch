# no-complex-global-init

- **Category:** B (AST / semantic)
- **Authority:** high — C++ Core Guidelines
- **Source:** [CCG I.22 — Avoid complex initialization of global objects](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#i22-avoid-complex-initialization-of-global-objects)

## Rule

> "Complex initialization can lead to undefined order of execution."

## Why for cpparch

Прямой путь к static initialization order fiasco. Это не баг логики — это архитектурный пах: глобал зависит от другого глобала из соседнего модуля, и порядок инициализации непредсказуем между TU. Часто проявляется как «то работает, то падает на старте» и почти всегда хорошо ложится в межмодульные баги.

## Detection

AST-проход: глобал с инициализатором, который:
- вызывает не-`constexpr` функцию, или
- читает другой глобал, или
- выделяет память (`new`, контейнерные операции).

Допустимы: literal init, `constexpr` constructor, `{}`-агрегатная инициализация POD.

## Fixtures

- `pass/` — `inline constexpr int kMax = 42;`, `inline const std::string_view kName = "x";`.
- `fail_function_call/` — `inline std::vector<int> g_data = ComputeData();`.
- `fail_cross_global/` — `inline int g_b = g_a + 1;` где `g_a` объявлен в другом TU.
