# no-protected-data

- **Category:** B (AST / semantic)
- **Authority:** high — C++ Core Guidelines
- **Source:** [CCG C.133 — Avoid `protected` data](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c133-avoid-protected-data)
- **Equivalent:** clang-tidy `cppcoreguidelines-non-private-member-variables-in-classes` (захватывает и public, и protected)

## Rule

> "`protected` data is a source of complexity and errors. `protected` data creates a restricted public interface."

## Why for cpparch

`protected` поле — это публичный API для всей иерархии наследников, который не виден внешнему миру и поэтому не имеет тестового покрытия. На границе модулей это особенно опасно: модуль B наследует класс из модуля A → A не может изменить `protected` без согласования. Это скрытая binary-зависимость, в include-графе её нет.

## Detection

AST: для каждого `CXXRecordDecl` пройти члены, флагать `FieldDecl` с access `protected`. Игнорировать `static constexpr` поля.

## Fixtures

- `pass/` — все данные `private`, доступ через protected/public getters.
- `fail_protected_int/` — `protected: int _count;`.
- `pass_protected_method/` — `protected: void DoStep();` (не данные — допустимо).
