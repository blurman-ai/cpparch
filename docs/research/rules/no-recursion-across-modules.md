# no-recursion-across-modules

- **Category:** B/C (AST + graph)
- **Authority:** medium — расширение clang-tidy
- **Source:** clang-tidy `misc-no-recursion` (за основу детектора call-graph); cpparch-специфика — мэппинг на модули

## Rule

> "No call-graph recursion that crosses module boundaries."

## Why for cpparch

Обычная рекурсия внутри модуля — нормально. Рекурсия, где цикл проходит через ≥ 2 модуля (`A::foo() → B::bar() → A::baz()`), означает скрытую двустороннюю зависимость — даже если includes выглядят как DAG, runtime call-graph замкнут. Часто признак того, что слой неаккуратно разрезан и две части модулей реально один компонент. cpparch уже строит граф компонентов — добавление call-graph поверх даёт дополнительный сигнал.

## Detection

1. AST-проход: построить call-graph `FunctionDecl → FunctionDecl` (Clang `CallGraph`).
2. Спроецировать вершины на модули (по mapping path → module).
3. Найти strongly-connected components в проекции; для каждой SCC размером > 1 — флагать как «inter-module recursion: A ↔ B».

Опционально, не дефолт: дорого, требует libclang и полный AST.

## Fixtures

- `pass_intra_module/` — `module_a::factorial()` рекурсивно вызывает себя.
- `pass_dag/` — `A::foo() → B::bar()`, без обратного вызова.
- `fail_cross_module/` — `A::foo() → B::bar() → A::baz() → A::foo()`.
