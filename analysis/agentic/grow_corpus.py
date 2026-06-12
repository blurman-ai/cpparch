#!/usr/bin/env python3
"""Догнать корпус до целевого числа реп, соблюдая CORPUS_CRITERIA.md.

Вход: worklist TSV (owner/repo \t commits_since_may), уже отфильтрованный по
agentic=1 & over300=1 & не-клонировано & без очевидных форков/мега-зеркал.

На каждую репу:
  1. gh api size → skip если upstream > MAX_OSS_GB (прокси «макс 50 ГБ» + детектор гигантов)
  2. git clone --filter=blob:none --shallow-since=2025-05-01 --single-branch
  3. размер C++/C контента ≤ MAX_CPP_MB → иначе TOOBIG (удалить)
  4. иначе KEEP

Стоп при достижении TARGET всего реп в OSS_ROOT (maxdepth 3).
Идемпотентно: пропускает уже существующие, дописывает ledger.
"""
from __future__ import annotations
import csv, os, subprocess, sys, time
from pathlib import Path

OSS_ROOT   = Path("/home/localadm/oss")
LEDGER     = Path("/home/localadm/projects/cpparch/experiments/ai_repo_run/grow_corpus_ledger.tsv")
TARGET     = 9999   # стоп по count отключён: oss/ уже >1000 мусором; идём по worklist (#122)
MAX_CPP_MB = 50          # «размер после C++ чистки не более 50 МБ»
MAX_OSS_GB = 2.0         # upstream-гигант → skip (прокси для «макс 50 ГБ», но режем раньше)
SHALLOW_SINCE = "2024-06-01"   # окно анализа #119 (было 2025-05-01)
CLONE_TIMEOUT = 600
CPP_EXTS = (".h", ".hh", ".hpp", ".hxx", ".ipp", ".inc", ".tcc",
            ".c", ".cc", ".cpp", ".cxx", ".cppm", ".mm")


def sh(cmd, timeout=120):
    try:
        r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
        return r.returncode, r.stdout, r.stderr
    except subprocess.TimeoutExpired:
        return 124, "", "timeout"


def total_repo_count() -> int:
    r = subprocess.run(
        ["find", str(OSS_ROOT), "-maxdepth", "3", "-name", ".git", "-type", "d"],
        capture_output=True, text=True)
    return len([x for x in r.stdout.splitlines() if x.strip()])


def upstream_size_kb(owner_repo: str):
    rc, out, _ = sh(["gh", "api", f"repos/{owner_repo}", "--jq", ".size"], timeout=30)
    if rc != 0:
        return None
    try:
        return int(out.strip())
    except ValueError:
        return None


def cpp_size_mb(dest: Path) -> float:
    # сумма размеров C/C++ файлов (исключая .git)
    total = 0
    for root, dirs, files in os.walk(dest):
        if ".git" in dirs:
            dirs.remove(".git")
        for f in files:
            if f.endswith(CPP_EXTS):
                try:
                    total += os.path.getsize(os.path.join(root, f))
                except OSError:
                    pass
    return total / (1024 * 1024)


def log(repo, status, detail=""):
    new = not LEDGER.exists()
    with open(LEDGER, "a", encoding="utf-8") as f:
        w = csv.writer(f, delimiter="\t")
        if new:
            w.writerow(["repo", "status", "detail", "ts"])
        w.writerow([repo, status, detail, int(time.time())])
    print(f"[{status:7}] {repo}  {detail}", flush=True)


def main():
    worklist = sys.argv[1] if len(sys.argv) > 1 else "/tmp/worklist_clean.tsv"
    rows = []
    with open(worklist, encoding="utf-8") as f:
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if parts and parts[0]:
                rows.append(parts[0])

    done = set()
    if LEDGER.exists():
        with open(LEDGER, encoding="utf-8") as f:
            for r in csv.DictReader(f, delimiter="\t"):
                done.add(r["repo"])

    cur = total_repo_count()
    print(f"start: {cur} repos, target {TARGET}, worklist {len(rows)}", flush=True)

    for owner_repo in rows:
        cur = total_repo_count()
        if cur >= TARGET:
            print(f"TARGET reached: {cur} repos", flush=True)
            break
        if owner_repo in done:
            continue
        dirname = owner_repo.replace("/", "_")
        dest = OSS_ROOT / dirname
        if dest.exists():
            log(owner_repo, "EXISTS"); continue

        sz = upstream_size_kb(owner_repo)
        if sz is not None and sz / (1024 * 1024) > MAX_OSS_GB:
            log(owner_repo, "GIANT", f"{sz/1024/1024:.1f}GB upstream"); continue
        if sz is None:
            log(owner_repo, "SKIP", "api-fail (gone/private/rate)"); continue

        url = f"https://github.com/{owner_repo}"
        rc, _, err = sh(["git", "clone", "--filter=blob:none",
                         f"--shallow-since={SHALLOW_SINCE}", "--single-branch",
                         url, str(dest)], timeout=CLONE_TIMEOUT)
        if rc != 0 or not (dest / ".git").is_dir():
            if dest.exists():
                subprocess.run(["rm", "-rf", str(dest)])
            log(owner_repo, "CLONEFAIL", err.strip().splitlines()[-1][:120] if err.strip() else f"rc={rc}")
            continue

        mb = cpp_size_mb(dest)
        if mb > MAX_CPP_MB:
            subprocess.run(["rm", "-rf", str(dest)])
            log(owner_repo, "TOOBIG", f"{mb:.1f}MB cpp > {MAX_CPP_MB}")
            continue
        if mb < 0.02:
            subprocess.run(["rm", "-rf", str(dest)])
            log(owner_repo, "NOCPP", f"{mb:.3f}MB cpp")
            continue

        log(owner_repo, "KEEP", f"{mb:.1f}MB cpp, {cur+1} total")

    print(f"done: {total_repo_count()} repos", flush=True)


if __name__ == "__main__":
    main()
