#!/usr/bin/env bash
# Budget-bounded полное клонирование AI-плотных реп в _aidev_dense.
# Последовательно (P=1) для точного учёта бюджета. Best-first порядок задаётся LIST.
#   $1 LIST          — файл, по одной "owner/repo" в строке (уже отсортировано)
#   $2 BUDGET_MB     — потолок суммарного НОВОГО объёма (по умолчанию 3000)
#   $3 MAX_REPO_MB   — крупнее этого репа не качаем (бережём бюджет на широту, по умолч. 500, #066)
#   $4 LOGTAG        — суффикс лог-файла
set -u
DST=/home/localadm/oss/_aidev_dense
LIST="$1"
BUDGET_MB="${2:-3000}"
MAX_REPO_MB="${3:-500}"
TAG="${4:-expand}"
log="/home/localadm/projects/cpparch/experiments/ai_repo_run/clone_${TAG}.log"
budget_kb=$(( BUDGET_MB * 1024 ))
maxrepo_kb=$(( MAX_REPO_MB * 1024 ))
new_kb=0; ok=0; fail=0; toobig=0; skip=0
mkdir -p "$DST"
echo "СТАРТ $(date '+%F %T')  list=$LIST budget=${BUDGET_MB}M maxrepo=${MAX_REPO_MB}M" > "$log"
while IFS= read -r repo; do
  [ -z "$repo" ] && continue
  name=$(echo "$repo" | tr / _); dst="$DST/$name"
  if [ -d "$dst/.git" ]; then skip=$((skip+1)); continue; fi
  if [ "$new_kb" -ge "$budget_kb" ]; then
    echo "BUDGET достигнут на cum=$(( new_kb/1024 ))M — стоп" >> "$log"; break
  fi
  # pre-check размера через API: не качать заведомо крупные репы (а потом удалять) — #066
  apisz=$(gh api "repos/$repo" --jq '.size' 2>/dev/null); apisz=${apisz:-0}
  if [ "$apisz" -gt "$maxrepo_kb" ]; then
    toobig=$((toobig+1)); echo "TOOBIG-API $repo ${apisz}k пропущен" >> "$log"; continue
  fi
  # Ретрай с нарастающим таймаутом/паузой: разовый клон фейлит на rate-limit/контеншене (#066)
  cloned=0
  for try in 1 2 3; do
    if timeout $(( 300 + (try-1)*180 )) git clone --quiet "https://github.com/$repo.git" "$dst" 2>/dev/null; then cloned=1; break; fi
    rm -rf "$dst"; sleep $(( try*15 ))
  done
  if [ "$cloned" -eq 1 ]; then
    sz=$(du -sk "$dst" 2>/dev/null | cut -f1); sz=${sz:-0}
    if [ "$sz" -gt "$maxrepo_kb" ]; then
      rm -rf "$dst"; toobig=$((toobig+1))
      echo "TOOBIG $repo ${sz}k удалён" >> "$log"; continue
    fi
    new_kb=$(( new_kb + sz )); ok=$((ok+1))
    echo "OK   $repo ${sz}k cum=$(( new_kb/1024 ))M" >> "$log"
  else
    rm -rf "$dst"; fail=$((fail+1))
    echo "FAIL $repo (3 попытки)" >> "$log"
  fi
done < "$LIST"
echo "ГОТОВО $(date '+%F %T') ok=$ok fail=$fail toobig=$toobig skip=$skip new=$(( new_kb/1024 ))M total_dirs=$(ls "$DST" | wc -l)" >> "$log"
