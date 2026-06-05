# [CORE][CLI][DOCS][BACKLOG] Полное выравнивание контрактов, scope и research-хвостов

**Дата создания:** 2026-06-05
**Дата старта:** 2026-06-05
**Статус:** wip
**Модуль:** CORE / CLI / DOCS / BACKLOG / RESEARCH-HYGIENE
**Приоритет:** critical
**Сложность:** XL
**Блокирует:** —
**Заблокирован:** —
**Related:** #045 (docs_sync_roadmap_mvp_spec), #051 (config_loader_v1), #073 (tech_debt_alignment_cleanup), #075 (mvp_v1_trusted_diff_workflow)
**Источник истины:** [docs/architecture-spec.md](../../docs/architecture-spec.md), [docs/ROADMAP.md](../../docs/ROADMAP.md), [docs/product_vision.md](../../docs/product_vision.md), [docs/config_format.md](../../docs/config_format.md)

## Цель

Одной большой задачей выровнять **реальное поведение**, **публичные CLI/док-контракты**,
**источники истины для агентов**, **backlog-состояние** и **границу между продуктом и research**.

Это не feature-task. Это **alignment + cleanup umbrella** по результатам аудита:
убрать ложные обещания, dead/stale следы, placeholder-ветки и рассинхрон между
документами, кодом и backlog.

Работа допускает **частичные коммиты**. Коммитим по срезам, но держим один общий
контекст и один acceptance contract.

## Почему это одна задача, а не россыпь мелких

Проблемы здесь связаны причинно:

- docs обещают то, чего нет в runtime;
- runtime surface уже включает preview/research участки;
- backlog и research-доки описывают разные состояния одних и тех же веток;
- AGENTS/source-of-truth для агента конфликтует с реальным деревом;
- часть cleanup-решений зависит от product decision, а не от механической правки текста.

Если разнести это на 5-10 микро-задач, потеряется главный смысл: надо не просто
«починить ссылки» или «обновить help», а **свести продукт к честному, самосогласованному состоянию**.

## TL;DR

Сейчас основной риск `archcheck` не в графовом ядре, а в том, что система уже
выглядит более зрелой, чем она есть на самом деле:

- `--config` выглядит как runtime-enforcement, но фактически это loader + thresholds;
- часть документов всё ещё обещает `file:line:column`, хотя модели `Violation` для
  этого нет;
- `AGENTS.md` живёт в альтернативной реальности pre-implementation;
- duplication/research слой частично попал в shipped tree и user-facing surface;
- есть placeholder/no-op реализации, включённые в production build;
- backlog и markdown-ссылки содержат stale references к удалённым артефактам.

Пока это не сведено, любая новая фича будет повышать стоимость сопровождения быстрее,
чем реальную ценность продукта.

## Контекст аудита

Задача собрана по итогам полного прохода по:

- core docs (`architecture-spec`, `MVP`, `ROADMAP`, `product_vision`, `config_format`);
- agent instructions (`AGENTS.md`);
- shipped code (`include/`, `src/`, `tests/`, `scripts/`);
- backlog (`new/`, `wip/`, `completed/`);
- исследовательским документам, которые уже влияют на product narrative.

Использованный критерий:

> искать не «ИИ писал / не ИИ писал», а проверяемые признаки AI-assisted accretion:
> обещания без реализации, placeholder-логика, scope creep, stale ветки, doc-code drift,
> local duplicate plumbing и test-harness, маскирующийся под product path.

## Основные проблемы

### 1. `--config` обещает больше, чем реально делает

Текущее состояние:

- CLI help пишет `validate .archcheck.yml v1 and run check`;
- loader реально парсит `modules`, `layers`, `independence`, `forbidden`;
- runtime из `Config` использует только `thresholds` для дефолтных intrinsic-rule limits.

Следствие:

- пользователь получает видимость product contract без actual enforcement;
- это опаснее обычного TODO, потому что поведение выглядит завершённым;
- любые integration/docs примеры на `.archcheck.yml` сейчас вводят в заблуждение.

Что нужно решить:

- либо честно понизить `--config` до validate-only surface;
- либо довести config-rules до реального runtime pipeline;
- промежуточного «вроде работает» состояния быть не должно.

