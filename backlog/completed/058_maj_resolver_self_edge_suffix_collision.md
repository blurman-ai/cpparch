# [SCAN] Резолвер: ложное self-ребро при suffix-коллизии системного include

**Дата создания:** 2026-05-31
**Дата старта:** 2026-05-31
**Дата завершения:** 2026-05-31
**Статус:** completed
**Модуль:** SCAN
**Приоритет:** major
**Сложность:** S
**Блокирует:** —
**Заблокирован:** —
**Related:** #036 (ambiguous_mirror_dirs — тот же класс, ambiguous-вариант), #054 (ai_repo_duplication_run — откуда всплыл баг), #057 (lakos_fanout_coupling_checks — `[SELF]` детектор там в кандидатах)

## Цель

Убрать ложное ребро зависимости X→X, которое резолвер создавал, когда системный/
библиотечный `#include <name.h>` суффиксно совпадал с одноимённым проектным файлом.
Такое ребро рождало фантомный 1-узловой цикл и заставляло SF.9 врать.

## Контекст

Баг всплыл не из юнит-теста, а из исследовательского прогона #054: дешёвый
граф-детектор `[SELF]` ([experiments/ai_repo_run/graph_probe.py](../../experiments/ai_repo_run/graph_probe.py))
пометил self-include в 16 репах корпуса. Ручная проверка `grep`'ом показала: почти
все — **не** `#include "self"`, а системные угловые includes:

```bash
grep -n include _aidev_run/bitcoin/src/compat/cpuid.h   # 11: #include <cpuid.h>   (системный!)
grep -n include _aidev_run/esphome/.../md5/md5.h        # 18: #include <md5.h>     (НЕ свой)
grep -n include _aidev_run/FastLED/src/fl/stl/alloca.h  # 37: #include <alloca.h>  (системный)
```

### Первопричина (трасса для `bitcoin/src/compat/cpuid.h` + `<cpuid.h>`)

1. `resolve_angle` → `find_exact("cpuid.h")` — промах (в индексе полный путь
   `src/compat/cpuid.h`, не `cpuid.h`).
2. → `resolve_by_suffix` → `suffixIndex["cpuid.h"]` возвращает **сам исходник**
   (его путь оканчивается на `cpuid.h`).
3. `candidates.size() == 1` → `make_project(... candidates.front())` → **ребро X→X**.

Тот же класс, что #036, но #036 чинил *ambiguous* (2+ кандидата, mirror-фильтр);
здесь — **единственный** кандидат, поэтому фильтр не срабатывал.

### Масштаб (по корпусу `_aidev_run/`, 68 реп)

- **26 ложных self-рёбер**.
- **8 реп, где self-loop был ЕДИНСТВЕННЫМ «циклом»** (sccs_cyclic полностью ложный):
  bitcoin, cvxpy, KataGo, llama.cpp, nntrainer, ScalableVectorSearch,
  scenario_simulator_v2, whisper.cpp → SF.9 у них репортил несуществующий цикл.

## Решение

Минимальный неоспоримый инвариант: **компонент не зависит от себя**. Один guard в
`resolveInclude` (единственная точка после обоих путей резолва — ловит и
angle-suffix, и quote-dir-relative варианты):

- self-target (`files[target].path == sourceFile`) понижается до not-found тега:
  External для `<...>`, Unresolved для `"..."` (ровно то, чем системный заголовок
  и является);
- легитимное ребро на *другой* одноимённый файл (path ≠ source) не трогается.

## Сделано

- [x] Guard в `resolveInclude` ([src/scan/include_resolver.cpp](../../src/scan/include_resolver.cpp)).
- [x] +3 unit-теста ([tests/unit/scan/include_resolver_test.cpp](../../tests/unit/scan/include_resolver_test.cpp)):
      angle system-suffix → External; quote token-suffix → Unresolved; same
      basename в ДРУГОМ файле → по-прежнему Project (guard не переусердствует).
- [x] Сборка чистая; **тесты 260/260**; **lizard 0 предупреждений**.
- [x] Корпусная перепроверка после фикса: **self-edges 0/68**; все 8 фантомных
      циклов исчезли (cyclic=0); реальные клубки целы (mc2 56, OptiScaler 13,
      FastLED 11, acts 46, esphome 1 — на месте, т.е. настоящий 2-узловой цикл
      esphome не подавлен).
- [x] CHANGELOG.md — bullet в `### Fixed`.
- [x] GRAPH_PROBE_FINDINGS.md §3 — диагноз/фикс/проверка.

## Принцип работы

`resolveInclude` сначала вызывает `resolve_quote`/`resolve_angle` как раньше,
затем проверяет результат: если вердикт `Project` и целевой узел — это сам
`sourceFile`, ребро отбрасывается (понижение до External/Unresolved по виду
директивы). Проверка по полному repo-relative пути (`files[target].path ==
sourceFile`), поэтому одноимённый файл в другом каталоге остаётся валидной целью.
Guard единственный и стоит в общей точке, поэтому покрывает оба пути резолва без
дублирования.

## Чем управляется

Ничем — zero-config инвариант, порогов и флагов нет. Self-ребро не бывает
осмысленным ни как зависимость, ни как цикл.

## С чем связана

- **#036** — родственный баг (suffix-коллизия), но ambiguous-вариант (2+
  кандидата → mirror-dir фильтр). Этот фикс — single-candidate вариант, который
  #036 не покрывал.
- **#054** — исследовательский прогон, в котором детектор `[SELF]` вскрыл баг.
- **SF.9 / DRIFT.2** — потребители графа, которые получали фантомные циклы; после
  фикса перестают.

## Диагностика

Если снова появится подозрение на self-loop:
```bash
build/debug/src/archcheck --save-graph-baseline /tmp/g.txt <repo>
python3 experiments/ai_repo_run/graph_probe.py /tmp/g.txt <repo> | grep -A3 '\[SELF\]'
```
Любое срабатывание `[SELF]` теперь должно быть реальным `#include "self"` —
проверять `grep -n include <file>`: кавычки + тот же относительный путь = настоящий
дефект; угловые скобки = смотреть, не вернулась ли регрессия резолвера.

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `src/scan/include_resolver.cpp` | guard от self-edge в `resolveInclude` |
| `tests/unit/scan/include_resolver_test.cpp` | +3 теста (angle-self / quote-self / different-file) |
| `CHANGELOG.md` | bullet в `### Fixed` |
| `experiments/ai_repo_run/GRAPH_PROBE_FINDINGS.md` | §3 диагноз/фикс/корпусная проверка |
