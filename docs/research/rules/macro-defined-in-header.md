# macro-defined-in-header

- **Category:** B (AST / semantic) или A (preprocessor scan)
- **Authority:** medium — C++ Core Guidelines + общая практика
- **Source:** [CCG ES.30 — Don't use macros for program text manipulation](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es30-dont-use-macros-for-program-text-manipulation), [CCG ES.31 — Don't use macros for constants or "functions"](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#es31-dont-use-macros-for-constants-or-functions).

## Rule

> "Macro definitions in headers leak into every translation unit that includes the header. Either guard with `#undef` at end of header, or don't use a macro at all."

## Why for cpparch

Макро в header — это утечка имени, не подчиняющаяся ни namespace, ни access control. Один `#define MAX 100` в `foo.h` коснётся каждого `.cpp`, который транзитивно включил его, и сломает совершенно постороннему классу `enum { MAX };`. Это архитектурный smell на границе двух модулей.

Граничит со стилем (есть в clang-tidy / cppcheck), поэтому **под флагом, не дефолт** — но имеет архитектурный смысл, в отличие от других макро-правил.

## Detection

Preprocessor scan (предпочтительно — без libclang):
1. Для каждого `.h` найти `#define`.
2. Проверить наличие парного `#undef` в том же файле перед закрывающим `#endif` include guard.
3. Если `#undef` нет — флагать.

Исключения:
- include guards сами (`#define FOO_H` парный с `#ifndef FOO_H`),
- макросы, имя которых начинается с заданного префикса (например, `CPPARCH_*`) — это публичный API проекта,
- определения, помеченные комментарием `// archcheck: allow-macro <reason>`.

## Fixtures

- `pass_no_macro/` — header без `#define` (кроме guard).
- `pass_undef_at_end/` — `#define HELPER ...` и парный `#undef HELPER` перед `#endif`.
- `pass_public_api/` — `#define CPPARCH_VERSION_MAJOR 1` (наш публичный префикс).
- `fail_leaking/` — `#define MAX 100` без `#undef`.
