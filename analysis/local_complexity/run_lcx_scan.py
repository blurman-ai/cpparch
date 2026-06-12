#!/usr/bin/env python3
"""LCX corpus validation run (#109): top-N suspicious repos -> per-commit
archcheck --diff, collect local-complexity-drift advisory findings.

Selection: corpus_summary.tsv ranked by ai_pct desc, prior findings
(graph_errors + dup_pairs) desc, commits desc; must be cloned in ~/oss,
have real C++ volume and enough history.

Per repo: walk non-merge commits touching C/C++ files newest-first,
run the PRODUCT binary (dogfood), stop at TARGET_FINDINGS finding-commits
or MAX_COMMITS scanned. Output: findings.jsonl + progress.log + summary.tsv.
"""
import csv
import json
import os
import subprocess
import sys
import time

ROOT = "/home/localadm/projects/cpparch"
OSS = "/home/localadm/oss"
BIN = f"{ROOT}/build/debug/src/archcheck"
SUMMARY = f"{ROOT}/experiments/ai_repo_run/corpus_summary.tsv"
OUTDIR = f"{ROOT}/experiments/lcx_corpus_run"
N_REPOS = 100
TARGET_FINDINGS = 12   # stop a repo after this many finding-commits (>=10 asked)
MAX_COMMITS = 250      # per-repo scan cap
CPP_GLOBS = ["*.cpp", "*.cc", "*.cxx", "*.hpp", "*.h", "*.c", "*.hh"]
CALL_TIMEOUT = 90

def pick_repos():
    # #109: пин выборки. corpus_summary.tsv и набор клонов в ~/oss — движущиеся
    # мишени (grow-джоб #115 клонит на ходу); без пина selection меняется между
    # прогонами и рвёт сопоставимость с эталоном triage.tsv.
    pinned = os.path.join(OUTDIR, "selection_pinned.txt")
    if os.path.exists(pinned):
        with open(pinned) as f:
            return [l.strip() for l in f if l.strip()]
    rows = []
    with open(SUMMARY, newline="", encoding="utf-8", errors="replace") as f:
        for r in csv.DictReader(f, delimiter="\t"):
            try:
                ai = float(r["ai_pct"]); commits = int(r["commits"])
                loc = int(r["cpp_loc"]); prior = int(r["graph_errors"]) + int(r["dup_pairs"])
            except (ValueError, KeyError):
                continue
            if loc < 1000 or commits < 8:
                continue
            if not os.path.isdir(os.path.join(OSS, r["name"], ".git")):
                continue
            rows.append((ai, prior, commits, r["name"]))
    rows.sort(reverse=True)
    return [n for _, _, _, n in rows[:N_REPOS]]

def git(repo, *args):
    return subprocess.run(["git", "-C", repo, *args], capture_output=True,
                          text=True, timeout=CALL_TIMEOUT)

def commits_touching_cpp(repo):
    p = git(repo, "log", "--no-merges", "--pretty=%H\t%s", "--", *CPP_GLOBS)
    out = []
    for line in p.stdout.splitlines():
        sha, _, subj = line.partition("\t")
        if sha:
            out.append((sha, subj))
    return out

def parse_advisory(text):
    """Return (violations:list[str], net_delta:str|None) from --diff output."""
    viol, delta, inside = [], None, False
    for line in text.splitlines():
        if line.startswith("local complexity drift (advisory):"):
            inside = True
            continue
        if inside:
            s = line.strip()
            if s.startswith("net complexity delta:"):
                delta = s.split(":", 1)[1].strip()
                inside = False
            elif "DRIFT.LOCAL_COMPLEXITY" in s:
                viol.append(s)
            elif not s:
                inside = False
    return viol, delta

def scan_repo(name, jout, log):
    repo = os.path.join(OSS, name)
    try:
        commits = commits_touching_cpp(repo)
    except subprocess.TimeoutExpired:
        log.write(f"{name}\tTIMEOUT git log\n"); return 0, 0
    found = scanned = errors = 0
    for sha, subj in commits[:MAX_COMMITS]:
        if found >= TARGET_FINDINGS:
            break
        scanned += 1
        try:
            p = subprocess.run([BIN, "--diff", f"{sha}^..{sha}", repo],
                               capture_output=True, text=True, timeout=CALL_TIMEOUT)
        except subprocess.TimeoutExpired:
            errors += 1
            continue
        viol, delta = parse_advisory(p.stdout)
        if viol:
            found += 1
            jout.write(json.dumps({"repo": name, "sha": sha, "subject": subj[:120],
                                   "violations": viol, "net_delta": delta}) + "\n")
            jout.flush()
    log.write(f"{name}\tscanned={scanned}\tfound={found}\terrors={errors}\n")
    log.flush()
    return scanned, found

def main():
    os.makedirs(OUTDIR, exist_ok=True)
    repos = pick_repos()
    with open(f"{OUTDIR}/selection.txt", "w") as f:
        f.write("\n".join(repos) + "\n")
    t0 = time.time()
    with open(f"{OUTDIR}/findings.jsonl", "w") as jout, \
         open(f"{OUTDIR}/progress.log", "w") as log:
        log.write(f"# {len(repos)} repos selected\n"); log.flush()
        for i, name in enumerate(repos, 1):
            log.write(f"[{i}/{len(repos)} {int(time.time()-t0)}s] {name}\n"); log.flush()
            try:
                scan_repo(name, jout, log)
            except Exception as e:  # keep the run alive; record and continue
                log.write(f"{name}\tEXC {type(e).__name__}: {e}\n"); log.flush()
        log.write(f"# done in {int(time.time()-t0)}s\n")

if __name__ == "__main__":
    sys.exit(main())
