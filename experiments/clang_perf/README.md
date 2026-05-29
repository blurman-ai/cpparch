# clang_perf spike (#043)

Throw-away spike — measures libclang cost on real C++ projects to decide whether
v0.1 needs a separate fast (preprocessor-only) backend or can ship with libclang
as the single backend.

Not part of the build. Delete after the spike is written up in
`docs/dev/spike_clang_perf.md`.

## Prereqs (Astra Linux 1.7)

```bash
sudo apt install -y libclang-19-dev time
```

- `libclang-19-dev` (~150 MB) — gives `clang-c/Index.h` and `libclang.so` under
  `/usr/lib/llvm-19/`. The `libclang1-11` runtime package that ships by default
  is NOT enough — we need the dev headers and the CMake-discoverable .so.
- `time` (GNU) — Astra ships only the bash builtin, which has no `-v` flag.
  We need `/usr/bin/time -v` for peak RSS.

Не пытаемся подтянуть `find_package(Clang)` — на Astra ClangConfig.cmake от
llvm-11 битый (ссылается на отсутствующий ClangTargets). CMakeLists ищет
`libclang.so` и `Index.h` напрямую под `/usr/lib/llvm-19/`.

## Run

```bash
# 1. Build the spike binary against libclang-19 (Astra 1.7).
cmake -B build -S . -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/usr/lib/llvm-19
cmake --build build

# 2. Generate compile_commands.json for the target project (one-time).
cmake -S ~/oss/fmt -B ~/oss/fmt/build-cc -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
# (no actual build needed — only the DB)

# 3. Run the spike against the DB.
/usr/bin/time -v ./build/clang_perf ~/oss/fmt/build-cc/compile_commands.json

# 4. Repeat the run command 3 times — report the median.
```

## What it measures

- Per-TU wall-clock for `clang_parseTranslationUnit` + `clang_getInclusions`.
- Total wall-clock across all TUs (sequential, no threading).
- Peak RSS (read from /usr/bin/time -v output).

## What the baseline is

Same TUs scanned by a naive regex (`#include`-line count) for an
order-of-magnitude comparison vs libclang. See `regex_baseline.cpp`.