### 2. Контракт diagnostics расходится с моделью данных

Текущее состояние:

- spec и MVP обещают `file:line:column`;
- `Violation` хранит только `ruleId`, `file`, `line`, `message`;
- text/json reporters физически не могут вывести `column`.

Следствие:

- публичный контракт шире доменной модели;
- baseline/json schema уже начинают цементировать неполную форму;
- дальше будет больнее вводить точные suppression/location-based semantics.

Что нужно решить:

- либо честно свести контракт к `file[:line]`;
- либо расширить `Violation` и весь output path до реального `column`.

### 3. Shipped tree содержит placeholder/no-op исследовательский код

Подозрительные зоны:

- `src/scan/duplication/fp_corpus_eval.cpp` — placeholder-метрика, фактически
  «предположим, что всё TP»;
- `phase12HeaderImplGate()` в duplication scanner — заглушка;
- рядом есть changelog/doc wording, которые выглядят как будто это уже полноценная фича.

Следствие:

- пользователю и будущему разработчику трудно понять, что production-grade, а что нет;
- review становится сложнее: no-op logic маскируется под feature;
- preview/research код тянется в core build без явной границы зрелости.

Что нужно решить:

- либо вынести такие куски из shipped path;
- либо очень жёстко промаркировать experimental/placeholder статус;
- либо дожать их до реального поведения, если они уже считаются shipped.

### 4. `AGENTS.md` и code-style truth конфликтуют между собой и с реальностью

Текущее состояние:

- `AGENTS.md` всё ещё пишет `pre-implementation`, «нет src/CMake/tests»;
- `AGENTS.md` требует `_name` и `I*`, а `docs/code_style.md` фиксирует `name_`
  и explicitly запрещает `I`-prefix как целевой стиль нового кода;
- сам код уже следует не тому, что написано в `AGENTS.md`.

Следствие:

- агент, следующий `AGENTS.md`, системно будет вносить не те решения;
- источник истины для coding agents не просто устарел, а **опасно устарел**;
- это прямой канал для нового slop и повторного рассинхрона.

Что нужно решить:

- привести `AGENTS.md` к фактическому состоянию;
- убрать оттуда дублирующие нормы, если они уже живут в `docs/code_style.md`;
- оставить один согласованный agent-facing contract.

### 5. Документы описывают несколько разных продуктов одновременно

Текущее состояние:

- `architecture-spec` описывает long-form продукт и v0.2+/future surface;
- `ROADMAP` уже пытается вернуть фокус к trusted drift core;
- `product_vision` отдельно говорит, что duplication — noisy preview/research;
- `MVP` всё ещё выглядит как старый pre-#006 документ;
- `README` и `CHANGELOG` местами уже догнали код, местами тащат исторические формулировки.

Следствие:

- новый человек не может быстро понять, что правда shipped, а что ещё идея;
- разные документы дают разные product stories;
- часть решений уже принята, но не дошла до всех слоёв документации.

Что нужно решить:

- зафиксировать один текущий product story;
- синхронизировать MVP/README/ROADMAP/spec;
- убрать обещания v0.2/v0.3 как будто это уже v0.1 core.

### 6. Backlog содержит stale narrative и битые следы удалённых веток

Текущее состояние:

- есть WIP/new/completed документы, ссылающиеся на удалённые `experiments/*`;
- markdown-check показал десятки локальных битых ссылок;
- часть duplication narrative уже отменена архитектурными решениями, но жива в backlog.

Следствие:

- backlog перестаёт быть надёжной системой состояния;
- historical notes выглядят как живые планы;
- человек тратит время на чтение того, чего уже физически нет в дереве.

Что нужно решить:

- почистить ссылки;
- перевести отменённые/исторические ветки в честное состояние;
- разделить «история решения» и «актуальная работа».

### 7. Research/preview слой слишком глубоко сидит в shipped surface

Текущее состояние:

- `--duplication` уже есть в help и CLI;
- duplication-подсистема собрана в `archcheck_core`;
- при этом собственные product docs говорят, что duplication пока preview/research;
- часть research tests завязана на hardcoded corpus paths и даже не входит в hermetic CI build.

