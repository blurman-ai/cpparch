# [DUPLICATION][RESEARCH] Количественная валидация по внешним оракулам (отложено из #107)

**Дата создания:** 2026-06-11
**Дата старта:** 2026-06-19
**Дата завершения:** 2026-06-19
**Статус:** completed
**Related:** #107 (методология + данные), #106 (фикстуры), #070/#059 (precision)

**Итог:** цель (количественная валидация против внешних оракулов) достигнута через
ДВА оракула. Шаг 1 (NiCad/monit) — числа + disagreement-triage, 0 recall-багов.
Шаг 3 (POJ-104) — FP-граница Type-4 подтверждена (0 cross-label на 39582 кандидатах).
Шаг 4 (триаж-ширина) — 0-FP за пределами LibreSprite (vmecpp 42/42 + корпус 18 реп).
Шаг 2 (Bellon) — descoped: канонический источник 2002-исходников мёртв (не долг).
Сводный отчёт под статью: `reports/clone_detection_comparison.md`.
Сырьё прогонов: `experiments/clone_oracle_validation/` (gitignored).

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

---

## Прогресс (2026-06-19)

### Шаг 1 — NiCad/monit ✅ ЗАКРЫТ (есть числа + триаж)

**Блокер TXL из #107 был ложным:** TXL установлен (`/home/localadm/bin/txl` v10.8b),
просто не в PATH. NiCad собран после 2 правок окружения (в распакованном NiCad, не в
archcheck): version-guard `10.[56]`→`10.[5-9]`; `tools/Makefile` `-m32`→`-m64`.

Результаты в `experiments/clone_oracle_validation/NICAD_QUANT.md` (+ `nicad_join.py`,
`results/archcheck_monit.txt`). Ключевое:
- NiCad: 27 пар / 12 классов; archcheck: 21 пара.
- Naive edge-recall 12/27=0.44 — **вводит в заблуждение** (NiCad перечисляет полные
  клики, мы — звёзды + nameренные guard'ы).
- **Класс-уровневый recall = 8/12 = 0.667.**
- Триаж 4 непокрытых классов: 1× whole-file suppression (категория a, by design),
  3× ниже нашего floor 0.75+P0.6 / benign read-write-сиблинги (категория b).
  **Настоящих recall-багов (c) — 0.**
- archcheck-only 9 пар — **0 FP**: все реальные клоны ниже NiCad `minsize=10`
  (мелкий same-file копипаст, который мы ловим, а NiCad — нет).

### Шаг 4 — триаж-ширина ✅ ЗАКРЫТ (0-FP подтверждён за пределами LibreSprite)

Результаты в `experiments/clone_oracle_validation/TRIAGE_WIDTH.md`
(+ `dup_triage_vmecpp.md`, `corpus_pairs.txt`, `corpus_sample.txt`).
- Разобран скачок 8→42 пары: июньский отчёт **протекал тест-файлами**; тесты
  исключаются из duplication намеренно с #070 (подтверждено `git show ec5988b^`).
  **Не регрессия.** v1-отчёт удалён, v2 — канонический.
- **vmecpp: все 42 пары размечены** (extractability #071): 12 TP + 14 TP-variant +
  16 benign, **0 FP**.
- **Корпус: 18 C++-реп по HEAD, 8094 пары.** Разобрана зона риска (22 пары с
  наименьшим weight 0.51–0.55) — все настоящие дубли, **0 FP**. Floor 0.6 режет
  перед зоной случайных совпадений.
- Наблюдение: генерёные Rcpp-биндинги (tulpa) — реальные дубли, кандидаты на
  @generated-исключение (#127/#131), не FP.

### Шаг 2 — Bellon 🔴 BLOCKED ОКОНЧАТЕЛЬНО (внешний источник мёртв)

Перепроверено: `.rcf` = текстовая XML-схема + **бинарные кортежи** (нужен RCF-ридер,
нет). ISO = только `results/` + `*-dummydeclarations.h`, **нет исходников cook/weltab
2002** → ключ join по `file:line` не восстановить.
- Попытка добыть исходники из сети (2026-06-19): канонический сайт Bellon
  `bauhaus-stuttgart.de/clones/` **умер** — 303-редирект на domain-parking
  (`ts.domainname.de`). Источника 2002-версий в открытом доступе нет.
- В отличие от TXL (нашёлся локально), Bellon доделать нечем. **Descoped окончательно**
  — не «долг», а отсутствие внешнего артефакта (ср. [[feedback_task_blocked_vs_completed]]).

### Шаг 3 — POJ-104 ✅ ЗАКРЫТ (FP-граница Type-4 подтверждена)

Результаты в `experiments/clone_oracle_validation/POJ_FP_BOUNDARY.md`
(+ `poj104/train.parquet`, `poj104/archcheck_poj.txt`). Датасет с HuggingFace
(`google/code_x_glue_cc_clone_detection_poj104`).
- 1950 программ / 65 классов задач / 39582 кандидата → **15 пар, все same-label,
  0 cross-label**. Type-4 (одна задача, текстуально разные реализации) НЕ метим.
- 15 same-label пар — настоящие текстуальные клоны (Type-1/2/3: шаренный код,
  rename), не семантические. Соответствует «Type-4 — нет, осознанно».

### Сводный отчёт под статью

`reports/clone_detection_comparison.md` — консолидирует: корпус-C++ head-to-head
(reports/nicad_vs_archcheck.md), новый monit-pure-C прогон (NICAD_QUANT.md) и
ландшафт по другим тулам (docs/research/clone_tools_landscape.md). Главный тезис:
NiCad ≠ ground truth; на чистом C мы на паритете с precision-эталоном, на C++ —
единственный, кто даёт actionable-сигнал. Исправляет чтение «recall 0.667 = хуже».
