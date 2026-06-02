# [RESEARCH][SCAN] Валидация и закалка чекера на per-commit отчётах (итеративно ≤5)

**Дата создания:** 2026-05-31
**Дата старта:** 2026-05-31
**Статус:** wip
**Модуль:** RESEARCH / SCAN / RULES
**Приоритет:** major
**Сложность:** large
**Related:** #056 (token-dup детектор), #059 (precision-фильтры), #054 (AI-repo dense-корпус), #053 (line-dup)

## Цель

Довести дедупликатор (#056) и archcheck до **доверяемого** состояния на реальных
данных: гонять по подозрительным коммитам dense-корпуса, выдавать подробные
per-commit отчёты, **проверять 200 глазами**, делать verification-отчёт
(подтвердилось / лажа по классам FP), по найденным FP — задачи на фиксы,
реализовать, перегнать. **Цикл ≤ 5 итераций.**

## Контекст

dense-корпус (#054): study-grade C++-репы с >50% агентских коммитов после мая 2025
(283 репы, `experiments/ai_repo_run/study_grade_all.tsv`), клоны в
`~/oss/_aidev_dense/`. Чекеру как истине пока НЕ доверяем — у #056/#053
известны классы FP (idiom, header↔impl, data-table, coincidental, вендор).

`#056 --diff <A>..<B> --repo <path>` даёт per-commit атрибуцию:
`commit=<sha> added_frags=N partial_hits=M` + пары `ADDED file:line <-> BASE file:line`
(дубль, рождённый в коммите = missing-reuse edge). Это и есть «подозрительный коммит».

## Протокол итерации (повторять ≤5)

1. **Генерация:** #056 --diff (+ archcheck drift-baseline где применимо) по
   агентским коммитам dense-реп → per-commit отчёты с `file:line` ADDED/BASE.
   Накопить ~250-300 отчётов (с запасом на выборку 200).
2. **Глазная проверка 200:** открыть ADDED и BASE на коммите, вердикт по каждому:
   TP (реальная копипаста/дрейф) / FP + класс (idiom / header↔impl / data-table /
   coincidental / generated / vendor / segmentation-artifact).
3. **Verification-отчёт:** что подтвердилось, что лажа, доля FP по классам,
   precision оценка. Файл `VALIDATION_ITER<N>.md`.
4. **Фиксы:** по доминирующим классам FP — отдельные задачи (#061+), реализовать
   в #056/archcheck (соблюдая code_quality: ≤30 строк/функция, ≤50 строк/коммит),
   пересобрать.
5. **Повтор** с той же выборкой коммитов → сравнить precision до/после. Стоп при
   достижении приемлемой precision или 5 итераций.

## План выполнения

- [ ] Генератор per-commit отчётов (#056 --diff по dense, bounded range) → JSONL+md
- [ ] Итерация 1: 200 глазных проверок → VALIDATION_ITER1.md (baseline precision)
- [ ] Классы FP → задачи на фиксы (#061+)
- [ ] Реализовать фиксы, пересобрать #056/archcheck
- [ ] Итерации 2..5: перегон той же выборки, precision до/после, стоп-критерий
- [ ] Итоговый отчёт: динамика precision по итерациям, что осталось

## Критерий приёмки

- [ ] ≥200 коммитов проверены глазами, вердикты записаны с обоснованием
- [ ] Verification-отчёт по классам FP воспроизводим (команды + версия #056)
- [ ] Фиксы привязаны к конкретным классам FP, прирост precision измерен
- [ ] Все находки — claims с `file:line`+SHA, по вердикту чекера ничего не удаляется

## Сделано

- **iter1:** глазная проверка 200 per-commit пар (10 агентов, читали реальный код)
  → precision `--diff` = **16.5%** (33 TP). Таксономия FP: coincidental 46%,
  other 17% (renames), segmentation 15%. → `VALIDATION_ITER1.md`.
- **iter2:** дешёвые корректные фиксы — #061 (rename `-M`) + #064 (vendor-excludes).
  Минорны (rename-пары это копии, не git-renames; vendored 1.6%). Коммит `0beb2f7`.
- **iter3-4:** 4 architecture-first surface-дискриминатора (content, cross-file,
  line, essence-LCS) **ВСЕ опровергнуты на размеченных данных** (калибровка ДО
  ре-верификации). Корень: все метрики обмануты одной нормализацией — coincidental
  «копийнее» настоящей renamed-копии. Код #056 откатан к чистому состоянию (no slop).
  Коммиты `81d2c8c`, `40e036d`. → `VALIDATION_ITER3.md`.

- **Корпусная глазная верификация (#067, 2026-06-02):** workflow `verify_findings.js`,
  агенты проверили **135 drift-реп** по реальному коду (drift-режим: дрейф по истории,
  не `--diff`). Подтверждено: **87/135 реп, 195 случаев** копипаста; ложных **406** →
  **precision ≈ 32%**; подтверждённых include-циклов **21** (в авторском src). Классы FP:
  idiom 189, coincidental 99, whole-file 51, other 31, data-table 21, generated 9 и т.д.
  Полный разбор → `experiments/ai_repo_run/VERIFICATION_REPORT.md`. Предложения по фиксам
  FP (405 шт от агентов) вынесены в **#070** для отдельного разбора.
- **round2 (расширенный охват, #066, 2026-06-02):** 135 реп, коммиты 3× (exclude round1) +
  археология циклов. **+143** новых copypaste, **+197** FP → суммарно 338/603, **precision ≈ 36%**.
  Главное новое: **28 cycle-intro коммитов** (когда/чем внесён include-цикл). Датировка показала
  раскол: AI-эра дрейф (Silencer/flox/FlashCpp/Astroray/SparkEngine/kyotodd) vs pre-AI legacy
  (BALL 2005, CppKAI 2016, LLL-TAO 2019) vs вендор (Qt Creator, ggml). → `VERIFICATION_REPORT_R2.md`.
  Новые фиксы доведены в #070 (всего 602).

**Вывод:** #056 surface = RECALL-стадия; `--diff` precision 16.5%, drift-режим 32% —
обе низки, coincidental порогами неотделим (доказано). Precision = **семантический
confirm-слой (#059)** — он и дал размеченные данные, т.е. работает; нужно встроить как
гейт. Ветка: `feat/060-checker-hardening` (не пушена), код-дифф = 15 строк (#061+#064).

## В работе

- Реализация confirm-слоя (#059) как стадии пайплайна — следующий шаг.

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `experiments/ai_repo_run/per_commit_*.{sh,jsonl,md}` | генератор + per-commit отчёты |
| `experiments/ai_repo_run/VALIDATION_ITER*.md` | verification-отчёты по итерациям |