Следствие:

- граница между product core и исследованием размазана;
- user-facing surface уже шире trust bar;
- каждое изменение duplication-кода влияет на shipped binary сильнее, чем хотелось бы.

Что нужно решить:

- решить, это уже продуктовая preview-фича или internal research path;
- под это решение перестроить CLI/help/docs/build/test hygiene;
- не держать “полушипнутую” зону без явного статуса.

### 8. Есть локальный accretion smell: большие оркестрационные файлы и дубли plumbing

Текущее состояние:

- `src/main.cpp` уже application-god-file;
- git subprocess/fork-exec plumbing повторено в нескольких translation units;
- duplication scanner вырос в длинный phase-pipeline с большим количеством локальных эвристик.

Следствие:

- дальнейшие изменения становятся дороже и рискованнее;
- policy decisions расползаются по нескольким местам;
- легко получить режимные расхождения.

Что нужно решить:

- разрезать application orchestration;
- схлопнуть дубли общего plumbing, если это можно сделать без лишних абстракций;
- оставить после cleanup более ясную границу между core logic и command wiring.

## Что входит в задачу

### A. Product contract alignment

- `--config`: принять честное решение по semantics.
- Diagnostics contract: выбрать и выровнять `file[:line]` vs `file:line:column`.
- Проверить, что help/README/spec не обещают невозможное.
- Проверить, что changelog не называет shipped то, что пока placeholder/preview.

### B. Runtime/code cleanup вокруг этих контрактов

- Убрать placeholder/no-op код из shipped critical path или честно маркировать.
- Разрезать `main.cpp`, если это нужно для честного выравнивания CLI semantics.
- Убрать явные policy duplicates и расхождения.

### C. Docs alignment

- Обновить `AGENTS.md`.
- Обновить `README.md`, `docs/MVP.md`, `docs/ROADMAP.md`, `docs/product_vision.md`,
  `docs/architecture-spec.md`, `docs/config_format.md` только в нужной степени.
- Не переписывать всё подряд; менять только то, что участвует в продуктовых контрактах.

### D. Backlog/research hygiene

- Почистить stale и broken references.
- Нормализовать статус удалённых/отменённых веток.
- Ясно развести product/current/research/historical.

## Что НЕ входит

- Новые архитектурные фичи поверх текущего ядра.
- Большой semantic/libclang expansion.
- Новый duplication detector, новый rule engine или новая preview-платформа.
- Переписывание половины проекта “заодно”.
- Рефакторинг ради эстетики без прямой пользы для контрактов.

## Порядок коммитов

Ниже не обязательные exact commit names, а **рабочие срезы**, которыми можно двигаться.

### Slice 1 — product truth first

#### Зачем нужен этот slice

Это стартовый срез. Его задача не в том, чтобы сразу переписать код, а в том,
чтобы **остановить ложные обещания** в пользовательском surface. После него
help, `README` и ключевые документы должны описывать ровно то, что реально есть.

#### Что читать перед началом

- [docs/architecture-spec.md](../../docs/architecture-spec.md)
- [docs/MVP.md](../../docs/MVP.md)
- [README.md](../../README.md)
- [docs/config_format.md](../../docs/config_format.md)
- [src/main.cpp](../../src/main.cpp)
- [include/archcheck/rules/violation.h](../../include/archcheck/rules/violation.h)

#### Главные вопросы этого slice

Нужно явно принять два решения:

1. Что в текущей версии реально означает `--config`.
2. Какой diagnostics contract честный на текущем этапе:
   `file`, `file:line` или `file:line:column`.

Пока эти решения не зафиксированы, дальше идти нельзя. Иначе последующие коммиты
будут строиться на разных предположениях.

#### Что именно делаем

1. Сравнить обещания в help и документах с реальным поведением кода.
2. Выписать, что уже работает, что только парсится, а что пока не исполняется.
3. Поправить только критичные точки входа:
   - CLI help;
   - `README`;
   - те продуктовые документы, которые прямо вводят пользователя в заблуждение.
4. Переписать формулировки так, чтобы они были короткими и однозначными.

