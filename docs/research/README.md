# Research — external architecture rule sources

Опрос общепризнанных источников архитектурных и дизайн-правил для C++ за пределами того, что уже зафиксировано в [../architecture-spec.md](../architecture-spec.md). Для каждого кандидата заведён отдельный файл в [rules/](rules/).

Это **research-каталог, не roadmap.** Каждое правило здесь — кандидат на дефолт cpparch, который надо позже:
1. оценить ещё раз в контексте текущего MVP,
2. при отборе — перенести в `backlog/` как задачу с fixtures,
3. реализовать как `IRule` под соответствующим источником в `src/rules/`.

## Категории формализуемости

| Cat | Что означает | Бэкенд |
|---|---|---|
| **A** | Include-only / препроцессорный скан | fast backend (без libclang) |
| **B** | AST / семантика | libclang |
| **C** | Метрики на графе компонентов | граф (после A/B парсинга) |
| **D** | Pattern-matching по тексту (regex) | fast backend |
| **E** | Design-only — не проверяется статически | — (сознательно опускаем) |
| **F** | Out of scope — баги / стиль / safety-cert | — (другие инструменты) |

## Источники

| Источник | Authority | Что полезно | Ссылка |
|---|---|---|---|
| C++ Core Guidelines, секция SF | high | уже в спеке | [#S-source](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-source) |
| C++ Core Guidelines, секция C (Classes) | high | C.121, C.133, C.134 | [#S-class](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-class) |
| C++ Core Guidelines, секция I (Interfaces) | high | I.2, I.3, I.22 | [#S-interfaces](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-interfaces) |
| C++ Core Guidelines, секция NL | high | NL.27 (file suffix) | [#S-naming](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#S-naming) |
| Lakos / Bloomberg BDE wiki | high | component naming, no inter-component friendship, declared external linkage | [bloomberg/bde wiki](https://github.com/bloomberg/bde/wiki/physical-code-organization), [isocpp blog](https://isocpp.org/blog/2020/02/C-Packaging-and-Design-Rules) |
| Martin package principles (REP/CCP/CRP/ADP/SDP/SAP) | high | SDP, SAP, CRP сверх существующих Ce/Ca/I/A/D | [ADP wiki](https://en.wikipedia.org/wiki/Acyclic_dependencies_principle), [principles-wiki](http://principles-wiki.net/collections:robert_c._martin_s_principle_collection) |
| Google C++ Style Guide | medium | foreign forward-decls, include order groups, `using namespace` policy | [styleguide/cppguide](https://google.github.io/styleguide/cppguide.html) |
| HIC++ 4.0 | medium | `using`-decl в header, one-extern-class-per-header | [HIC++ PDF](https://abougouffa.github.io/awesome-coding-standards/hi-cpp-4.0.pdf) |
| clang-tidy (cppcoreguidelines/misc/google/llvm) | medium (импл-референс) | `misc-no-recursion`, готовые equivalents для SF.7/8/9/21/2 | [clang-tidy checks](https://clang.llvm.org/extra/clang-tidy/checks/list.html) |
| ArchUnit / Deptrac / SonarQube | medium (таксономия) | default-deny + allow-list, layer templates, slice cycles | [ArchUnit](https://www.archunit.org/userguide/html/000_Index.html), [Deptrac](https://deptrac.github.io/deptrac/concepts/) |
| JSF AV C++ (Lockheed) | medium | в основном F; локально — fan-in/fan-out cap | [JSF AV PDF](https://www.stroustrup.com/JSF-AV-rules.pdf) |
| MISRA C++ / CERT C++ / AUTOSAR C++14 | — | **F (out of scope)** — safety-certification | — |

## Кандидаты на правила

Полный список в [rules/](rules/). Группировка ниже только для навигации.

### Из C++ Core Guidelines (high authority)
- [no-mutable-globals](rules/no-mutable-globals.md) — I.2
- [no-singletons](rules/no-singletons.md) — I.3
- [no-complex-global-init](rules/no-complex-global-init.md) — I.22
- [interface-pure-abstract](rules/interface-pure-abstract.md) — C.121
- [no-protected-data](rules/no-protected-data.md) — C.133
- [uniform-access-level](rules/uniform-access-level.md) — C.134
- [file-suffix](rules/file-suffix.md) — NL.27

### Из Lakos / Bloomberg BDE (high authority)
- [no-inter-component-friendship](rules/no-inter-component-friendship.md)
- [component-naming-prefix](rules/component-naming-prefix.md)
- [external-linkage-declared-in-header](rules/external-linkage-declared-in-header.md)
- [transitive-fan-in](rules/transitive-fan-in.md) — Lakos CD threshold

### Из Martin package principles (high authority)
- [depends-on-less-stable](rules/depends-on-less-stable.md) — SDP
- [stable-but-concrete](rules/stable-but-concrete.md) — SAP
- [unused-symbols-from-dep](rules/unused-symbols-from-dep.md) — CRP

### Из Google C++ Style (medium authority)
- [no-foreign-forward-decls](rules/no-foreign-forward-decls.md)
- [no-using-namespace-in-cpp](rules/no-using-namespace-in-cpp.md) — stricter SF.7
- [include-order-groups](rules/include-order-groups.md)

### Из HIC++ 4.0 (medium authority)
- [no-using-decl-in-header](rules/no-using-decl-in-header.md) — 7.3.6
- [one-extern-class-per-header](rules/one-extern-class-per-header.md) — 3.3.1

### Расширения существующих метрик
- [god-namespace](rules/god-namespace.md) — расширение god-headers
- [no-recursion-across-modules](rules/no-recursion-across-modules.md) — clang-tidy mapped
- [forward-decl-of-std](rules/forward-decl-of-std.md) — UB по `[namespace.std]`
- [header-cpp-pairing](rules/header-cpp-pairing.md) — BDE pairing
- [deep-nested-namespace](rules/deep-nested-namespace.md) — sprawl detector
- [macro-defined-in-header](rules/macro-defined-in-header.md) — leakage detector

## Исключено — design-only (category E)

Сознательно не пытаемся формализовать:
- High cohesion within a module — субъективно, требует human judgment.
- Right level of abstraction — не формализуется.
- DDD domain model purity — требует знания доменной логики.
- ADP cycle resolution by DIP — циклы детектим (SF.9), но *предписывать* DIP не наша задача.
- CCP (classes that change together) — нужна change history (git log), это другой инструмент.
- REP (granule of release = granule of reuse) — нужна release metadata, не статика.
- Bloomberg "narrow contracts" — design talk, не AST.

## Out of scope (category F)

MISRA C++, CERT C++, AUTOSAR C++14, бо́льшая часть JSF AV (cyclomatic complexity, function length, lines-per-class) — это safety/style. Спека [../architecture-spec.md](../architecture-spec.md) явно их исключает: embedded-команды под жёсткие сертификации платят за каталог чужих правил, это другая ниша (PVS-Studio, Coverity).

## Структурные находки

- **clang-tidy уже формализовал** SF.7 (`google-build-using-namespace`), SF.8 (`llvm-header-guard`), SF.9 (`misc-header-include-cycle`), SF.21 (`misc-anonymous-namespace-in-header`), SF.2 (`misc-definitions-in-headers`), I.2 (`cppcoreguidelines-avoid-non-const-global-variables`), C.121/C.133/C.134 (`cppcoreguidelines-non-private-member-variables-in-classes`). В отчёте cpparch можно ссылаться на эти ID как `equivalent: clang-tidy ...` — это снижает порог недоверия.
- **Bloomberg BDE wiki** — самый практичный источник после CCG SF. Все процитированные правила статически проверяемы; имеет смысл выделить как "Уровень 1.5" между SF и Lakos-метриками.
- **Martin package principles** дают 3 новых проверяемых правила сверх Ce/Ca/I/A/D: ADP (= SF.9, уже есть), **SDP-violation** (depend → less stable), **SAP-violation** (stable + concrete), **CRP** (unused symbols from dep). Заголовок маркетинга: *"checks all 6 Martin package principles where statically checkable"*.
- **ArchUnit/Deptrac/SonarQube** — референс таксономии, не новые правила. Совпадает со спекой.
