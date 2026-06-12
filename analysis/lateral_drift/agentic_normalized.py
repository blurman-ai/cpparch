#!/usr/bin/env python3
"""Normalized agentic vs human analysis of lateral drift events (#115).

Raw event counts (294 agent / 201 human) confound exposure: a repo with more
agent commits will mechanically have more agent events. This normalizes by the
denominator — agent vs human commit counts per repo over the analysis window —
and applies repo fixed effects: only repos that have BOTH agent and human
commits contribute to the within-repo comparison, so repo-level drift propensity
cancels out.

Author classification reuses the scan's exact BOT_HINTS + co-author regex, so
event-side and commit-side labels agree.
"""
import csv
import glob
import os
import re
import subprocess
import sys
from collections import defaultdict

RUN_DIR = os.path.dirname(os.path.abspath(__file__))
OSS_DIR = '/home/localadm/oss'
CSV = os.path.join(RUN_DIR, 'lateral_drift_new.csv')

# Must match lateral_drift_scan.py exactly.
BOT_HINTS = ('copilot', 'swe-agent', 'cursor', 'devin-ai', 'devin',
             'google-labs-jules', 'claude', 'codex', 'aider')
_COAUTHOR_RE = re.compile(
    r'co-authored-by:.*\b(claude|copilot|cursor|devin|aider|codex|jules)\b', re.IGNORECASE)


def local_name(gh_repo):
    return gh_repo.replace('/', '_', 1)


def classify_all_commits(repo_dir):
    """sha -> 'agent'|'human' for every commit in the repo (one git call)."""
    sep = '\x01'
    fmt = '%H|%an|%ae|%cn|%ce%x00%b' + sep
    try:
        out = subprocess.run(
            ['git', '-C', repo_dir, '-c', 'gc.auto=0', 'log', '--all',
             f'--format={fmt}'],
            capture_output=True, text=True, timeout=120, check=True,
        ).stdout
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired, FileNotFoundError):
        return {}
    result = {}
    for rec in out.split(sep):
        rec = rec.strip('\n')
        if not rec or '|' not in rec:
            continue
        head, _, body = rec.partition('\x00')
        sha = head.split('|', 1)[0]
        ident = head.lower()
        if any(h in ident for h in BOT_HINTS):
            result[sha] = 'agent'
        elif _COAUTHOR_RE.search(rec):
            result[sha] = 'agent'
        else:
            result[sha] = 'human'
    return result


def window_shas(jsonl_path):
    shas = []
    with open(jsonl_path, encoding='utf-8') as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            i = line.find('"sha"')
            if i < 0:
                continue
            # cheap extract without full json parse
            seg = line[i:i + 80]
            m = re.search(r'"sha"\s*:\s*"([0-9a-f]+)"', seg)
            if m:
                shas.append(m.group(1))
    return shas


