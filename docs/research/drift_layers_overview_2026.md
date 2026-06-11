# Четыре слоя Constraint Decay: архитектура vs состояние (2026)

_Обзор комплементарных сигналов дрейфа на корпусе 185 агентских C++ проектов._

---

## Введение: почему четыре?

Классические глобальные метрики дупликации **молчат** на AI-дрейфе (CMU diff-in-differences, MSR 2026, arXiv 2511.04427: duplicated lines density +7.03% ±4.79 — незначимо, при этом cognitive complexity +41.6% — стойко). Constraint decay (Dente et al., EURECOM, 2026, arXiv 2605.06445 — это другая работа, не CMU) действует **двойным механизмом**:

1. **Архитектурный дрейф** — new edges в include-графе (cross-module связность растёт)
2. **Состояние дрейф** — bool-поля в structs накапливаются (локальная сложность растёт)

Оба работают **независимо**. Можно лить архитектуру с чистым состоянием (или наоборот). Поэтому четыре:
- Graph-drift per-commit (архитектура)
- Copypaste (метрика поддержки)
- Boolean-drift per-commit (состояние, временный срез)
- Boolean-drift per-struct (состояние, накопленный эффект)

---

## Слой 1: Per-Commit Graph-Drift (архитектура)

### Что это?

Каждый коммит → diff `include/` → парсим рёбра → `archcheck --diff` → фиксируем **новые зависимости** (include-рёбра, которых не было раньше).

### Формат

**`*_graph_drift.jsonl`** — строка на коммит:
```json
{
  "sha": "3a4a2a525f", "date": "2025-05-04", "subject": "Fix normals",
  "added_edges": 0, "removed_edges": 0, "grown_cycles": 0, "new_area_deps": 0,
  "added": ["+ indra/llui/llkeywords.cpp -> indra/llxml/llcontrol.h"],
  "new_cross_area_dependencies": []
}
```

### Сигналы

- **added_edges > 0** — рядовая зависимость (файл X теперь включает файл Y)
- **grown_cycles > 0** — 🚨 вырос цикл (раньше был DAG, теперь есть loop)
- **new_area_deps > 0** — ⚠️ новая cross-area (модульная граница нарушена)

### Примеры из toplist (EXAMPLES_50.md)

| Репа | Коммит | Сигнал | Ребро |
|---|---|---|---|
| QuartermindGames/ape | `6a602455` | ⟲cycle+1 →13 | gui.c → editor.h |
| Collabora/online | `7a64768c` | ⟲cycle+1 →2 | TestStubs.cpp → Log.hpp |
| netdata/netdata | `cc0502ab` | ⟲cycle+1 ⊗area+2 →82 | api_v2_contexts_agents.c → rrd-retention.h |

### Тренд (185 новых реп, окно май 2025)

- **Топ по graph-errors** (кумулятивно): facebook/react-native (278), AlchemyViewer (256), LegalizeAdulthood/iterated-dynamics (242)
- **Репы без graph-drift**: ~70 из 185 (чистая архитектура, только copypaste)
- **Режим**: shallow-since=2025-05-01 (окно ~1 год)

---

## Слой 2: Copypaste Clones (поддержка)

### Что это?

Снимок HEAD → сканируем все файлы C/C++ → ищем пары (фрагмент A, фрагмент B) с совпадением 3+ строк → классифицируем: EXACT / RENAMED / LITERAL.

### Почему это дрейф?

По Juergens et al. (ICSE 2009): 52% клонов неконсистентны (один изменился, второй нет). Каждый новый клон = техдолг поддержки. Агенты склонны к copy-paste (опорная демография: курсы, бутстрапы), поэтому — проблема.

### Примеры (EXAMPLES_50.md, 25 пар)

