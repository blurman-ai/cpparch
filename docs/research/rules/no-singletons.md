# no-singletons

- **Category:** B (AST / semantic)
- **Authority:** high — C++ Core Guidelines
- **Source:** [CCG I.3 — Avoid singletons](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#i3-avoid-singletons)

## Rule

> "Singletons are basically complicated global objects in disguise."

## Why for cpparch

Singleton — глобал, замаскированный под объект. Создаёт скрытые межмодульные зависимости: `Foo::Instance()` дёргает кто угодно откуда угодно, в графе это не видно. Архитектурный анти-паттерн ровно того типа, который cpparch должен ловить.

## Detection

AST-pattern:
- класс с приватным/удалённым конструктором,
- статический метод `Instance()` / `GetInstance()` / `instance()` (имена настраиваемые),
- внутри метода — `static T instance;` или `static T* instance = new T;`.

Уровень уверенности — heuristic, не 100%. Под флагом `--enable=I.3`, не дефолт.

## Fixtures

- `pass/` — обычные классы без статической `Instance()`.
- `fail_classic/` — каноничный Meyer's singleton.
- `fail_pointer/` — singleton через `new` в static-local.
- `pass_false_positive_candidate/` — класс с публичным конструктором и удобной статической `Default()` функцией — не singleton, не флагать.
