#!/usr/bin/env bash
# #066 RESUMABLE перемер CLONEFAIL. Стейт в ~/oss/_aidev_state (НЕ /tmp —
# переживает reboot). Пропускает уже перемеренные репы → можно убивать/перезапускать.
set -u
HERE=~/projects/cpparch/experiments/ai_repo_run
MEAS="$HERE/new300_measured.tsv"
STATE=~/oss/_aidev_state
OUT="$STATE/clonefail_remeasured.tsv"
TODO="$STATE/remeasure_todo.txt"
DONEFLAG="$STATE/remeasure.done"
log="$STATE/remeasure_resumable.log"
PAR="${1:-2}"
mkdir -p "$STATE"; touch "$OUT"

echo "remeasure_resumable старт $(date '+%F %T'), уже в OUT: $(wc -l <"$OUT")" >> "$log"
while true; do
  # CLONEFAIL из MEAS, которых ещё нет в OUT (по repo)
  grep -P '\tCLONEFAIL\t' "$MEAS" | cut -f1 | sort -u > "$STATE/_cf.txt"
  cut -f1 "$OUT" | sort -u > "$STATE/_done.txt"
  comm -23 "$STATE/_cf.txt" "$STATE/_done.txt" > "$TODO"
  # Заведомо-гигантские upstream-форки/зеркала: AI-дрейфа нет, блоблес-клон гиганта = зря время.
  # Помечаем TOOBIG-skip без клона (name-фильтр, т.к. API-precheck размера на 16k реп снёс бы rate-limit).
  GIANT='chromium|llvm-project|gecko-dev|[Ww]eb[Kk]it|torvalds/linux|/linux\.src|aosp|android\.googlesource|/tensorflow$|/pytorch$|src-leak|-src\.leak|o3de/o3de|/godot$|/unreal|UnrealEngine|/mozilla-central'
  grep -iE "$GIANT" "$TODO" | awk '{printf "%s\tTOOBIG\tskip\t\n",$1}' >> "$OUT"
  grep -ivE "$GIANT" "$TODO" > "$TODO.f" && mv "$TODO.f" "$TODO"
  n=$(wc -l < "$TODO")
  echo "$(date +%T) осталось перемерить: $n" >> "$log"
  if [ "$n" -eq 0 ]; then
    # merge OUT в MEAS с дедупом (успех/skip вытесняет CLONEFAIL)
    cp "$MEAS" "$MEAS.preresume"
    awk -F'\t' '{r=$1;f=($2=="CLONEFAIL");if(!(r in s)||(fr[r]&&!f)){l[r]=$0;s[r]=1;fr[r]=f}}END{for(r in l)print l[r]}' \
      "$MEAS.preresume" "$OUT" > "$MEAS.tmp" && mv "$MEAS.tmp" "$MEAS"
    echo "$(date +%T) MERGE готов. MEAS=$(wc -l <"$MEAS"), CLONEFAIL осталось=$(grep -cP '\tCLONEFAIL\t' "$MEAS")" >> "$log"
    touch "$DONEFLAG"
    echo "REMEASURE_RESUMABLE_DONE $(date '+%F %T')" >> "$log"
    break
  fi
  # один проход measure по TODO (внутри свой retry=3, timeout=300); дописываем в OUT
  bash "$HERE/measure_candidates.sh" "$TODO" 300 "$PAR" 3 >> "$OUT" 2>>"$log"
done
