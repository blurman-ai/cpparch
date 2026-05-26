# forward-decl-of-std

- **Category:** B (AST) или D (text pattern)
- **Authority:** high — стандарт C++ (`[namespace.std]`)
- **Source:** C++ standard §16.4.5.7 [namespace.std]; в спеке cpparch упомянуто как «несомненная практика» без явного rule ID.

## Rule

> "It is undefined behavior to add declarations or definitions to namespace `std` or to a namespace within namespace `std`, with limited exceptions (template specializations of certain library templates)."

## Why for cpparch

Forward-decl типов из `std::` — формальный UB по стандарту. Часто пишут `namespace std { class string; }` чтобы избежать `#include <string>` в заголовке, но это нарушение `[namespace.std]`: реализация может объявить `string` как typedef, шаблон, или находящийся в inline-namespace, и линковка тихо отвалится или поведение поедет. cpparch обязан это ловить как security-critical паттерн, потому что компилятор обычно молчит.

## Detection

Два варианта:
1. **AST (надёжно):** найти `NamespaceDecl` с `getName() == "std"` в файле проекта (не в STL header), внутри которого есть любые декларации. Флагать всё, что находится внутри. Исключение: template specializations через `template<> ...` — допустимо стандартом.
2. **Text pattern (быстро):** regex `\bnamespace\s+std\s*\{` в файлах проекта. Дёшево, может давать false negatives на `namespace ::std`.

В fast backend — D, в clang backend — B (точнее).

## Fixtures

- `pass_specialization/` — `template<> struct std::hash<MyKey> { ... };` (допустимо).
- `pass_no_std/` — никаких объявлений в `std`.
- `fail_forward_class/` — `namespace std { class string; }` в `foo.h`.
- `fail_forward_struct/` — `namespace std { struct vector; }`.
