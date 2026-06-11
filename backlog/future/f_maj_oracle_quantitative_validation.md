# [DUPLICATION][RESEARCH] Количественная валидация по внешним оракулам (отложено из #107)

**Дата создания:** 2026-06-11
**Статус:** future (отложено решением пользователя 2026-06-11)
**Related:** #107 (методология + данные), #106 (фикстуры), #070/#059 (precision)

Отложенный остаток #107 — получить ЧИСЛА (precision/recall) против внешнего ground truth:

1. **NiCad/monit**: поставить TXL (txl.ca) + собрать NiCad → снять XML-оракул на
   `examples/monit-4.2` → сравнить с нашим выводом по перекрытию `file:line`
   через disagreement-triage (#071 extractability). Оценка setup: 0.5–1 день.
2. **Bellon**: блокеры — нет исходников cook/weltab версии 2002 (ISO содержит только
   результаты) и оракул в бинарном RCF (нужен Bauhaus-ридер или CSV-переиздание).
   Текстовые CPF per-tool кандидаты парсятся тривиально.
3. (опц.) POJ-104 как FP-граница (Type-4 мы НЕ должны метить).

Данные уже скачаны: `experiments/clone_oracle_validation/{downloads,bellon,nicad,pmd}`.
Методология и вся история — в `backlog/completed/107_*.md` и
`experiments/clone_oracle_validation/FINDINGS.md`.

4. (из #071) Триаж-ширина: прогнать `experiments/triage_dup_commits.py` на vmecpp и
   corpus-репах — подтвердить 0-FP за пределами LibreSprite.