#### Что НЕ делать

- Не доделывать тут же весь runtime по `--config`, если это отдельная работа.
- Не переписывать весь `architecture-spec.md`.
- Не чистить backlog и research-доки в этом коммите.
- Не добавлять optimistic wording в стиле "скоро будет", если это не нужно.

#### Как понять, что slice завершён

- Пользовательский surface больше не врёт.
- Между help, `README` и ключевыми продуктными документами нет прямого конфликта.
- В задаче явно записано, какое решение принято по `--config` и diagnostics.

#### Что должно попасть в коммит

- Минимальные правки help и docs, убирающие ложные обещания.
- Без большого кодового cleanup.

#### На что смотреть в review

- Не осталось ли скрытых двусмысленных формулировок.
- Не появились ли новые обещания сверх того, что реально есть.
- Понятно ли джуну, где факт, а где future work.

### Slice 2 — runtime alignment

#### Зачем нужен этот slice

После первого среза документы уже честные. Теперь нужно сделать так, чтобы
**код точно совпадал с зафиксированным контрактом**.

#### Что читать перед началом

- [src/main.cpp](../../src/main.cpp)
- [src/rules/rule_set.cpp](../../src/rules/rule_set.cpp)
- [include/archcheck/rules/violation.h](../../include/archcheck/rules/violation.h)
- [src/report/text_reporter.cpp](../../src/report/text_reporter.cpp)
- [src/report/json_reporter.cpp](../../src/report/json_reporter.cpp)
- тесты вокруг config/reporters/violations

#### Что именно делаем

1. Привести runtime по `--config` к решению из `Slice 1`.
2. Привести `Violation`, reporters и связанные схемы к выбранному diagnostics contract.
3. Убрать или изолировать placeholder/no-op код, который выглядит как готовая feature.

Практическая логика такая:

- если `--config` сейчас только валидирует и применяет `thresholds`,
  код не должен создавать впечатление полноценного rules runtime;
- если контракт diagnostics теперь без `column`,
  код, вывод и docs должны жить без `column`;
- если `column` всё-таки считается обязательным,
  его надо дотянуть по всей цепочке, а не только в одном месте.

#### Что НЕ делать

- Не тянуть сюда cleanup backlog и AGENTS.
- Не раздувать задачу до нового rule engine.
- Не рефакторить архитектуру ради красоты.

#### Как понять, что slice завершён

- Код и help отвечают на вопрос про `--config` одинаково.
- Модель `Violation` соответствует публичному diagnostics contract.
- Placeholder/no-op участки больше не маскируются под shipped semantics.

#### Что должно попасть в коммит

- Runtime-правки.
- При необходимости точечные тестовые правки под честный контракт.

#### На что смотреть в review

- Не осталось ли второго скрытого поведения.
- Не введены ли новые временные заглушки.
- Не появился ли новый лишний слой абстракции.

### Slice 3 — source-of-truth cleanup

#### Зачем нужен этот slice

Сейчас агентские инструкции конфликтуют и с кодом, и с остальными документами.
Этот срез нужен, чтобы **человек и агент стартовали из одной реальности**.

#### Что читать перед началом

- [AGENTS.md](../../AGENTS.md)
- [docs/code_style.md](../../docs/code_style.md)
- [docs/code_quality.md](../../docs/code_quality.md)
- [docs/architecture-spec.md](../../docs/architecture-spec.md)
- текущее дерево `src/`, `tests/`, `CMakeLists.txt`

#### Что именно делаем

1. Обновить `AGENTS.md` под фактическое состояние репозитория.
2. Убрать из него заведомо ложные утверждения:
   - про `pre-implementation`;
   - про отсутствие `src/`, `tests`, `CMakeLists.txt`;
   - про устаревшие style-rules, если они больше не соответствуют проекту.
3. Свести style/naming guidance к одному источнику истины.

Здесь важно:

- если правило уже зафиксировано в `docs/code_style.md`,
  в `AGENTS.md` лучше дать короткую согласованную выжимку или ссылку;
- нельзя держать рядом два разных набора правил для нового кода.

#### Что НЕ делать

