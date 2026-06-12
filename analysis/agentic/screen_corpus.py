#!/usr/bin/env python3
"""Воронка с ЗНАМЕНАТЕЛЕМ: скринуем большой пул OSS C++ реп ДЁШЕВО (blobless метаданные — без
скачивания кода), КАЖДУЮ отмечаем в реестре (проверена → агентная/нет). Агентность — по
trailer-независимому сигналу (длина коммит-сообщения) + bot-авторство + трейлеры.

Нарратив на выходе: «проверили N OSS-реп → M агентских → по ним дрифт с H2 2025».
Не-агентные НЕ скачиваем глубже и НЕ анализируем — только метим как проверенные.

  screen_ledger.tsv  — ВСЕ проверенные (знаменатель)
  agentic_found.tsv  — подмножество AGENTIC=1 (идут в дрифт)
Resumable, диск-безопасно (blobless клон → лог → удалить), троттлинг-aware.
"""
import json
import os
import re
import shutil
import statistics
import subprocess
import time
from collections import defaultdict
from concurrent.futures import ThreadPoolExecutor, as_completed

HERE = "/home/localadm/projects/cpparch/experiments/ai_repo_run"
SCRATCH = "/home/localadm/oss/_scratch_screen"
LEDGER = os.path.join(HERE, "screen_ledger.tsv")
POOL = os.path.join(HERE, "screen_pool.tsv")
PAR = 8

BOT = re.compile(r"copilot|swe-agent|\bbot\b|claude|cursor|codex|devin|google-labs-jules", re.I)
TRAILER = re.compile(r"co-authored-by:\s*(claude|copilot|cursor|codex)|noreply@anthropic|"
                     r"generated with claude|claude code|cursoragent|🤖", re.I)
WIN = {f"2025-{m:02d}" for m in (7, 8, 9, 10, 11, 12)} | {f"2026-{m:02d}" for m in range(1, 7)}
BASE = {f"2024-{m:02d}" for m in range(6, 13)} | {f"2025-{m:02d}" for m in (1, 2, 3, 4)}

# --- агентность: длинные сообщения (AI-почерк) ИЛИ бот-автор ИЛИ трейлеры ---
MSGLEN_AI = 100      # медиана длины сообщения в окне >= 100 симв = AI-почерк (человек ~20-45)
BOT_AI = 0.10        # >=10% коммитов бот-авторских
TRAILER_AI = 3       # >=3 коммита с AI-трейлером


def gh_search():
    repos = {}
    star_buckets = [">1000", "500..1000", "200..500", "100..200", "50..100",
                    "20..50", "10..20", "5..10", "2..5", "1..2"]
    created_ranges = ["<2020-01-01", "2020-01-01..2021-06-01", "2021-06-01..2022-06-01",
                      "2022-06-01..2023-06-01"]
    queries = [(lang, cr, sb) for lang in ("cpp", "c")
               for cr in created_ranges for sb in star_buckets]
    for i, (lang, cr, sb) in enumerate(queries):
        for _ in range(4):
            try:
                r = subprocess.run(
                    ["gh", "search", "repos", "--language", lang, "--created", cr, "--stars", sb,
                     "--updated", ">2025-05-01", "--include-forks=false", "--sort", "updated",
                     "--limit", "500", "--json", "fullName,createdAt"],
                    capture_output=True, text=True, timeout=60)
            except Exception:
                time.sleep(10)
                continue
            if "rate limit" in (r.stderr or "").lower() or "403" in (r.stderr or ""):
                time.sleep(60)
                continue
            try:
                for it in json.loads(r.stdout or "[]"):
                    repos[it["fullName"]] = it.get("createdAt", "")[:10]
            except Exception:
                pass
            break
        if i % 10 == 0:
            print(f"  search {i}/{len(queries)} -> уник {len(repos)}", flush=True)
        time.sleep(3)
    return repos


