# [SCAN] #056: precision против coincidental-FP (главный класс)

**Дата создания:** 2026-05-31
**Статус:** new
**Модуль:** SCAN
**Приоритет:** critical
**Related:** #060, #056, #059

## Цель
Убрать FP-coincidental (~46% в iter1, ГЛАВНЫЙ класс): две РАЗНЫЕ функции матчатся,
т.к. selective normalization схлопывает `id`/`lit` → разный код даёт похожий
токен-поток, token-LCS добивает до weighted=1.0 при низком `line` (0.3–0.6).

## Гипотезы фикса (мерить по iter)
1. **Min raw-line floor**: гейт `line >= X` (≈0.5) — отсекает над-нормализованные
   совпадения (разный сырой код). Риск: теряем сильно-renamed TP → подобрать X
   по распределению TP/FP из verified-данных iter1.
2. **Structural discriminator**: учитывать callee-сигнатуры / форму control-flow,
   а не только нормализованный поток (см. essence-clones, #059).
3. Возможно: ограничить `--diff` cross-component (как snapshot), убрать same-file
   пары (большинство coincidental — same-file разные функции).

## Проверка
- [ ] iter-N: precision до/после, TP-retention.
