# AI smell signals — наблюдаемые признаки AI-assisted accretion

_Справочник по признакам некачественного AI-assisted кода и документации.
Назначение файла — не «детектить авторство», а дать рабочую таксономию
наблюдаемых smell-сигналов, которые можно потом переводить в проверки,
review-чеклисты и backlog-задачи._

Связано с:

- [constraint_decay.md](constraint_decay.md) — почему проблема вообще существует;
- [ai_code_detection_landscape.md](ai_code_detection_landscape.md) — что измерили внешние работы;
- [ai_drift_cases.md](ai_drift_cases.md) — наши реальные drift-case'ы;
- [code_clones.md](code_clones.md) — дублирование как отдельный класс сигнала.

---

## 1. Что считается AI smell

Под **AI smell** в этом документе понимается не «код точно написан ИИ», а:

> наблюдаемый артефакт AI-assisted accretion — такой паттерн в коде, тестах,
> CLI, конфиге или документации, который статистически часто появляется при
> агентной/LLM-разработке и повышает риск structural drift, технического долга
> или ложного product-contract.

Ключевая мысль: **мы не атрибутируем авторство, мы оцениваем риск.**

Один и тот же smell может появиться:

- у человека без всякого ИИ;
- у джуна;
- у опытного разработчика, который торопился;
- у агента, который оптимизировал локальную задачу и не удержал глобальные
  инварианты.

Поэтому правильный вопрос не «это AI-код?», а:

> есть ли здесь smell, который делает систему менее честной, менее понятной
> или более связной, чем должна быть?

---

## 2. Что этот документ НЕ делает

### 2.1. Не определяет авторство

Признак smell-а не доказывает, что код написан ИИ.

Сильный сигнал AI-assistance для репозитория — это:

- явные git-маркеры;
- agent config files;
- self-declared attribution.

Это отдельно разобрано в [ai_code_detection_landscape.md](ai_code_detection_landscape.md).

### 2.2. Не превращает archcheck в общий style-linter

Многие smell-ы можно увидеть глазами, но не все из них нужно автоматически
проверять в `archcheck`.

Для `archcheck` интересны прежде всего smell-ы, которые:

- выражаются как архитектурный инвариант;
- имеют явный авторитетный источник;
- статически проверяемы;
- помогают держать CI-границы, а не просто стиль.

### 2.3. Не считает prose-сигналы достаточными

Подозрительная документация, overly-generic wording, одинаковые фразы в
комментариях и прочие «текстовые вайбы» сами по себе слишком слабы.

Сильный сигнал — это **код + контракт + структура**.

---

## 3. Откуда взята таксономия

Файл синтезирует четыре типа источников:

1. **Empirical papers про AI-код**
   - EURECOM `Constraint Decay` — structural constraints деградируют сильнее
     чисто функциональных задач.
   - `AI-Generated Smells` (arXiv:2605.02741) — у AI-кода есть различимые
     code-level, structural и architectural smell-ы.
   - `Debt Behind the AI Boom` (arXiv:2603.28592) — основная масса найденных
     AI-introduced issues относится к maintainability/code smells.
2. **Vendor/industry measurements**
   - GitClear — рост дублирования и падение moved/refactored code.
3. **Review guidance**
   - GitHub Docs советуют ревьюить AI-код через intent, integration,
     edge-cases, tests и maintainability, а не через попытку угадать автора.
4. **Наши наблюдения по репозиторию и drift-прогонам**
   - shortcut edges;
   - preview/research leakage;
   - contract drift;
   - placeholder/no-op shipped paths.

---

## 4. Уровни уверенности

Полезно разделять признаки по силе сигнала.

| Уровень | Что это значит | Пример |
|---|---|---|
| **H1 strong structural** | smell прямо виден как нарушение структуры или контракта | новый shortcut edge, fan-out spike, parse-but-don't-enforce config |
| **H2 medium local** | smell виден локально, но возможны нормальные объяснения | near-clone, wrapper without value, giant orchestration file |
| **H3 weak prose/process** | smell заметен в нарративе/тестах/доках, но без кода сигнал слабее | stale docs, review-theater tests, generic TODO framing |

