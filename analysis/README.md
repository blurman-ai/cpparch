# analysis/ — constraint-decay research pipeline (committed snapshot)

Курируемый снимок измерительного конвейера. **Зачем существует:** живые скрипты
и данные живут в gitignored `experiments/` — проза в `docs/research/` воспроизводима,
а метод-как-код там пропадал. Здесь — runner каждого семейства, спека корпуса и
сводные отчёты, чтобы анализ можно было прочитать И перезапустить.

**Это снимок, не рабочая копия.** Скрипты используют абсолютные пути
(`/home/localadm/oss/<owner>_<name>`, `experiments/ai_repo_run/...`) и хардкод-cutoff —
отражают то, что реально гонялось. Живые версии и данные — в `experiments/`.
Окно анализа going-forward — последние 24 месяца (см. [CORPUS_CRITERIA.md](CORPUS_CRITERIA.md)).

## Корпус

Стратифицирован, чистой «1000» нет:
- **1271+** реп клонировано в `/home/localadm/oss/` (после top-up 2026-06-12 — больше)
- **481** с per-commit `*_graph_drift.jsonl` (главный измерительный корпус; протухли —
  см. #122, реген обязателен)
- **805** ELIGIBLE по чистым воротам (`corpus_gate.py`, 2026-06-13) из всего on-disk пула
- **185** KEEP — agentic-подмножество с полным набором метрик
- **29 106** пред-скринено (`screen_ledger.tsv`), ~7510 помечены agentic

Ворота отбора и окно анализа — [CORPUS_CRITERIA.md](CORPUS_CRITERIA.md).

### Ограничение: корпус НЕ репрезентативная выборка C++ OSS — и не может ей быть

Зафиксировано честно (2026-06-13), чтобы ни один отчёт не притворялся иначе.

**Почему «возьмём случайную 1000 из 25k+ C++ реп» не работает.** Сочетание наших
жёстких ворот редкое: из **29 106** проскриненных C++ реп agentic-сигнал имеют ~7 510
(26 %), а **agentic ∧ >300 коммитов с 2025-05** — лишь **~800** (≈ 2.7 %). После отсева
форков/зеркал/гигантов-с-ревью остаётся ещё меньше. Популяция, удовлетворяющая воротам,
физически мала — «добить до круглой 1000» можно только ослабив ворота (затащив
гигантов/форки) или расширив discovery на непроскриненные страты.

**Чем корпус является на самом деле:** это **criteria-complete convenience-выборка** —
мы berём *практически всех*, кто проходит ворота и находится доступными методами поиска
(repo-search шарды + AI-маркеры в коммитах), а не случайную долю генеральной совокупности.
Следствия для выводов:
- Выводы вида «X % agentic-реп дрейфуют» — **о популяции, прошедшей ворота**, не о
  «C++ OSS вообще». Генерализация на весь C++ — незаконна.
- Систематические смещения отбора: (а) скрининг 29k покрывал только репы
  `created < 2023-06` — молодые born-agentic репы добирались отдельными заходами
  (top-up 2026-06-12/13) и недопредставлены ранними прогонами; (б) AI-маркер-поиск
  находит только тех, кто НЕ скрывает агентность (трейлеры/боты) — «тихие» agentic-репы
  невидимы; (в) гиганты с сильным ревью исключены by design — корпус сознательно смещён
  в сторону проектов, где constraint decay *может* происходить.
- Внутрикорпусные сравнения (agent vs human within-repo, fixed effects) от этого
  **не страдают** — они не требуют репрезентативности популяции, только валидной
  классификации внутри реп.

**Вывод для отчётов:** знаменатель всегда «N реп, прошедших явные ворота
(CORPUS_CRITERIA.md)», с числом N из `corpus_eligible.tsv`, а не «выборка из C++ OSS».
Круглость числа (1000) — косметика; чистота ворот важнее размера.

## Семейства метрик

| Семейство | Скрипт(ы) | Что меряет | Выход | Корпус-прогон |
|---|---|---|---|---|
| **Graph drift** (per-commit) | [graph_drift/generate_per_commit_graph_drift.py](graph_drift/generate_per_commit_graph_drift.py) | added/removed рёбра, циклы, cross-area deps по коммитам | `*_graph_drift.jsonl` | FULL 481 |
| **Graph baselines** | [graph_drift/make_window_baselines.py](graph_drift/make_window_baselines.py) | снимок графа на старте окна | `baselines/*.yml` | FULL 479 |
| **Graph metrics** (HEAD) | [graph_drift/graph_metrics.py](graph_drift/graph_metrics.py) | fan-in, god-header, SCC, chain length | TSV/репа | PARTIAL 151 |
| **Duplication** | [duplication/filtered_dup.py](duplication/filtered_dup.py), [classify_dup.py](duplication/classify_dup.py), [control_measure.py](duplication/control_measure.py) | токеновый копипаст, authored vs vendor | `*_dup.txt` | FULL 493 |
| **Local complexity drift** | [local_complexity/run_lcx_scan.py](local_complexity/run_lcx_scan.py), [scan_commit.py](local_complexity/scan_commit.py) | NLOC/CCN/arity дельта по функциям | `findings.jsonl` | SAMPLE 100 |
| **Boolean state drift** | [boolean_state/bool_history_scan.py](boolean_state/bool_history_scan.py), [perstruct_drift_all.py](boolean_state/perstruct_drift_all.py), [broad_scan.py](boolean_state/broad_scan.py) | накопление bool-полей в структурах (state bloat) | `*.csv` | per-commit/struct 185, broad 1200+ |
| **Lateral (inter-module) drift** | [lateral_drift/lateral_drift_scan.py](lateral_drift/lateral_drift_scan.py) (+ make_window_baselines, [reclone_missing.py](lateral_drift/reclone_missing.py), [agentic_normalized.py](lateral_drift/agentic_normalized.py), [before_after_probe.py](lateral_drift/before_after_probe.py)) | первая боковая связь peer-модулей: CYCLE/SDP/NEW | `lateral_drift_new.csv` | 479 |
| **Author classification** | [agentic/agent_author_scan.py](agentic/agent_author_scan.py), agentic_normalized.py | agent vs human по BOT_HINTS + Co-Authored-By | `screen_ledger.tsv` | FULL 29k |
| **Corpus growth/screen** | [agentic/grow_corpus.py](agentic/grow_corpus.py), [screen_corpus.py](agentic/screen_corpus.py) | отбор реп по воротам | ledger/excluded TSV | — |
| **Crosslang surge** | [crosslang/surge_scan.py](crosslang/surge_scan.py), [aggregate.py](crosslang/aggregate.py) | H2-2024 vs H2-2025 плотность, 8 языков | `*_surge.tsv` | ~2300 (8 языков) |

Все C++-only, кроме crosslang и author-классификации (метаданные коммитов).
Дорогие на полном корпусе — per-commit replay (graph drift, local complexity):
параллелится, но миллионы вызовов archcheck.

## Найденные/исследованные сигналы и их статус

- **Lateral drift** — провалидирован, кандидат в продуктовое правило DRIFT.4
  (CYCLE → gate, SDP/NEW → advisory). Отчёт:
  [docs/research/lateral_module_drift_corpus_run.md](../docs/research/lateral_module_drift_corpus_run.md),
  критерий: [...criterion.md](../docs/research/lateral_module_drift_criterion.md).
- **Boolean/flag-argument** (≥2 bool-параметра, `enable*`/`force*`/`skip*`…) — **спланирован**
  как задача #093 (ARG.1/ARG.2), не реализован; арность уже частично в коде
  (`paramArity` в `include/archcheck/scan/function_body_scan.h`). Рядом #094
  (param accretion >4 / рост ≥2).
- **Boolean state drift** (накопление bool-полей структуры) — исследован (#089/#090),
  history-based; статический вариант откатывался (78% шума).
- **Главный agentic-вывод**: на per-commit уровне агентское авторство дрейф НЕ повышает
  (repo fixed effects); сырое преобладание — композиция (см. отчёты в `reports/` и
  [docs/research/lateral_module_drift_corpus_run.md §8.4](../docs/research/lateral_module_drift_corpus_run.md)).

## reports/

Кросс-секущие сводки: `CPP_AI_DRIFT_REPORT.md`, `AGENTIC_CODE_REPORT.md`,
`CORPUS_FINDINGS.md`, `CORPUS_SUMMARY_REPORT.md`, `CONCRETE_EXAMPLES.md`,
`CONSEQUENCES_RANKING.md`. Профильные отчёты (lateral, дубликаты) — в `docs/research/`
и `docs/duplication_architecture.md`.
