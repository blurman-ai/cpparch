# [CLEANUP][QUALITY] Post-audit sweep: секции 2–4 (отложено из #104)

**Дата создания:** 2026-06-11
**Статус:** new
**Модуль:** SCAN / QUALITY
**Приоритет:** minor
**Сложность:** S-M
**Related:** #104 (секция 1 закрыта, be56245), #107 (self-scan подтвердил toLowerCopy-дубль)

Отложенный остаток #104 (формулировки и grep-доказательства — в
`backlog/completed/104_min_post_audit_cleanup_sweep.md`):

1. **Секция 2 — дубли >5 строк**: fork/exec, `toLowerCopy`
   (`drift_bidirectional_coupling.cpp:48` ↔ `duplication_scanner.cpp` — наш же
   `--duplication src` это репортит, EXACT w=1), JSON-сериализация,
   vendor/test-фильтр, plainJaccard.
2. **Секция 3 — гигиена duplication_scanner.cpp**: опечатки
   (`phase13FileLoclalIDFDownweight`, `filerPairKey`), нумерация phase8 (два phase8).
3. **Секция 4 — lizard-долг**: длинные функции (см. полный список warnings
   `lizard --warnings_only src/ include/ tests/`).
