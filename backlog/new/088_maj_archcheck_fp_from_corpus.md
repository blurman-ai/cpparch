# 088 — archcheck v0.1 FP-фиксы из корпус-стресс-теста

**Тип:** maj. **Откуда:** структурный ценз 767 нарушителей (1493 агентских C++) вытащил
систематические false-positive в SF.9 и duplication. Отчёты:
`docs/research/cpp_cycles_report.md`, `docs/research/cpp_copypaste_report.md`.

## Находки (FP-классы)
1. **Системные `<...>` mis-resolution.** `<string.h>` (и др. стандартные C-хедеры)
   suffix-матчатся на одноимённый файл проекта → фантомное ребро/цикл
   (PipeWire `defs.h ↔ string.h`). → `resolve_angle` должен резолвить стандартные
   системные хедеры как External, без suffix-match.
2. **`.inl`-идиома в SF.9.** Цикл `X.h ↔ X.{inl,ipp,tcc,tpp,tmpl.h,hxx}` одного stem —
   легитимный сплит модуля (header + inline-impl), петля рвётся `#pragma once`/include-guard.
   SF.9 не должен это репортить (legate 163 «циклов», acts 46 — почти всё это).
3. **Расхождение `--graph sccs_cyclic` vs SF.9.** `--graph` даёт сырой счёт SCC,
   SF.9 фильтрует conditional (и теперь .inl). pyA2L: graph=22, SF.9=0. Ценз мерил сырой
   `--graph`, а не SF.9 → завышение. Привести в согласие / ценз мерить по SF.9.
4. **Duplication: вендоринг/генерёнка.** Лидеры плотности — `argparse.hpp` (вендор),
   `bootstrap.c` (генерёнка). Токеновый детектор не отделяет вендоренное/сгенерированное
   от рукотворной копии. → расширить mirror/generated-фильтр на duplication.

> **Сверка с кодом (бэклог-ревью 2026-06-11):** 3 из 5 пунктов уже реализованы — отмечены ниже
> с коммитами. Живой остаток задачи: №2.3 + контрольный перепрогон.

## План (по одному фиксу = коммит, фикстура+тест обязательны)
- [x] #2.1 resolve_angle: стандартные системные хедеры → External (+ unit-тест). — реализовано `9b3696e` (`is_system_header` в `src/scan/include_resolver.cpp`).
- [x] #2.2 SF.9: пропускать SCC = same-stem header+inline-impl (+ pass-фикстура+тест). — реализовано `8c05878` (`isInlineSplitScc` в `src/rules/sf9_no_cycles.cpp`). Не путать с conditional-фильтром из #032 (`04b523b`) — это два независимых механизма: #032 гасит `#ifdef`-циклы (spdlog), #2.2 — безусловные same-stem пары (legate/acts).
- [ ] #2.3 graph↔SF.9 согласовать / ценз по SF.9.
- [x] #2.4 duplication: skip mirror/generated/vendored (+ тест). — закрыто другими задачами: vendor-exclude централизован в #069 (`project_files.cpp`), generated-гард P0.9 `isGeneratedPath` из #065/#070 (`duplication_scanner.cpp`), whole-file twin suppression P0.2 (#070). Задача #064 на ту же тему закрыта как поглощённая.
- [ ] Перепрогнать ценз/FP-репы (PipeWire, legate, acts; + spdlog — хвост из #032), подтвердить что FP ушли.

## Acceptance
Каждый фикс с фикстурой/тестом; FP-репы из отчётов перестают флагаться; дерево
проходит gate (build+ctest+coverage); dogfooding archcheck на себе зелёный.
