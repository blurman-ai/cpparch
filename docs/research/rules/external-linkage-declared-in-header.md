# external-linkage-declared-in-header

- **Category:** B (AST / semantic)
- **Authority:** high — Bloomberg BDE / Lakos
- **Source:** [BDE physical code organization](https://github.com/bloomberg/bde/wiki/physical-code-organization)

## Rule

> "All external-linkage constructs defined in a `.cpp` file must be declared in the corresponding `.h` file."

## Why for cpparch

Ловит «теневой API»: функция/переменная определена в `.cpp` без `static`/`anonymous namespace`, не объявлена в `.h`, но имеет external linkage — её можно слинковать из чужого TU через `extern`-declaration. Это обход header-как-единственной-точки-входа, прямая дыра в архитектурном контракте: модуль обещает интерфейс через `.h`, а реально пускает через бэк-вход.

## Detection

Для каждого `.cpp`:
1. Найти все `FunctionDecl` и `VarDecl` с external linkage (не `static`, не в `anonymous namespace`, не `inline`).
2. Прочитать парный `.h` (по правилу SF.5) и собрать declared names.
3. Если что-то из `.cpp` не объявлено в `.h` — флагать.

Игнорировать: implicit instantiation шаблонов, `extern "C"` API внутри своего слоя.

## Fixtures

- `pass/` — все функции `.cpp` имеют объявление в `.h`.
- `fail_undeclared_function/` — `.cpp` содержит `void Helper() { ... }` (external linkage), `.h` его не знает.
- `pass_static/` — та же функция помечена `static` — допустимо.
- `pass_anonymous_namespace/` — функция в `namespace { ... }` — допустимо.
