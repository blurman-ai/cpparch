# file-suffix

- **Category:** A (include-only / filesystem scan)
- **Authority:** high — C++ Core Guidelines
- **Source:** [CCG NL.27 — Use a `.cpp` suffix for code files and `.h` for interface files](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#nl27-use-a-cpp-suffix-for-code-files-and-h-for-interface-files)

## Rule

> "Long-standing convention. `.cpp` for source files, `.h` for headers."

## Why for cpparch

Тривиально проверяется, ловит реальный мусор (`.cc` рядом с `.cpp`, `.hpp` рядом с `.h`), даёт пользователю быструю победу на первом запуске. Допускает настройку — проект может выбрать `.cc/.hh` или `.cpp/.hpp`, главное единообразие.

## Detection

Filesystem-scan по корню проекта: собрать суффиксы исходников и заголовков, посчитать частоты. Если найден файл с миноритарным суффиксом — флагать. Допустимые наборы пар (одна на проект):
- `.cpp` + `.h`
- `.cpp` + `.hpp`
- `.cc` + `.h`
- `.cxx` + `.hxx`

## Fixtures

- `pass_cpp_h/` — только `.cpp` + `.h`.
- `pass_cpp_hpp/` — только `.cpp` + `.hpp`.
- `fail_mixed/` — `foo.cpp` рядом с `bar.cc`.
- `fail_outlier_header/` — все `.h`, один файл `.hxx`.
