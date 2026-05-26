# include-order-groups

- **Category:** A (preprocessor scan)
- **Authority:** medium — Google C++ Style Guide
- **Source:** [Google C++ Style Guide — Names and Order of Includes](https://google.github.io/styleguide/cppguide.html#Names_and_Order_of_Includes)

## Rule

> "Use standard order for readability: 1) own header (in `.cpp`), 2) C system headers, 3) C++ standard library, 4) other libraries, 5) your project's headers. Each non-empty group separated by a blank line."

## Why for cpparch

Не архитектура в строгом смысле, но низко-висящее. Косвенно проверяет SF.5 (`.cpp` обязан включать свой `.h` первым → если он первым, значит он self-contained). Дёшево, проверяется препроцессорным сканом без libclang. Помогает в early adoption — пользователь видит результат на первом запуске без сложного config.

## Detection

Preprocessor scan: для каждого файла собрать `#include` директивы по группам:
1. own header (только для `.cpp`, имя совпадает с базовым именем `.cpp`).
2. `<...>` — system или 3rd party.
3. `"..."` — project headers.

Подразделить группу 2 на C system (`<sys/...>`, `<unistd.h>`, и т.п.), C++ stdlib (`<vector>`, `<string>`, etc.), 3rd party — по словарю стандартных headers.

Флагать:
- own header не первым в `.cpp`,
- группы не разделены пустой строкой,
- группы перепутаны (project headers до stdlib).

## Fixtures

- `pass/` — каноничный порядок с пустыми строками.
- `fail_own_not_first/` — `.cpp` начинается с `#include <vector>`, потом `#include "foo.h"`.
- `fail_mixed_groups/` — `<vector>` рядом с `"foo.h"` без разделителя.
