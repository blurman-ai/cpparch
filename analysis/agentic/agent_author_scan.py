#!/usr/bin/env python3
"""Correct agentic signal: share of commits AUTHORED by an agent bot — no clone.

Velocity-ramp turned out to be a poor proxy (top-ramp repos had 0 AI trailers). The reliable
signal is bot-authorship (copilot-swe-agent[bot], Copilot, cursor, devin). The contributors API
gives per-contributor commit counts in one call → bot-share, ranked.

Input : new_winners_clean.tsv  (repo \t commits ...)
Output: agent_author_ranked.tsv (repo, total_contrib_commits, bot_commits, bot_share, top_bot)
"""
import json
import subprocess
import time
import os

HERE = "/home/localadm/projects/cpparch/experiments/ai_repo_run"
INP = os.path.join(HERE, "new_winners_clean.tsv")
OUT = os.path.join(HERE, "agent_author_ranked.tsv")
BOT_HINTS = ("copilot", "swe-agent", "cursor", "devin-ai", "devin", "google-labs-jules",
             "claude", "codex", "aider")


def gh_json(path, tries=3):
    for t in range(tries):
        try:
            r = subprocess.run(["gh", "api", path], capture_output=True, text=True, timeout=40)
        except Exception:
            time.sleep(2)
            continue
        out = r.stdout.strip()
        if not out:
            time.sleep(1 + t)
            continue
        try:
            d = json.loads(out)
        except Exception:
            return None
        if isinstance(d, list):
            return d
        return None  # dict = error (e.g. 403/empty repo)
    return None


def main():
    repos = []
    for ln in open(INP, encoding="utf-8", errors="ignore"):
        p = ln.rstrip("\n").split("\t")
        if p and "/" in p[0]:
            repos.append(p[0])
    rows = []
    with open(OUT, "w", encoding="utf-8") as f:
        f.write("repo\ttotal_commits\tbot_commits\tbot_share\ttop_bot\n")
        for i, repo in enumerate(repos):
            data = gh_json(f"repos/{repo}/contributors?per_page=100&anon=false")
            if not data:
                continue
            total = sum(c.get("contributions", 0) for c in data)
            bot_c = 0
            top_bot, top_bot_n = "", 0
            for c in data:
                login = (c.get("login") or "").lower()
                if any(h in login for h in BOT_HINTS):
                    n = c.get("contributions", 0)
                    bot_c += n
                    if n > top_bot_n:
                        top_bot, top_bot_n = c.get("login", ""), n
            if total <= 0:
                continue
            share = bot_c / total
            f.write(f"{repo}\t{total}\t{bot_c}\t{share:.3f}\t{top_bot}\n")
            f.flush()
            if i % 100 == 0:
                print(f"  {i}/{len(repos)} {repo} bot_share={share:.2f}", flush=True)
            time.sleep(0.25)
    print(f"DONE {len(repos)} -> {OUT}", flush=True)


if __name__ == "__main__":
    main()
