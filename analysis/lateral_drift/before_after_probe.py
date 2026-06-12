#!/usr/bin/env python3
"""Feasibility probe for an interrupted-time-series (before/after) agentic design.

For each repo with lateral events, find the agent-adoption point and measure
whether there's a clean natural experiment: enough HUMAN history before agents
arrived, sustained agent activity after, and drift events on both sides.

A repo is a good ITS candidate when:
  - pre-agent period has many commits (real human baseline), AND
  - post-agent period is agent-dominated (clear treatment), AND
  - drift events exist on BOTH sides (so before/after rates are estimable).
"""
import csv
import glob
import os
import re
import subprocess
from collections import defaultdict

RUN_DIR = os.path.dirname(os.path.abspath(__file__))
OSS_DIR = '/home/localadm/oss'
CSV = os.path.join(RUN_DIR, 'lateral_drift_new.csv')

BOT_HINTS = ('copilot', 'swe-agent', 'cursor', 'devin-ai', 'devin',
             'google-labs-jules', 'claude', 'codex', 'aider')
_COAUTHOR_RE = re.compile(
    r'co-authored-by:.*\b(claude|copilot|cursor|devin|aider|codex|jules)\b', re.IGNORECASE)


def local_name(gh):
    return gh.replace('/', '_', 1)


def commit_timeline(repo_dir):
    """list of (date 'YYYY-MM-DD', kind) sorted ascending by date."""
    sep = '\x01'
    fmt = '%cI|%an|%ae|%cn|%ce%x00%b' + sep
    try:
        out = subprocess.run(
            ['git', '-C', repo_dir, '-c', 'gc.auto=0', 'log', '--all', f'--format={fmt}'],
            capture_output=True, text=True, timeout=120, check=True).stdout
    except Exception:
        return []
    rows = []
    for rec in out.split(sep):
        rec = rec.strip('\n')
        if not rec or '|' not in rec:
            continue
        head, _, _body = rec.partition('\x00')
        date = head.split('|', 1)[0][:10]
        ident = head.lower()
        if any(h in ident for h in BOT_HINTS) or _COAUTHOR_RE.search(rec):
            kind = 'agent'
        else:
            kind = 'human'
        rows.append((date, kind))
    rows.sort()
    return rows


def first_sustained_agent(timeline, min_run=5):
    """date of the first agent commit that is followed by >=min_run agents
    within the next 20 agent-or-human commits — i.e. real adoption, not a
    one-off drive-by agent commit."""
    for i, (date, kind) in enumerate(timeline):
        if kind != 'agent':
            continue
        window = timeline[i:i + 20]
        if sum(1 for _, k in window if k == 'agent') >= min_run:
            return date
    return None


def main():
    # events per repo with dates
    ev_dates = defaultdict(list)
    for row in csv.DictReader(open(CSV, encoding='utf-8')):
        ev_dates[row['repo']].append(row['date'][:10])

    cands = []
    for gh in sorted(ev_dates):
        repo_dir = os.path.join(OSS_DIR, local_name(gh))
        if not os.path.isdir(repo_dir):
            continue
        tl = commit_timeline(repo_dir)
        if not tl:
            continue
        switch = first_sustained_agent(tl)
        n_total = len(tl)
        if switch is None:
            continue
        pre = [r for r in tl if r[0] < switch]
        post = [r for r in tl if r[0] >= switch]
        pre_h = sum(1 for _, k in pre if k == 'human')
        post_a = sum(1 for _, k in post if k == 'agent')
        post_h = sum(1 for _, k in post if k == 'human')
        ev_pre = sum(1 for d in ev_dates[gh] if d < switch)
        ev_post = sum(1 for d in ev_dates[gh] if d >= switch)
        post_agent_share = post_a / max(1, len(post))
        cands.append({
            'repo': gh, 'switch': switch, 'total': n_total,
            'pre_commits': len(pre), 'pre_human': pre_h,
            'post_commits': len(post), 'post_agent_share': post_agent_share,
            'ev_pre': ev_pre, 'ev_post': ev_post,
            'span': f'{tl[0][0]}…{tl[-1][0]}',
        })

    # rank: good ITS = lots of pre-human, agent-dominated post, events both sides
    def score(c):
        both = c['ev_pre'] > 0 and c['ev_post'] > 0
        return (both, min(c['pre_human'], 300), c['post_agent_share'])
    cands.sort(key=score, reverse=True)

    print(f'{"repo":34} {"switch":11} {"pre_h":>6} {"post_a%":>7} {"ev_pre":>6} {"ev_post":>7}  span')
    print('-' * 100)
    for c in cands[:25]:
        flag = '★' if c['ev_pre'] > 0 and c['ev_post'] > 0 and c['pre_human'] >= 50 else ' '
        print(f'{flag}{c["repo"]:33} {c["switch"]:11} {c["pre_human"]:6} '
              f'{c["post_agent_share"]*100:6.0f}% {c["ev_pre"]:6} {c["ev_post"]:7}  {c["span"]}')

    good = [c for c in cands if c['ev_pre'] > 0 and c['ev_post'] > 0 and c['pre_human'] >= 50]
    print(f'\n★ clean ITS candidates (>=50 pre-agent human commits AND drift events both sides): {len(good)}')
    for c in good:
        print(f'  {c["repo"]}: {c["pre_human"]} human commits before {c["switch"]}, '
              f'{c["ev_pre"]} events before / {c["ev_post"]} after')


if __name__ == '__main__':
    main()