def main():
    # event counts per repo per author-kind
    ev_agent = defaultdict(int)
    ev_human = defaultdict(int)
    with open(CSV, encoding='utf-8') as f:
        for row in csv.DictReader(f):
            if row['author_kind'] == 'agent':
                ev_agent[row['repo']] += 1
            elif row['author_kind'] == 'human':
                ev_human[row['repo']] += 1
    repos_with_events = set(ev_agent) | set(ev_human)

    # denominator: commit counts per repo per author-kind over the window
    rows = []
    for gh_repo in sorted(repos_with_events):
        ln = local_name(gh_repo)
        repo_dir = os.path.join(OSS_DIR, ln)
        jsonl = os.path.join(RUN_DIR, f'{ln}_graph_drift.jsonl')
        if not os.path.isdir(repo_dir) or not os.path.isfile(jsonl):
            continue
        cmap = classify_all_commits(repo_dir)
        if not cmap:
            continue
        ca = ch = 0
        for sha in window_shas(jsonl):
            k = cmap.get(sha) or cmap.get(sha[:12])
            if k is None:
                # try prefix match
                for full, kind in cmap.items():
                    if full.startswith(sha):
                        k = kind
                        break
            if k == 'agent':
                ca += 1
            elif k == 'human':
                ch += 1
        rows.append({
            'repo': gh_repo,
            'commits_agent': ca, 'commits_human': ch,
            'events_agent': ev_agent.get(gh_repo, 0),
            'events_human': ev_human.get(gh_repo, 0),
        })

    # write per-repo table
    out_tsv = os.path.join(RUN_DIR, 'agentic_normalized.tsv')
    with open(out_tsv, 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=['repo', 'commits_agent', 'commits_human',
                                          'events_agent', 'events_human',
                                          'rate_agent', 'rate_human', 'log_ratio'],
                           delimiter='\t')
        w.writeheader()
        mixed = []
        for r in rows:
            ra = r['events_agent'] / r['commits_agent'] if r['commits_agent'] else None
            rh = r['events_human'] / r['commits_human'] if r['commits_human'] else None
            lr = ''
            if ra and rh:  # both positive rates → log ratio defined
                import math
                lr = math.log(ra / rh)
            r2 = dict(r, rate_agent=f'{ra:.4f}' if ra is not None else '',
                      rate_human=f'{rh:.4f}' if rh is not None else '',
                      log_ratio=f'{lr:.3f}' if lr != '' else '')
            w.writerow(r2)
            if r['commits_agent'] > 0 and r['commits_human'] > 0:
                mixed.append((r, ra, rh))

    # ── pooled (exposure-normalized) ──
    tot_ea = sum(r['events_agent'] for r in rows)
    tot_eh = sum(r['events_human'] for r in rows)
    tot_ca = sum(r['commits_agent'] for r in rows)
    tot_ch = sum(r['commits_human'] for r in rows)
    print('=== POOLED (all repos with events) ===')
    print(f'agent: {tot_ea} events / {tot_ca} commits = '
          f'{tot_ea/tot_ca*1000:.2f} per 1k commits' if tot_ca else 'agent: no commits')
    print(f'human: {tot_eh} events / {tot_ch} commits = '
          f'{tot_eh/tot_ch*1000:.2f} per 1k commits' if tot_ch else 'human: no commits')
    if tot_ca and tot_ch:
        rr = (tot_ea/tot_ca) / (tot_eh/tot_ch)
        print(f'pooled rate ratio agent/human = {rr:.2f}×')

    # ── repo fixed effects (within-repo, mixed-authorship repos only) ──
    print(f'\n=== REPO FIXED EFFECTS ({len(mixed)} repos with BOTH agent & human commits) ===')
    if mixed:
        import math
        agent_higher = sum(1 for _, ra, rh in mixed if ra > rh)
        human_higher = sum(1 for _, ra, rh in mixed if rh > ra)
        tie = sum(1 for _, ra, rh in mixed if ra == rh)
        log_ratios = [math.log(ra/rh) for _, ra, rh in mixed if ra > 0 and rh > 0]
        print(f'agent rate > human rate: {agent_higher} repos')
        print(f'human rate > agent rate: {human_higher} repos')
        print(f'tie (both incl. 0==0): {tie} repos')
        if log_ratios:
            mean_lr = sum(log_ratios) / len(log_ratios)
            print(f'mean within-repo log(rate_agent/rate_human) = {mean_lr:+.3f} '
                  f'→ {math.exp(mean_lr):.2f}× (n={len(log_ratios)} repos where both rates>0)')
        # sign test p (two-sided, exact) on agent_higher vs human_higher
        n = agent_higher + human_higher
        if n:
            from math import comb
            k = max(agent_higher, human_higher)
            p = sum(comb(n, i) for i in range(k, n + 1)) / 2**n * 2
            p = min(1.0, p)
            print(f'sign test: {k}/{n} in majority direction, two-sided p ≈ {p:.3f}')
    print(f'\nPer-repo table: {out_tsv}')


if __name__ == '__main__':
    sys.exit(main())