Для автоматизации в `archcheck` приоритетны **H1**, затем часть **H2**.

---

## 5. Карта smell-ов и применимость к archcheck

| ID | Smell | Тип | Уверенность | Проверяемость |
|---|---|---|---|---|
| **AIS.1** | Promise > implementation | contract drift | H1 | review / repo-internal |
| **AIS.2** | Parse-but-don't-enforce | fake-config support | H1 | repo-internal |
| **AIS.3** | Placeholder/no-op in shipped path | dead semantics | H1 | text/AST/review |
| **AIS.4** | Dead surface: flag accepted, output mode unused, schema field inert | false surface | H1 | text/review |
| **AIS.5** | Near-clone families | duplication | H2 | duplication backend |
| **AIS.6** | Parallel branch explosion | local copy-paste schema | H2 | duplication/text |
| **AIS.7** | Shortcut edge across layers | architecture drift | H1 | graph/rules/drift |
| **AIS.8** | Fan-out explosion / god consumer | coupling growth | H1 | graph/rules |
| **AIS.9** | Fan-in hub / god header/file | bottleneck accretion | H1 | graph/rules |
| **AIS.10** | Chain-length / blast-radius growth | hidden propagation cost | H1 | graph/metrics/drift |
| **AIS.11** | Duplicate plumbing / zero-value wrappers | accidental indirection | H2 | text/review |
| **AIS.12** | Giant orchestration / context sink | accumulation hotspot | H2 | metrics/review |
| **AIS.13** | Research leakage into product path | maturity drift | H1 | review/repo-internal |
| **AIS.14** | Source-of-truth fragmentation | doc-code drift | H2 | docs/review |
| **AIS.15** | Test theater / scaffolding tests | false confidence | H2 | review |

`Проверяемость` в таблице означает:

- **graph/rules/drift** — хороший кандидат на продуктовую проверку `archcheck`;
- **duplication backend** — кандидат в duplication/research слой;
- **repo-internal/review** — smell важный, но скорее для dogfood-аудита, чем
  для user-facing default rule.

---

## 6. Подробные признаки

### AIS.1. Promise > implementation

**Суть.** Публичный surface обещает capability, которой нет в runtime.

**Как выглядит:**

- help обещает режим работы, который код не выполняет;
- spec или README обещают поле/семантику, которой нет в модели данных;
- changelog пишет "added", хотя по факту это partial scaffold.

**Почему типично для AI-assisted работы:**

- модель очень хорошо строит внешний contract first;
- внешняя форма часто появляется раньше честной semantics;
- человек видит красивый CLI/docs слой и недооценивает глубину незавершённости.

**Чем опасно:**

- пользователь доверяет несуществующей функции;
- тесты могут проходить на superficial path;
- debt растёт не как TODO, а как ложное обещание.

**False positives:**

- обычная незавершённая фича человека;
- миграционный период между двумя контрактами.

**Для archcheck:** smell важный, но в основном для dogfood и repo-audit.
Это не хорошее дефолтное правило для чужих репозиториев.

### AIS.2. Parse-but-don't-enforce

**Суть.** Конфиг, флаг или схема парсятся и валидируются, но не влияют на
реальное поведение.

**Как выглядит:**

- loader знает структуру YAML/JSON;
- тесты проверяют parsing/validation;
- runtime использует только малую часть данных или не использует вовсе.

**Почему типично:**

- агенту легко "дотянуть" парсер и тесты на синтаксис;
- доведение до semantic enforcement требует глобального понимания pipeline.

**Чем опасно:**

- это самая коварная форма feature-theater;
- формально "поддержка есть", фактически её нет.

**Для archcheck:** очень сильный внутренний smell. В пользовательский rule set
обычно не выносится, но должен регулярно ловиться dogfood-аудитом.

### AIS.3. Placeholder / no-op в shipped path

**Суть.** В production path лежит заглушка, которая выглядит как feature.

**Как выглядит:**

