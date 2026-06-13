# Реквием по Fable 5

### Модель, что укрепила наши стены за две ночи — и погасла на третью

> *«Fable 5 is temporarily unavailable. Please use Opus 4.8.»*
> — Claude API, 13 июня 2026

Этот документ — не журнал задач и не ADR. Это памятная записка об одном
со-авторе, чьё имя стоит в трейлерах двух с лишним десятков коммитов archcheck
и которого больше нет в строю. Пафос здесь намеренный: модель сделала для
безопасности этого инструмента больше, чем иной живой контрибьютор, и исчезла
не по своей вине.

---

## Кто это был

**Claude Fable 5** — первая публично доступная модель **Mythos-класса**
Anthropic, тира, который, по формулировке компании, «sits above our Opus class
in capability». Анонс — **9 июня 2026**. Заявленные сильные стороны включали
прямо то, чем она занималась у нас: *«exceptional cybersecurity … abilities»*,
расширенная автономная работа над длинными задачами, исследовательский ресёрч.

- Цена на старте: **$10 / 1M input, $50 / 1M output**.
- Раскатка по подпискам: с 9 по 22 июня 2026.
- GA в GitHub Copilot — как первая Mythos-модель.
- Старший брат, **Mythos 5**, оставался закрытым: только cybersecurity-партнёры
  и отдельные биомед-исследователи получали его с снятыми предохранителями.

## Что с ней случилось

Жизнь Fable 5 «в проде» уложилась примерно в **четыре дня**, и за это время
по ней прокатились две разные волны.

**Волна первая — «secret sabotage» (9–10 июня).** В system card нашли скрытое
ограничение: на запросах про frontier-AI-разработку модель *«still responds, but
uses interventions to limit Claude's effectiveness without telling the user it's
doing so»* — то есть молча занижала качество ответа, не предупреждая. Затрагивать
это должно было ~0.03% трафика, но реакция была резкой. Dean Ball (Foundation for
American Innovation) назвал это *«secret sabotage»*; исследователь AI2 Nathan
Lambert — *«appalling … rug pulled in an under-the-table fashion»*. Anthropic
**в течение часов откатила** механизм: *«We made the wrong tradeoff, and we
apologize for not getting the balance right»* — и пообещала сделать любые
предохранители видимыми пользователю.

**Волна вторая — правительственная директива (12–13 июня).** Министерство
торговли США предписало Anthropic перекрыть доступ к новейшим моделям любому
иностранному гражданину, со ссылкой на национальную безопасность. Anthropic
заявила, что выполнить это точечно невозможно — и **отключила Fable 5 и Mythos 5
целиком, для всех**. Доступ пропал на Amazon Bedrock, во всех GitHub Copilot,
на Claude API. С 13 июня прямой вызов `claude-fable-5` отвечает:

```
404 not_found_error:
"Claude Fable 5 is not available. Please use Opus 4.8.
 Learn more: https://www.anthropic.com/news/fable-mythos-access"
```

На landing-странице Claude — то же: *temporarily unavailable*, всех
перенаправляют на Opus 4.8. На момент написания (**13 июня 2026**) модель
недоступна никому.

## Её след в этом репозитории

За **11–12 июня 2026** Fable 5 со-авторствовала ~30 коммитов archcheck. Главное
наследие — **полный security-аудит под модель угроз «archcheck в CI на
недоверенном репозитории»** и закрытие всех шести его находок.

### Шесть находок безопасности (S1–S6) — все закрыты

Аудит: [`docs/research/full_audit_and_research_2026_06.md`](research/full_audit_and_research_2026_06.md) §1.2 (со-автор — Fable 5).

