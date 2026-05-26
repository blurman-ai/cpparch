# no-inter-component-friendship

- **Category:** B (AST / semantic)
- **Authority:** high — Lakos / Bloomberg BDE
- **Source:** [BDE physical code organization](https://github.com/bloomberg/bde/wiki/physical-code-organization), Lakos *LSCSD* §3.6

## Rule

> "Friendship may be granted only within the boundaries of a single component."

## Why for cpparch

`friend` обходит всю инкапсуляцию и не виден в include-графе как обычная зависимость. Если модуль A объявляет `friend class B::Bar;` — A неявно знает приватные детали B, любое изменение в B может сломать A, но cpparch без этой проверки этого не увидит. Это классическая дыра в архитектурном контракте.

## Detection

AST: `FriendDecl` внутри `CXXRecordDecl`. Определить компонент-владелец класса (по path/module mapping) и компонент-владелец `friend` target. Если разные — флагать.

Допустимо: friend между классами одного компонента (`.h`/`.cpp` пара) или внутри одного namespace, если они в одном файле.

## Fixtures

- `pass_same_component/` — `foo.h` объявляет class `Foo` с `friend class FooImpl;`, `FooImpl` в `foo.cpp`.
- `fail_cross_module/` — `domain/Order.h` объявляет `friend class infrastructure::SqlRepo;`.