- функция всегда возвращает default/empty result;
- phase hook существует, но ничего не делает;
- внутри shipped pipeline есть комментарий уровня "temporary / assume / stub".

**Почему типично:**

- агент охотно достраивает scaffold до "компилируется и выглядит завершённо";
- semantic finishing step откладывается.

**Чем опасно:**

- ревьюер недооценивает незавершённость;
- команда начинает строить поверх заглушки новые ожидания.

**Для archcheck:** частично проверяемо текстово, но чаще это review-smell.

### AIS.4. Dead surface

**Суть.** Surface расширен, но реальной поведенческой мощности не прибавилось.

**Как выглядит:**

- флаг принимается, но не влияет на результат;
- schema/output mode объявлен, но не заполняется;
- public option есть, но downstream path не подключён.

**Отличие от AIS.3:** здесь основной smell не в заглушке функции, а в ложном
расширении интерфейса.

**Для archcheck:** полезно для внутренних аудитов CLI и format contracts.

### AIS.5. Near-clone families

**Суть.** Появляются локальные семейства почти одинаковых кусков кода, которые
отличаются 1-2 идентификаторами, литералами или условием.

**Как выглядит:**

- повторяющиеся блоки 5+ строк;
- одинаковая структура ветвей;
- те же вызовы в другом порядке или с другими именами.

**Почему типично:**

- агент оптимизирует "получить working code fast";
- abstraction-after-the-fact почти не происходит автоматически;
- GitClear как раз фиксирует рост copy/paste и падение moved/refactored code.

**Чем опасно:**

- размножаются места изменений;
- потом agent продолжает копировать уже испорченный паттерн.

**Для archcheck:** хороший кандидат для duplication/research слоя, но не для
trusted mandatory gate без точной калибровки FP.

### AIS.6. Parallel branch explosion

**Суть.** Один и тот же шаблон размножен по `if/else`, `switch`, табличным
инициализациям и похожим блокам.

**Как выглядит:**

- 4+ sibling-ветки с почти идентичным телом;
- изменяется только литерал, enum, field name или один вызов.

**Почему типично:**

- агент предпочитает локально понятную развертку вместо новой абстракции;
- особенно часто в UI bindings, config-fill code, per-sensor/per-field logic.

**Для archcheck:** скорее duplication-normalization / smell-classifier, не
базовый архитектурный rule.

### AIS.7. Shortcut edge across layers

**Суть.** Для быстрой локальной задачи добавляется прямое ребро через
архитектурную границу: слой начинает зависеть от того, от чего раньше не зависел.

**Как выглядит:**

- новый `#include` через слой;
- UI напрямую тянет preferences/widgets/data-access;
- generic/util начинает зависеть от feature namespace.

**Почему типично:**

- агент выбирает кратчайший путь к нужному символу;
- без внешнего guardrail-а architectural route почти всегда проигрывает local convenience.

**Чем опасно:**

- это канонический structural drift;
- такие рёбра быстро становятся нормой и начинают самовоспроизводиться.

**Для archcheck:** один из лучших кандидатов. Уже хорошо ложится на graph rules,
module constraints и DRIFT-rules.

### AIS.8. Fan-out explosion / god consumer

**Суть.** Компонент начинает напрямую зависеть от слишком многих соседей.

**Как выглядит:**

- `.cpp` или header тянет десятки project includes;
- orchestration unit превращается в сборщик всех подсистем сразу;
- один модуль «знает про всё».

**Почему типично:**

- агент добавляет очередной include/dep без cost model для coupling;
- refactoring к промежуточному abstraction layer чаще не происходит.

**Чем опасно:**

- растёт coupling even without cycles;
- компонент становится дорогим для изменений и тестирования.

**Для archcheck:** отличный кандидат на простую graph-проверку:
`Lakos.GodComponentFanOut`.

### AIS.9. Fan-in hub / god header/file

**Суть.** Слишком много кода начинает зависеть от одного узла.

**Как выглядит:**

- header с очень высоким fan-in;
- utility header постепенно становится dumping ground.

**Почему типично:**

- агент любит "положить рядом с похожим";
- как только файл стал удобной точкой, он начинает притягивать новые обязанности.

