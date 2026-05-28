#!/usr/bin/env bash
# Coverage gate: configure build/coverage, run tests, check thresholds.
# Uses a dedicated build directory — does NOT modify src/ or CMakeLists.txt.
#
# Thresholds (override via env):
#   MIN_LINES=70  MIN_FUNCTIONS=60  MIN_BRANCHES=40
#
# Exit: 0 = PASS, 1 = FAIL, 2 = tool/build error

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$ROOT/build/coverage"

MIN_LINES="${MIN_LINES:-90}"
MIN_FUNCTIONS="${MIN_FUNCTIONS:-90}"
MIN_BRANCHES="${MIN_BRANCHES:-40}"

# ── helpers ──────────────────────────────────────────────────────────────────
red()   { printf '\033[31m%s\033[0m\n' "$*"; }
green() { printf '\033[32m%s\033[0m\n' "$*"; }
bold()  { printf '\033[1m%s\033[0m\n' "$*"; }

need() { command -v "$1" &>/dev/null || { red "ERROR: '$1' not found — install it first"; exit 2; }; }

pct_int() {
   # extract integer percentage from lcov summary line: "  lines......: 38.5% (..."
   echo "$1" | grep -oP '\d+\.\d+(?=%)' | head -1 | cut -d. -f1
}

pct_full() {
   echo "$1" | grep -oP '\d+\.\d+(?=%)' | head -1
}

# ── preflight ─────────────────────────────────────────────────────────────────
need cmake
need ninja
need lcov
need genhtml

bold "=== coverage gate ==="
echo "Thresholds: lines≥${MIN_LINES}%  functions≥${MIN_FUNCTIONS}%  branches≥${MIN_BRANCHES}%"
echo

# ── configure (once; reuse cache on subsequent runs) ─────────────────────────
if [[ ! -f "$BUILD_DIR/CMakeCache.txt" ]]; then
   echo "► Configuring $BUILD_DIR ..."
   cmake -B "$BUILD_DIR" -S "$ROOT" -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DARCHCHECK_ENABLE_COVERAGE=ON \
      -DFETCHCONTENT_UPDATES_DISCONNECTED=ON \
      -DARCHCHECK_ENABLE_WARNINGS=OFF \
      2>&1 | grep -E '(CMake|error|warning|Configuring)' || true
else
   echo "► Reusing existing cmake cache in $BUILD_DIR"
fi

# ── build ─────────────────────────────────────────────────────────────────────
echo "► Building ..."
cmake --build "$BUILD_DIR" --parallel 2>&1 | tail -3

# ── reset counters for a clean run ───────────────────────────────────────────
find "$BUILD_DIR" -name '*.gcda' -delete

# ── run tests ─────────────────────────────────────────────────────────────────
echo "► Running tests ..."
if ! "$BUILD_DIR/tests/archcheck_tests" --reporter compact 2>&1 | tail -5; then
   red "ERROR: tests failed — fix them before checking coverage"
   exit 2
fi

# ── collect coverage ──────────────────────────────────────────────────────────
echo "► Collecting coverage data ..."
INFO_ALL="$BUILD_DIR/cov_all.info"
INFO_SRC="$BUILD_DIR/cov_src.info"

lcov --capture \
     --directory "$BUILD_DIR" \
     --rc lcov_branch_coverage=1 \
     --output-file "$INFO_ALL" \
     --quiet

# lcov 1.13 requires patterns without nested wildcards — use */_deps/* not */build/_deps/*
lcov --remove "$INFO_ALL" \
     '*/_deps/*' '/usr/*' '*/c++/*' '*/tests/*' '*/catch2/*' \
     --rc lcov_branch_coverage=1 \
     --output-file "$INFO_SRC" \
     --quiet

# ── parse results ─────────────────────────────────────────────────────────────
SUMMARY=$(lcov --summary "$INFO_SRC" --rc lcov_branch_coverage=1 2>&1)

LINE_LINE=$(echo "$SUMMARY"     | grep 'lines\.\.')
FUNC_LINE=$(echo "$SUMMARY"     | grep 'functions\.')
BRANCH_LINE=$(echo "$SUMMARY"   | grep 'branches\.')

LINE_PCT=$(pct_full   "$LINE_LINE")
FUNC_PCT=$(pct_full   "$FUNC_LINE")
BRANCH_PCT=$(pct_full "$BRANCH_LINE")

LINE_INT=$(pct_int   "$LINE_LINE")
FUNC_INT=$(pct_int   "$FUNC_LINE")
BRANCH_INT=$(pct_int "$BRANCH_LINE")

# ── generate HTML (always — useful even on fail) ──────────────────────────────
HTML_DIR="$BUILD_DIR/coverage_html"
genhtml "$INFO_SRC" \
        --output-directory "$HTML_DIR" \
        --title "archcheck coverage" \
        --branch-coverage \
        --legend \
        --quiet

# ── evaluate thresholds ───────────────────────────────────────────────────────
PASS=true
check() {
   local name=$1 actual=$2 actual_full=$3 threshold=$4
   if (( actual >= threshold )); then
      green "  ✓ $name: ${actual_full}% (≥${threshold}%)"
   else
      red   "  ✗ $name: ${actual_full}% (need ≥${threshold}%)"
      PASS=false
   fi
}

echo
bold "Coverage results (src/ only):"
check "lines    " "$LINE_INT"   "$LINE_PCT"   "$MIN_LINES"
check "functions" "$FUNC_INT"   "$FUNC_PCT"   "$MIN_FUNCTIONS"
check "branches " "$BRANCH_INT" "$BRANCH_PCT" "$MIN_BRANCHES"

echo
echo "HTML report: $HTML_DIR/index.html"
echo

if $PASS; then
   green "RESULT: PASS"
   exit 0
else
   red "RESULT: FAIL"
   exit 1
fi
