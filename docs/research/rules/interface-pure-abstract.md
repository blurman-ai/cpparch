# interface-pure-abstract

- **Category:** B (AST / semantic)
- **Authority:** high — C++ Core Guidelines
- **Source:** [CCG C.121 — If a base class is used as an interface, make it a pure abstract class](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c121-if-a-base-class-is-used-as-an-interface-make-it-a-pure-abstract-class)

## Rule

> "An interface should have no data and no implementation. Mixing concerns gets in the way."

## Why for cpparch

Усиливает Martin's *Abstractness* (A) — превращает «интерфейсный» слой в реально интерфейсный. Если базовый класс назван `IFoo` / `FooInterface` (или просто используется только как точка наследования), он обязан быть pure abstract. Иначе «интерфейс» тянет за собой реализацию — модуль, который должен был зависеть от абстракции, цепляется к конкретике.

## Detection

AST-проход: класс, который:
- наследуется хотя бы одним другим классом, **и**
- имеет хотя бы одну виртуальную функцию.

Проверка: все non-static функции — pure virtual; нет data members; есть virtual destructor.

Опционально под флагом «строгий» — флагать только классы с префиксом `I` или суффиксом `Interface`.

## Fixtures

- `pass/` — `class IFoo { virtual ~IFoo() = default; virtual void Do() = 0; };`.
- `fail_data/` — interface с `int _state;`.
- `fail_impl/` — interface с непустым телом метода.
- `fail_no_virtual_dtor/` — все методы pure, но нет `virtual ~`.
