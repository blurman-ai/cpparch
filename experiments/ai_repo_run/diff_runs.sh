#!/bin/bash
# Сравнить два прогона по корпусу: что сдвинулось после правки инструмента.
# Usage: ./diff_runs.sh <tagA> <tagB>
#   показывает по каждой репе ratio/blocks/cross_file в A -> B и дельту.

set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
A="$HERE/runs/${1:?need tagA}/summary.tsv"
B="$HERE/runs/${2:?need tagB}/summary.tsv"

for f in "$A" "$B"; do
  [ -f "$f" ] || { echo "ERROR: no summary: $f" >&2; exit 1; }
done

echo "A=$1  B=$2   (ratio | blocks | cross_file)"
echo "------------------------------------------------------------"
# join по repo (колонка 1). summary колонки: repo el sl dl ratio blocks cf
join -t$'\t' -1 1 -2 1 \
  <(tail -n +2 "$A" | sort) \
  <(tail -n +2 "$B" | sort) \
| awk -F'\t' '{
    # A: $5 ratio $6 blocks $7 cf ; B: $11 ratio $12 blocks $13 cf
    mark = ($5!=$11 || $6!=$12 || $7!=$13) ? " *" : "";
    printf "%-24s %8s->%-8s  %4s->%-4s  %3s->%-3s%s\n", $1, $5, $11, $6, $12, $7, $13, mark;
  }'