- Не превращать `AGENTS.md` в дубликат всей документации.
- Не переписывать весь style guide без необходимости.
- Не менять код только ради того, чтобы он подстроился под устаревший `AGENTS.md`.

#### Как понять, что slice завершён

- Агент, читающий `AGENTS.md`, больше не будет автоматически ошибаться.
- Между `AGENTS.md` и `docs/code_style.md` нет прямых противоречий.
- Документ короткий, практичный и отражает текущее дерево проекта.

#### Что должно попасть в коммит

- Правки `AGENTS.md`.
- При необходимости точечные уточнения в одном-двух связанных docs.

#### На что смотреть в review

- Не остались ли исторические утверждения как будто они ещё актуальны.
- Нет ли теперь опасного дубляжа правил.
- Понятно ли из документа, как реально работать в репозитории сейчас.

### Slice 4 — backlog and link hygiene

#### Зачем нужен этот slice

Backlog и docs должны быть навигацией по текущему состоянию проекта. Сейчас часть
ссылок битая, а часть исторических хвостов выглядит как активный план.

#### Что читать перед началом

- [backlog/README.md](../../backlog/README.md)
- затронутые файлы в `backlog/new/`, `backlog/wip/`, `backlog/completed/`
- документы, где уже были найдены broken local links

#### Что именно делаем

1. Починить локальные markdown-ссылки там, где это часть текущей темы.
2. Убрать ссылки на удалённые `experiments/*`, если они поданы как current reference.
3. Развести текущее состояние и исторические заметки.

Рабочее правило:

- если ссылка должна вести на живой файл, исправляем её;
- если ссылка ведёт в прошлое, но прошлое важно, оставляем краткую historical note;
- если ссылка больше не нужна, удаляем её.

#### Что НЕ делать

- Не переписывать все backlog-файлы подряд.
- Не менять смысл старых задач, если проблема только в ссылках и статусах.
- Не открывать новые продуктовые решения в рамках link hygiene.

#### Как понять, что slice завершён

- Затронутые файлы больше не ссылаются в пустоту.
- Historical notes не выглядят как current roadmap.
- Backlog снова можно читать как рабочую систему состояния.

#### Что должно попасть в коммит

- Правки ссылок.
- Точечные правки формулировок и статусов в backlog/docs.

#### На что смотреть в review

- Не потерян ли полезный исторический контекст.
- Не появились ли новые битые ссылки из-за механической замены.
- Ясно ли различаются `current`, `historical`, `cancelled`, `research`.

### Slice 5 — preview/research boundary

#### Зачем нужен этот slice

Duplication сейчас находится в промежуточной зоне: он уже виден пользователю,
но ещё не выглядит как полностью доверенная часть core. Этот срез проводит
**чёткую границу зрелости**.

#### Что читать перед началом

- [docs/product_vision.md](../../docs/product_vision.md)
- [docs/ROADMAP.md](../../docs/ROADMAP.md)
- [src/main.cpp](../../src/main.cpp)
- [src/CMakeLists.txt](../../src/CMakeLists.txt)
- duplication-related код и тесты в `src/scan/duplication/` и `tests/`

#### Главный вопрос этого slice

Нужно выбрать один статус duplication:

- internal research;
- preview;
- shipped capability.

Один и тот же ответ должен читаться из:

- help;
- docs;
- build;
- tests;
- changelog;
- backlog.

#### Что именно делаем

1. Зафиксировать выбранный статус duplication.
2. Привести пользовательский surface к этому статусу.
3. Привести build и test hygiene к этой же границе зрелости.
4. При необходимости убрать research-artifacts из тех мест, где они выглядят как
   обычный product path.

Практически это означает:

- preview должен быть явно помечен как preview;
- research не должен маскироваться под обычную shipped feature;
- shipped capability не может держаться на placeholder и non-hermetic хвостах.

#### Что НЕ делать

- Не улучшать сам алгоритм duplication "заодно".
- Не строить новую preview-platform.
- Не тащить сюда unrelated cleanup.

#### Как понять, что slice завершён

- По duplication во всех слоях сказано одно и то же.
- Пользователь не принимает noisy preview за trusted core.
- Build/test/doc surface больше не спорят между собой.

