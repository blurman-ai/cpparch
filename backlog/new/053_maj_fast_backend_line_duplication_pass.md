# [RULES][SCAN] Fast-backend line-based duplication pass (порт проверенного прототипа)

**Дата создания:** 2026-05-29
**Дата старта:** —
**Статус:** new
**Модуль:** RULES / SCAN
**Приоритет:** major
**Сложность:** L (порт алгоритма + plumbing + fixtures + perf note)
**Целевой релиз:** v0.2
**Блокирует:** —
**Заблокирован:** —
**Related:** #006 (spec_refactor — fast backend как часть двух-бекендной схемы), #033 (ai_drift_dataset — duplication как сильный AI-drift сигнал), #052 (cross_tu_duplication_detector — AST-слой, точный и дорогой)

## Цель

Добавить в fast backend дешёвый snapshot-проход Type-1 line duplication: без
libclang, без `compile_commands.json`, с репортингом только кросс-компонентных
дублированных блоков как **missing reuse edge** и с метрикой duplication ratio.

## Контекст

Детекция дублей в archcheck должна строиться тремя слоями по цене и точности:

1. **Эта задача:** line-based Type-1, дёшево, мгновенно, на любой build-системе.
2. **#052:** AST `CloneDetector` Type-2/3, точно, но дорого.
3. **Будущее:** IR-canon для рерайтов «другими словами».

Этот проход нужен как fast-backend сигнал для проектов без
`compile_commands.json`: даже если семантический бэкенд недоступен, репозиторий
должен получить хотя бы дешёвую проверку на verbatim copy-paste. В словаре
archcheck это та же рамка, что и у AST-слоя: **кросс-компонентный
дублированный блок = missing reuse edge** в смысле Lakos physical design.

Атрибуция:

- **Lakos** — duplication между компонентами означает отсутствие reuse-edge и
  ухудшение physical design.
- **GitClear** — headline-метрика «блоки из 5+ дублированных строк» и
  duplication ratio как эмпирическая мотивация.

Проход должен быть **off by default**. После настройки FP-порогов его можно
будет отдельно обсуждать как кандидата в fast-defaults.

## Что уже подтверждено прототипом (НЕ переоткрывать)

Стартовая точка — рабочий `copypaste.py`, прогнанный на `fmt` и `spdlog`.
Портировать логику **1:1**, не передизайнивая алгоритм.

### Алгоритм

- Поток значимых строк на файл:
  - пропускать пустые строки;
  - пропускать чисто-комментарные строки;
  - именно **пропускать**, а не заменять пустышками, чтобы комментарии и пустые
    строки не рвали матч.
- Нормализация:
  - `strip`;
  - схлопывание пробелов (`\s+` -> ` `).
- Скользящее окно из `N` значимых строк (`N=5` по умолчанию).
- Хеш каждого окна быстрым non-crypto хешем.
- Группировка окон по хешу; `>= 2` вхождений = seed duplicated window.
- Слияние в **максимальные блоки** обязательно:
  - критерий схлопывания: одна пара файлов, одна диагональ (`ia - ib`),
    перекрытие;
  - без этого один регион раздувается в пачку сдвинутых окон.
- Duplication ratio:
  - числитель = значимые строки, покрытые хотя бы одним дублированным окном;
  - знаменатель = все значимые строки;
  - эта метрика надёжна, а raw-count блоков хрупок.
- Ignore-mask:
  - игнорировать окна, состоящие целиком из тривиальной scaffolding-лексики
    (`{}`, `;`, `else`, `break;`, `return;`, `public:` и т.п.);
  - игнорировать окна с суммарной длиной текста меньше примерно `60` символов.
- Default excludes:
  - `third_party`
  - `bundled`
  - `generated`
  - `*_generated*`
  - `*.pb.*`

### Подтверждённые референс-цифры

Нужны для sanity-check порта, но **не как pixel-perfect assert**:

- `fmt/include` -> ratio около `2.07%`, `0` cross-file block.
- `fmt` whole tree -> ratio около `2.24%`, `8` cross-file block.
- `spdlog/include` -> ratio около `4.50%`, `29` cross-file block.
- На `spdlog/include` должны всплывать известные пары:
  - `daily_file_sink.h` <-> `hourly_file_sink.h`
  - `systemd_sink.h` <-> `syslog_sink.h`

Отдельно важно: `spdlog` тянет `fmt/bundled/`, поэтому `bundled` обязан быть в
дефолтном exclude-листе.

## Non-goals (YAGNI)

- Не делать Type-2 через нормализацию идентификаторов. Это работа #052.
- Не делать per-commit / temporal GitClear-метрику. Только snapshot по дереву.
- `--changed-files` — это фильтр отчёта, а не сужение анализируемого корпуса.
- Не добавлять семантический / IR-уровень.
- Не оставлять Python в продукте. Нужен чистый C++20 single-binary путь.

## План выполнения

### 1. Порт алгоритма в fast backend

- [ ] Добавить `src/scan/line_dup_scanner.h/.cpp` как сосед `include_scanner`.
- [ ] Работать без libclang и без `compile_commands.json`.
- [ ] Читать файлы напрямую и строить `vector<SigLine{string normalized; unsigned lineno;}>`.
- [ ] Держать индекс окон как `unordered_map<uint64_t, vector<pair<FileId, uint32_t>>>`.
- [ ] Реализовать слияние seeds в максимальные блоки по `(file_a, file_b, ia - ib, overlap)`.
- [ ] Взять быстрый non-crypto hash (`xxhash`, `wyhash` или аналог), не crypto.

### 2. Обернуть в одно правило

