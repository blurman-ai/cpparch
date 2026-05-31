# [SCAN] #056 --diff: vendor-exclude (miniz/stb и пр.)

**Дата создания:** 2026-05-31
**Статус:** new
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