#### Что должно попасть в коммит

- Правки help/docs/build/test hygiene.
- Только те кодовые изменения, которые нужны для честной границы статуса.

#### На что смотреть в review

- Не осталась ли полушипнутая зона без явного статуса.
- Не потерян ли доступ к internal research workflow, если он ещё нужен.
- Достаточно ли явно отделён trusted core от preview/research.

### Slice 6 — final alignment pass

#### Зачем нужен этот slice

Это финальный проход. Его задача не открыть новый фронт работ, а убедиться,
что предыдущие коммиты действительно сложились в одну систему.

#### Что читать перед началом

- diff всех предыдущих slice-коммитов;
- финальные версии help, `README`, ключевых docs и затронутых runtime-файлов;
- эту задачу целиком

#### Что именно делаем

1. Пройти цепочку "обещание -> код -> вывод -> документация -> backlog".
2. Добить остаточные противоречия в терминах, статусах и формулировках.
3. Обновить итоговые секции самой задачи, чтобы она фиксировала результат,
   а не только процесс.

Проверяемые вопросы:

- help не врёт?
- код делает именно это?
- reporters выводят именно этот контракт?
- docs не спорят между собой?
- backlog не ссылается на старую реальность?

#### Что НЕ делать

- Не запускать новый большой рефакторинг.
- Не добавлять новую функциональность.
- Не открывать новые product-направления.

#### Как понять, что slice завершён

- Один и тот же вопрос, заданный по help, docs и коду, получает один и тот же ответ.
- После чтения репозитория не возникает ощущения, что внутри "три разных продукта".
- В задаче зафиксированы итоговые решения и сознательно оставленные вне scope хвосты.

#### Что должно попасть в коммит

- Мелкие финальные выравнивания.
- Обновление самой задачи до итогового диагностического состояния.

#### На что смотреть в review

- Не появились ли последние маленькие исключения, которые снова создают drift.
- Ясно ли, что специально оставлено вне этой umbrella-задачи.
- Можно ли после этого безопасно возвращаться к feature-work.

## Acceptance criteria

- [ ] `--config` больше не создаёт ложного впечатления runtime-enforcement.
- [ ] Diagnostics contract (`line`/`column`) одинаков в коде и документах.
- [ ] В shipped core не остаётся placeholder/no-op логики, выдаваемой за готовую feature.
- [ ] `AGENTS.md` соответствует фактическому состоянию репозитория и актуальному style contract.
- [ ] `README`, `MVP`, `ROADMAP`, `product_vision`, `architecture-spec` не противоречат друг другу по текущему shipped surface.
- [ ] Broken local markdown links, найденные в рамках этой темы, либо исправлены, либо переведены в честные historical notes.
- [ ] Research/preview ветки больше не маскируются под product core без явной маркировки.
- [ ] После завершения задачи новый разработчик может за один проход понять:
      что shipped, что preview, что research, что future.

## Риски

### 1. Случайно расширим scope

Есть риск превратить cleanup в «перепишем полпроекта». Нельзя.

Правило:

- если правка не уменьшает ложный контракт, stale state или research drift,
  значит это не часть этой задачи.

### 2. Сломаем уже работающий core

Особенно опасно вокруг:

- baseline formats;
- JSON/report contract;
- CLI exits and help;
- preview duplication wiring.

Правило:

- сначала фиксируем решение по контракту, потом меняем код;
- без параллельного “ещё чуть-чуть улучшим архитектуру”.

### 3. Начнём лечить документы, не решив product decision

Например, по `--config` нельзя сначала переписать все docs, если не решено:
это validate-only или enforcement.

Правило:

- сначала decision, потом массовая синхронизация.

### 4. Смешаем historical note и current plan

Нельзя просто удалять следы старых решений, если они нужны как история.

Правило:

- устаревшее либо архивируем как historical,
- либо переводим в completed/obsolete note,
- но не держим как будто это текущая работа.

## Диагностика

Сигналы, что задача закрыта хорошо:

