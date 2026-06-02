#!/usr/bin/env bash
# #066: автономный финишер. Ждёт завершения полного перемера, затем пересобирает
# dense-clonelist по обновлённому MEAS и доклонирует НОВЫЕ AI-плотные ≤500МБ.
# Полностью detached (nohup) — доедет ночью без участия Claude.
set -u
HERE=/home/localadm/projects/cpparch/experiments/ai_repo_run
MEAS="$HERE/new300_measured.tsv"
log="$HERE/finish_remeasure.log"

echo "finish старт $(date '+%F %T'): жду конца перемера..." > "$log"
while pgrep -f 'remeasure_clonefail.sh' >/dev/null; do sleep 120; done
echo "$(date +%T) перемер завершён. MEAS=$(wc -l <"$MEAS"), CLONEFAIL осталось=$(grep -cP '\tCLONEFAIL\t' "$MEAS")" >> "$log"

# воронка после перемера
n300=$(awk -F'\t' '$2~/^[0-9]+$/&&$2>=300&&$4~/^[0-9]+$/' "$MEAS" | wc -l)
ndense=$(awk -F'\t' '$2~/^[0-9]+$/&&$2>=300&&$4~/^[0-9]+$/&&$4>=5' "$MEAS" | wc -l)
echo "≥300=$n300  AI-плотных conc≥5=$ndense (было 81)" >> "$log"

# clonelist: conc≥5 ≥300, по коммитам/год убыв.
awk -F'\t' '$2~/^[0-9]+$/&&$2>=300&&$4~/^[0-9]+$/&&$4>=5' "$MEAS" \
  | sort -t$'\t' -k2,2 -nr | cut -f1 > /tmp/dense_all_clonelist.txt
echo "$(date +%T) clonelist (conc≥5) = $(wc -l </tmp/dense_all_clonelist.txt)" >> "$log"

# докачка: cap 500МБ (API-precheck отсечёт тяжёлые), пропуск уже склонированных, бюджет 14ГБ
bash "$HERE/clone_expand.sh" /tmp/dense_all_clonelist.txt 14000 500 dense_full >> "$log" 2>&1
echo "$(date +%T) ГОТОВО. $(tail -1 "$HERE/clone_dense_full.log")" >> "$log"
echo "_aidev_dense: $(du -sh /home/localadm/oss/_aidev_dense 2>/dev/null|cut -f1), дир=$(ls /home/localadm/oss/_aidev_dense|wc -l)" >> "$log"
echo "FINISH_DONE $(date '+%F %T')" >> "$log"
