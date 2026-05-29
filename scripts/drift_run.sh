#!/usr/bin/env bash
# Usage: drift_one.sh <repo-path> <subdir> <after-sha> <label>
set -e
repo=$1; sub=$2; after=$3; label=$4
cd "$repo"
parent=$(git rev-parse "$after"^1 2>/dev/null)
if [ -z "$parent" ]; then echo "$label: no parent for $after"; exit 1; fi
git clean -fdx "$sub" >/dev/null 2>&1
git checkout -f "$parent" -- "$sub" 2>/dev/null
~/projects/cpparch/build/debug/src/archcheck --save-graph-baseline "/tmp/clean_${label}_base.json" "$repo/$sub" 2>&1 | tail -1
git clean -fdx "$sub" >/dev/null 2>&1
git checkout -f "$after" -- "$sub" 2>/dev/null
~/projects/cpparch/build/debug/src/archcheck --drift-baseline "/tmp/clean_${label}_base.json" "$repo/$sub" > "/tmp/clean_${label}_drift.txt" 2>&1 || true
echo "=== $label ==="
grep -E 'DRIFT' "/tmp/clean_${label}_drift.txt" | head -20 || true
grep 'violation(s)' "/tmp/clean_${label}_drift.txt" || echo "(no violations)"
