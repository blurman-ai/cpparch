# Источник фикстур

Адаптировано из тест-ресурсов PMD CPD (BSD-3-Clause):
`pmd/pmd-cpp/src/test/resources/net/sourceforge/pmd/lang/cpp/cpd/testdata/`
(https://github.com/pmd/pmd, файлы `literals.cpp`, `ignoreIdents.cpp`).

PMD-файлы — тесты их токенизатора; здесь они переразмечены под семантику НАШЕГО
selective-normalization лексера (см. docs/duplication_architecture.md §3.1, §6):
- `literals_a.cpp` / `literals_b.cpp` — тот же код, изменены только ЗНАЧЕНИЯ
  литералов (вся экзотика C++: wide/char16/char32, hex-эскейпы, raw strings,
  digit separators, бинарные) → нормализованные seq обязаны совпасть.
- `idents_a.cpp` / `idents_b.cpp` — тот же код, переименованы только локальные
  идентификаторы → seq обязаны совпасть (rename-blind).
- `idents_c.cpp` — как `idents_a`, но переименован ВЫЗОВ (callee) → seq обязан
  ОТЛИЧАТЬСЯ (selective: имена вызовов — различающий сигнал, не стираются).
