# AI-код в репозиториях: методы обнаружения, датасеты, измерения

_Справочник по внешним исследованиям (state of the field). Собран в сессии
2026-05-30 при разборе вопроса «как опознать AI-репозиторий и где искать
архитектурный дрейф»._

**Назначение файла:** зафиксировать факты, методы и цифры из чужих работ —
без проектных выводов. Интерпретация и решения для archcheck выносятся в
отдельную задачу (см. в конце §«Куда это идёт»). Здесь только то, что измерили
другие.

Связано с: [constraint_decay.md](constraint_decay.md) (первопричина проекта),
[ai_drift_runlog.md](ai_drift_runlog.md) (наш собственный DRIFT-прогон по
33 PR'ам), [backlog/future/033](../../backlog/future/033_maj_ai_drift_dataset.md)
(AI-drift dataset).

---

## 1. Как опознают AI-код в репозитории — три класса методов

По убыванию точности и по возрастанию стоимости.

### 1.1. Explicit git-метаданные (atribution-based)

Самый простой и точный метод: AI-инструменты оставляют следы в git-метаданных.
Опознаётся без классификаторов, чисто по строковому совпадению.

Сигналы (из [«Debt Behind the AI Boom»](https://arxiv.org/abs/2603.28592)):

- **actor login** — напр. `copilot-swe-agent[bot]`, `github-copilot[bot]`
- **author email** — напр. `noreply@anthropic.com`
- **author name** — напр. `Cursor Agent`
- **`Co-Authored-By` trailer** в теле коммита

Работа сообщает, что **29 AI-инструментов** оставляют опознаваемые следы в
git-метаданных.

Аналогичный набор маркеров для PR-уровня систематизирован в open-source наборе
правил [Coderbuds](https://coderbuds.com/blog/open-source-ai-code-detection-yaml-rules):

| Инструмент | Маркер | Уверенность |
|------------|--------|-------------|
| Claude Code | footer `[Claude Code](https://claude.com/claude-code)`; `Co-Authored-By: Claude … <noreply@anthropic.com>` | 100% |
| GitHub Copilot | bot-автор `github-copilot[bot]` (workspace feature) | 100% |
| Cursor | footer `cursor.com`; HTML-коммент `<!-- Cursor -->` | 90% |
| Cursor | имя ветки `cursor-` / `cursor/` | 70% |
| OpenAI Codex | имя ветки `codex-` | 60% |

Coderbuds оценивают, что ~75% AI-assisted PR'ов ловятся explicit-маркерами со
100% уверенностью; остальное уходит в поведенческий tier (см. §1.3).

**Ограничение метода (named в источниках):** «истинный объём AI-вклада
недооценивается», т.к. inline-подсказки (Copilot без workspace, Cursor ранних
версий) и отключённая атрибуция следов не оставляют.

### 1.2. Config-артефакты в дереве

Метод: искать в репозитории конфигурационные файлы AI-агентов. Присутствие
конфига — прокси «в проекте используют AI».

Источник: [«A Dataset of Agentic AI Coding Tool Configurations»](https://arxiv.org/html/2605.08435).

Что искали (8 механизмов конфигурации для 5 инструментов — Claude Code, GitHub
Copilot, OpenAI Codex, Cursor, Gemini):

- **Context Files** — `CLAUDE.md`, `AGENTS.md`, `.cursorrules`, `GEMINI.md`
  (самый частый тип: 4 463 репо)
- Skills, Subagents, Commands, Rules, Settings
- Hooks (редко: 101 репо), MCP configurations (редко: 124 репо)

Пайплайн построения датасета:

1. 187 304 репо из SEART GitHub Search → фильтр по лицензии/активности →
   40 585 активных
2. классификация GPT-5.2 «engineered software project» → 36 710 годных
3. поиск конфиг-артефактов → **4 738 репо с AI-конфигами**

Измеренная корреляция (ключевая для метода): **71.6% репозиториев с конфигом
(3 392 из 4 738) содержат хотя бы один AI-co-authored коммит.**

Состав по языкам (топ-5 среди репо с конфигом): TypeScript 24.5%, Python 18.7%,
Go 13.8%, Java 8.0%, C# 7.5%.

Датасет публичный (15 591 конфиг-артефакт, полное содержимое из 18 167 файлов;
148 519 AI-co-authored коммитов).

### 1.3. Поведенческий fingerprint (для случаев без атрибуции)

Метод: ML-классификатор по поведенческим признакам кода и коммитов — работает,
когда атрибуции нет вообще.

Источник: [«Fingerprinting AI Coding Agents on GitHub»](https://arxiv.org/pdf/2601.17406)
(MSR '26).

Признаки, различающие инструменты (Codex, Copilot, Cursor, Claude Code):
формат commit-сообщений, структура кода, стиль отступов, соглашения об
именовании. Напр. Codex — характерные multiline-коммиты (feature importance
67.5%); Claude Code — структура кода, доля условных операторов (importance
27.2%).

Техника: классификаторы XGBoost / Random Forest, SMOTE для дисбаланса классов,
cross-validation. Заявленный вывод: AI-агенты оставляют опознаваемые
«отпечатки» даже без explicit-маркеров.

Смежные работы по стилометрии / authorship attribution AI-кода:

- [«I Know Which LLM Wrote Your Code Last Summer»](https://arxiv.org/abs/2506.17323)
  — атрибуция модели для C-программ; до 97.56% точности (binary, внутри семьи
  моделей), до 95.40% (multi-class).
- [«The Hidden DNA of LLM-Generated JavaScript»](https://arxiv.org/html/2510.10493)
  — структурные паттерны, 50 000 Node.js-программ от 20 LLM.
- [«Detection of LLM-Generated Java Code Using Discretized Nested Bigrams»](https://arxiv.org/pdf/2502.15740)
  — >96% точности на GPT-переписанном Java.

---

## 2. Готовые датасеты (можно брать, не строить с нуля)

| Датасет | Объём | Языки | Метод сбора |
|---------|-------|-------|-------------|
| **AIDev** | 932 791 agentic PR + 6 618 human PR / 116 211 репо | — | agentic PR от Codex, Copilot, Devin, Cursor, Claude Code |
| **AIDev-pop** | 33 596 agentic PR | — | подмножество AIDev |
| **Configs dataset** ([2605.08435](https://arxiv.org/html/2605.08435)) | 4 738 репо / 148 519 AI-коммитов | TS/Py/Go/Java/C# | config-артефакты + git-маркеры |
| **Debt Behind the AI Boom** ([2603.28592](https://arxiv.org/abs/2603.28592)) | 304 362 AI-коммита / 6 275 репо (100+ stars) | Python, JS, TS | explicit git-метаданные |
| **DevGPT** ([2309.03914](https://arxiv.org/abs/2309.03914)) | 29 778 prompt/response, 19 106 сниппетов | — | shared ChatGPT-беседы ↔ commits/PR/issues |
| **Security Vulns** ([2510.26103](https://arxiv.org/html/2510.26103v1)) | 7 703 файла | — | explicit attribution к 4 инструментам |

---

## 3. Что измерили о свойствах AI-кода

### 3.1. Где концентрируется AI-код

[«AI Code in the Wild»](https://arxiv.org/abs/2512.18567) (детекшн-пайплайн по
топ-1000 GitHub-репо, 2022–2025, + 7000+ CVE-linked изменений):

- AI-код — существенная доля нового кода;
- концентрация в **glue code, тестах, рефакторинге, документации, boilerplate**;
- **core logic и security-critical конфиги остаются преимущественно
  человеческими**.

### 3.2. Дублирование и эрозия переиспользования

[GitClear AI Code Quality 2025](https://www.gitclear.com/ai_assistant_code_quality_2025_research)
(211 млн строк за 5 лет; ранее —
[Coding on Copilot](https://www.gitclear.com/coding_on_copilot_data_shows_ais_downward_pressure_on_code_quality),
153 млн строк 2020–2023):

- блоки из **5+ дублированных строк выросли ~8× за 2024**;
- дублирование изменённых строк: **8.3% (2021) → 12.3% (2024)**, ~4× рост;
- **2024 — первый год, когда copy/paste обогнал moved-code**;
- рефакторинг (доля изменённых строк): **25% (2021) → <10% (2024)**;
- code churn (строки, переписанные <2 недель): **5.5% (2020) → 7.9% (2024)**.

### 3.3. Технический долг и code smells

[«Debt Behind the AI Boom»](https://arxiv.org/abs/2603.28592)
(304k AI-коммитов / 6 275 репо):

- 484 606 различных issues; **89.1% — code smells** (maintainability),
  5.8% runtime bugs, 5.1% security;
- **>15% коммитов каждого инструмента** вносят ≥1 issue (17.3% Copilot …
  28.7% Gemini);
- **24.2% AI-introduced issues выживают** до HEAD;
- люди чинят сопоставимый объём code smells, но AI вносит **~2× больше security
  issues, чем чинит**.

[«Security Vulnerabilities in AI-Generated Code»](https://arxiv.org/html/2510.26103v1)
(7 703 файла; ChatGPT 91.52% / Copilot 7.50% / CodeWhisperer 0.52% /
Tabnine 0.46%): 4 241 CWE-инстанс, 77 типов уязвимостей (CodeQL); при этом
**87.9% AI-кода без CWE-mapped уязвимостей**.

### 3.4. AI-PR vs human-PR (поведение)

[«How AI Coding Agents Modify Code»](https://arxiv.org/pdf/2601.17581)
(AIDev: 932 791 agentic + 6 618 human PR): agentic PR существенно отличаются по
числу коммитов, умеренно — по числу тронутых файлов и удалённых строк; чуть
выше description-to-diff similarity.

[«Programming by Chat»](https://arxiv.org/html/2604.00436v1)
(74 998 сообщений / 11 579 сессий / 1 300 репо, Cursor + Copilot) — поведенческий
анализ IDE-native conversational programming.

---

## 4. Прочие релевантные работы

- [«A Large-Scale Empirical Study of AI-Generated Code in Real-World Repositories»](https://arxiv.org/abs/2603.27130)
  — детекшн-пайплайн (эвристический фильтр + LLM-классификатор); свойства кода
  и коммитов (размер, post-commit эволюция).
- [«On Developers' Self-Declaration of AI-Generated Code»](https://arxiv.org/html/2504.16485v1)
  — 613 self-declared сниппетов; анализ практик самодекларации в комментариях.
- [«Developers and Generative AI: Self-Admitted Usage»](https://arxiv.org/pdf/2603.26277)
  — self-admitted использование в OSS.
- [«AI builds, We Analyze»](https://arxiv.org/html/2601.16839v1)
  — качество AI-сгенерированного build-кода.
- [Как обрабатывать AI-PR в OSS](https://docs.bswen.com/blog/2026-03-20-ai-generated-pull-requests-opensource/)
  — практические эвристики «AI-slop» (скорость сабмита, дубль-сабмиты,
  10+ файлов / <20 строк, отсутствие тестов, симптомное латание).

---

## Куда это идёт

Этот файл — только факты поля. Проектные выводы (что из этого применимо к
archcheck, как менять корпус DRIFT-исследования, гипотеза про duplication как
детектор) выносятся в **отдельную задачу** и не смешиваются сюда, чтобы
справочник оставался нейтральным.

## Источники

- [Debt Behind the AI Boom](https://arxiv.org/abs/2603.28592)
- [A Dataset of Agentic AI Coding Tool Configurations](https://arxiv.org/html/2605.08435)
- [Fingerprinting AI Coding Agents on GitHub](https://arxiv.org/pdf/2601.17406)
- [AI Code in the Wild](https://arxiv.org/abs/2512.18567)
- [A Large-Scale Empirical Study of AI-Generated Code in Real-World Repositories](https://arxiv.org/abs/2603.27130)
- [How AI Coding Agents Modify Code](https://arxiv.org/pdf/2601.17581)
- [Programming by Chat](https://arxiv.org/html/2604.00436v1)
- [On Developers' Self-Declaration of AI-Generated Code](https://arxiv.org/html/2504.16485v1)
- [Developers and Generative AI: Self-Admitted Usage](https://arxiv.org/pdf/2603.26277)
- [Security Vulnerabilities in AI-Generated Code](https://arxiv.org/html/2510.26103v1)
- [AI builds, We Analyze](https://arxiv.org/html/2601.16839v1)
- [I Know Which LLM Wrote Your Code Last Summer](https://arxiv.org/abs/2506.17323)
- [The Hidden DNA of LLM-Generated JavaScript](https://arxiv.org/html/2510.10493)
- [Detection of LLM-Generated Java Code Using Discretized Nested Bigrams](https://arxiv.org/pdf/2502.15740)
- [DevGPT: Studying Developer-ChatGPT Conversations](https://arxiv.org/abs/2309.03914)
- [GitClear AI Code Quality 2025](https://www.gitclear.com/ai_assistant_code_quality_2025_research)
- [GitClear: Coding on Copilot (2023/2024)](https://www.gitclear.com/coding_on_copilot_data_shows_ais_downward_pressure_on_code_quality)
- [Coderbuds: open-source AI code detection rules](https://coderbuds.com/blog/open-source-ai-code-detection-yaml-rules)
- [Handling AI-generated PRs in OSS (BSWEN)](https://docs.bswen.com/blog/2026-03-20-ai-generated-pull-requests-opensource/)