| Репа | Тип | Длина | Фрагмент A | Фрагмент B |
|---|---|---|---|---|
| libvirt | EXACT | 53 строк | bhyve_driver.c:2085 | qemu_driver.c:16657 |
| postgresql | EXACT | 150 строк | rangetypes_selfuncs.c:858 | multirangetypes_selfuncs.c:969 |
| apache/arrow | EXACT | 39 строк | c_glib/.../read-stream.c:27 | receive-network.c:57 |
| sqlite | EXACT | 48 строк | tool/showdb.c:1019 | tool/showtmlog.c:17 |
| dolphin | EXACT | 90 строк | RangeSet.h:143 | RangeSizeSet.h:242 |

### Метрики

- **Per-commit слоя для copypaste пока НЕТ** — есть только HEAD-снимок; закрывается
  задачей #103 (5514 per-commit записей — это bool_history, слой 3, не клоны)
- **Среднее dup/repo**: 6–8 пар, max 1288 (AlchemyViewer)
- **Размер типичного клона**: 20–60 строк (exact), 10–40 (renamed)

---

## Слой 3: Boolean-Drift Per-Commit (состояние, временный)

### Что это?

Каждый коммит → diff всех `*.h` заголовков → ищем `+ bool field;` в объявлениях структур (не в локалях, не в функ-параметрах) → фиксируем **добавление bool-полей в существующие структуры**.

### Формат

**`bool_history_new185.csv`** — строка на коммит:
```
repo,sha,date,author,subject,exist_bools,new_bools,files
ape,6a602455,2025-05-04,author@,Overhauled profiler,13,3,[file1.h:line,...]
```

### Сигналы

- **new_bools > 0** → bool-поле **впервые добавлено** в существующий header
- **exist_bools > 0** → bool-поле **добавлено к уже существующим** (structure-growing pattern)

### Тренд (185 новых реп, май 2025)

- **Всего записей**: 5514 коммитов с bool-добавлением
- **Топ репо**: 
  - OloEngineBase (207 коммитов)
  - FastLED (129)
  - llama.cpp (113)
  - Serial-Studio (102)
  - AlchemyViewer (86)

### Интерпретация

Высокие числа свидетельствуют о **пошаговом накоплении состояния**. На окне май 2025 – май 2026 (year) — натуральный показатель активности + дизайна.

---

## Слой 4: Boolean-Drift Per-Struct (состояние, накопленный эффект)

### Что это?

Парсим все структуры из заголовков → считаем bool-поля → для каждой структуры с ≥4 bool-полями гоним `git blame` на каждое поле → считаем **сколько разных коммитов** внесли bool в эту структуру. Фильтр: ≥ MIN_COMMITS (по умолчанию 4) = **constraint decay**.

### Формат

**`perstruct_drift_new185.csv`** — строка на структуру:
```
repo,struct,nfields,ncommits,span_days,first,last,is_config,file
circt,LoweringOptions,22,5,735,2024-06-01,2026-05-26,1,include/.../Options.h
```

### Сигналы

- **nfields** — сколько bool-полей в структуре (>= MIN_FIELDS=4)
- **ncommits** — сколько РАЗНЫХ коммитов их добавляли (>= MIN_COMMITS=4)
- **span_days** — временной диапазон (первый bool – последний bool)
- **is_config=1** — структура похожа на конфиг/сумку параметров (имя содержит config/opts/params)

### Примеры Top Decay (735+ дней = 2+ года)

| Репа | Структура | Полей | Коммитов | Span, дн | Период |
|---|---|---|---|---|---|
| llvm/circt | LoweringOptions | 22 | 5 | 735 | 2024-06-01 → 2026-05-26 |
| Phobos-dev | ExtData | 22 | 15 | 734 | 2024-06-02 → 2026-06-06 |
| qt/qtmultimedia | QWasmVideoOutput | 11 | 6 | 733 | ? → ? |
| sailfishos/qt | QWasmVideoOutput | 11 | 6 | 733 | ? → ? |
| intel/compute | MockDebugSession | 18 | 4 | 732 | 2024-06-04 → 2026-04-20 |

### Что это значит?

