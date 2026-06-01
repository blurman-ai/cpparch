# [SCAN][GRAPH] archcheck: исключать third_party/vendor из include-графа (циклы/метрики)

**Дата создания:** 2026-06-01
**Дата старта:** 2026-06-01
**Дата завершения:** 2026-06-01
**Статус:** done
**Модуль:** SCAN / GRAPH
**Приоритет:** major
**Related:** #064 (vendor-exclude в #056), #067 (ночная верификация — вскрыла баг)

## Цель

archcheck строил include-граф (и считал SF.9-циклы / SCC / Lakos-метрики) по ВСЕМУ
дереву, включая вендоренный код. Циклы внутри `third_party/borealis` (fmt, SDL)
засчитывались как архитектурный дрейф проекта → завышенная доля «структурного дрейфа»
в корпусном прогоне (#054). Вендор надо исключать безусловно, во всех вариациях.

## Как работает

`src/scan/project_files.cpp` → `is_excluded_dir_name`: к существующим build-артефактам
(`.git`, `build`, `cmake-build-*`, …) добавлено исключение вендорных каталогов.
Имя каталога нормализуется (lowercase, выкинуты `_`/`-`/пробел) и сверяется с
каноническим списком `kExcludedVendor`: `thirdparty`, `3rdparty`, `vendor`, `vendored`,
`vendors`, `external`, `externals`, `extern`, `deps`, `dependencies`, `submodules`,
`submodule`, `nodemodules`, `contrib`. Так ловятся все варианты написания
(`third_party` / `thirdParty` / `ThirdParty` / `3rd-party` / `node_modules` …).
Каталог целиком пропускается (`disable_recursion_pending`), поддерево не сканируется.

## Чем управляется

Безусловный дефолт (как `.git`/`build`). Флага нет — вендор исключается всегда.

## Проверено

- `beiklive/GBAStation`: было `sccs_cyclic=4` (все в `third_party/borealis`), стало **0**
  (nodes 1037→128). В авторском `src/` циклов и не было.
- build OK (GCC8), lizard чисто (функция ≤30 строк).
- Перемер корпуса (#054): доля «структурного дрейфа» пересчитана (была завышена вендором).

## С чем связана

<!-- TODO -->

## Диагностика

Если цикл подозрителен — `archcheck --graph <repo>` даёт `sccs_cyclic`; сравнить с
прогоном по авторскому подкаталогу. Список вендор-вариаций — `kExcludedVendor` в
`project_files.cpp`. Дополнять при встрече новых имён.
