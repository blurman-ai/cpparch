# [SCAN] #056 --diff: vendor-exclude (miniz/stb и пр.)

**Дата создания:** 2026-05-31
**Статус:** dropped (поглощена #065/#069/#070)
**Модуль:** SCAN
**Приоритет:** minor
**Related:** #060, #056

## Цель
Часть «TP» в iter1 — вендоренные копии библиотек (две копии `miniz`, `stb_image`):
технически копипаста, но не код проекта. Должны отсекаться excludes (как snapshot),
регистронезависимо: `third_party`, `extern`, `vendor`, `borealis/.../extern`, и по
сигналам (license-шапка / `@generated`).

## Проверка
- [ ] iter-N: vendored-пары не репортятся; реальные project-копии остаются.

## Итог

**Статус:** dropped — цель достигнута другими задачами, собственной работы не осталось.
**Дата закрытия:** 2026-06-11 (бэклог-ревью).

Запрошенное поведение реализовано трижды на разных уровнях:
- в спайке — vendor-exclude вошёл в iter2-фиксы #060 (коммит `0beb2f7`, вместе с #061);
- в продукте — vendor/third_party/extern-исключение централизовано в #069
  (`src/scan/project_files.cpp`), duplication-проход вендоренные файлы не видит вовсе;
- generated-файлы (`@generated`, protobuf/moc/flex-bison) отсекает P0.9-гард `isGeneratedPath`
  (#065 → #070, `src/scan/duplication/duplication_scanner.cpp`), whole-file vendored-twin
  подавляет P0.2 (#070).

Незакрытым из исходной идеи остался только сигнал «license-шапка» — сознательно не делаем:
на корпусе FP этого класса уже гасятся путями и `@generated`-маркерами.