**Constraint decay** (термин Dente et al. — у них это деградация LLM-агентов под накоплением структурных ограничений; применение к контрактам структур — наша экстраполяция): структура хорошо определена (контракт), потом **постепенно размывается** (каждый разработчик добавляет свой bool флажок, потому что:
- он не хочет рефакторить всю структуру
- новый флаг «локален» и не ломает старый код
- через 2 года получается монстр с 20+ флаго́в, из которых половина мёртвая

### Статистика (455 структур с decay)

- **Средний span**: 400–600 дней (1.5+ года)
- **Средний nfields**: 8–15 bool-полей
- **Средний ncommits**: 5–8 разных коммитов

---

## Интеграция: от теории к практике

### Как они связаны?

```
Коммит добавляет:
├─ Graph-drift (include-ребро) ──→ архитектурная деградация
├─ Bool-field (в структуру) ──────→ локальная сложность
├─ Copypaste (клон кода) ─────────→ техдолг поддержки
└─ Span расширяется ──────────────→ constraint decay накапливается
```

### Одновременное воздействие

Пример: **facebook/react-native**
- Graph-errors: **278** (архитектура заплывает)
- AI%: **0.3** (старый базовый код, в окне май25–май26 почти нет AI)
- Bool-commit: **не в топе** (состояние чистое)
- Copypaste: **104 пары** (клоны есть, но неагентские)

→ Вывод: это **чистый architectural drift**, не state/AI-дрейф, а просто большой проект с наследством.

---

## Методология: почему именно эти четыре?

1. **Независимость** — каждый слой ловит другой сигнал, невыводимый из остальных
2. **Объективность** — парсим из git log и заголовков, без семантического анализа
3. **Масштабируемость** — работают даже на 185 агентских репах без OOM (после стриминг-фикса)
4. **Окно**: все в shallow-since=2025-05-01 (консистентны по времени)
5. **Валидность**: рост локальной сложности при AI-разработке подтверждён CMU MSR 2026 (arXiv 2511.04427, +41.6% cognitive complexity); constraint decay — Dente et al., EURECOM (arXiv 2605.06445); bool-drift — собственный результат archcheck (1.6–2.3×, `boolean_state_agentic_vs_not.md`), прямых аналогов в литературе не найдено; copypaste — классика (Juergens, ICSE 2009)

---

## Ограничения и интерпретация

### Shallow History Bias
Клоны репы `--shallow-since=2025-05-01` видят только коммиты с мая 2025 → були, добавленные раньше, не видны как отдельные коммиты: per-struct git blame схлопывает их в один граничный коммит (консервативно — занижает ncommits, абсолютные числа = нижняя граница).

### В окне ≠ вызвано AI
Окно включает и агентских (с коммитом), и обычных разработчиков. Тенденция: **если bool-drift высокий И ai%>20% ИЛИ copypaste выше baseline** → подозрение на AI-влияние.

### Контроль
Для каждого сигнала нужен baseline (старые агентские vs старые неагентские, до мая 2025). CSVы содержат флаг `agentic=1`, поэтому группировка по трудозатратам простая.

---

## Файлы и воспроизведение

- **Скрипты**: `experiments/ai_repo_run/generate_per_commit_graph_drift.py`, `experiments/boolean_state/bool_history_scan.py`, `perstruct_drift_all.py`, `experiments/ai_repo_run/make_examples.py`
- **Данные**: `experiments/ai_repo_run/EXAMPLES_50.md`, `experiments/ai_repo_run/corpus_summary.tsv`, `experiments/boolean_state/bool_history_new185.csv`, `perstruct_drift_new185.csv`
- **Воспроизведение**: `bash experiments/RESUME_pending.sh` (идемпотентен для недосчитанного)

---

_Окончательно 2026-06-11 (атрибуция источников поправлена 2026-06-11: CMU ≠ Dente et al.). Эта структура (4 слоя) используется как basis для constraint-decay analysis v1 в исследованиях archcheck._
