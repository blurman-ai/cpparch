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

## План (по одному фиксу = коммит, фикстура+тест обязательны)
- [ ] #2.1 resolve_angle: стандартные системные хедеры → External (+ unit-тест).
- [ ] #2.2 SF.9: пропускать SCC = same-stem header+inline-impl (+ pass-фикстура+тест).
- [ ] #2.3 graph↔SF.9 согласовать / ценз по SF.9.
- [ ] #2.4 duplication: skip mirror/generated/vendored (+ тест).
- [ ] Перепрогнать ценз/FP-репы (PipeWire, legate, acts), подтвердить что FP ушли.

## Acceptance
Каждый фикс с фикстурой/тестом; FP-репы из отчётов перестают флагаться; дерево
проходит gate (build+ctest+coverage); dogfooding archcheck на себе зелёный.
