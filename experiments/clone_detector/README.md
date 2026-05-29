# experiments/clone_detector/

Starter для задачи [#052 cross-TU duplication detector](../../backlog/new/052_maj_cross_tu_duplication_detector.md).

## clone_spike.cpp

Доказывает: `clang::CloneDetector` находит клон, члены которого лежат в РАЗНЫХ translation unit'ах, если все `ASTUnit`'ы держать живыми и кормить их тела в один общий детектор.

Что показывает:
- **Q1.** Кросс-TU детект работает (Type-2 клон с консистентным переименованием).
- **Q1 + sweep `MinComplexity ∈ {5,15,30}`.** При `5` — реальный клон + под-statement шум; при `15` — только реальный.
- **Q2.** `MatchingVariablePatternConstraint` через два `ASTContext` не падает, но шум он НЕ режет — шум режет `MinComplexity`.

## Сборка (Linux, clang 18)

```bash
# Узнать актуальные suffix'ы:
ls /usr/lib/x86_64-linux-gnu/libclang-cpp.so*
ls /usr/lib/x86_64-linux-gnu/libLLVM.so*

clang++ -std=c++17 clone_spike.cpp \
  -l:libclang-cpp.so.18 \
  -l:libLLVM.so.18 \
  $(llvm-config-18 --cxxflags --ldflags --system-libs) \
  -o clone_spike
./clone_spike
```

## Stage 0 — что предстоит

Заменить строковые исходники на реальный обход `compile_commands.json` (через `clang::tooling::ClangTool` / `buildASTs`, либо `buildASTFromCodeWithArgs` по списку файлов из БД). Прогнать на `fmt` → `spdlog` → `folly|llvm-project`. Записать в `PERF.md`:

| repo | LOC | #TU | peak RSS (держим все AST) | parse time | findClones time | #cross-component function clones |

GATE: peak RSS на `spdlog` > ~4 GB ИЛИ `findClones` суперлинейно/неюзабельно → СТОП, идём в индекс/fingerprint Stage 1 (тяжёлый вариант) или пересмотр фичи.

## Pointers

- API: `clang/Analysis/CloneDetection.h`
- Стартовый pipeline: `RecursiveCloneTypeIIHashConstraint` + `MinComplexityConstraint(N)` + `MinGroupSizeConstraint(2)` + `RecursiveCloneTypeIIVerifyConstraint` + `OnlyLargestCloneConstraint`
- `MatchingVariablePatternConstraint` — НЕ использовать в продакшене: ~50% точность suggestions (FIXME в исходнике clang)
- Атрибуция правила: **Lakos** (replication vs hierarchical reuse) + **GitClear** (2024: копи-паст ×8, обогнал рефакторинг)
