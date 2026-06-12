#!/usr/bin/env python3
"""Forward-тест: чистый authored-копипаст у CONTROL-группы (дисципл. зрелый OSS),
тем же v3-классификатором, что и агентские. Клон --depth 1 (нужен только снимок),
мерим, УДАЛЯЕМ (control транзитный — нужны только числа). Сравним плотность с agentic.
"""
import csv
import os
import re
import shutil
import subprocess
import sys

sys.path.insert(0, "/home/localadm/projects/cpparch/experiments/ai_repo_run")
from classify_dup import classify, PAIR  # noqa: E402

AC = "/home/localadm/projects/cpparch/build/debug/src/archcheck"
HERE = "/home/localadm/projects/cpparch/experiments/ai_repo_run"
TMP = "/home/localadm/oss_control"
OUT = os.path.join(HERE, "control_classified.tsv")
SCANNED = re.compile(r"scanned (\d+) files")


def measure(repo):
    owner = re.sub(r"[^a-z0-9]", "", repo.split("/", 1)[0].lower())
    dest = os.path.join(TMP, repo.replace("/", "_"))
    if os.path.isdir(dest):
        shutil.rmtree(dest, ignore_errors=True)
    url = f"https://github.com/{repo}.git"
    try:
        subprocess.run(["git", "clone", "--depth", "1", "--quiet", url, dest],
                       capture_output=True, timeout=600)
    except Exception:
        return None
    if not os.path.isdir(dest):
        return None
    try:
        out = subprocess.run([AC, "--duplication", dest], capture_output=True, text=True, timeout=480).stdout
        files = 0
        m = SCANNED.search(out)
        if m:
            files = int(m.group(1))
        counts = {"generated": 0, "vendored": 0, "file_dup": 0, "authored": 0}
        crc = {}
        for ln in out.splitlines():
            pm = PAIR.match(ln)
            if not pm:
                continue
            counts[classify(pm.group(1), pm.group(2), owner, dest, crc)] += 1
        return [repo, files, counts["generated"], counts["vendored"], counts["file_dup"], counts["authored"]]
    finally:
        shutil.rmtree(dest, ignore_errors=True)


def main():
    os.makedirs(TMP, exist_ok=True)
    ctl = [r["repo"] for r in csv.DictReader(open(os.path.join(HERE, "..", "..", "docs", "research",
           "cpp_ai_drift_metrics.tsv")), delimiter="\t") if r["group"] == "control"]
    done = set()
    if os.path.exists(OUT):
        for ln in open(OUT):
            done.add(ln.split("\t")[0])
    todo = [r for r in ctl if r not in done]
    print(f"control: {len(ctl)}, дозапуск {len(todo)}", flush=True)
    new = not os.path.exists(OUT)
    f = open(OUT, "a", encoding="utf-8")
    if new:
        f.write("repo\tfiles\tgenerated\tvendored\tfile_dup\tauthored\n")
        f.flush()
    for i, repo in enumerate(todo, 1):
        row = measure(repo)
        if row:
            f.write("\t".join(str(x) for x in row) + "\n")
            f.flush()
        print(f"  {i}/{len(todo)} {repo} {'ok' if row else 'FAIL'}", flush=True)
    f.close()
    print("ГОТОВО", flush=True)


if __name__ == "__main__":
    main()