- [ ] Добавить один `IRule` под `src/rules/duplication/`.
- [ ] Зарегистрировать статически, без универсального rule-engine.
- [ ] Если #052 уже вынес общий helper для component mapping / duplication reporting,
      переиспользовать его; если нет, не тянуть большой refactor ради симметрии.

### 3. Маппинг блоков в компоненты

- [ ] Маппить найденные блоки в компоненты через `graph/component_graph`.
- [ ] По умолчанию репортить только **кросс-компонентные** блоки.
- [ ] Формулировка нарушения:
      `"missing reuse edge: A and B share a <N>-line block"`.
- [ ] Внутрифайловые и внутрикомпонентные блоки либо подавлять, либо скрывать за verbosity.

### 4. Конфиг и CLI

- [ ] Включение только за флагом: `--fast-duplication` и/или `defaults.line_duplication`.
- [ ] Дефолтное состояние: **off**.
- [ ] Поддержать thresholds:
  - `min_lines` (default `5`)
  - `min_window_chars` (default около `60`)
  - `exclude` glob-список
- [ ] Уважать `// archcheck: allow`.
- [ ] Считать duplication ratio.
- [ ] `--changed-files <list>` трактовать как фильтр на вывод, но корпус всегда строить по всему дереву.

### 5. Вывод

- [ ] Протащить результат в text/json/sarif reporters.
- [ ] В text/json/sarif выводить:
  - duplication ratio;
  - `file:line <-> file:line`;
  - длину блока в значимых строках.

### 6. Perf note

- [ ] Прогнать на `fmt`, `spdlog` и одном большом дереве (`opencv` или `llvm`).
- [ ] Зафиксировать `time + peak RSS` в `PERF-fast.md`.
- [ ] Не делать из этого жёсткий CI-gate; это проверка здравости, а не контракт производительности.

## Тесты

- [ ] **A / clean:** один файл без дублей -> ratio `0`, `0` блоков.
- [ ] **B / cross-file:** два файла с общим блоком `>= 5` строк -> найден
      один кросс-компонентный блок, длина верна, блок **слит в максимальный**.
- [ ] **C / trivial-only:** совпадают только скобки и keyword-scaffolding ->
      `0` блоков.
- [ ] **D / vendored:** дубли в `bundled/` исключаются по умолчанию.
- [ ] **Regression / loose:** на `fmt/include` ratio около `2%` и `0`
      cross-file; на `spdlog/include` находится блок `daily` <-> `hourly`.

## Критерий приёмки

- [ ] Чистый C++20, без libclang, без `compile_commands.json`, без Python.
- [ ] Слияние seeds в максимальные блоки реализовано; нет инфляции сдвинутых окон.
- [ ] Репортятся именно кросс-компонентные блоки как missing reuse edge.
- [ ] Duplication ratio попадает в отчёт и в json/sarif.
- [ ] Vendored/generated исключены по умолчанию.
- [ ] `// archcheck: allow` уважается.
- [ ] Проход за флагом, off by default.
- [ ] Один `IRule`, переиспользует существующий plumbing там, где он уже есть.
- [ ] Тесты B/C/D зелёные.
- [ ] Свободная регрессия на `fmt` / `spdlog` подтверждает, что порт не уехал от прототипа.

## Сделано

- (пусто)

## В работе

- (пусто)

## Следующие шаги

1. Уточнить, живёт ли флаг как `--fast-duplication`, как `--duplication=line`,
   или как общий umbrella рядом с #052.
2. Проверить, появился ли к моменту старта общий duplication-reporting helper из #052.
3. Перед стартом реализации положить рядом в задачу или в `experiments/`
   краткий reference-run по `copypaste.py`, чтобы порт сверять не на память.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Fast-pass только Type-1 | дешёвый zero-setup слой без libclang |
| Skip blanks/comment-only вместо blanking | разные пустые строки и комментарии не должны рвать матч |
| Слияние по file-pair + diagonal + overlap | без этого один регион распадается на множество сдвинутых окон |
| Duplication ratio важнее raw-count блоков | число блоков нестабильно и зависит от merge-эвристики |
| Off by default | нужен этап настройки FP до возможного промоушена в fast-defaults |
| Vendored excludes по умолчанию | иначе шум на `bundled/generated/third_party` ломает ценность сигнала |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `src/scan/line_dup_scanner.h/.cpp` | fast line-dup scanner |
| `src/rules/duplication/...` | одно правило поверх fast scanner |
| `src/report/text_reporter.cpp` | text output для duplication ratio и блоков |
| `src/report/json_reporter.cpp` | json output |
| `src/report/sarif_reporter.cpp` | sarif output |
| `src/main.cpp` / CLI plumbing | флаг включения и `--changed-files` |
| config schema/docs | `defaults.line_duplication`, thresholds, excludes |
| `fixtures/line_duplication/pass/` | clean / trivial / vendored pass-cases |
| `fixtures/line_duplication/fail_cross_component/` | один реальный cross-component clone-case |
| `tests/...` | unit/integration coverage |
| `PERF-fast.md` | time + peak RSS на референс-деревьях |

## Fixtures

- [ ] `fixtures/line_duplication/pass/no_duplicates_single_file/`
- [ ] `fixtures/line_duplication/pass/trivial_windows_only/`
- [ ] `fixtures/line_duplication/pass/bundled_excluded/`
- [ ] `fixtures/line_duplication/fail_cross_component/`

## Pointers

- Референс-алгоритм: `copypaste.py`, проверенный на `fmt` и `spdlog`.
- Смежная задача: #052 (`cross_tu_duplication_detector`) — AST-слой, точный, дорогой.
- Ключевой урок прототипа: merge в максимальные блоки обязателен; duplication ratio надёжнее, чем raw-count блоков.
