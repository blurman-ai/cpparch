#!/usr/bin/env python3
"""Export per-commit graph drift summaries for manual review.

Runs `archcheck --diff <sha>~1..<sha> .` over first-parent history of one repo,
captures the summary counters plus the drift detail sections, and writes:

- <prefix>.jsonl   all commits with parsed graph diff summary
- <prefix>.md      only commits with non-zero graph drift signals
"""

from __future__ import annotations

import argparse
import json
import subprocess
from pathlib import Path


def git(repo: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", "-C", str(repo), *args],
        capture_output=True,
        text=True,
        timeout=600,  # 10x — never silently drop a commit on a slow git op
        check=True,
    )
    return result.stdout.strip()


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("repo", help="path to git repository")
    parser.add_argument(
        "--archcheck",
        default="/home/localadm/projects/cpparch/build/debug/src/archcheck",
        help="path to archcheck binary",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=0,
        help="max number of commits to inspect (0 = all in window)",
    )
    parser.add_argument(
        "--since",
        default="2025-05-01",
        help="only scan commits since this date (window); matches the corpus window",
    )
    parser.add_argument(
        "--until",
        default="",
        help="only scan commits up to this date (optional upper bound, for testing)",
    )
    parser.add_argument(
        "--output-prefix",
        default="",
        help="output path without extension; default is experiments/ai_repo_run/<repo>_graph_drift",
    )
    return parser.parse_args()


def parse_summary(text: str) -> dict[str, int]:
    values = {
        "added_edges": 0,
        "removed_edges": 0,
        "grown_cycles": 0,
        "new_area_deps": 0,
    }

    for line in text.splitlines():
        for key in values:
            if line.startswith(f"{key}:"):
                tail = line.split(":", 1)[1].strip()
                # Only the summary line carries a number. `grown_cycles:` ALSO appears
                # later as a detail-section header (empty tail) — do NOT let that
                # overwrite the real count back to 0.
                if tail.isdigit():
                    values[key] = int(tail)
    return values


def collect_section(text: str, header: str) -> list[str]:
    lines: list[str] = []
    active = False
    for line in text.splitlines():
        if line.startswith(header):
            active = True
            continue
        if active:
            if not line.strip():
                break
            lines.append(line)
    return lines


def run_diff(binary: Path, repo: Path, sha: str) -> tuple[int, str]:
    result = subprocess.run(
        [str(binary), "--diff", f"{sha}~1..{sha}", "."],
        cwd=repo,
        capture_output=True,
        text=True,
        timeout=1200,  # 10x — never silently drop a commit's diff on a big tree
    )
    return result.returncode, result.stdout


def output_prefix(repo: Path, explicit: str) -> Path:
    if explicit:
        return Path(explicit)
    root = Path("/home/localadm/projects/cpparch/experiments/ai_repo_run")
    return root / f"{repo.name}_graph_drift"


def has_signal(record: dict[str, object]) -> bool:
    return any(
        int(record[key]) > 0
        for key in ("added_edges", "removed_edges", "grown_cycles", "new_area_deps")
    )


def main() -> int:
    args = parse_args()
    repo = Path(args.repo).resolve()
    binary = Path(args.archcheck).resolve()
    prefix = output_prefix(repo, args.output_prefix)

    # NB: scan ALL non-merge commits in the window, NOT --first-parent. first-parent
    # skips the branch commits where cycles/edges are actually introduced (a cycle
    # born on a feature branch only reaches HEAD via the merge, so grown_cycles
    # never fires on the mainline). --no-merges + --since walks every real commit.
    revargs = ["rev-list", "--reverse", "--no-merges", f"--since={args.since}"]
    if args.until:
        revargs.append(f"--until={args.until}")
    revargs.append("HEAD")
    commits = git(repo, *revargs).splitlines()
    if args.limit > 0:
        commits = commits[-args.limit :]
    if len(commits) < 2:
        raise SystemExit("not enough commits")

    # NB: append, don't use .with_suffix — repo names with dots (e.g. "AstrOs.ESP",
    # "Decodium-4.0") would have everything after the dot eaten, misnaming the file
    # so the orchestrator can't find it (-> false graph_errors=0).
    jsonl_path = prefix.parent / (prefix.name + ".jsonl")
    md_path = prefix.parent / (prefix.name + ".md")

    records: list[dict[str, object]] = []
    # Scan EVERY in-window commit, including the first: commits[0]'s parent (sha~1) is
    # the pre-window commit, so its diff is valid and may introduce graph structure
    # (a fork-import commit can add a whole codebase + cycle in the first windowed commit).
    # A true repo-root commit has no sha~1 -> run_diff returns an error rc -> skipped below.
    for sha in commits:
        subject = git(repo, "log", "-1", "--format=%s", sha)
        date = git(repo, "log", "-1", "--format=%as", sha)
        rc, out = run_diff(binary, repo, sha)
        if rc not in (0, 1):
            continue

        summary = parse_summary(out)
        record: dict[str, object] = {
            "repo": str(repo),
            "sha": sha,
            "date": date,
            "subject": subject,
            "returncode": rc,
            **summary,
            "added": collect_section(out, "added:"),
            "new_cross_area_dependencies": collect_section(out, "new_cross_area_dependencies:"),
        }
        records.append(record)

    jsonl_path.parent.mkdir(parents=True, exist_ok=True)
    with jsonl_path.open("w", encoding="utf-8") as fh:
        for record in records:
            fh.write(json.dumps(record, ensure_ascii=True) + "\n")

    with md_path.open("w", encoding="utf-8") as fh:
        fh.write("# Per-commit graph drift\n\n")
        fh.write(f"> Repo: `{repo}`\n\n")
        for record in records:
            if not has_signal(record):
                continue
            fh.write(
                f"## `{record['sha'][:12]}` {record['subject']}  "
                f"({record['date']})\n"
            )
            fh.write(
                f"- added_edges={record['added_edges']}\n"
                f"- removed_edges={record['removed_edges']}\n"
                f"- grown_cycles={record['grown_cycles']}\n"
                f"- new_area_deps={record['new_area_deps']}\n"
            )
            added = record["added"]
            if added:
                fh.write("- added edges:\n")
                for line in added[:12]:
                    fh.write(f"  `{line.strip()}`\n")
            area = record["new_cross_area_dependencies"]
            if area:
                fh.write("- new cross-area deps:\n")
                for line in area[:12]:
                    fh.write(f"  `{line.strip()}`\n")
            fh.write("\n")

    print(f"commits scanned: {len(records)}")
    print(f"-> {jsonl_path}")
    print(f"-> {md_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
