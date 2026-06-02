#!/usr/bin/env bash
# #066: перемер CLONEFAIL на НИЗКОЙ параллельности (P=2) с ретраем/пейсингом.
# CLONEFAIL в new300_measured.tsv — смесь rate-limit (P=16) + таймаут на крупных +
# реально мёртвые. Перемер чинит первые две причины и даёт честный funnel ≥300.
#   $1 PAR — параллелизм (по умолч. 2)
set -u
HERE=/home/localadm/projects/cpparch/experiments/ai_repo_run
MEAS="$HERE/new300_measured.tsv"
PAR="${1:-2}"
FAILS=/tmp/clonefail_remeasure.txt
OUT=/tmp/clonefail_remeasured.tsv
log="$HERE/remeasure_clonefail.log"

grep -P '\tCLONEFAIL\t' "$MEAS" | cut -f1 | sort -u > "$FAILS"
echo "remeasure старт $(date '+%F %T'): CLONEFAIL=$(wc -l <"$FAILS"), P=$PAR retry=3 timeout=300" > "$log"
: > "$OUT"
bash "$HERE/measure_candidates.sh" "$FAILS" 300 "$PAR" 3 >> "$OUT" 2>>"$log"

# статистика перемера
nfail=$(grep -cP '\tCLONEFAIL\t' "$OUT"); nskip=$(grep -cP '\tskip\t' "$OUT")
ntot=$(wc -l <"$OUT"); nmeas=$(( ntot - nfail - nskip ))
echo "remeasure готово $(date '+%F %T'): измерено=$ntot, оживших skip=$nskip, ≥300 измерено=$nmeas, всё ещё CLONEFAIL=$nfail" >> "$log"

# merge+dedup в MEAS: успешный/skip-замер вытесняет старый CLONEFAIL по repo
cp "$MEAS" "$MEAS.pre066"
awk -F'\t' '{
  r=$1; isfail=($2=="CLONEFAIL")
  if(!(r in seen) || (failrow[r] && !isfail)){ line[r]=$0; seen[r]=1; failrow[r]=isfail }
}END{for(r in line) print line[r]}' "$MEAS.pre066" "$OUT" > "$MEAS.tmp"
mv "$MEAS.tmp" "$MEAS"
echo "merge: было $(wc -l <"$MEAS.pre066"), стало $(wc -l <"$MEAS"), CLONEFAIL осталось $(grep -cP '\tCLONEFAIL\t' "$MEAS")" >> "$log"
cat "$log"
echo "REMEASURE_DONE"