- help/README/spec больше не требуют устных оговорок;
- grep по ключевым контрактам не выдаёт взаимоисключающих формулировок;
- локальная проверка markdown-ссылок не показывает битые ссылки в затронутой зоне;
- в core build не остаются явные placeholder-комментарии на shipped path, кроме
  осознанно оставленных и явно помеченных research stubs;
- при чтении backlog видно, какие ветки живые, а какие исторические.

Сигналы, что задача закрыта плохо:

- `--config` всё ещё “как будто работает”;
- docs по-прежнему говорят `file:line:column`, а код нет;
- `AGENTS.md` всё ещё конфликтует с `docs/code_style.md`;
- duplication остаётся одновременно “preview only” и “core shipped” без явного решения;
- broken links и ссылки на удалённые experiments остаются в current docs/WIP.

## Решения, которые нужно принять в ходе задачи

### Решение 1 — `--config`

Выбрать одно:

1. **Validate-only сейчас**
   Плюсы: честно, быстро, соответствует v0.1/v1 trusted core.
   Минусы: user-facing surface временно слабее ожиданий.

2. **Довести enforcement сейчас**
   Плюсы: закрывает долг полностью.
   Минусы: это уже почти feature work, риск выйти за scope.

Текущая рекомендация:

- **validate-only сейчас**, enforcement отдельной фазой после cleanup.

**ПРИНЯТО (2026-06-05): validate-only сейчас.** `--config` валидирует `.archcheck.yml`
и применяет `thresholds`; enforcement `modules/layers/independence/forbidden` —
отдельной фазой после cleanup. Help/docs понижаем до validate-only.

### Решение 2 — diagnostics contract

Выбрать одно:

1. **Снизить обещание до `file[:line]`**
2. **Расширить модель и output до `column`**

Текущая рекомендация:

- не принимать “полумеру”; выбрать одно явно и провести сквозь docs/code/schema.

**ПРИНЯТО (2026-06-05): понизить до `file:line`.** `Violation` уже хранит только
`file`+`line`; `column` убираем из spec/MVP/AGENTS/README. Расширение модели до
`column` отложено (semantic expansion вне scope этой umbrella).

### Решение 3 — duplication status

Выбрать одно:

1. preview surface остаётся user-facing, но честно маркируется;
2. duplication временно уходит из product-facing help/story;
3. duplication поднимается до продукта и тогда перестаёт жить как полу-research.

Текущая рекомендация:

- оставить **preview**, но сделать это везде одинаково и без фальшивой product-finality.

**ПРИНЯТО (2026-06-05): поднять до продукта** (расходится с рекомендацией —
осознанное решение пользователя). Duplication перестаёт быть preview/research и
становится shipped capability.

**Важное следствие для порядка работ:** снять ярлык `preview` нельзя, пока в
shipped path остаются placeholder/no-op (`src/scan/duplication/fp_corpus_eval.cpp`,
`phase12HeaderImplGate()`) и non-hermetic тесты на hardcoded corpus paths — иначе
промоушен создаёт *новое* ложное обещание. Поэтому:

- **Slice 1** трогает только `--config` и diagnostics (безопасные doc/help контракты);
- **Slice 2** убирает/дожимает placeholder-код duplication;
- **Slice 5** проводит промоушен `preview → shipped` по всем слоям
  (help/docs/build/tests/changelog/backlog) — только после того, как код стал честным.

## Прогресс

### Slice 1 — product truth first ✅ (2026-06-05)

Зафиксированы три решения (см. секцию «Решения»): `--config` → validate-only;
diagnostics → `file:line`; duplication → поднять до продукта (после code-cleanup).

Выровнен user-facing surface по двум безопасным контрактам (`--config`, diagnostics):

- `src/main.cpp` — help для `--config`: «validate + apply thresholds; module rules not yet enforced».
- `docs/config_format.md` — добавлена секция «Enforcement status (current release)»:
  module rules валидируются, но не enforced; runtime применяет только `thresholds`.
- `docs/MVP.md` — diagnostics `file:line:column` → `file:line` (+ пояснение про модель `Violation`).
- `docs/architecture-spec.md` — diagnostics `file:line:column` → `file:line`.