**Для archcheck:** уже существует как `Lakos.GodHeader`. Это не AI-specific
правило, но очень полезный guardrail против AI-driven accretion.

### AIS.10. Chain-length / blast-radius growth

**Суть.** Даже без циклов и явных violation-ов растёт глубина и радиус влияния.

**Как выглядит:**

- include chains становятся длиннее;
- изменение одного компонента потенциально касается всё большего числа узлов;
- связность `edges/nodes` растёт быстрее, чем это можно оправдать новой архитектурой.

**Почему типично:**

- агент успешно добавляет рабочие связи, но не платит стоимость за глобальную
  propagation complexity;
- это "тихий дрейф", который не заметен по одному PR.

**Для archcheck:** очень хороший кандидат для report-only метрик и drift-gates.

### AIS.11. Duplicate plumbing / zero-value wrappers

**Суть.** В репозитории размножаются локальные pipework/helpers/обёртки,
которые добавляют мало или ноль новой semantics.

**Как выглядит:**

- две почти одинаковые функции запуска subprocess/file IO/path normalization;
- thin wrapper просто прокидывает вызов ниже;
- одинаковый lifecycle code повторяется в нескольких TUs.

**Почему типично:**

- агент чаще копирует существующий локальный шаблон, чем ищет точку объединения;
- "сделать ещё один helper" дешевле по локальной задаче, чем переразложить модуль.

**Чем опасно:**

- policy drift между копиями;
- мелкие расхождения начинают вести себя как bugs.

**Для archcheck:** чаще review/audit smell, чем user-facing default rule.

### AIS.12. Giant orchestration / context sink

**Суть.** Один файл или класс становится местом, куда agent продолжает
складывать новые ветки управления.

**Как выглядит:**

- `main.cpp`, `dispatcher.cpp`, `pipeline.cpp`, `manager.cpp` быстро разрастаются;
- phase-style нумерация (`phase7`, `phase12`, `step9`) живёт слишком долго;
- файл знает и про CLI, и про policy, и про reporting, и про file-system.

**Почему типично:**

- агенту выгодно продолжать писать туда, где уже есть контекст;
- локально это дешёвый путь, глобально — future maintenance sink.

**Для archcheck:** можно мерить size/degree вспомогательно, но как дефолтное
правило — осторожно, чтобы не превратить продукт в length-linter.

### AIS.13. Research leakage into product path

**Суть.** Экспериментальный или noisy слой попадает в shipped CLI/build/docs
раньше, чем достигнут trust bar.

**Как выглядит:**

- preview/research capability рекламируется как обычная feature;
- internal harness лежит рядом с обычными тестами;
- placeholder evaluator попадает в core build.

**Почему типично:**

- агент и человек легко "доводят до работоспособности", но не ставят явную
  границу зрелости;
- внешняя форма фичи появляется раньше policy о её статусе.

**Для archcheck:** это скорее repo-governance smell, но для самого archcheck
он критичен.

### AIS.14. Source-of-truth fragmentation

**Суть.** В репозитории одновременно живут несколько разных правд про продукт.

**Как выглядит:**

- `AGENTS.md`, spec, roadmap и README описывают разные состояния;
- naming/style rules расходятся;
- backlog и docs ссылаются на удалённые ветки как на текущие.

**Почему типично:**

- agent-assisted работа ускоряет производство артефактов;
- разные слои обновляются с разной скоростью;
- никто не платит цену за выравнивание narrative сразу.

**Для archcheck:** не rule для пользователя, а важнейший smell для dogfood-а.

### AIS.15. Test theater / scaffolding tests

**Суть.** Тесты создают ощущение надёжности, но проверяют не то, что реально
важно для пользовательского контракта.

**Как выглядит:**

- тестируется parsing/creation, а не enforcement/behavior;
- тест подтверждает наличие объекта/флага, а не effect;
- integration path без fixtures заменён локальной синтетикой, которая не ловит
  реальную семантическую дыру.

**Почему типично:**

- агенту проще быстро добрать coverage на shallow checks;
- superficial green tests маскируют контрактные дыры.

