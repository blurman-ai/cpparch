# #080: Ручной анализ корпуса — корреляции, drill-down, выводы

**Дата создания:** 2026-06-03  
**Дата старта:** 2026-06-03  
**Статус:** wip  
**Приоритет:** high  
**Тип:** [ANALYSIS][RESEARCH]

---

## Цель

По результатам полного корпусного прогона (#079) проводим **ручной анализ**:
1. **Корреляция AI% ↔ граф-дрифт/дупликаты** (Spearman)
2. **Drill-down на топ-найденные репо** (CnC_Generals 2538 pairs, graphillion_kyotodd 101 errors)
3. **Гипотезы** — AI-adoption закономерности, риски, паттерны
4. **Вывод** — отчёт с рекомендациями

---

## Данные

**Основной источник:** `experiments/ai_repo_run/corpus_summary.tsv`

Таблица 317 реп × 9 колонок:
```
name, cpp_files, cpp_loc, commits, ai_commits, ai_pct, graph_errors, dup_pairs, about
```

**Ключевые метрики:**
- **AI adoption:** min=0%, max=100%, mean=52.8%, median~55%
- **Graph errors:** min=0, max=101 (graphillion_kyotodd)
- **Dup pairs:** min=0, max=2538 (awest813_CnC_Generals_Zero_Hour)

---

## План анализа

### 1. Spearman корреляция (Python + scipy)

```python
from scipy.stats import spearmanr
import csv

rows = []
with open('corpus_summary.tsv') as f:
    reader = csv.DictReader(f, delimiter='\t')
    for row in reader:
        ai_pct = float(row['ai_pct'])
        graph_errors = int(row['graph_errors'])
        dup_pairs = int(row['dup_pairs'])
        rows.append((ai_pct, graph_errors, dup_pairs))

ai_list = [r[0] for r in rows]
graph_list = [r[1] for r in rows]
dup_list = [r[2] for r in rows]

# Корреляции
rho_graph, p_graph = spearmanr(ai_list, graph_list)
rho_dup, p_dup = spearmanr(ai_list, dup_list)
rho_graph_dup, p_graph_dup = spearmanr(graph_list, dup_list)

print(f"AI% vs graph_errors: ρ={rho_graph:.3f}, p={p_graph:.2e}")
print(f"AI% vs dup_pairs: ρ={rho_dup:.3f}, p={p_dup:.2e}")
print(f"graph_errors vs dup_pairs: ρ={rho_graph_dup:.3f}, p={p_graph_dup:.2e}")
```

### 2. Top-10 по каждой метрике

Извлечь:
- Top-10 по AI%
- Top-10 по graph_errors
- Top-10 по dup_pairs
- Пересечения (high AI + high errors/dup)

### 3. Drill-down на интересные репо

#### CnC_Generals_Zero_Hour (100% AI, 2538 pairs)
- Структура: 1.8M LOC, 5 commits (все AI)
- Граф: 1 ошибка (скромная)
- Дупликация: **2538 pairs** (огромная!)
- **Гипотеза:** large-scale code generation → копипаст артефакт
- **Действие:** `git blame` на нескольких парах, классифицировать источник

#### graphillion_kyotodd (99% AI, 711 commits, 101 graph errors)
- LOC: 80K, commits в окне: 711
- **101 граф-ошибок** за 711 commits = 14% commits с дрифтом
- **Гипотеза:** активная AI-разработка → растущая сложность включаемых зависимостей
- **Действие:** посмотреть `_graph_drift.md` (уже есть), выделить когда начался дрифт

#### fox1245_NeoGraph (94% AI, 369 commits, 114 graph errors)
- 227K LOC
- **114 граф-ошибок** за 369 commits = 31% commits с дрифтом
- Выше, чем graphillion
- **Гипотеза:** неопытная AI → плохие архитектурные решения?

### 4. Гипотезы — ОБНОВЛЕНО из анализа

**✓ H1: AI adoption ↔ граф-дрифт — REFUTED**
- Spearman ρ = +0.006, p = 0.91 (NO correlation)
- Вывод: Высокий AI% НЕ предсказывает граф-дрифт
- Примеры: melkyr_znineeight (0% AI, 273 errors), FiveTechSoft_OpenADS (92.7% AI, 152 errors)
- **Граф-качество = независимая переменная от AI adoption**

**✓ H2: AI adoption ↔ копипаст — REFUTED**
- Spearman ρ = +0.029, p = 0.61 (NO correlation)
- Вывод: Высокий AI% НЕ предсказывает дупликаты
- CnC_Generals (100% AI, 2538 pairs) — outlier, не тренд
- HyperCogWizard (44% AI, 735 pairs) — можно же наломать код при низком AI%

**✓ H3: graph_errors ↔ dup_pairs — CONFIRMED**
- Spearman ρ = +0.319, p < 0.0001 (**strong significant**)
- Вывод: Граф-ошибки и копипаст — коррелируют
- Интерпретация: Both = индикаторы плохого качества кода, не причина друг друга
- **Граф-дрифт и дупликация — разные симптомы одной болезни (technical debt)**

**✓ H4: Two-problem paradigm (not one)**
- **Граф-проблемы:** архитектурное усложнение включаемых зависимостей
  - Примеры: FiveTechSoft_OpenADS (152 errors, 3 pairs), Krilliac_SparkEngine (244 errors)
- **Код-проблемы:** повторный копипаст в имплементации
  - Примеры: CnC_Generals (1 error, 2538 pairs), Dewm-3 (0 errors, 323 pairs)
- **Beide:** репы с обоими (fox1245_NeoGraph: 114 errors, 15 pairs)
- **Ни того ни другого:** clean projects

**✓ H5: "AI-native batch generation" pattern**
- 100% AI (13 реп): CnC_Generals, Dewm-3 — целые проекты сгенерированы в batch
- Качество граф: хорошо (AI не пустит циклы?)
- Качество код: дубляется (batch generation reuses templates)
- **Риск:** дупликаты, не граф-циклы

---

## Сделано

- ✅ **Spearman корреляция** — все три вычислены
  - AI% → graph_errors: ρ=+0.006, p=0.91 (NO)
  - AI% → dup_pairs: ρ=+0.029, p=0.61 (NO)
  - graph_errors ↔ dup: ρ=+0.319, p<0.0001 (YES ***)
- ✅ **Top-10 анализ** — по всем 3 метрикам + пересечения
- ✅ **Гипотезы** — обновлены с выводами

## В работе

1. **Итоговый отчёт** — `experiments/MANUAL_ANALYSIS.md` (начать)
2. **Drill-down** (опционально) — git blame на CnC_Generals, graphillion_kyotodd

---

## Ключевые решения

- **Только Spearman** (не Pearson) — данные могут быть не-нормальны
- **Drill-down вручную** — смотрим `.md` файлы графа и выводы блейма
- **Гипотезы без proof** — пока only exploratory, доказательства в рецензии

---

## Изменённые файлы

(будут заполнены)

---

## Related

- `[[backlog/wip/079_maj_corpus_run_graph_dup_ai_correlation.md]]` — корпусный прогон (база)
- `[[experiments/ai_repo_run/corpus_summary.tsv]]` — данные (317 реп)
- `[[experiments/ai_repo_run/corpus_report.md]]` — отчёт по AI%
