# Fixture source

Adapted from the PMD CPD test resources (BSD-3-Clause):
`pmd/pmd-cpp/src/test/resources/net/sourceforge/pmd/lang/cpp/cpd/testdata/`
(https://github.com/pmd/pmd, files `literals.cpp`, `ignoreIdents.cpp`).

The PMD files are tests of their tokenizer; here they are relabeled for the semantics of OUR
selective-normalization lexer (see docs/duplication_architecture.md §3.1, §6):
- `literals_a.cpp` / `literals_b.cpp` — the same code, only the literal VALUES are changed
  (the full C++ exotica: wide/char16/char32, hex escapes, raw strings,
  digit separators, binary) → the normalized sequences must match.
- `idents_a.cpp` / `idents_b.cpp` — the same code, only local identifiers are renamed
  → the sequences must match (rename-blind).
- `idents_c.cpp` — like `idents_a`, but the CALL (callee) is renamed → the sequence must
  DIFFER (selective: call names are a distinguishing signal, not erased).