def gh_api(path, tries=4):
    """Core API с backoff. git-протокол троттлит — поэтому метаданные берём через API."""
    for t in range(tries):
        try:
            r = subprocess.run(["gh", "api", path], capture_output=True, text=True, timeout=40)
        except Exception:
            time.sleep(3)
            continue
        if r.returncode == 0 and r.stdout.strip():
            try:
                return json.loads(r.stdout)
            except Exception:
                return None
        err = (r.stderr or "").lower()
        if "rate limit" in err or "secondary" in err or "403" in err:
            time.sleep(60)
            continue
        return []   # 404/409 (пусто/DMCA/empty) — не агент, но проверена
    return None


def screen(repo, created):
    # 1 API-вызов: до 100 свежих коммитов с H2 2025 — сообщения/авторы/даты, БЕЗ clone
    data = gh_api(f"repos/{repo}/commits?since=2025-07-01T00:00:00Z&per_page=100")
    if data is None:
        return [repo, created, "ERR", 0, 0, 0.0, 0, 0]
    if not isinstance(data, list) or not data:
        return [repo, created, "NODATA", 0, 0, 0.0, 0, 0]
    lens, bot_n, trailer_n = [], 0, 0
    for c in data:
        commit = c.get("commit", {})
        msg = commit.get("message", "") or ""
        an = (commit.get("author", {}) or {}).get("name", "") or ""
        login = (c.get("author") or {}).get("login", "") or ""
        lens.append(len(msg.strip()))
        if BOT.search(an) or BOT.search(login):
            bot_n += 1
        if TRAILER.search(msg):
            trailer_n += 1
    total = len(data)
    win_med = int(statistics.median(lens)) if lens else 0
    bot_share = bot_n / total
    agentic = int(win_med >= MSGLEN_AI or bot_share >= BOT_AI or trailer_n >= TRAILER_AI)
    return [repo, created, "OK", total, win_med, round(bot_share, 2), trailer_n, agentic]


def main():
    os.makedirs(SCRATCH, exist_ok=True)
    if os.path.exists(POOL):
        cand = {}
        for ln in open(POOL, encoding="utf-8"):
            p = ln.rstrip("\n").split("\t")
            if "/" in p[0]:
                cand[p[0]] = p[1] if len(p) > 1 else ""
        print(f"пул из кэша: {len(cand)}", flush=True)
    else:
        cand = gh_search()
        with open(POOL, "w") as f:
            for r, c in cand.items():
                f.write(f"{r}\t{c}\n")
        print(f"ПУЛ (знаменатель): {len(cand)} старых активных C/C++ реп", flush=True)

    done = set()
    if os.path.exists(LEDGER):
        for ln in open(LEDGER, encoding="utf-8"):
            done.add(ln.split("\t")[0])
    todo = [(r, c) for r, c in cand.items() if r not in done]
    new = not os.path.exists(LEDGER)
    print(f"к скрину: {len(todo)} (готово {len(done)})", flush=True)
    lf = open(LEDGER, "a", encoding="utf-8")
    if new:
        lf.write("repo\tcreated\tstatus\tcommits_sampled\twin_msglen\tbot_share\ttrailers\tagentic\n")
    agentic = scanned = 0
    with ThreadPoolExecutor(max_workers=PAR) as ex:
        futs = {ex.submit(screen, r, c): r for r, c in todo}
        for fut in as_completed(futs):
            row = fut.result()
            lf.write("\t".join(str(x) for x in row) + "\n")
            lf.flush()
            scanned += 1
            if len(row) > 7 and row[7] == 1:
                agentic += 1
            if scanned % 100 == 0:
                print(f"  скрин {scanned}/{len(todo)}, агентских: {agentic}", flush=True)
    lf.close()
    print(f"\nГОТОВО: проверено {scanned}, агентских {agentic} -> {LEDGER}", flush=True)


if __name__ == "__main__":
    main()
