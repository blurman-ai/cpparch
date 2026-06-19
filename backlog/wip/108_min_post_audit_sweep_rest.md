# [CLEANUP][QUALITY] Post-audit sweep: секции 2–4 (отложено из #104)

**Дата создания:** 2026-06-11
**Статус:** new
**Модуль:** SCAN / QUALITY
**Приоритет:** minor
**Сложность:** S-M
**Related:** #104 (секция 1 закрыта, be56245), #107 (self-scan подтвердил toLowerCopy-дубль)

Отложенный остаток #104 (формулировки и grep-доказательства — в
`backlog/completed/104_min_post_audit_cleanup_sweep.md`):

1. **Секция 2 — дубли >5 строк** — ⏳ остаётся, нужен выбор «куда выносить»:
   - **fork/exec**: `git_exec.cpp` ↔ `git_object_file_source.cpp` — реальный дубль,
     общий spawn-хелпер; место выноса = решение (вероятно экспортировать из `git_exec`).
   - **toLowerCopy**: фактический дубль — `duplication_scanner.cpp:166` ↔
     `area_of.h:52` (идентичная transform-лямбда), НЕ `drift_bidirectional_coupling.cpp`
     (как было записано — тот лишь зовёт `areaOf`). Вынос = новый общий string-util
     заголовок (cross-cutting, «0 абстракций без запроса» → требует явного решения).
   - **plainJaccard**: ❌ сверено по коду 2026-06-13 — НЕ дублируется (одно
     определение `similarity.cpp:42`, один вызов). Пункт снят как ошибочный.
   - JSON-сериализация, vendor/test-фильтр: проверить аналогично перед работой —
     описание из #104 могло устареть.
2. **Секция 3 — гигиена duplication_scanner.cpp** — ✅ сделано 2026-06-13:
   `filerPairKey`→`filePairKey`, `phase13FileLoclalIDFDownweight`→`phase13FileLocalIdfDownweight`,
   второй `phase8WholeFileSuppress`→`phase9WholeFileSuppress` (устранён двойной phase8).
   Все ренеймы файл-локальные (анонимный namespace), 526/526 тестов зелёные, dogfood чист.
3. **Секция 4 — lizard-долг** — ⏳ остаётся: длинные функции (`lizard --warnings_only
   src/ include/ tests/`). Это рефактор-долг, требует разбивки функций (дизайн).
