#!/usr/bin/env bash
# #066 round2 resume через headless claude (вызывается resume_all.sh после reboot, если
# verify_results2 неполон). Workflow внутри сам пропускает готовые репы.
export HOME=~
export PATH=~/.local/bin:/usr/local/bin:/usr/bin:/bin
HERE=~/projects/cpparch/experiments/ai_repo_run
STATE=~/oss/_aidev_state
LOG="$STATE/round2_headless.log"
cd ~/projects/cpparch || exit 1
echo "=== round2_headless старт $(date '+%F %T') ===" >> "$LOG"
PROMPT="$(cat "$HERE/round2_resume_prompt.txt")"
~/.local/bin/claude -p "$PROMPT" --dangerously-skip-permissions >> "$LOG" 2>&1
echo "=== round2_headless конец $(date '+%F %T') exit=$? ===" >> "$LOG"
