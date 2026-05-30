#!/bin/bash
# Прогон line-duplication спайка по корпусу AIDev C++ (50 реп).
# Многократный: правишь инструмент -> ./run_dup.sh <tag> -> сравниваешь run-папки.
#
# Usage: ./run_dup.sh <run_tag> [extra spike args...]
#   run_tag   — метка прогона (напр. v1_baseline, v2_excludes). Папка runs/<tag>/.
#   extra     — доп. аргументы спайку (напр. --min-lines 6).
#
# Читает корпус из corpus_50.tsv, репы из $CORPUS_DIR, бинарь из $BIN.
# Складывает: runs/<tag>/<repo>.txt (сырьё) + runs/<tag>/summary.tsv (сводка).

set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
CORPUS_TSV="$HERE/corpus_50.tsv"
CORPUS_DIR="${CORPUS_DIR:-~/oss/_aidev_run}"
BIN="${BIN:-/tmp/line_dup_build/line_duplication}"

TAG="${1:-untagged}"
shift || true
EXTRA=("$@")

OUT="$HERE/runs/$TAG"
mkdir -p "$OUT"

if [ ! -x "$BIN" ]; then
  echo "ERROR: spike binary not found/executable: $BIN" >&2
  exit 1
fi

# Зафиксировать контекст прогона для воспроизводимости.
{
  echo "tag:        $TAG"
  echo "binary:     $BIN"
  echo "binary_mtime: $(stat -c '%y' "$BIN" 2>/dev/null)"
  echo "corpus_dir: $CORPUS_DIR"
  echo "extra_args: ${EXTRA[*]:-(none)}"
} > "$OUT/_run_context.txt"

SUMMARY="$OUT/summary.tsv"
printf 'repo\teligible\tsigLOC\tdupLOC\tratio\tblocks\tcross_file\n' > "$SUMMARY"

grep -v '^#' "$CORPUS_TSV" | awk -F'\t' '{print $1}' | while read -r full; do
  [ -z "$full" ] && continue
  name=$(basename "$full")
  repo_path="$CORPUS_DIR/$name"
  if [ ! -d "$repo_path" ]; then
    printf '%s\tMISSING\t-\t-\t-\t-\t-\n' "$name" >> "$SUMMARY"
    continue
  fi
  raw="$OUT/$name.txt"
  "$BIN" "$repo_path" --top 12 "${EXTRA[@]}" > "$raw" 2>/dev/null

  el=$(grep -m1 'eligible files'    "$raw" | grep -oE '[0-9]+' | head -1)
  sl=$(grep -m1 'significant LOC'   "$raw" | grep -oE '[0-9]+' | head -1)
  dl=$(grep -m1 'duplicated LOC'    "$raw" | grep -oE '[0-9]+' | head -1)
  ra=$(grep -m1 'duplication ratio' "$raw" | grep -oE '[0-9.]+%' | head -1)
  bl=$(grep -m1 'duplicated blocks' "$raw" | grep -oE '[0-9]+' | head -1)
  cf=$(grep -m1 'cross-file blocks' "$raw" | grep -oE '[0-9]+' | head -1)
  printf '%s\t%s\t%s\t%s\t%s\t%s\t%s\n' \
    "$name" "${el:-?}" "${sl:-?}" "${dl:-?}" "${ra:-?}" "${bl:-?}" "${cf:-?}" >> "$SUMMARY"
done

echo "=== run '$TAG' done -> $OUT ==="
column -t -s$'\t' "$SUMMARY"
