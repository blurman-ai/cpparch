#!/usr/bin/env bash
# Clean-checkout helper for DRIFT-baseline runs.
# Required because `git checkout <sha> -- <path>` does not remove files
# absent in <sha>, and `git clean -fd` does not touch tracked files from
# prior worktree state, leading to false-positive DRIFT.1 findings.
# See backlog #048 for detailed rationale and empirical evidence.
#
# Usage: drift_run.sh <repo-path> <subdir> <before-sha> <after-sha> <label>
set -euo pipefail

repo=$1
sub=$2
before=$3
after=$4
label=$5

cd "$repo"

# Before revision: clean, checkout, scan baseline
git clean -fdx "$sub" >/dev/null
git checkout -f "$before" -- "$sub"
"$(dirname "$0")/../build/debug/src/archcheck" --save-graph-baseline "/tmp/drift_${label}_base.json" "$repo/$sub" 2>&1 | tail -1

# After revision: clean, checkout, scan and report drift
git clean -fdx "$sub" >/dev/null
git checkout -f "$after" -- "$sub"
"$(dirname "$0")/../build/debug/src/archcheck" --drift-baseline "/tmp/drift_${label}_base.json" "$repo/$sub" > "/tmp/drift_${label}_drift.txt" 2>&1 || true

echo "=== $label ==="
grep -E 'DRIFT' "/tmp/drift_${label}_drift.txt" | head -20 || true
grep 'violation(s)' "/tmp/drift_${label}_drift.txt" || echo "(no violations)"