Build archcheck зелёный. README уже был честен (`file:line`, config-rules помечены v0.2) — не трогали.
`AGENTS.md:30` (`file:line:column`) сознательно оставлен на Slice 3 (полный rewrite AGENTS).

### Slice 2 — runtime alignment ✅ (2026-06-05, verification-only)

Проверка показала: **runtime уже соответствовал обоим контрактам Slice 1** — лгали только docs.
Кодовых правок не потребовалось.

- **`--config` validate-only:** `makeDefaultRuleSet` ([src/rules/rule_set.cpp](../../src/rules/rule_set.cpp))
  читает только `config.thresholds` (godHeaderFanIn, chainLength). `modules/layers/independence/forbidden`
  парсятся в `Config`, но runtime их не трогает — полуподключённого enforcement-stub нет.
- **diagnostics `file:line`:** `Violation` = {ruleId, file, line, message};
  `text_reporter` печатает `file:line: [rule] msg`; `json_reporter` — поля rule/file/line/message;
  `violation_baseline` — triple (ruleId, file, line). `column` отсутствует во всей output-цепочке.
- **placeholder/no-op:** grep по `src/`/`include/` подтвердил — **весь** placeholder-код
  duplication-scoped (`phase12HeaderImplGate`, `fp_corpus_eval`); вне duplication shipped-путь чист.

**Решение по placeholder (важное):** `phase12HeaderImplGate()` — no-op в боевом
пайплайне `scanForDuplication`, НО docs (`duplication_fp_analysis.md`) и `CHANGELOG`
заявляют P1.3 как работающий классификатор («-4 FP», «70%», «✅»). Удалить код,
оставив доки врать → новый drift. Поэтому phase12 + `fp_corpus_eval` + реконсиляция
P1.3-нарратива/CHANGELOG переносятся в **Slice 5** (duplication→product), где код и
нарратив выравниваются одним проходом.

### Slice 3 — source-of-truth cleanup ✅ (2026-06-05)

`AGENTS.md` переписан целиком под фактическое состояние (по правилу «устаревший
.md переписывать целиком, не клеить outdated-баннеры»):

- убрано ложное `pre-implementation` / «нет src/CMake/tests» → реальное v0.1-состояние
  с указателем на `CHANGELOG` как authoritative по shipped-набору;
- diagnostics `file:line:column` → `file:line` (закрыт хвост Slice 1/2 в AGENTS);
- naming приведён к `docs/code_style.md`: `name_` (не `_name`), методы/функции
  `lowerCamelCase` (старое «public PascalCase» было неверно), интерфейсы без `I`-prefix
  для нового кода (legacy `IRule` помечен как не-цель, rename вне scope);
- **стиль больше не дублируется** — AGENTS даёт короткую выжимку + указатель на
  `code_style.md` как единственный источник истины (убран двойной набор правил);
- подсистемы переведены в present tense (существуют); deps зафиксированы (`ryml`,
  Catch2 v3, не «или»); `clang_scanner` помечен v0.2;
- убран устаревший pre-impl bootstrap-порядок;
- правило «не запускай сборку» → «сборку можно свободно, Debug по умолчанию»
  (снят конфликт с `CLAUDE.md`);
- имя продукта зафиксировано `archcheck` (убран открытый вопрос брендинга);
- все markdown-ссылки проверены — ведут на живые файлы.

**Следующий шаг:** Slice 4 — backlog/link hygiene (битые локальные ссылки,
ссылки на удалённые `experiments/*`, развести current/historical/cancelled).

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| `backlog/wip/082_crt_full_alignment_cleanup_umbrella.md` | umbrella-задача + зафиксированы 3 решения, лог Slice 1 |
| `src/main.cpp` | Slice 1: help `--config` понижен до validate-only |
| `docs/config_format.md` | Slice 1: секция «Enforcement status (current release)» |
| `docs/MVP.md` | Slice 1: diagnostics `file:line:column` → `file:line` |
| `docs/architecture-spec.md` | Slice 1: diagnostics `file:line:column` → `file:line` |
| `AGENTS.md` | Slice 3: полный rewrite под фактическое состояние, naming → code_style.md, build-правило → CLAUDE.md |
