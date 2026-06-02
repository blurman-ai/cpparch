#!/usr/bin/env bash
# Параллельная докачка: бесконечный цикл measure→clone, пока есть кандидаты и
# не упёрлись в потолок диска по _aidev_dense. Качает ИИ-плотные ≥300 (conc>=5).
#   $1 CEIL_GB — потолок размера _aidev_dense (по умолч. 50)
set -u
HERE=~/projects/cpparch/experiments/ai_repo_run
MEAS="$HERE/new300_measured.tsv"
CAND=/tmp/harvest_candidates.txt
DST=~/oss/_aidev_dense
CEIL_GB="${1:-50}"
log="$HERE/keep_downloading.log"
sel(){ awk -F'\t' -v lo="$1" -v hi="$2" '$2~/^[0-9]+$/&&$2>=300&&$3~/^[0-9]+$/&&$4~/^[0-9]+$/&&$4>=lo&&$4<hi' "$MEAS" | sort -t$'\t' -k4,4 -nr -k2,2 -nr; }
selai(){ awk -F'\t' -v n="$1" '$2~/^[0-9]+$/&&$2>=300&&$3~/^[0-9]+$/&&$4~/^[0-9]+$/&&$3>=n' "$MEAS" | sort -t$'\t' -k4,4 -nr -k2,2 -nr; }
build_clonelist(){ { sel 50 9999; selai 150; sel 25 50; sel 5 25; } | awk -F'\t' '!seen[$1]++'; }
echo "keep_downloading старт $(date '+%F %T'), потолок ${CEIL_GB}G" > "$log"
while true; do
  used=$(du -sg "$DST" 2>/dev/null | cut -f1); used=${used:-0}
  if [ "$used" -ge "$CEIL_GB" ]; then echo "$(date +%T) потолок ${CEIL_GB}G (used=${used}G) — стоп" >>"$log"; break; fi
  build_clonelist | cut -f1 > /tmp/keepdl_list.txt
  remain_mb=$(( (CEIL_GB - used) * 1024 ))
  echo "$(date +%T) клон: used=${used}G, clonelist=$(wc -l </tmp/keepdl_list.txt), budget=${remain_mb}M" >>"$log"
  bash "$HERE/clone_expand.sh" /tmp/keepdl_list.txt "$remain_mb" 500 keepdl >/dev/null 2>&1
  tail -1 "$HERE/clone_keepdl.log" >>"$log"
  awk 'NR==FNR{m[$1]=1;next} !($1 in m)' "$MEAS" "$CAND" > /tmp/keepdl_rem.txt
  if [ ! -s /tmp/keepdl_rem.txt ]; then echo "$(date +%T) кандидаты исчерпаны — стоп" >>"$log"; break; fi
  head -3000 /tmp/keepdl_rem.txt > /tmp/keepdl_chunk.txt
  echo "$(date +%T) домер +$(wc -l </tmp/keepdl_chunk.txt) кандидатов" >>"$log"
  bash "$HERE/measure_candidates.sh" /tmp/keepdl_chunk.txt 300 3 3 >> "$MEAS"  # P=3: P=16 → rate-limit (#066)
done
echo "keep_downloading стоп $(date '+%F %T')" >>"$log"
