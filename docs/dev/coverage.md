# Test coverage

Coverage is measured with **gcov/lcov** via the CMake option `ARCHCHECK_ENABLE_COVERAGE`.
The option is `OFF` by default, so regular `build/debug` and `build/release` builds are unaffected.

## Prerequisites

```bash
sudo apt install lcov   # Ubuntu/Debian
brew install lcov       # macOS
```

## One-time setup

Configure a dedicated build directory — do this once, then reuse it:

```bash
cmake -B build/coverage -S . -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DARCHCHECK_ENABLE_COVERAGE=ON
```

## Rebuild and collect coverage

```bash
cmake --build build/coverage
cd build/coverage
ctest --output-on-failure
```

Collect raw counters, strip noise (system headers, third-party deps, test files), generate HTML:

```bash
lcov --capture --directory . --output-file coverage.info

lcov --remove coverage.info \
     '*/build/_deps/*' '/usr/*' '*/c++/*' '*/tests/*' '*/ryml/*' '*/c4/*' \
     --output-file coverage_src.info

genhtml coverage_src.info \
     --output-directory coverage_html \
     --title "archcheck coverage" \
     --legend
```

Open the report:

```bash
xdg-open coverage_html/index.html   # Linux
open coverage_html/index.html       # macOS
```

## Interpreting results

The `--remove` chain keeps only `src/` and `include/archcheck/` — the production code.
Test sources and vendored dependencies are excluded so the percentages reflect real gaps.

| Metric    | Meaning |
|-----------|---------|
| Lines     | Executable lines hit at least once |
| Functions | Functions called at least once |
| Branches  | Not collected by default (requires `--branch-coverage` flag and recompile) |

## Reset counters between runs

gcov counters accumulate across runs. To get a clean measurement:

```bash
find build/coverage -name '*.gcda' -delete
ctest --output-on-failure   # re-run tests
# then repeat the lcov steps above
```

Or just wipe and rebuild the directory:

```bash
rm -rf build/coverage
cmake -B build/coverage -S . -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DARCHCHECK_ENABLE_COVERAGE=ON
cmake --build build/coverage
```
