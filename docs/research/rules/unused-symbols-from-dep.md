# unused-symbols-from-dep (CRP)

- **Category:** B (AST / semantic)
- **Authority:** high — Robert C. Martin
- **Source:** Martin. *OO Design Quality Metrics* (1994); Common Reuse Principle. [principles-wiki](http://principles-wiki.net/principles:common_reuse_principle)

## Rule

> "The classes in a package are reused together. If you reuse one of them, you reuse them all. Don't depend on packages whose symbols you don't use."

## Why for cpparch

Если модуль X включает заголовок из модуля Y, но реально использует <20% объявленных там символов — X слишком крупно-гранулярно зависит от Y. Любое изменение в неиспользуемой части Y всё равно перезапустит ребилд X. Это снижает реальную transitive cohesion и расходится с тем, на что согласился пользователь.

Граничит с IWYU, но не дублирует его: IWYU минимизирует includes на уровне TU, CRP смотрит на module-level granularity.

## Detection

Для каждого include `X → Y`:
1. Собрать публичные символы Y (по public `.h`).
2. Собрать использованные в X символы из Y (через clang `MatchFinder` или USR cross-reference).
3. Если `used / declared < threshold` (по умолчанию 0.2) — флагать.

Дорогостоящая проверка (AST + symbol resolution). Опциональна, не дефолт MVP.

## Fixtures

- `pass/` — X использует ≥ половину публичных символов Y.
- `fail_overbroad/` — Y объявляет 20 классов, X использует 1.
- `pass_typed_facade/` — Y — фасад с одной функцией, X использует её. Ничего лишнего, низкий used/declared не флагается потому что declared мало.
