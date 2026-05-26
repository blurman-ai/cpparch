# header-cpp-pairing

- **Category:** A (filesystem scan)
- **Authority:** medium — Bloomberg BDE
- **Source:** [BDE physical code organization](https://github.com/bloomberg/bde/wiki/physical-code-organization) — каждый компонент = пара `.h`/`.cpp`.

## Rule

> "Каждому `.h` должен соответствовать `.cpp` с тем же базовым именем, кроме явно header-only файлов."

## Why for cpparch

Косвенно поддерживает SF.5 (`.cpp` обязан включать свой `.h`) и SF.11 (header self-contained). Если пара `.h`/`.cpp` есть всегда — пользователь не уйдёт в дрейф «решили header-only, но потом всё равно добавили implementation в другом месте». Под флагом, не дефолт: для template-heavy / header-only библиотек правило не применимо.

## Detection

Filesystem-scan: собрать все базовые имена в каждом каталоге. Для каждого `.h` проверить наличие парного `.cpp`. Исключения:
- файл явно header-only — определяется через комментарий-маркер `// archcheck: header-only` в первых N строках, или через regex в имени (`*_inl.h`),
- категории каталогов, помеченные в конфиге как `header_only: true`,
- если в `.h` есть только `class declaration` без определений — допустимо (тогда возможно нет нужды в `.cpp`, проверяется по AST или эвристически).

Парная проверка — для `.cpp` без `.h` (SF.5 это уже частично ловит).

## Fixtures

- `pass/` — каждый `.h` имеет `.cpp`.
- `pass_header_only/` — `concepts.h` с маркером `// archcheck: header-only`.
- `fail_orphan_header/` — `foo.h` без `foo.cpp` и без маркера.
- `fail_orphan_cpp/` — `bar.cpp` без `bar.h`.
