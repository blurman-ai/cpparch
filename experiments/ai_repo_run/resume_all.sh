#!/usr/bin/env bash
# #066 МАСТЕР автозапуска после reboot (вызывается @reboot из /etc/cron.d). Возобновляет
# все долгие процессы с того места, где встали. Идемпотентен: пропускает завершённое
# (.done-флаги) и уже запущенное (pgrep). Можно безопасно гонять вручную.
export HOME=/home/localadm
export PATH=/home/localadm/.local/bin:/usr/local/bin:/usr/bin:/bin
HERE=/home/localadm/projects/cpparch/experiments/ai_repo_run
STATE=/home/localadm/oss/_aidev_state
log="$STATE/resume_all.log"
mkdir -p "$STATE"

# single-instance
if ! mkdir "$STATE/resume_all.lock" 2>/dev/null; then echo "$(date +%T) resume_all уже идёт" >> "$log"; exit 0; fi
trap 'rmdir "$STATE/resume_all.lock" 2>/dev/null' EXIT
echo "=== resume_all $(date '+%F %T') ===" >> "$log"

# 1) перемер (скачивание) — если не done и не запущен
if [ ! -f "$STATE/remeasure.done" ] && ! pgrep -f remeasure_resumable.sh >/dev/null; then
  nohup bash "$HERE/remeasure_resumable.sh" 2 >>"$log" 2>&1 &
  echo "$(date +%T) remeasure_resumable запущен" >> "$log"
else echo "$(date +%T) перемер: done или уже идёт — пропуск" >> "$log"; fi

# 2) финишер (докачка dense) — ждёт remeasure.done сам
if [ ! -f "$STATE/finish.done" ] && ! pgrep -f finish_resumable.sh >/dev/null; then
  nohup bash "$HERE/finish_resumable.sh" >>"$log" 2>&1 &
  echo "$(date +%T) finish_resumable запущен" >> "$log"
else echo "$(date +%T) финишер: done или уже идёт — пропуск" >> "$log"; fi

# 3) round2 верификация — если неполна (json < число drift) и нет активного headless
drift=$(awk -F'\t' '$NF=="drift"' "$HERE/corpus_check_summary.tsv" 2>/dev/null | wc -l)
done2=$(ls "$HERE/../verification/round2_verdicts"/*.json 2>/dev/null | wc -l)  # переехало (#070)
if [ "$done2" -lt "$drift" ] && ! pgrep -f 'claude -p' >/dev/null; then
  nohup /bin/bash "$HERE/round2_headless.sh" >>"$log" 2>&1 &
  echo "$(date +%T) round2 headless запущен ($done2/$drift)" >> "$log"
else echo "$(date +%T) round2: полон ($done2/$drift) или headless идёт — пропуск" >> "$log"; fi

echo "$(date +%T) resume_all готов" >> "$log"
