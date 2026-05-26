# no-foreign-forward-decls

- **Category:** B (AST / semantic)
- **Authority:** medium — Google C++ Style Guide
- **Source:** [Google C++ Style Guide — Forward Declarations](https://google.github.io/styleguide/cppguide.html#Forward_Declarations)

## Rule

> "Avoid using forward declarations where possible. Instead, include the headers you need. Do not use forward declarations of entities your project does not own."

## Why for cpparch

Forward-declaration чужого типа — это обход include-графа. Модуль формально не зависит от чужого `.h`, но реально использует чужой тип. Если чужой проект изменит layout / namespace / шаблонные параметры — наш код тихо сломается на link или сделает UB. Это особенно опасно для типов из `std::` (формальный UB по `[namespace.std]` — см. [forward-decl-of-std](forward-decl-of-std.md)).

## Detection

Для каждой forward-declaration (`class Foo;` или `struct Foo;` без определения в TU):
1. Определить «свой» namespace проекта (по конфигу: `project_namespace: cpparch::`).
2. Если объявляемый тип не принадлежит project namespace — флагать.

## Fixtures

- `pass/` — `class cpparch::ComponentGraph;` (свой namespace).
- `fail_foreign/` — `class llvm::raw_ostream;` в нашем `.h`.
- `fail_std/` — `namespace std { class string; }` — отдельно флагать как UB.
