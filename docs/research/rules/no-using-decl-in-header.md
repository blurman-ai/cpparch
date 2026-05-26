# no-using-decl-in-header

- **Category:** B (AST / semantic)
- **Authority:** medium — HIC++ 4.0
- **Source:** [HIC++ 4.0, rule 7.3.6](https://abougouffa.github.io/awesome-coding-standards/hi-cpp-4.0.pdf)

## Rule

> "Do not write `using` declarations at namespace scope in a header."

## Why for cpparch

Дополняет SF.7 (`using namespace`). SF.7 ловит «directive» (`using namespace std;`), это правило ловит «declaration» (`using std::vector;`) — она тоже инжектит имя в namespace заголовка, тоже протекает во все TU, которые `#include` этот заголовок. Каждый кейс по отдельности кажется безобидным, в сумме создаёт name pollution.

## Detection

AST: `UsingDecl` в namespace scope в файле, который входит в include-граф (т.е. в header или в файле, на который кто-то `#include`-ает). Допустимо:
- внутри class body (это alias для наследования),
- внутри function body,
- в `namespace detail { ... }` если конфиг проекта разрешает (под флагом).

## Fixtures

- `pass_class_using/` — `class Derived : Base { using Base::method; };`.
- `pass_function_local/` — `void f() { using std::vector; ... }`.
- `fail_namespace_scope/` — в `foo.h`: `using std::vector;` на уровне namespace.
