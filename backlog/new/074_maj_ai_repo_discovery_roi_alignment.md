# [RESEARCH][DISCOVERY] Свести discovery-техдолг по ROI-фильтрам, giant-skip и resumable pipeline

**Дата создания:** 2026-06-02
**Дата старта:** —
**Статус:** new
**Модуль:** RESEARCH / SCAN / DISCOVERY
**Приоритет:** major
**Сложность:** M
**Блокирует:** быстрый добор AI-плотного корпуса без лишнего расхода сети/диска/времени
**Заблокирован:** —
**Related:** #054 (ai_repo_duplication_run), #066 (airepo_remeasure_clonefail), #067 (overnight_eye_verification), #073 (tech_debt_alignment_cleanup)
**Источник истины:** `experiments/ai_repo_run/measure_candidates.sh`, `experiments/ai_repo_run/remeasure_resumable.sh`, `experiments/ai_repo_run/clone_expand.sh`, `experiments/ai_repo_run/resume_all.sh`

## Цель

Убрать лишний расход времени на giant/established репозитории и свести к одному
реальному контракту discovery/resume pipeline для AI-реп корпуса.

Задача не про новые метрики, а про **ROI и честность пайплайна**:

- не качать заведомо бесполезные гиганты;
- тянуть только ту историю, которую реально меряем;
- различать `skip` и `CLONEFAIL`, а не смешивать их;
- убрать расхождение между тем, что написано в `#066`, и тем, что реально в коде.

## Симптом

Сейчас discovery уже частично оптимизировался, но оптимизации размазаны и живут в
разных ветках пайплайна:

1. В `resume`/`remeasure` есть giant name-filter.
2. В основном `measure`-пути giant-filter нет.
3. В `clone_expand` есть size API precheck, но это уже поздняя стадия.
4. `measure_candidates.sh` всё ещё делает обычный `git clone --filter=blob:none`
   без `--shallow-since=2025-05-01`.
5. При падении клона `measure_candidates.sh` сразу пишет `CLONEFAIL`, не пытаясь
   отличить «репа не кандидат / нет свежей истории» от реального сетевого сбоя.

Итог:

- тратим время на giant-репы типа `chromium`/`llvm`/`webkit`-класса;
- часть `CLONEFAIL` шумовая и не несёт полезного сигнала;
- resume-путь и основной путь ведут себя по-разному;
- документация в `#066` уже обещает оптимизации, которых в коде нет.

## Подтверждённое расхождение

Проверка состояния на 2026-06-02:

- `resume_all.sh` действительно поднимает resumable-пайплайн после reboot.
- giant name-filter реально есть только в `remeasure_resumable.sh`.
- `measure_candidates.sh` **не** использует `--shallow-since=2025-05-01`.
- `measure_candidates.sh` **не** делает API-fallback, чтобы развести `skip` и `CLONEFAIL`.
- `clone_expand.sh` использует API только для precheck размера, не для measure-фазы.
- При этом `backlog/wip/066_...` уже описывает `shallow-since` и API-fallback как внедрённые.

Это уже не просто perf TODO, а **research-methodology debt + backlog/code mismatch**.

## Почему это важно

Для текущей фазы нам не нужен «репрезентативный срез всех больших C++-реп».
Нам нужен корпус с максимальной вероятностью полезного сигнала на единицу времени.

Giant established репы дают плохой ROI:

- дороги в скачивании и ретраях;
- часто имеют низкую AI-концентрацию;
- вероятно проходят через сильный review/process barrier;
- дают меньше шансов на новый drift-signal, чем mid-size/high-velocity AI-плотные репы.

Значит pipeline должен быть заточен под:

- **high velocity**;
- **AI density**;
- **умеренный размер**;
- **ранний giant-skip**;
- **минимум ложного CLONEFAIL**.

## Что сделать

### 1. Протянуть `--shallow-since=2025-05-01` в реальный measure-path

- [ ] Добавить shallow-fetch/clone, ограниченный пост-май окном, в `measure_candidates.sh`.
- [ ] Проверить, что измерение `git log --since=2025-05-01` остаётся корректным на shallow-истории.
- [ ] Явно задокументировать fallback, если конкретная forge/репа не поддерживает нужный shallow-path.

### 2. Поднять giant denylist в основной funnel

- [ ] Вынести GIANT-regex в одно место.
- [ ] Применять giant-skip не только в `remeasure_resumable.sh`, но и в основном `measure/discovery` пути.
- [ ] Зафиксировать статус таких реп как `TOOBIG-skip`, а не как `CLONEFAIL`.

### 3. Ввести API-fallback для различения `skip` vs `CLONEFAIL`

- [ ] На падении shallow/clone делать один дешёвый API-check.
- [ ] Если у репы нет нужной свежей истории/она не кандидат — писать `skip`, а не `CLONEFAIL`.
- [ ] Только реальные сетевые/доступные сбои оставлять как `CLONEFAIL`.

### 4. Свести основной и resumable пути к одному контракту

- [ ] Убрать ситуацию, где resume-путь умнее основного.
- [ ] Проверить `discover_finish*.sh`, `measure_candidates.sh`, `remeasure_resumable.sh`, `finish_resumable.sh` на единые правила отбора.
- [ ] Сделать так, чтобы reboot/resume не менял semantics отбора, а только продолжал ту же работу.

### 5. Привести backlog/doc state к реальности

- [ ] Обновить `#066`: отделить «внедрено в коде» от «задумано / нужно внедрить».
- [ ] Если часть оптимизаций остаётся только в resumable-ветке, это должно быть явно написано.
- [ ] Убрать ложный контракт из task log, чтобы следующий запуск не исходил из неверных предпосылок.

## Критерии приёмки

- [ ] `measure_candidates.sh` больше не тянет полную историю, если достаточно пост-май окна.
- [ ] giant-репы отсекаются до тяжёлого клона как в обычном, так и в resumable режиме.
- [ ] `CLONEFAIL` после прогона заметно чище: там остаются реальные сбои, а не «не кандидат».
- [ ] В коде и backlog одинаково описано, какие оптимизации реально работают.
- [ ] Есть один понятный pipeline discovery, а не два похожих, но неравных по логике.

## Не делать в этой задаче

- не менять сами drift-метрики;
- не вводить ML/классификатор AI-реп;
- не расширять product-scope `archcheck`;
- не смешивать это с верификацией копипаст-precision.

## Следующие шаги

1. Сначала исправить alignment между `#066` и кодом.
2. Затем протащить `shallow-since` и giant-skip в основной `measure`-pipeline.
3. После этого уже пересчитать ROI discovery и обновить methodology report.