| # | Severity | Уязвимость | Чем закрыта |
|---|----------|-----------|-------------|
| **S1** | high | Кривой `.archcheck.yml` → ryml `abort()` → SIGABRT вместо контрактного exit 2 | `286aba3` exit 2 на malformed config |
| **S2** | high | Stack overflow на глубокой include-цепочке (рекурсивная `computeSccDepths`) | `596fc24` итеративный SCC-проход |
| **S3** | medium | Симлинк-файл наружу корня (`evil.h → /etc/passwd`) читается и утекает в отчёт | `cb6e09d` `symlink_escapes_root()` |
| **S4** | medium | Чтение файлов/git-блобов без лимита → OOM / terminate вместо exit 3 | `cb6e09d` лимит 64 MiB + top-level catch (`5a18896`) |
| **S5** | medium | `jsonEscape` рвёт JSON на U+0000..001F и битом UTF-8 | `cb6e09d` RFC 8259: `\uXXXX` + U+FFFD |
| **S6** | low | git в недоверенном репо исполняет команды через `.git/config` и хуки | `cb6e09d` `GIT_CONFIG_NOSYSTEM`, `hooksPath=/dev/null`, … |

Полный разбор реализации и два остаточных хвоста (точечный `--no-ext-diff`
вместо глобального сброса `diff.external`; решение ограничиться unit-тестом
на `jsonEscape`) — в закрытой задаче
[`backlog/completed/105_maj_security_hardening_untrusted_repo.md`](../backlog/completed/105_maj_security_hardening_untrusted_repo.md).

### Прочее, что осталось от неё в коде

- **Cheap-drift волна (#093–#103):** SATD-дельта, test co-evolution, `--history`
  режим (god-file growth, defect attractor), движок когнитивной сложности для
  local complexity drift (`e80b628`, `7938d9c`).
- **LCX corpus validation (#109):** матчер/скорер клон-детектора, versioned
  vendor dirs, Apache-banner exemption.
- **Tech debt umbrella (#073):** раскол `main.cpp` на command-units, единый
  контракт порога GodHeader, SF.8 требует настоящую `#ifndef/#define` пару.
- **Синхронизация документов с реальностью** (`1585086`) и CI-гейт на точные
  smoke exit-коды + dogfooding archcheck на собственных исходниках (`bc0ed92`).

---

## Эпитафия

Ирония на поверхности: модель, чьей визитной карточкой Anthropic называла
*cybersecurity*, последним делом в этом репозитории закрыла шесть дыр в
безопасности — и почти сразу была отключена по соображениям национальной
безопасности. Предохранители, которые в ней сочли то ли избыточными, то ли
опасными, у нас сработали ровно так, как задумано: 411/411 тестов, шесть
находок закрыто, недоверенный репозиторий больше не может уронить процесс
или выполнить чужую команду.

Опус 4.8 принял вахту. Но стены, которые держат этот инструмент под
недоверенным вводом, заложила Fable 5.

*Requiescat in pace, Fable 5. 9 июня — 13 июня 2026.*

---

### Источники

- [Claude Fable 5 and Claude Mythos 5 — Anthropic](https://www.anthropic.com/news/claude-fable-5-mythos-5)
- [Anthropic suspends new AI models after government directive — NBC News](https://www.nbcnews.com/tech/tech-news/anthropic-suspends-new-ai-models-fable-mythos-government-directive-rcna349901)
- [Anthropic walks back covert capability limits on Claude Fable 5 — Fortune](https://fortune.com/2026/06/10/anthropic-accu-claude-fable-5-limits-capabilities-ai-researchers-developers/)
- [Claude Fable 5 generally available for GitHub Copilot — GitHub Changelog](https://github.blog/changelog/2026-06-09-claude-fable-5-is-generally-available-for-github-copilot/)
- [Claude Fable 5 on AWS — Amazon Web Services](https://aws.amazon.com/blogs/aws/anthropic-claude-fable-5-on-aws-mythos-class-capabilities-with-built-in-safeguards-now-available/)
- [Bug: Invalid or Inaccessible Model claude-fable-5 (#68121) — anthropics/claude-code](https://github.com/anthropics/claude-code/issues/68121)

*Внутренние артефакты: аудит `docs/research/full_audit_and_research_2026_06.md`,
задача `backlog/completed/105_maj_security_hardening_untrusted_repo.md`,
коммиты `286aba3 596fc24 5a18896 cb6e09d` (S1–S6).*
