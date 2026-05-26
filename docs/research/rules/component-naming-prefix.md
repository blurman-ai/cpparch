# component-naming-prefix

- **Category:** A (filesystem scan)
- **Authority:** high — Bloomberg BDE
- **Source:** [BDE physical code organization](https://github.com/bloomberg/bde/wiki/physical-code-organization)

## Rule

> "Each component is named `packagename_componentname`. All files of the component share this prefix."

## Why for cpparch

Опциональное правило (под флагом, не дефолт). Даёт жёсткую привязку файла к модулю без необходимости описывать `modules:` в YAML — модуль выводится из префикса имени файла. Полезно для команд, готовых принять BDE-конвенцию; для всех остальных — выключено.

## Detection

Filesystem-scan: для каждого файла извлечь префикс до первого `_`. Сгруппировать по префиксу. Если в проекте включён `--bde-naming`, проверить что все файлы каталога имеют одинаковый префикс, и префикс совпадает с именем каталога.

## Fixtures

- `pass/` — `bdlt/bdlt_date.h`, `bdlt/bdlt_date.cpp`, `bdlt/bdlt_calendar.h`.
- `fail_no_prefix/` — `bdlt/date.h` (без префикса).
- `fail_wrong_prefix/` — `bdlt/bsls_assert.h` (префикс не от текущего каталога).
