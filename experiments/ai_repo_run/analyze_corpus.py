#!/usr/bin/env python3
"""Analyze corpus data: statistics, correlations, interesting findings."""

from __future__ import annotations

import csv
from pathlib import Path
from statistics import mean, stdev


def load_tsv(path: Path) -> list[dict[str, str]]:
    """Load TSV file into list of dicts."""
    rows = []
    with open(path, "r", encoding="utf-8") as f:
        reader = csv.DictReader(f, delimiter="\t")
        for row in reader:
            rows.append(row)
    return rows


def parse_float(val: str) -> float:
    """Parse float from string, return 0 on error."""
    try:
        return float(val.rstrip("%"))
    except ValueError:
        return 0.0


def parse_int(val: str) -> int:
    """Parse int from string, return 0 on error."""
    try:
        return int(val)
    except ValueError:
        return 0


def main() -> None:
    summary_path = Path("/home/localadm/projects/cpparch/experiments/ai_repo_run/corpus_summary.tsv")
    if not summary_path.exists():
        print(f"error: {summary_path} not found", file=sys.stderr)
        return

    rows = load_tsv(summary_path)
    if not rows:
        print("no data", file=sys.stderr)
        return

    # Parse metrics
    ai_pcts = [parse_float(r["ai_pct"]) for r in rows]
    graph_errors = [parse_int(r["graph_errors"]) for r in rows]
    dup_pairs = [parse_int(r["dup_pairs"]) for r in rows]
    commits = [parse_int(r["commits"]) for r in rows]
    loc_list = [parse_int(r["cpp_loc"]) for r in rows]

    # Statistics
    print("# Corpus Statistics\n")
    print(f"**Total repos:** {len(rows)}")
    print(f"**AI percentage (mean):** {mean(ai_pcts):.1f}%")
    print(f"**AI percentage (stdev):** {stdev(ai_pcts):.1f}%")
    print(f"**Commits with graph errors:** {sum(1 for g in graph_errors if g > 0)}")
    print(f"**Repos with dup pairs:** {sum(1 for d in dup_pairs if d > 0)}\n")

    # Distribution
    ai_100 = sum(1 for p in ai_pcts if p >= 95)
    ai_50_95 = sum(1 for p in ai_pcts if 50 <= p < 95)
    ai_0_50 = sum(1 for p in ai_pcts if 0 < p < 50)
    ai_0 = sum(1 for p in ai_pcts if p == 0)

    print("## AI Percentage Distribution\n")
    print(f"| Range | Count | % |\n|-------|-------|-----|\n")
    print(f"| 95-100% | {ai_100} | {100*ai_100/len(rows):.1f}% |")
    print(f"| 50-95% | {ai_50_95} | {100*ai_50_95/len(rows):.1f}% |")
    print(f"| 0-50% | {ai_0_50} | {100*ai_0_50/len(rows):.1f}% |")
    print(f"| 0% | {ai_0} | {100*ai_0/len(rows):.1f}% |\n")

    # Interesting findings
    print("## Interesting Findings\n")

    # Largest projects with high AI%
    high_ai_large = [
        r
        for r in rows
        if parse_float(r["ai_pct"]) >= 80 and parse_int(r["cpp_loc"]) > 100000
    ]
    if high_ai_large:
        print(f"**Largest projects with ≥80% AI:**")
        high_ai_large.sort(key=lambda r: parse_int(r["cpp_loc"]), reverse=True)
        for r in high_ai_large[:5]:
            print(f"- {r['name']}: {parse_int(r['cpp_loc']):,} LOC, {r['ai_pct']}% AI, {r['commits']} commits")
        print()

    # Most AI-heavy
    print(f"**100% AI repos:** {ai_100}")
    print(f"- Smallest: {min((r for r in rows if parse_float(r['ai_pct']) == 100), key=lambda r: parse_int(r['cpp_loc']), default={}).get('name', 'N/A')}")
    print(f"- Largest: {max((r for r in rows if parse_float(r['ai_pct']) == 100), key=lambda r: parse_int(r['cpp_loc']), default={}).get('name', 'N/A')}\n")

    # Least AI
    least_ai = sorted([r for r in rows if parse_float(r["ai_pct"]) < 5], key=lambda r: parse_float(r["ai_pct"]))
    if least_ai:
        print(f"**Repos with <5% AI ({len(least_ai)}):**")
        for r in least_ai[:10]:
            print(f"- {r['name']}: {r['ai_pct']}% AI ({r['commits']} commits, {r['cpp_loc']} LOC)")
        print()

    # Repos with graph errors (if available)
    with_errors = [r for r in rows if parse_int(r["graph_errors"]) > 0]
    if with_errors:
        print(f"**Repos with graph errors:** {len(with_errors)}")
        for r in sorted(with_errors, key=lambda r: parse_int(r["graph_errors"]), reverse=True)[:5]:
            print(f"- {r['name']}: {r['graph_errors']} errors, {r['ai_pct']}% AI")
        print()

    # Repos with duplicates
    with_dup = [r for r in rows if parse_int(r["dup_pairs"]) > 0]
    if with_dup:
        print(f"**Repos with copy-paste:** {len(with_dup)}")
        for r in sorted(with_dup, key=lambda r: parse_int(r["dup_pairs"]), reverse=True)[:5]:
            print(f"- {r['name']}: {r['dup_pairs']} pairs, {r['ai_pct']}% AI")


if __name__ == "__main__":
    import sys

    main()
