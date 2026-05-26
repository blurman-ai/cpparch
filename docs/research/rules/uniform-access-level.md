# uniform-access-level

- **Category:** B (AST / semantic)
- **Authority:** high — C++ Core Guidelines
- **Source:** [CCG C.134 — Ensure all non-`const` data members have the same access level](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#c134-ensure-all-non-const-data-members-have-the-same-access-level)

## Rule

> "Prevention of logical confusion leading to errors. If the non-`const` data members don't have the same access level, the type is confused about what it's trying to do."

## Why for cpparch

Маркер «god-class» / смешанных concerns: часть полей публичные (struct-like), часть приватные (class-like). Такие классы обычно вырастают из POD, которым прикрутили инвариант, но не довели до конца — то есть это технический долг прямо на границе ответственности модуля.

## Detection

AST: для каждого `CXXRecordDecl` собрать access-уровни всех non-`const` non-static `FieldDecl`. Если множество уровней > 1 — флагать.

## Fixtures

- `pass_struct/` — все поля `public` (POD).
- `pass_class/` — все non-const поля `private`.
- `pass_mixed_const/` — `public: const int kVersion = 1; private: int _state;` (const разрешён публично).
- `fail_mixed/` — `public: int x; private: int _y;` — оба non-const, разные уровни.
