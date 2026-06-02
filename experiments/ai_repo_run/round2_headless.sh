#!/usr/bin/env bash
# #066 round2 resume через headless claude (вызывается resume_all.sh после reboot, если
# round2_verdicts неполон). Workflow внутри сам пропускает готовые репы.
export HOME=/home/localadm
export PATH=/home/localadm/.local/bin:/usr/local/bin:/usr/bin:/bin
HERE=/home/localadm/projects/cpparch/experiments/ai_repo_run
STATE=/home/localadm/oss/_aidev_state
LOG="$STATE/round2_headless.log"
cd /home/localadm/projects/cpparch || exit 1
echo "=== round2_headless старт $(date '+%F %T') ===" >> "$LOG"
PROMPT="$(cat "$HERE/round2_resume_prompt.txt")"
/home/localadm/.local/bin/claude -p "$PROMPT" --dangerously-skip-permissions >> "$LOG" 2>&1
echo "=== round2_headless конец $(date '+%F %T') exit=$? ===" >> "$LOG"
