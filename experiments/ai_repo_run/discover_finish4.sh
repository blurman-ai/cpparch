#!/usr/bin/env bash
# Task 2: добор ещё +5 ГБ активных ИИ-проектов (≥300 коммитов, conc>=5).
# Домер остатка с таймаутом → re-tier по всему MEAS → клон под СВЕЖИЙ бюджет 5 ГБ
# (clone_expand пропускает уже склонированные 23). conc<5 (pure-human) отбрасывается.
set -u
HERE=/home/localadm/projects/cpparch/experiments/ai_repo_run
MEAS="$HERE/new300_measured.tsv"
CAND=/tmp/harvest_candidates.txt
BUDGET_MB="${1:-5000}"
WINNERS="$HERE/new300_winners.tsv"
CLONELIST="$HERE/new300_clonelist.tsv"
log="$HERE/discover_finish4.log"

# остаток = кандидаты минус уже измеренные
awk -F'\t' 'NR==FNR{m[$1]=1;next} !($1 in m)' "$MEAS" "$CAND" > /tmp/remaining2.txt
echo "task2 старт $(date '+%F %T'): остаток $(wc -l </tmp/remaining2.txt), домер P=3 timeout 2ч" > "$log"
timeout 7200 bash "$HERE/measure_candidates.sh" /tmp/remaining2.txt 300 3 3 >> "$MEAS"
echo "домер стоп $(date '+%F %T'): всего строк $(wc -l <"$MEAS")" >> "$log"

# ретрай CLONEFAIL (P=2 — максимально бережно к rate-limit, #066)
grep -P '\tCLONEFAIL\t' "$MEAS" | cut -f1 | sort -u > /tmp/clonefail2.txt
[ -s /tmp/clonefail2.txt ] && timeout 1800 bash "$HERE/measure_candidates.sh" /tmp/clonefail2.txt 300 2 3 >> "$MEAS" 2>>"$log"

# re-tier по всему MEAS
sel(){ awk -F'\t' -v lo="$1" -v hi="$2" '$2~/^[0-9]+$/&&$2>=300&&$3~/^[0-9]+$/&&$4~/^[0-9]+$/&&$4>=lo&&$4<hi' "$MEAS" | sort -t$'\t' -k4,4 -nr -k2,2 -nr; }
selai(){ awk -F'\t' -v n="$1" '$2~/^[0-9]+$/&&$2>=300&&$3~/^[0-9]+$/&&$4~/^[0-9]+$/&&$3>=n' "$MEAS" | sort -t$'\t' -k4,4 -nr -k2,2 -nr; }
{ sel 50 9999; selai 150; } | awk -F'\t' '!seen[$1]++' > "$WINNERS"
{ cat "$WINNERS"; sel 25 50; sel 5 25; } | awk -F'\t' '!seen[$1]++' > "$CLONELIST"
echo "tier1=$(wc -l <"$WINNERS")  clonelist(conc>=5)=$(wc -l <"$CLONELIST")" >> "$log"

# клон под свежий 5 ГБ (TAG=discovery2, пропускает уже склонированные)
cut -f1 "$CLONELIST" > /tmp/winners_list2.txt
bash "$HERE/clone_expand.sh" /tmp/winners_list2.txt "$BUDGET_MB" 500 discovery2
echo "ALL DONE $(date '+%F %T')" >> "$log"
tail -1 "$HERE/clone_discovery2.log" >> "$log"
