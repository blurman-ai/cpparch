# no-using-namespace-in-cpp

- **Category:** B (AST / semantic) — простой случай также A (текстовый скан)
- **Authority:** medium — Google C++ Style Guide
- **Source:** [Google C++ Style Guide — Namespaces / Unnamed Namespaces and Static Variables](https://google.github.io/styleguide/cppguide.html#Namespaces)

## Rule

> "You may not use a `using`-directive to make all names from a namespace available. Do not use namespace aliases at namespace scope in header files except in explicitly marked internal-only namespaces."

## Why for cpparch

SF.7 запрещает `using namespace` только в заголовках. Это правило — stricter mode, опциональный flag для проектов с высокой дисциплиной: `using namespace` запрещён везде, включая `.cpp`. Польза: имена не «протекают» в TU и читатель видит чёткий префикс, откуда символ. Опционально, не дефолт.

## Detection

AST: `UsingDirectiveDecl` в namespace scope в любом файле проекта. Допустимы:
- внутри function body (локальный scope),
- в `namespace { ... }` (anonymous namespace в `.cpp`) — реально не «протекает» наружу.

## Fixtures

- `pass_function_local/` — `void f() { using namespace std::chrono_literals; ... }`.
- `pass_anonymous/` — `namespace { using namespace std::ranges; }` в `.cpp`.
- `fail_file_scope/` — `using namespace std;` в начале `.cpp`.
- `fail_in_namespace/` — `namespace cpparch { using namespace std; }` в `.cpp`.
