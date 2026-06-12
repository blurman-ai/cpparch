#!/usr/bin/env python3
"""
DRIFT scan: walk git history of each repo, find COMMITS that ADD bool fields to
structs (added `+ bool field;` lines in headers, excluding function params).

Separates growth of EXISTING files (real drift) from brand-new files.
Runs on the agentic-repo subset present locally.
"""
import os
import re
import subprocess
import time
from collections import defaultdict

OSS = "/home/localadm/oss"
LIST = "/tmp/agentic_local.txt"
HDR = ["*.h", "*.hpp", "*.hh", "*.hxx"]

BOOL_ADD = re.compile(r'^\+\s*(?:mutable\s+)?bool\s+([A-Za-z_]\w*)\s*(?::\s*\d+)?(?:\s*=\s*[^;]+)?;\s*$')


def scan_repo(repo_dir):
    """Yield per-commit records of bool additions."""
    cmd = ["git", "-C", repo_dir, "log", "-p", "-U0", "--no-color", "--no-renames",
           "--reverse", "--format=@@C@@|%H|%ad|%an|%s", "--date=short", "--"] + HDR
    try:
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE,
                                stderr=subprocess.DEVNULL, text=True,
                                errors="ignore", bufsize=1)
    except OSError:
        return []
    start = time.time()

    cur = None            # (sha, date, author, subj)
    is_new = False        # current file is brand-new (--- /dev/null)
    cur_file = None
    # per-commit accumulators
    bucket = None

    def flush():
        if cur and bucket and (bucket["exist"] or bucket["new"]):
            yield_rec.append({
                "repo": os.path.basename(repo_dir),
                "sha": cur[0][:10], "date": cur[1], "author": cur[2], "subj": cur[3],
                "exist": bucket["exist"], "new": bucket["new"],
                "files": dict(bucket["files"]),
            })

    yield_rec = []
    for line in proc.stdout:
        line = line.rstrip("\n")
        if time.time() - start > 1800:
            proc.kill()
            break
        if line.startswith("@@C@@|"):
            flush()
            parts = line.split("|", 4)
            cur = (parts[1], parts[2], parts[3], parts[4] if len(parts) > 4 else "")
            bucket = {"exist": 0, "new": 0, "files": defaultdict(list)}
            is_new = False
            cur_file = None
            continue
        if bucket is None:
            continue
        if line.startswith("--- "):
            is_new = ("/dev/null" in line)
            continue
        if line.startswith("+++ "):
            cur_file = line[6:] if line.startswith("+++ b/") else line[4:]
            continue
        if line.startswith("+") and not line.startswith("+++"):
            if "(" in line or ")" in line:
                continue
            m = BOOL_ADD.match(line)
            if m and cur_file:
                if is_new:
                    bucket["new"] += 1
                else:
                    bucket["exist"] += 1
                    bucket["files"][cur_file].append(m.group(1))
    flush()
    try:
        proc.stdout.close()
        proc.terminate()
        proc.wait(timeout=5)
    except Exception:
        pass
    return yield_rec


def main():
    repos = [r.strip() for r in open(LIST) if r.strip()]
    all_recs = []
    for idx, repo in enumerate(repos, 1):
        rd = os.path.join(OSS, repo)
        if not os.path.isdir(os.path.join(rd, ".git")):
            continue
        recs = scan_repo(rd) or []
        all_recs.extend(recs)
        print(f"[{idx}/{len(repos)}] {repo}: commits-adding-bools={len(recs)}", flush=True)

    # records with growth of EXISTING files = real drift
    growth = [r for r in all_recs if r["exist"] > 0]
    growth.sort(key=lambda r: -r["exist"])

    per_repo = defaultdict(int)
    for r in all_recs:
        per_repo[r["repo"]] += r["exist"]

    print(f"\n=== HISTORY DRIFT: {len(all_recs)} коммитов добавили bool в заголовки; "
          f"{len(growth)} — в СУЩЕСТВУЮЩИЕ файлы (дрейф) ===\n")
    print("Топ-15 репо по числу bool, добавленных в существующие заголовки за историю:")
    for repo, n in sorted(per_repo.items(), key=lambda x: -x[1])[:15]:
        print(f"  {n:5d}  {repo}")

    csv = "/home/localadm/projects/cpparch/experiments/boolean_state/bool_history.csv"
    with open(csv, "w") as fh:
        fh.write("exist_bools,new_bools,repo,sha,date,author,subject,files\n")
        for r in all_recs:
            files = ";".join(f"{f}:{','.join(v)}" for f, v in r["files"].items())
            subj = r["subj"].replace('"', "'")[:120]
            fh.write(f'{r["exist"]},{r["new"]},{r["repo"]},{r["sha"]},{r["date"]},'
                     f'"{r["author"]}","{subj}","{files}"\n')

    md = ["# Boolean-State DRIFT по истории коммитов (агентские репо)\n",
          f"**Репо:** {len(repos)} агентских (локально склонированных).\n",
          f"**Коммитов, добавивших bool в заголовки:** {len(all_recs)}; "
          f"из них **{len(growth)}** — в УЖЕ существующие файлы (= дрейф, рост структур во времени).\n",
          "**Сигнал дрейфа:** один коммит добавляет несколько bool-полей в существующий заголовок.\n",
          "## Топ-40 коммитов по приросту bool в существующих заголовках\n",
          "| +bool | repo | commit | date | файлы:поля | subject |",
          "|---|---|---|---|---|---|"]
    for r in growth[:40]:
        files = "; ".join(f"`{os.path.basename(f)}`: {', '.join(v[:6])}" for f, v in list(r["files"].items())[:3])
        md.append(f"| **{r['exist']}** | {r['repo']} | `{r['sha']}` | {r['date']} | {files} | {r['subj'][:60]} |")
    md.append(f"\n*(полный список — bool_history.csv)*\n")
    out = "/home/localadm/projects/cpparch/docs/research/boolean_state_history_drift.md"
    with open(out, "w") as fh:
        fh.write("\n".join(md) + "\n")
    print(f"\nReport: {out}\nCSV:    {csv}")


if __name__ == "__main__":
    main()
