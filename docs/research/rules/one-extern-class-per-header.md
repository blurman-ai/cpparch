# one-extern-class-per-header

- **Category:** B (AST / semantic)
- **Authority:** medium — HIC++ 4.0
- **Source:** [HIC++ 4.0, rule 3.3.1](https://abougouffa.github.io/awesome-coding-standards/hi-cpp-4.0.pdf)

## Rule

> "Define each externally visible class in a separate header."

## Why for cpparch

Прямое следствие CRP/SDP: если в одном `.h` объявлено пять публичных классов, любой клиент, который хочет использовать один из них, вынужден транзитивно тащить все пять. Это раздувает реальный fan-in модуля и снижает её модульность. Один публичный класс = один header — каноничная BDE-конвенция, заодно поддерживающая SF.5 (парный `.cpp`).

## Detection

AST: для каждого `.h` посчитать `CXXRecordDecl` (классы и структуры) с external linkage и не nested. Если > 1 — флагать.

Допустимо:
- nested classes (через `Foo::Inner`),
- классы в `namespace detail` или `namespace internal` (по convention — приватные импл-детали),
- forward declarations (только объявление, не определение),
- POD-структуры без методов (helper data carriers — под флагом).

## Fixtures

- `pass_one_class/` — `foo.h` объявляет `class Foo`, всё остальное в `detail`.
- `pass_nested/` — `foo.h` объявляет `class Foo` с nested `class Foo::Builder`.
- `fail_two_externals/` — `foo.h` объявляет `class Foo` и `class Bar`, оба external.
