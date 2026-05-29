# [RULES] SF.21 — anonymous namespace в `.h` (перенос в v0.3)

**Дата создания:** 2026-05-29
**Дата старта:** —
**Статус:** new
**Модуль:** RULES
**Приоритет:** minor
**Сложность:** S (через libclang) / M (надёжный text-scan)
**Целевой релиз:** **v0.3** (см. обоснование)
**Блокирует:** —
**Заблокирован:** #042 (clang_semantic_backend) — если делать точно через AST

> **2026-05-29:** этап 1 (docs sync) сделан перед v0.1-релизом и закоммичен; этап 2 (реализация) отложен в `future/` вместе с #042 — оба отрабатываются в v0.2+. См. `## Сделано` ниже.
**Related:** #028 (rules_engine_mvp, уже отложил SF.21 с пометкой «требует libclang»), #006 (spec_refactor), #042 (clang_semantic_backend)

## Что это

Правило C++ Core Guidelines [SF.21](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#Rs-unnamed): «Don't use an unnamed (anonymous) namespace in a header».

```cpp
// foo.h — НАРУШЕНИЕ SF.21
namespace {                // ← анонимный namespace на верхнем уровне header'а
  int counter = 0;
  void helper() { ... }
}
```

Анонимный namespace даёт internal linkage. В `.h` это значит: каждый translation unit, включающий заголовок, получает **свою отдельную копию** объектов и функций. Не формальное ODR-violation, но бесполезный bloat бинаря и source surprises (`&helper` разный в разных TU, статические переменные дублируются).

## Почему перенос с v0.1/v0.2 в v0.3

**1. Это не про архитектурный дрифт.** Дрифт в archcheck — это изменение **формы графа зависимостей**: shortcut edges (DRIFT.1), растущие SCC (DRIFT.2), god-headers, удлиняющиеся chain'ы, нарушение модульных границ. Анонимный namespace в header'е граф не трогает — это ODR-pitfall и bloat бинаря, micro-level hygiene того же класса что SF.7 (`using namespace` в header). По смыслу ближе к clang-tidy, чем к physical-design в духе Lakos.

**2. Это не самый частый AI-паттерн.** Типичный AI-агент, рефакторящий код, скорее напишет shortcut-import (DRIFT.1), создаст цикл при «вынес общее в helper» (DRIFT.2), забудет guard в новом header'е (SF.8) или скопирует `using namespace` (SF.7). Анонимный namespace в `.h` — контр-интуитивный паттерн, который чаще пишут junior'ы вручную, скопировав из `.cpp` без понимания. AI чаще выберет `static inline` или `inline` (что корректно) или вынесет в `.cpp`.

**3. Точная версия не дорогая, но и не критичная.** Через libclang определение `clang::NamespaceDecl::isAnonymousNamespace()` тривиально — S. Тащить text-scan версию для v0.1 ради галочки в MVP-чеклисте — лишний код без продуктовой ценности. clang-tidy (`google-build-namespaces`) и cppcheck это правило уже покрывают; пользователи, которым оно нужно, его получат и так.

**4. v0.1 ценность — physical design + AI drift, не SF-hygiene.** Cover-story v0.1 в спеке: «module boundaries + cycles в CI». SF.7/8/9 включены потому что это hygiene-добивка для физического дизайна (`using namespace` ломает namespace-границы; missing guard ломает include-graph). SF.21 не вписывается даже в эту логику — анонимный namespace в `.h` не ломает ни границы, ни граф.

**5. v0.3 лучше подходит чем v0.2.** v0.2 — это libclang backend + остальные SF (SF.2/5/10/11) + SARIF. Эти четыре SF — про корректность физического слоя (defs в headers, .cpp→.h pairing, no implicit includes, self-contained). SF.21 — отдельная hygiene-задача, она там «случайный гость». v0.3 — секции C/I/NL + BDE + AI-контур; туда SF.21 ложится логически: «остальные hygiene-правила из CCG, которые не критичны для physical design».

## План

### Этап 1 — синхронизация документов (можно делать без #042)

- [x] Обновить ROADMAP.md: убрать SF.21 из v0.1 blocker'ов, уточнить v0.2 (preview через `--with-clang`), добавить в v0.3 как default-ON.
- [x] Обновить architecture-spec.md: таблица SF.* + раздел v0.3, пути на 050 (new → wip).
- [x] Проверить MVP.md, CHANGELOG.md, README.md — упоминания SF.21 не противоречат решению.

### Этап 2 — реализация (после #042 / clang_semantic_backend)

- [ ] Спека SF.21 правила: `Sf21NoAnonymousNamespace` в `src/rules/`, рядом с другими SF-классами. Использует libclang (#042) — `clang::NamespaceDecl::isAnonymous()` на translation unit'ах из `compile_commands.json`. Сводится к ~30 строк.
- [ ] Регистрация в `makeDefaultRuleSet` через `rules.push_back(std::make_unique<Sf21NoAnonymousNamespace>())`. По умолчанию OFF до v0.3 (включается через `--with-clang` в v0.2 как preview, если #042 уже зарелизился).
- [ ] Фикстуры `fixtures/sf21_anonymous_namespace/pass/` (named namespace + .cpp с anon) и `fixtures/sf21_anonymous_namespace/fail/` (anon в `.h`).
- [ ] Unit-тест с моком libclang или живым `compile_commands.json` от фикстуры.
- [ ] CHANGELOG (под v0.3).

## Изменённые файлы (план)

| Файл | Изменение |
|------|-----------|
| `include/archcheck/rules/sf21_anonymous_namespace.h` | new |
| `src/rules/sf21_anonymous_namespace.cpp` | new |
| `src/rules/rule_set.cpp` | регистрация |
| `tests/unit/rules/sf21_anonymous_namespace_test.cpp` | new |
| `fixtures/sf21_anonymous_namespace/` | new (pass + fail) |
| `docs/architecture-spec.md` | таблица SF.* + roadmap v0.1/v0.2/v0.3 |
| `docs/MVP.md` | (если упоминается) |
| `docs/STATUS.md` | убрать SF.21 из «открыто до v0.1» |

## Сделано

**2026-05-29 — этап 1: синхронизация документов**

- `docs/ROADMAP.md` — SF.21 убран из v0.1 blocker'ов (решение зафиксировано), уточнено в v0.2 как preview через `--with-clang`, добавлен пункт в v0.3 (default-ON).
- `docs/architecture-spec.md` — таблица SF.*: фаза изменена с «v0.3» на «v0.2 (preview, `--with-clang`) / v0.3 default-ON»; пути на 050 переведены `new → wip`; раздел v0.3 уточнён.
- `docs/MVP.md`, `CHANGELOG.md` — SF.21 не упомянут, правки не нужны.
- `README.md:111` — уже корректен (SF.21 unlocks с `--with-clang` в v0.2), не правлю.

Этап 2 (реализация правила) ждёт #042 (clang_semantic_backend).

## Принцип работы

(заполнится при closing)
