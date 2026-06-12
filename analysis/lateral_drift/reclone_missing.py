#!/usr/bin/env python3
"""Reclone the 297 corpus repos deleted from disk (#115).

Reads baselines/manifest.tsv, takes every row with status == 'repo_missing',
maps the flat dir name <owner>_<repo> back to the GitHub slug <owner>/<repo>
(GitHub usernames never contain '_', so the FIRST '_' is the separator), and
full-clones each into /home/localadm/oss/<name>.

Design (per task #115):
  - FULL clone (no --depth: shallow cost 68 repos an off-by-one baseline;
    no --filter: partial clones trigger network blob fetches at ls-tree time).
  - gh repo clone → uses the authenticated token, dodges anon rate limits.
  - Retry transient failures; repos gone from GitHub (404/DMCA) are logged
    and skipped, not retried.
  - Per-repo line to reclone.log; final tally to reclone_results.tsv.
  - Idempotent: an existing non-empty clone dir is left alone.
"""
import concurrent.futures
import subprocess
import sys
import time
from pathlib import Path

RUN_DIR = Path(__file__).resolve().parent
MANIFEST = RUN_DIR / 'baselines' / 'manifest.tsv'
OSS_DIR = Path('/home/localadm/oss')
LOG = RUN_DIR / 'reclone.log'
RESULTS = RUN_DIR / 'reclone_results.tsv'

WORKERS = 3
MAX_RETRIES = 3
RETRY_SLEEP = 5  # seconds, grows linearly per attempt

# Substrings in gh/git stderr that mean "repo is gone" — do NOT retry.
GONE_MARKERS = (
    'could not resolve to a repository',
    'repository not found',
    'not found',
    'access blocked',
    'dmca',
    'has been disabled',
    'repository unavailable',
)


def load_missing():
    rows = []
    for raw in MANIFEST.read_text().splitlines()[1:]:
        parts = raw.rstrip('\r').split('\t')
        if len(parts) >= 4 and parts[3].strip() == 'repo_missing':
            rows.append(parts[0].strip())
    return rows


def slug(name):
    owner, _, repo = name.partition('_')
    return f'{owner}/{repo}'


def log(msg):
    line = f'[{time.strftime("%H:%M:%S")}] {msg}'
    print(line, flush=True)
    with LOG.open('a') as fh:
        fh.write(line + '\n')


def clone_one(name):
    target = OSS_DIR / name
    if target.exists() and any(target.iterdir()):
        return name, 'exists', ''
    repo = slug(name)
    last_err = ''
    for attempt in range(1, MAX_RETRIES + 1):
        try:
            proc = subprocess.run(
                ['gh', 'repo', 'clone', repo, str(target), '--', '--quiet'],
                capture_output=True, text=True, timeout=1800,
            )
        except subprocess.TimeoutExpired:
            last_err = 'timeout(1800s)'
            _cleanup(target)
            continue
        if proc.returncode == 0:
            return name, 'cloned', ''
        err = (proc.stderr or proc.stdout or '').strip()
        last_err = err.replace('\n', ' ')[:200]
        low = err.lower()
        if any(m in low for m in GONE_MARKERS):
            _cleanup(target)
            return name, 'gone', last_err
        _cleanup(target)
        time.sleep(RETRY_SLEEP * attempt)
    return name, 'failed', last_err


def _cleanup(target):
    """Remove a half-written clone dir so a retry starts clean."""
    if target.exists():
        subprocess.run(['rm', '-rf', str(target)], check=False)


def main():
    names = load_missing()
    log(f'reclone start: {len(names)} repo_missing rows, {WORKERS} workers, full clones')
    tally = {'cloned': 0, 'exists': 0, 'gone': 0, 'failed': 0}
    done = 0
    with RESULTS.open('w') as out:
        out.write('name\tstatus\terror\n')
        with concurrent.futures.ThreadPoolExecutor(max_workers=WORKERS) as ex:
            futs = {ex.submit(clone_one, n): n for n in names}
            for fut in concurrent.futures.as_completed(futs):
                name, status, err = fut.result()
                tally[status] = tally.get(status, 0) + 1
                done += 1
                out.write(f'{name}\t{status}\t{err}\n')
                out.flush()
                mark = {'cloned': '✓', 'exists': '·', 'gone': '✗', 'failed': '!'}.get(status, '?')
                log(f'{mark} [{done}/{len(names)}] {status:7} {name}'
                    + (f'  — {err}' if err else ''))
    log(f'reclone done: cloned={tally["cloned"]} exists={tally["exists"]} '
        f'gone={tally["gone"]} failed={tally["failed"]}')
    return 0 if tally['failed'] == 0 else 1


if __name__ == '__main__':
    sys.exit(main())
