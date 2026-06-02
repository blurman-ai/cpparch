#!/usr/bin/env bash
# #066 RESUMABLE финишер: ждёт remeasure.done, затем докачивает все AI-плотные
# (conc≥5 ≥300, ≤500МБ) в _aidev_dense. clone_expand идемпотентен (пропускает готовые),
# так что безопасно перезапускать.
set -u
HERE=~/projects/cpparch/experiments/ai_repo_run
MEAS="$HERE/new300_measured.tsv"
STATE=~/oss/_aidev_state
DONEFLAG="$STATE/finish.done"
log="$STATE/finish_resumable.log"
mkdir -p "$STATE"

echo "finish_resumable старт $(date '+%F %T'): жду remeasure.done..." >> "$log"
while [ ! -f "$STATE/remeasure.done" ]; do sleep 120; done
echo "$(date +%T) перемер завершён, собираю clonelist" >> "$log"

# conc≥5 ≥300, по коммитам/год убыв.
awk -F'\t' '$2~/^[0-9]+$/&&$2>=300&&$4~/^[0-9]+$/&&$4>=5' "$MEAS" \
  | sort -t$'\t' -k2,2 -nr | cut -f1 > "$STATE/dense_clonelist.txt"
echo "$(date +%T) clonelist (conc≥5) = $(wc -l <"$STATE/dense_clonelist.txt")" >> "$log"

# докачка: cap 500МБ (API-precheck), бюджет 14ГБ, пропуск уже склонированных
bash "$HERE/clone_expand.sh" "$STATE/dense_clonelist.txt" 14000 500 dense_full >> "$log" 2>&1
echo "$(date +%T) ГОТОВО. $(tail -1 "$HERE/clone_dense_full.log" 2>/dev/null)" >> "$log"
echo "_aidev_dense: $(du -sh ~/oss/_aidev_dense 2>/dev/null|cut -f1), дир=$(ls ~/oss/_aidev_dense|wc -l)" >> "$log"
touch "$DONEFLAG"
echo "FINISH_RESUMABLE_DONE $(date '+%F %T')" >> "$log"
