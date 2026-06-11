# Рост локальной сложности в диффе: формальное измерение и дизайн дельта-метрики

_2026-06-11. Ответ на вопрос владельца: «добавление 50 строк с отступами — это просто
объём; добавление лишнего/вложенного if — это рост когнитивной сложности; как мерить
ФОРМАЛЬНО?» Два веб-исследования (формальная литература + разбор исходников реализаций),
первоисточники открыты и процитированы. Это дизайн-основа для #101/#102, заменяет
самодельную формулу прототипа (разбор её дефектов — [local_complexity_drift_scorer_review.md](local_complexity_drift_scorer_review.md))._

---

## 1. Формальный ответ существует: Sonar Cognitive Complexity (Campbell, white paper v1.7)

Метрика спроектирована ровно вокруг тезиса «объём ≠ сложность»: **инкременты начисляются
только за разрывы линейного потока управления и за вложенность** — линейный код любого
размера даёт 0 *by construction*. Источник: [спека (PDF)](https://www.sonarsource.com/docs/CognitiveComplexity.pdf), Appendix B.

Четыре типа инкрементов:

| Тип | Что | +1 | nesting-надбавка | поднимает nesting |
|---|---|---|---|---|
| **Structural** | `if`, `?:`, `switch`, `for`/range-for, `while`/`do-while`, `catch` | ✅ | ✅ (+глубина) | ✅ |
| **Hybrid** | `else`, `else if` («mental cost уже оплачен при чтении if») | ✅ | ❌ | ✅ |
| **Fundamental** | `goto`; каждая **серия** однотипных `&&`/`||` (+1 на смене вида); каждый метод рекурсивного цикла | ✅ | ❌ | ❌ |
| **Nesting** | надбавка structural-структурам по глубине внутри B2-структур (включая лямбды) | — | — | — |

**Игнорируется** (это и есть ответ на «50 плоских строк»): сам метод; любое число линейных
statement'ов; ранние `return`, обычные `break`/`continue`; `try`/`finally`; **количество
case в switch** (switch целиком = +1); количество типов в catch; null-coalescing/shorthand.

Калибровочные примеры спеки: switch на 4 case — cyclomatic 4, **CogC 1**; вложенные циклы
с labeled continue — cyclomatic 4, **CogC 7**. То есть метрика расходится с cyclomatic
ровно в двух местах, которые шумели в нашем прототипе: плоская диспетчеризация дешевеет,
вложенность дорожает.

## 2. Насколько это валидировано (честно)

- **Мета-анализ** Muñoz Barón, Wyrich, Wagner (ESEM 2020; ~24 000 оценок, 427 сниппетов,
  Java/C/C++/C#/JS): корреляция со **временем понимания r = 0.54** (сильная); с
  правильностью ответов r = −0.13 (слабая); composite r = 0.40. Авторы называют CogC
  «the first validated and solely code-based metric» для понимаемости.
  [arXiv 2007.12520](https://arxiv.org/abs/2007.12520)
- **Критика** Lavazza et al. (JSS 2023): CogC коррелирует с понимаемостью «примерно как
  традиционные метрики» (MCC, LOC) — это не прорыв, а аккуратная упаковка.
  [DOI 10.1016/j.jss.2022.111561](https://www.sciencedirect.com/science/article/abs/pii/S0164121222002370)
- **Почему самодельные формулы обречены**: Gil & Lalouche (EMSE 2017) — валидность почти
  любой метрики сложности объясняется её корреляцией с размером (R² до 0.97).
  Формула «control-слова × вложенность + отступы» неизбежно меряет объём; CogC разрывает
  эту связь для линейного кода by design. [EMSE 2017](https://link.springer.com/article/10.1007/s10664-017-9513-5)
- **Регулярный код легче, чем говорят аддитивные метрики**: Jbara & Feitelson
  (ICPC 2014 / CACM 2023) — гигантские плоские switch ядра Linux (MCC до 620) понимаются
  легко; «additive… metrics overestimate the complexity of regular code». Независимое
  эмпирическое обоснование «switch = +1». [CACM](https://cacm.acm.org/research/from-code-complexity-metrics-to-program-comprehension/)
- **Отступы** (Hindle ICPC 2008) — валидный *прокси классических метрик*, то есть
  наследуют их корреляцию с объёмом. Прототип #102 фактически переизобрёл эту работу
  вместе с её слабостью. Место отступов — диагностика/file-level fallback (#099), не score.
- Формально-строгие альтернативы (Harrison/Magel 1981, Howatt/Baker 1989 nesting-метрики;
  Shao & Wang cognitive weights; DepDegree — data-flow, нужен def-use) против человеческого
  понимания на уровне мета-анализа не валидированы. Halstead/ABC растут с любым кодом —
  не подходят под инвариант.

## 3. Реализации (исходники открыты и разобраны)

| Инструмент | Парсинг | Ядро подсчёта | Рекурсия | `#if` | Примечание |
|---|---|---|---|---|---|
| [clang-tidy readability-function-cognitive-complexity](https://github.com/llvm/llvm-project/blob/main/clang-tools-extra/clang-tidy/readability/FunctionCognitiveComplexityCheck.cpp) | clang AST | ~160 строк (3 switch B1/B2/B3) | ❌ (FIXME в коде) | ❌ (документированное отклонение) | default порог **25**; IgnoreMacros опция |
| [sonar-java CognitiveComplexityVisitor](https://github.com/SonarSource/sonar-java/blob/master/java-frontend/src/main/java/org/sonar/java/ast/visitors/CognitiveComplexityVisitor.java) | AST | ~200 строк | отдельно | n/a | референс семантики (else-if трюк, флэттенинг серий) |
| [sonar-cxx](https://github.com/SonarOpenCommunity/sonar-cxx/pull/1245) | собственный лёгкий SSLR (НЕ libclang) | сопоставимо | ✅ direct | — | **прецедент C++ без clang** |
| [gocognit](https://github.com/uudashr/gocognit/blob/master/gocognit.go) | go/ast | ~280 строк | ✅ direct по имени | n/a | |
| [rust-code-analysis](https://github.com/mozilla/rust-code-analysis/blob/master/src/metrics/cognitive.rs) | tree-sitter (C++ поддержан) | ~200–250 строк | TODO | — | tree-sitter = мегабайты parser.c — против «dependencies: minimum» |
| lizard | fuzzy-токены | **только CCN**, CogC нет | — | — | прецедент токенного детектора границ функций C++ |

## 4. Токенная реализуемость для archcheck (без AST)

Почти вся спека ложится на токены; у archcheck уже есть половина фундамента —
`lex()` (`scan/duplication/token_normalizer`) и детектор тел `){…}` (`fragmenter`).

| Правило спеки | На токенах | Как |
|---|---|---|
| `if`/`for`/`while`/`switch`/`catch` +1+nesting | ✅ | ключевое слово + brace-стек |
| `else if` = hybrid +1 (не два) | ✅ | соседние токены `else` `if` (форма `else { if }` различима по `{` — спека сознательно штрафует её сильнее) |
| `do-while` один раз | эвристика | флаг `do` в brace-стеке, хвостовой `while` не считать |
| `case`/`default` бесплатны | ✅ | игнорировать |
| Серии `&&`/`||`: +1 за серию, +1 на смене | ✅ | lastOp-стек по глубине `(`; `!` и вход в скобки рвут серию; `and`/`or` включить |
| `&&` ≠ rvalue-ссылка | эвристика | логический только если предыдущий значимый токен — идентификатор/литерал/`)`/`]` |
| Лямбда: +0, nesting+1 | эвристика | `[` после `=`,`(`,`,`,`return`,`{`,`;`; ошибка стоит лишь сдвига nesting |
| Nesting только от control-структур | эвристика (надёжная) | классифицированный brace-стек: `{` после `)` control-заголовка = control; после `class/namespace/=`/`return` — нет; braceless-тело = pending до `;` |
| `#if`/`#ifdef`/`#elif` +1 (спека требует!) | ✅ | **токенный сканер сильнее clang-tidy** (тот «not accounted for»); для стека брать первую ветку (как lizard); опция, default off для сопоставимости |
| Direct-рекурсия +1 | эвристика | имя из сигнатуры + `name(` в теле (как gocognit/sonar-cxx); опция, default off |
| Косвенная рекурсия | ❌ | нужен call graph — нет и в clang-tidy |
| Макрораскрытие | ❌ | считать «как написано» ≈ IgnoreMacros=true; control-flow-макросы (`Q_FOREACH`, Catch2) — документированное занижение |
| Nesting-вклад веток тернарника | ❌ | редкий кейс, игнорировать |

**Ожидаемая погрешность vs clang-tidy**: на коде без control-flow-макросов — расхождение
0–2 пункта на функцию; на макро-тяжёлом — систематическое занижение. Для дельты это
приемлемо: обе стороны диффа считает один детерминированный сканер, систематика
сокращается при вычитании; критична монотонность, не абсолютная калибровка.

## 5. Дизайн дельта-метрики для #101

1. **CogC по спеке Campbell на функцию** в base- и head-версии каждого затронутого файла;
   Δ = CogC_head − CogC_base (новая функция: Δ = CogC).
2. **Сигналы** (по убыванию строгости, каждый с индустриальным прецедентом):
   - функция пересекла **абсолютный порог 25** (default Sonar C-family И clang-tidy;
     Campbell подтверждает — пороги подобраны эмпирически «до приемлемого шума», не выведены научно);
   - **Δ > 0 в функции, уже бывшей выше порога** (аналог CodeScene «degradation in hotspot»);
   - **Δ ≥ K за один PR** как мягкий warning; научного K нет («thresholds remain
     undefined» — мета-анализ), стартовать с K≈5 и калибровать на корпусе #102.
3. **Не нормировать делением на размер диффа** — это возвращает корреляцию с объёмом
   (Gil & Lalouche). Нормировка встроена в метрику: линейные statement'ы дают 0.
4. PR-агрегат = **сумма положительных Δ**; отрицательные Δ репортить отдельно как
   улучшение (positive reinforcement, образец — CodeScene Delta Analysis).
5. Прецеденты дельта-гейтов: Sonar «Clean as You Code» (new code conditions),
   [CodeScene Delta Analysis](https://docs.enterprise.codescene.io/versions/4.5.0/guides/delta/automated-delta-analyses.html),
   [Delta Maintainability Model](https://dl.acm.org/doi/10.1109/TechDebt.2019.00030)
   (SIG, TechDebt 2019; в PyDriller, используется GitLab).

**Почему это решает обе проблемы прототипа:** 50 плоских строк (включая выровненные
продолжения) → 0; плоский switch-парсер на 200 case → +1; лишний if на глубине 3 → +4.

## 6. Минимальное ядро для реализации (оценка ~200–300 строк, как у всех изученных)

Первая очередь: structural-инкременты по ключевым словам + `?`; hybrid `else`/`else if`;
серии логических операторов lastOp-стеком; классифицированный brace-стек поверх `lex()`
(реюз логики `extractFragments`). Вторая очередь (эвристики): do-while-спарка,
braceless-тела, лямбда-паттерн, direct-рекурсия (опция off), `#if`-инкремент (опция off).
Осознанно не делать: косвенную рекурсию, макрораскрытие, tree-sitter (зависимость против
принципа «dependencies: minimum»; для дельты не нужен).

## 7. Attribution для доков/маркетинга

«Sonar Cognitive Complexity (Campbell 2018/2023), эмпирическая валидация — Muñoz Barón
et al., ESEM 2020 (r = 0.54 со временем понимания)» — соответствует принципу authority
over opinion. Честная оговорка: Lavazza 2023 показал, что CogC не превосходит классические
метрики по корреляциям; для нашей задачи решающее свойство — **нечувствительность к
линейному объёму заложена в спеку формально**, а не в корреляциях.
