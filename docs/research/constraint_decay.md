# Constraint Decay — источник мотивации для archcheck

Этот документ хранит ссылки и исчерпывающий пересказ двух материалов, которые стали
непосредственным поводом для создания archcheck. Читать, когда нужно вспомнить *зачем*.

---

## Источники

| Материал | Ссылка |
|---|---|
| Препринт (arxiv) | https://arxiv.org/abs/2605.06445 |
| HN-дискуссия | https://news.ycombinator.com/item?id=48256912 |

---

## Статья: «Constraint Decay: The Fragility of LLM Agents in Backend Code Generation»

**Авторы:** Francesco Dente, Dario Satriani, Paolo Papotti (EURECOM, Университет Базиликаты)
**Дата:** 7 мая 2026, препринт под рецензией.

### Суть в одной фразе

LLM-агенты хорошо генерируют рабочий код при свободной постановке задачи — но деградируют
при накоплении структурных ограничений (архитектурный паттерн, база данных, ORM). Авторы
назвали это *constraint decay*.

### Что измерялось

Авторы зафиксировали единый API-контракт (OpenAPI 3.0, 19 endpoint'ов, 291 assertion) и
прогнали его через четыре уровня структурных ограничений:

| Уровень | Активные ограничения |
|---|---|
| **L0** | только веб-фреймворк (baseline) |
| **L1** | + Clean Architecture |
| **L2** | + Clean Architecture + PostgreSQL или SQLite |
| **L3** | + Clean Architecture + PostgreSQL + SQLAlchemy/Sequelize ORM |

Итого 80 задач (greenfield) + 20 задач (feature implementation в существующем репо) ×
8 веб-фреймворков (Flask, FastAPI, Django, Express, Fastify, Koa, aiohttp, Hono) ×
2 агента (Mini-SWE-Agent, OpenHands) × 6 моделей. Всего ~5 млрд токенов.

Оценка двойная: **поведенческая** (HTTP-тесты, независимые от кода) +
**структурная** (верификаторы архитектуры, базы, ORM).

### Главный результат: constraint decay

Восемь сильных конфигураций (L0 A% > 50%) теряют в среднем **30 процентных пунктов**
от L0 до L3 — это 40% от baseline.

Таблица assertion pass rate A% для ключевых конфигураций:

| Агент | Модель | L0 | L1 | L2 | L3 | ΔA% |
|---|---|---:|---:|---:|---:|---:|
| Mini-SWE | MiniMax-M2.5 | 88.6 | 92.5 | 66.8 | 58.3 | −30.3 |
| OpenHands | MiniMax-M2.5 | 91.0 | 97.0 | 87.3 | 78.6 | −17.0 |
| Mini-SWE | Kimi-K2.5 | 85.4 | 70.9 | 62.9 | 53.7 | −31.7 |
| Mini-SWE | GPT-5.2 | 78.2 | 49.8 | 27.1 | 48.0 | −30.2 |
| OpenHands | Qwen3-Coder-Next | 73.0 | 51.7 | 42.7 | 27.6 | −45.5 |

Худший случай (OpenHands + Qwen3-Coder-Next) — потеря **45 пп (62% от L0)**.
Самый устойчивый (OpenHands + MiniMax-M2.5) — потеря 17 пп.

### Какое ограничение стоит дороже всего

Маргинальный эффект каждого ограничения (matched-pair analysis):

| Ограничение | Средний штраф ΔA% |
|---|---:|
| PostgreSQL | −19.3 ± 2.5 |
| SQLite | −14.3 ± 2.5 |
| Clean Architecture | −9.1 ± 1.6 |
| SQLAlchemy | −1.5 ± 2.1 |
| Sequelize | −0.6 ± 0.2 |

**База данных** — главный убийца. Архитектурный паттерн стоит дорого, но не катастрофично.
ORM сам по себе почти бесплатен (потому что сложность уже заложена в БД-ограничении).

### Framework sensitivity

Лёгкие фреймворки с минимальными конвенциями (Express, Koa, Flask) в среднем ~50% A%.
«Convention-heavy» (Django, FastAPI) отстают на 25–32 пп.

Объяснение: Django и FastAPI встроены в конкретные паттерны, агент вынужден инвертировать
их конвенции под чужую архитектуру — это дополнительная нагрузка.

### Анализ причин отказов

Логические ошибки составляют ~71% неудач. Внутри них:

| Подкатегория | Qwen3 | MiniMax |
|---|---:|---:|
| Incorrect query logic | 25.5% | 15.0% |
| DB/ORM runtime error | 21.2% | 15.0% |
| Auth misconfiguration | 22.6% | 5.0% |
| Framework idiosyncrasy | 9.5% | 50.0% |
| Business logic defect | 11.7% | 10.0% |

**Data-layer defects** (первые две строки) — ведущая причина для обеих моделей (~45%
логических ошибок), что и объясняет, почему БД-ограничение даёт самый большой штраф.

### Проверка на реальных кодовых базах

Feature implementation tasks — агент читает существующий репозиторий RealWorld Conduit
(уже с PostgreSQL + SQLAlchemy + Clean Architecture), инферирует конвенции и дописывает
функциональность. Результат: pass@1 остаётся низким (только GPT-5.2 > 50%). Constraint
decay не артефакт синтетики — он воспроизводится и на реальных репозиториях.

### Вывод авторов

> For end-users, this dichotomy implies that agents are reliable for rapid prototyping
> but remain unreliable for production-grade backend development. Overcoming this bottleneck
> requires a shift for agent developers: moving beyond purely functional benchmarks to
> actively integrate structural awareness, potentially through retrieval-augmented framework
> documentation, constraint-oriented planning, or targeted pre-training on
> convention-heavy codebases.

---

## HN-дискуссия (285 points, 195 comments, ~24 мая 2026)

Ссылка: https://news.ycombinator.com/item?id=48256912

### Тема 1: «Мы перекладываем сложность в markdown»

Топ-комментарий (автор — практикующий разработчик, 80%+ кода генерирует LLM):

> At some point this starts to look like we're all just moving complexity from the more
> formal and deterministic world of programming languages to the informal and
> non-deterministic world of natural language.

Ответ-продолжение:

> It's like using a compiler that generates semantically different code every time you run it.
> Basically like compiling a program that's full of UB but "seems to work" most of the time.

Это самая острая формулировка проблемы: правила в CLAUDE.md / system prompt — это не
машинный контракт. Они деградируют вместе с контекстом, их нельзя diff'ить, на них нельзя
положиться в CI.

### Тема 2: «Нужны linter-правила, не markdown»

> This is why you need to be generating more linter rules instead of just having things
> be in markdown files. I had never written an eslint rule until i started having agents
> pump them out for me and now I've encoded a bunch of important rules as lint rules that
> will fail CI if violated.

Это дословно описывает место archcheck в экосистеме: статическая проверка в CI
вместо неопределённого промпта.

### Тема 3: «Классические практики работают — AI это подтвердил»

> If there is one good thing that the generative AI tools have shown beyond any doubt it's
> that the classic "good programming" practices are still useful and effective. Self-
> documenting code. Modular design. Clearly defined architecture. Incremental development.
> Coding standards. Automated tests. Automated everything.

Lakos, Core Guidelines, Martin — это не академические артефакты. AI-эра сделала их снова
актуальными как единственный способ удерживать структуру при агентной разработке.

### Тема 4: «Calcification» — паттерн самоусиливается

Наблюдение из практики:

> I have found something I've been calling "calcification", where a pattern starts
> appearing in the codebase and the agent follows the pattern to the point where it
> dominates the context and becomes self-reinforcing.

Это и сила, и слабость: если архитектура правильная — агент её воспроизводит.
Если нет — тоже воспроизводит.

### Тема 5: «God files» — агент не абстрагирует по собственной инициативе

> The models are OK at modularization when given space to "plan" their implementation,
> but rarely decide that abstracting something would be helpful after the fact [...].
> This often leads to "god files" which, when pointed to by the user/architect, causes
> the models to correctly critique the code but often be confused about how to remedy it.

God-headers / god-files — это то, что archcheck детектирует по fan-in метрике.

### Тема 6: Context window vs. constraints — нулевая сумма

> You can't use all of the context window because at the end, the output would not
> respect the constraints (or guardrails) but to reliably produce production grade code
> you want the model to have expansive awareness which fills up the context window pretty
> quickly.

Constraint decay — это в том числе проявление «ограничения тонут в контексте». CI-чек
этой проблемы не имеет: ему не нужен контекст, он смотрит на граф.

### Тема 7: «Не успеваешь чинить инварианты»

> The situation is worse. Not only do agents have more difficulty under "structural
> constraints", but structural constraints may need to change, and agents are even worse
> at that. When designing a system or a component we have ideas that form invariants.
> Sometimes the invariant is big, like a certain grand architecture, and sometimes it's
> small, like the selection of a data structure. Except, eventually, you'll want to add
> a feature that clashes with that invariant.

Именно для этого — `--baseline`: фиксируем текущий уровень нарушений, новые ломают CI,
устаревшие правила явно снимаются конфигом.

### Тема 8: «Агенты полагаются на стиль существующего кода»

Практический вывод из нескольких независимых комментариев: лучший способ передать
архитектурные ожидания агенту — не правила в markdown, а **примеры в коде**.
Агент читает «как это уже делается» и воспроизводит паттерн. archcheck — гарантия,
что эти паттерны реально соблюдаются и не «дрейфуют».

---

## Связь с archcheck

Статья и дискуссия вместе дают три аргумента:

1. **Empirical:** constraint decay измерен — 30 пп. Не интуиция, не анекдот.
2. **Механистический:** CLAUDE.md / промпт деградируют; CI-чек — нет.
3. **Практический:** HN-сообщество самостоятельно пришло к выводу «нужны linter-правила,
   не markdown» — archcheck это и есть.

Короткое резюме для маркетинга:
> «LLM agents are reliable for rapid prototyping but remain unreliable for production-grade
> development» (Dente et al., 2026) — archcheck — это то, что делает production надёжным.
