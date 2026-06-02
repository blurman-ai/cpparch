#!/usr/bin/env bash
# #066: перемер случайного сэмпла CLONEFAIL для ОЦЕНКИ undercount-фактора без
# полного 45ч-прогона. НЕ мёржит в MEAS — пишет в отдельный файл и считает funnel.
# Ждёт завершения sanity-прогона (общий scratch/сеть), потом гонит P=3 retry=3.
set -u
HERE=/home/localadm/projects/cpparch/experiments/ai_repo_run
SAMPLE=/tmp/sample2000.txt
OUT="$HERE/sample2000_measured.tsv"
log="$HERE/sample_estimate.log"

echo "sample_estimate старт $(date '+%F %T'): жду освобождения сети от sanity..." > "$log"
while pgrep -f 'measure_candidates.sh /tmp/sanity12' >/dev/null; do sleep 10; done
echo "$(date +%T) sanity свободен, гоню сэмпл $(wc -l <"$SAMPLE") при P=3 retry=3 timeout=300" >> "$log"

: > "$OUT"
bash "$HERE/measure_candidates.sh" "$SAMPLE" 300 3 3 >> "$OUT" 2>>"$log"

n=$(wc -l <"$OUT")
nfail=$(grep -cP '\tCLONEFAIL\t' "$OUT")
nskip=$(grep -cP '\tskip\t' "$OUT")
n300=$(( n - nfail - nskip ))            # ожили И ≥300 коммитов
ndense=$(awk -F'\t' '$2~/^[0-9]+$/&&$2>=300&&$4~/^[0-9]+$/&&$4>=5' "$OUT" | wc -l)
revived=$(( n - nfail ))
{
  echo "=== ОЦЕНКА (#066) $(date '+%F %T') ==="
  echo "сэмпл:                 $n из 16395 CLONEFAIL"
  echo "всё ещё CLONEFAIL:     $nfail ($(( nfail*100/n ))%)"
  echo "ожили (skip+≥300):     $revived ($(( revived*100/n ))%)"
  echo "  из них <300 (skip):  $nskip"
  echo "  из них ≥300:         $n300"
  echo "  из них ≥300 conc≥5:  $ndense"
  echo "--- экстраполяция на 16395 ---"
  echo "доп. ≥300 репо ≈ $(( n300 * 16395 / n ))"
  echo "доп. ≥300 AI-плотных (conc≥5) ≈ $(( ndense * 16395 / n ))"
  echo "(было измерено ≥300=794, AI-плотных=81 — см. насколько занижено)"
} >> "$log"
cat "$log"
echo "SAMPLE_ESTIMATE_DONE"
