#!/usr/bin/env python3
"""Build window-start include-graph baselines for the lateral drift scan.

For each <name>_graph_drift.jsonl whose repo still exists at
/home/localadm/oss/<name>: take the first in-window commit's parent, extract
only C++ sources from that tree (git ls-tree + cat-file --batch — `git archive`
hangs on asset-heavy repos), run `archcheck --save-graph-baseline`, store as
baselines/<name>_window_baseline.yml.  Writes a manifest TSV with one row per
jsonl: name, base sha, node count, status.

Usage:
    python3 make_window_baselines.py [--limit N] [--workers 4]
"""

from __future__ import annotations

import argparse
import concurrent.futures
import csv
import glob
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

RUN_DIR = Path('/home/localadm/projects/cpparch/experiments/ai_repo_run')
OSS_DIR = Path('/home/localadm/oss')
ARCHCHECK = '/home/localadm/projects/cpparch/build/debug/src/archcheck'

# Matches kSourceExtensions in include/archcheck/scan/file_classification.h
CPP_EXTS = ('.c', '.cc', '.cpp', '.cxx', '.h', '.hh', '.hpp', '.hxx',
            '.ipp', '.tpp', '.inl', '.inc')


def git(repo: Path, *args: str, timeout: int = 120) -> str:
    result = subprocess.run(
        ['git', '-C', str(repo), '-c', 'gc.auto=0', *args],
        capture_output=True, text=True, timeout=timeout, check=True,
    )
    return result.stdout


def extract_sources(repo: Path, sha: str, dest: Path) -> int:
    """Extract C++ sources of `sha` tree into dest; returns file count."""
    # Classic output (old git, no --format): "<mode> <type> <oid>\t<path>"
    listing = git(repo, 'ls-tree', '-r', sha, timeout=300)
    wanted: list[tuple[str, str]] = []
    for line in listing.splitlines():
        meta, _, path = line.partition('\t')
        parts = meta.split()
        if len(parts) == 3 and parts[1] == 'blob' and path.lower().endswith(CPP_EXTS):
            wanted.append((parts[2], path))
    if not wanted:
        return 0

    proc = subprocess.Popen(
        ['git', '-C', str(repo), '-c', 'gc.auto=0', 'cat-file', '--batch'],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE,
    )
    assert proc.stdin and proc.stdout
    for oid, path in wanted:
        proc.stdin.write((oid + '\n').encode())
        proc.stdin.flush()
        header = proc.stdout.readline().decode()
        parts = header.split()
        if len(parts) != 3 or parts[1] != 'blob':
            continue
        size = int(parts[2])
        data = proc.stdout.read(size)
        proc.stdout.read(1)  # trailing newline
        out = dest / path
        out.parent.mkdir(parents=True, exist_ok=True)
        out.write_bytes(data)
    proc.stdin.close()
    proc.wait(timeout=30)
    return len(wanted)


def build_one(jsonl_path: str) -> tuple[str, str, int, str]:
    name = Path(jsonl_path).name.replace('_graph_drift.jsonl', '')
    out_yml = RUN_DIR / 'baselines' / f'{name}_window_baseline.yml'
    out_yml.parent.mkdir(parents=True, exist_ok=True)
    if out_yml.exists():
        return name, '', -1, 'exists'

    repo = OSS_DIR / name
    if not repo.is_dir():
        return name, '', 0, 'repo_missing'

    with open(jsonl_path, encoding='utf-8') as f:
        line = f.readline()
    if not line.strip():
        return name, '', 0, 'empty_jsonl'
    first_sha = json.loads(line)['sha']
    base = f'{first_sha}~1'

    status = 'ok'
    try:
        git(repo, 'cat-file', '-e', base, timeout=60)
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
        if (repo / '.git' / 'shallow').exists():
            # Shallow boundary == first window commit; its parent is gone but
            # its own tree is available.  Off-by-one baseline: the first
            # commit's own added edges never become events (under-detection).
            base = first_sha
            status = 'ok_off_by_one'
        else:
            # Root commit: repo born inside the window, empty pre-window graph
            # is the truth.
            out_yml.write_text('format_version: "1"\nnodes: []\nedges: []\n')
            return name, base, 0, 'ok_root_commit'

    tmp = Path(tempfile.mkdtemp(prefix=f'lat_{name}_', dir='/tmp'))
    try:
        n_files = extract_sources(repo, base, tmp)
        if n_files == 0:
            out_yml.write_text('format_version: "1"\nnodes: []\nedges: []\n')
            return name, base, 0, 'ok_empty'
        result = subprocess.run(
            [ARCHCHECK, '--save-graph-baseline', str(out_yml), str(tmp)],
            capture_output=True, text=True, timeout=600,
        )
        if result.returncode != 0 or not out_yml.exists():
            return name, base, 0, f'archcheck_rc{result.returncode}'
        nodes = sum(1 for l in out_yml.open() if l.lstrip().startswith('- "'))
        return name, base, nodes, status
    except subprocess.TimeoutExpired:
        return name, base, 0, 'timeout'
    except Exception as e:  # noqa: BLE001 — manifest must record every repo
        return name, base, 0, f'error:{type(e).__name__}'
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--limit', type=int, default=0)
    parser.add_argument('--workers', type=int, default=4)
    args = parser.parse_args()

    (RUN_DIR / 'baselines').mkdir(exist_ok=True)
    jsonl_files = sorted(glob.glob(str(RUN_DIR / '*_graph_drift.jsonl')))
    if args.limit:
        jsonl_files = jsonl_files[: args.limit]

    rows: list[tuple[str, str, int, str]] = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=args.workers) as pool:
        futures = {pool.submit(build_one, jf): jf for jf in jsonl_files}
        done = 0
        for fut in concurrent.futures.as_completed(futures):
            rows.append(fut.result())
            done += 1
            if done % 20 == 0:
                print(f'  {done}/{len(jsonl_files)}', file=sys.stderr)

    rows.sort()
    manifest = RUN_DIR / 'baselines' / 'manifest.tsv'
    with manifest.open('w', newline='') as f:
        w = csv.writer(f, delimiter='\t')
        w.writerow(['name', 'base_sha', 'nodes', 'status'])
        w.writerows(rows)

    counts: dict[str, int] = {}
    for _, _, _, status in rows:
        key = status.split(':')[0].split('_rc')[0]
        counts[key] = counts.get(key, 0) + 1
    print('Status summary:', counts, file=sys.stderr)
    print(f'Manifest: {manifest}', file=sys.stderr)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