**Для archcheck:** это review-smell; частично лечится обязательными fixtures и
dogfood-политикой.

---

## 7. Что особенно важно для archcheck

Не все smell-ы одинаково полезны для нашего продукта. Для `archcheck` высокий
приоритет имеют сигналы, которые:

- выражаются графом зависимостей;
- связаны с structural drift;
- не требуют гадать про намерение автора;
- дают мало ложных срабатываний;
- объяснимы пользователю через Lakos / Core Guidelines / drift story.

### Приоритет P1 — можно и нужно переводить в проверки

- **AIS.7 Shortcut edge**
- **AIS.8 Fan-out explosion**
- **AIS.9 Fan-in hub**
- **AIS.10 Chain-length / blast-radius growth**

Это хорошие кандидаты для `graph/rules/drift` слоя.

### Приоритет P2 — можно переводить, но осторожно

- **AIS.5 Near-clone families**
- **AIS.6 Parallel branch explosion**
- **AIS.12 Giant orchestration**

Здесь уже выше риск FP или размывания продуктового scope.

### Приоритет P3 — важно для dogfood и репо-аудита, но не для user rules

- **AIS.1 Promise > implementation**
- **AIS.2 Parse-but-don't-enforce**
- **AIS.3 Placeholder/no-op**
- **AIS.4 Dead surface**
- **AIS.13 Research leakage**
- **AIS.14 Source-of-truth fragmentation**
- **AIS.15 Test theater**

---

## 8. Что НЕ стоит объявлять AI smell-ом

Нельзя делать smell-ом то, что слишком слабо связано с архитектурным риском.

Плохие кандидаты:

- просто длинный комментарий;
- просто английские generic названия;
- просто большой PR;
- просто много созданных файлов;
- просто "код выглядит слишком аккуратно";
- просто наличие `AGENTS.md` или `CLAUDE.md`;
- просто следы атрибуции в git.

Это может говорить об AI-assistance, но почти ничего не говорит о качестве.

---

## 9. Практический вывод

Если задача — удерживать архитектуру в эпоху агентной разработки, то бороться
нужно не с «авторством ИИ», а с **машинно-типичными траекториями деградации**:

- локально удобное ребро через слой;
- незаметный рост связности;
- копипаст вместо консолидации;
- поверхностный feature surface без настоящего enforcement;
- утечка эксперимента в продуктовую поверхность.

Именно поэтому для `archcheck` самые полезные AI-smell checks — это не
стилометрия и не authorship detection, а **graph/drift/coupling guardrails**.

---

## 10. Что из этого можно превратить в простые проверки первым

Если выбирать дешёвые и честные проверки с минимальным scope creep, порядок
такой:

1. **`Lakos.GodComponentFanOut`**
   - простой graph-rule;
   - уже зафиксирован в спеке как planned;
   - очень хорошо ловит AI-типичный "consumer knows too much".
2. **Report-only метрики `edges/nodes` и `max blast radius`**
   - хороший ранний сигнал тихого дрейфа;
   - можно показать пользователю без жёсткого gate.
3. **Drift-gate на рост blast radius**
   - сильный regression signal, когда появится уверенность в порогах.
4. **Дальше уже duplication-кандидаты**
   - только после отдельной стабилизации FP.

---

## Источники

- GitHub Docs: [Review AI-generated code](https://docs.github.com/en/enterprise-cloud%40latest/copilot/tutorials/review-ai-generated-code)
- EURECOM / Dente, Satriani, Papotti: [Constraint Decay](https://arxiv.org/abs/2605.06445)
- Zhu, Tsantalis, Rigby: [AI-Generated Smells](https://arxiv.org/abs/2605.02741)
- Aymen et al.: [Debt Behind the AI Boom](https://arxiv.org/abs/2603.28592)
- GitClear: [AI Code Quality 2025](https://www.gitclear.com/ai_assistant_code_quality_2025_research)
- Практическое обзорное поле по детекции и датасетам:
  [ai_code_detection_landscape.md](ai_code_detection_landscape.md)
