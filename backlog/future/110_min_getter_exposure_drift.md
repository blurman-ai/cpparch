# [SCAN] Getter exposure drift — новый геттер как сигнал дрейфа инкапсуляции

**Дата создания:** 2026-06-12
**Дата старта:** —
**Статус:** new
**Модуль:** SCAN][RULES
**Приоритет:** minor
**Сложность:** unknown
**Целевой релиз:** v0.3+
**Блокирует:** —
**Заблокирован:** —
**Related:** #090 (boolean_state_drift_metric, future), #095 (config_bag_growth), #042 (clang_semantic_backend — точная детекция аксессоров)

## Цель

Исследовать и формализовать «добавление нового публичного геттера к существующему классу»
как per-commit сигнал дрейфа инкапсуляции (encapsulation erosion), по образцу boolean-drift.

## Контекст

Идея пользователя (2026-06-12, сессия по corpus drift): новый геттер — тоже дрейф, но
«наружу», зеркальный boolean-drift:

- **bool-поле** = состояние течёт **внутрь** структуры (config-bag, флажки);
- **геттер** = состояние течёт **наружу** класса: внутреннее поле становится наблюдаемым,
  клиенты начинают зависеть от представления, инвариант «поле трогает только сам класс»
  умирает. Каждый геттер — ослабление контракта (constraint decay в терминах Dente et al.).

Механика деградации: getter → клиентский код строит логику на чужом состоянии (Feature
Envy) → цепочки `a.getB().getC().x()` (нарушение Law of Demeter) → класс постепенно
превращается в struct с церемониями, поведение размазывается по клиентам.

Семейство метрик то же, что #093–#100: дешёвый текстовый per-commit скан без семантики.

## Литобзор (выполнен 2026-06-12, веб-разведка)

**Главная находка: C.131 содержит готовую Enforcement-секцию** — *«Flag multiple `get` and
`set` member functions that simply access a member without additional semantics»*. То есть
правило подаётся не как наша самодеятельность, а как **темпоральная реализация официального
enforcement из Core Guidelines**. Критерий тривиальности из C.131: флагать только геттеры,
неотличимые от публичного поля; исключения — поддержание инварианта класса и конверсия
internal→interface type.

Атрибуция (всё VERIFIED по первоисточникам, кроме помеченных):

| Источник | Что даёт правилу |
|---|---|
| [Core Guidelines C.131](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#rh-get) | Enforcement-секция + критерий тривиальности + исключения. Нюанс: issue isocpp#776 — pushback практиков → аргумент за advisory |
| [Core Guidelines C.9](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#rc-private) | «Minimize exposure of members... simplifies maintenance» — заглавная атрибуция |
| Parnas, CACM 1972 | Accessing/modifying procedures — часть модуля, «not shared by many modules as is conventionally done»; раскрытая информация сужает пространство будущих изменений — дословно constraint decay |
| Riel 1996, Heuristic 3.3 | «Beware of classes that have many accessor methods... related data and behavior are not being kept in one place» — статическая форма нашей метрики |
| Fowler, [GetterEradicator](https://martinfowler.com/bliki/GetterEradicator.html) / [TellDontAsk](https://martinfowler.com/bliki/TellDontAsk.html) | «Encapsulation = hiding design decisions» + Data Class smell. **Граница**: сам Fowler против войны со всеми query-методами → флагать только тривиальные и только дельту |
| [Hyrum's Law](https://www.hyrumslaw.com/) | Механизм невозвратности: однажды выставленный геттер становится контрактом — убрать нельзя |
| Law of Demeter + Guo et al., WCRE 2011 (FOUND) | Нарушения LoD ↔ bug-proneness (Eclipse, logistic regression); JPL Pathfinder: интеграция LoD-нарушающих частей на порядок дороже |
| Khomh et al., EMSE 2012 (FOUND) | Антипаттерн ClassDataShouldBePrivate → статистически значимая change/fault-proneness (54 релиза ArgoUML/Eclipse/Mylyn/Rhino) |
| Zoller/Schmolitzky 2012, Vidal et al. 2016 (FOUND) | Over-exposure массов: ~20% методов / 25% классов публичнее необходимого (>3.6 MLOC Java) |
| Romano/Pinzger, ICSM 2011 (FOUND) | Fat interfaces: низкая usage cohesion интерфейса предсказывает churn — ближайшая опора для «рост публичной поверхности → нестабильность» |
| Izurieta/Bieman, ICST 2008 / SQJ 2012 (FOUND) | Design grime: распад дизайна = накопление мелких связей со временем — эволюционный фрейм всей drift-категории |
| [arXiv 2510.03029](https://arxiv.org/html/2510.03029v1) | **AI-аргумент**: encapsulation-смеллы в LLM-коде +118–165% против эталона (4 LLM, 1000 задач, Designite). Контрпример для честности: Molison ESEM 2025 — на Python/SonarQube LLM-код не хуже |

**Пробел в литературе = наша новизна**: работ про *добавление* аксессоров во времени
(per-commit дельта как предиктор) не найдено — ни в MSR, ни в API-evolution. Статическая
база есть, темпоральной версии нет ни у кого.

### Ответы на вопросы к литературе

- [x] Эмпирика «аксессоры ↔ дефекты»: да — Guo 2011 (LoD→bugs), Khomh 2012 (data exposure→faults), over-exposure 20–25%
- [x] Цитируемое обоснование default-правила: C.131 Enforcement (готовое!) + C.9 + Parnas + Riel 3.3
- [x] Эволюционные работы: прямых НЕТ (пробел=новизна); ближайшие — Romano/Pinzger (fat interfaces→churn), grime
- [x] AI-угол: encapsulation-смеллы +118–165% (arXiv 2510.03029) — вешать аргумент на это, не на «AI пишет плохо вообще»

## План выполнения

- [x] Литобзор — выполнен, см. выше
- [ ] Regex-прототип per-commit скана (аналог `bool_history_scan.py`):
      added-строки в существующих заголовках вида
      `Type getX() const { return x_; }` / `const T& x() const noexcept;` /
      объявление `getX() const;` + naming convention.
      **Скоуп строго по C.131: только тривиальные** (тело = `return field;`)
- [ ] Классы FP: computed getters (НЕ флагать — C.131 исключение), поддержание инварианта,
      конверсия типов, реализации интерфейсов, генерированный код, override
- [ ] Корпусный прогон: getter-add rate agentic vs human внутри смешанных реп
      (дизайн как у boolean-drift, repo fixed effects); проверка предсказания
      arXiv 2510.03029 (+118–165% encapsulation smells) на C++ корпусе
- [ ] Доля «голых» геттеров (`return field;`) vs computed среди новых
- [ ] Если сигнал есть → metric design doc: advisory «class X gained N trivial getters
      over M commits» (паттерн как config-bag growth #095); gate не предлагать

## Сделано

- 2026-06-12: литобзор (12 источников, 7 VERIFIED) — основание для правила есть,
  включая готовую Enforcement-формулировку C.131

## В работе

- (пусто)

## Следующие шаги

1. Прототип скана — после закрытия активной волны #093–#103.

## Ключевые решения

| Решение | Причина |
|---------|---------|
| Папка future, v0.3+ | Пользователь явно: «история на будущее» |
| Текстовый скан, не clang | Семейство дешёвых drift-метрик; clang-точность — отдельно в #042 |
| Advisory, не gate | Fowler против GetterEradicator-абсолютизма; issue isocpp#776; вероятностный сигнал |
| Только тривиальные геттеры (тело `return field;`) | Критерий C.131; computed/инвариантные — легитимны |
| Только дельта (новые), не статический счёт | Статический счёт = Riel 3.3, уже чужое; темпоральная версия — наша новизна |

## Изменённые файлы

| Файл | Изменение |
|------|-----------|
| — | — |
