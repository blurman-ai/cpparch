#!/usr/bin/env python3
"""Кросс-языковое расширение C++ корпус-исследования: ищем НЕ-C++ OSS-репы с РЕЗКИМ ростом
частоты коммитов после мая 2025 и метим признаки ИИ-разработки.

Метод (по урокам C++ прогона):
  1) Discovery: gh search repos по языку, created<2024, updated>2025-05, stars>100, не форки.
     Search API имеет отдельный лимит 30/мин — используем свободно, но с паузами.
  2) Фиксированные окна page-count (БЕЗ клона, БЕЗ changepoint-детектора — он смещён):
     base = H2-2024 (2024-07-01..2025-01-01), win = H2-2025 (2025-07-01..2026-01-01).
     ratio = win/base только для реп, активных в ОБОИХ окнах.
  3) Для ratio >= 1.5 (настоящий рост) — агентный сигнал из выборки 100 свежих коммитов:
     медиана длины сообщения, доля bot-авторства, число AI-трейлеров;
     agentic = msglen>=100 OR bot_share>=0.10 OR trailers>=3.

Бюджет: core REST 5000/ч ДЕЛИТСЯ с C++ скрином. PAR<=4, на 403 спим 60с, чекаем
X-Ratelimit-Remaining из заголовков. Детерминированно, resumable (append в per-lang TSV,
пропуск готовых), диск-безопасно (без клонов).

Запуск:  surge_scan.py <lang> [discover|window|agentic|all]
По умолчанию all для языка.
"""
import json
import os
import re
import statistics
import subprocess
import sys
import time
from concurrent.futures import ThreadPoolExecutor, as_completed

HERE = os.path.dirname(os.path.abspath(__file__))
PAR = 4

BASE = ("2024-07-01T00:00:00Z", "2025-01-01T00:00:00Z")   # H2 2024
WIN = ("2025-07-01T00:00:00Z", "2026-01-01T00:00:00Z")    # H2 2025
SURGE_RATIO = 1.5

LAST = re.compile(r'[?&]page=(\d+)>;\s*rel="last"')
BOT = re.compile(r"copilot|swe-agent|\bbot\b|claude|cursor|codex|devin|google-labs-jules", re.I)
TRAILER = re.compile(r"co-authored-by:\s*(claude|copilot|cursor|codex)|noreply@anthropic|"
                     r"generated with claude|claude code|cursoragent|🤖", re.I)
MSGLEN_AI = 100
BOT_AI = 0.10
TRAILER_AI = 3

# Бюджет core REST делится с C++ скрином — держим пол, чтобы не задушить соседа, но и не
# голодать самим при длительной контенции. 400 = умеренный кооперативный пол.
BUDGET_FLOOR = 400
REMAIN = re.compile(r"^x-ratelimit-remaining:\s*(\d+)", re.I)
RESET = re.compile(r"^x-ratelimit-reset:\s*(\d+)", re.I)


def budget_remaining():
    """Авторитет бюджета — заголовок РЕАЛЬНОГО content-эндпоинта. ВАЖНО: rate_limit-эндпоинт и
    дешёвые мета-вызовы (octocat/Hello-World) врут — показывают ~5000 при реально исчерпанном core.
    Бьём по настоящему commits-эндпоинту (тратит 1 вызов, но даёт правду)."""
    try:
        r = subprocess.run(["gh", "api", "-i", "repos/torvalds/linux/commits?per_page=1"],
                           capture_output=True, text=True, timeout=30)
    except Exception:
        return None, 0
    rem = reset = None
    for ln in (r.stdout or "").splitlines():
        m = REMAIN.match(ln)
        if m:
            rem = int(m.group(1))
        m = RESET.match(ln)
        if m:
            reset = int(m.group(1))
    return rem, reset or 0


def wait_for_budget(floor=BUDGET_FLOOR):
    """Блокируемся, пока remaining < floor; ждём до reset, вежливо к соседу."""
    while True:
        rem, reset = budget_remaining()
        if rem is None or rem >= floor:
            return rem
        now = int(time.time())
        sl = max(10, min(45, reset - now + 2)) if reset > now else 20
        print(f"  [budget] remaining={rem} < {floor}, сон {sl}s (reset через {max(0,reset-now)}s)",
              flush=True)
        time.sleep(sl)

# gh search --language принимает «человеческое» имя языка
LANG_SEARCH = {
    "python": "python", "typescript": "typescript", "javascript": "javascript",
    "rust": "rust", "go": "go", "java": "java", "csharp": "c#",
    "kotlin": "kotlin", "swift": "swift", "cpp": "cpp",
}


def pool_path(lang):
    return os.path.join(HERE, f"{lang}_pool.tsv")


def out_path(lang):
    return os.path.join(HERE, f"{lang}_surge.tsv")


def run_gh(args, timeout=60, tries=4):
    """gh с backoff. Возвращает (stdout, ok)."""
    for _ in range(tries):
        try:
            r = subprocess.run(args, capture_output=True, text=True, timeout=timeout)
        except Exception:
            time.sleep(5)
            continue
        err = (r.stderr or "").lower()
        if "rate limit" in err or "secondary" in err or "was submitted too quickly" in err:
            time.sleep(60)
            continue
        return r.stdout or "", r.returncode == 0
    return "", False


def discover(lang):
    """Дискавери кандидатов. Star-бакеты, чтобы обойти 1000-cap search-результатов.
    created<2024-01-01, updated>2025-05-01, не форки. Цель ~150-200 уникальных."""
    sl = LANG_SEARCH[lang]
    repos = {}
    star_buckets = [">2000", "1000..2000", "500..1000", "200..500", "100..200"]
    created_ranges = ["<2021-01-01", "2021-01-01..2022-06-01", "2022-06-01..2024-01-01"]
    queries = [(cr, sb) for sb in star_buckets for cr in created_ranges]
    for cr, sb in queries:
        out, ok = run_gh(
            ["gh", "search", "repos", "--language", sl, "--created", cr, "--stars", sb,
             "--updated", ">2025-05-01", "--include-forks=false", "--sort", "updated",
             "--limit", "100", "--json", "fullName,createdAt,stargazersCount"],
            timeout=60)
        if ok:
            try:
                for it in json.loads(out or "[]"):
                    fn = it["fullName"]
                    if fn not in repos:
                        repos[fn] = (it.get("createdAt", "")[:10], it.get("stargazersCount", 0))
            except Exception:
                pass
        time.sleep(2.5)  # search API 30/мин — ~2.4с/запрос безопасно
        if len(repos) >= 220:
            break
    with open(pool_path(lang), "w", encoding="utf-8") as f:
        for r, (c, s) in repos.items():
            f.write(f"{r}\t{c}\t{s}\n")
    print(f"[{lang}] discovery -> {len(repos)} уникальных кандидатов", flush=True)
    return repos


def count_range(repo, since, until, tries=4):
    """page-count трюк: per_page=1 → номер rel=last == число коммитов в [since,until)."""
    q = f"repos/{repo}/commits?since={since}&until={until}&per_page=1"
    for _ in range(tries):
        try:
            r = subprocess.run(["gh", "api", "-i", q], capture_output=True, text=True, timeout=40)
        except Exception:
            time.sleep(3)
            continue
        out = r.stdout or ""
        err = (r.stderr or "").lower()
        if "rate limit" in err or "secondary" in err or "403" in err:
            time.sleep(60)
            continue
        for ln in out.splitlines():
            if ln.lower().startswith("link:"):
                m = LAST.search(ln)
                if m:
                    return int(m.group(1))
        body = out.split("\r\n\r\n", 1)[-1] if "\r\n\r\n" in out else out
        if r.returncode == 0:
            return 1 if '"sha"' in body else 0
        return -1
    return -1


def gh_api(path, tries=4):
    for _ in range(tries):
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
        return []
    return None


def agentic_signal(repo):
    """Выборка до 100 свежих коммитов с H2-2025: медиана длины, bot_share, trailers, agentic."""
    data = gh_api(f"repos/{repo}/commits?since={WIN[0]}&per_page=100")
    if not isinstance(data, list) or not data:
        return (0, 0.0, 0, 0)
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
    return (win_med, round(bot_share, 2), trailer_n, agentic)


def load_pool(lang):
    cand = {}
    p = pool_path(lang)
    if not os.path.exists(p):
        return cand
    for ln in open(p, encoding="utf-8"):
        parts = ln.rstrip("\n").split("\t")
        if "/" in parts[0]:
            created = parts[1] if len(parts) > 1 else ""
            stars = parts[2] if len(parts) > 2 else "0"
            cand[parts[0]] = (created, stars)
    return cand


def process(lang):
    cand = load_pool(lang)
    if not cand:
        cand = discover(lang)
    op = out_path(lang)
    done = set()
    if os.path.exists(op):
        for ln in open(op, encoding="utf-8"):
            done.add(ln.split("\t")[0])
    todo = [(r, c, s) for r, (c, s) in cand.items() if r not in done]
    new = not os.path.exists(op)
    print(f"[{lang}] кандидатов {len(cand)}, к проверке {len(todo)} (готово {len(done)-(0 if new else 1)})",
          flush=True)
    f = open(op, "a", encoding="utf-8")
    if new:
        f.write("repo\tcreated\tstars\tbase_h2_2024\twin_h2_2025\tratio\t"
                "win_msglen\tbot_share\ttrailers\tagentic\n")

    def window(rec):
        repo, created, stars = rec
        b = count_range(repo, *BASE)
        w = count_range(repo, *WIN)
        ratio = ""
        wm = bs = tr = ag = ""
        is_surge = 0
        if b > 0 and w > 0:
            rr = w / b
            ratio = f"{rr:.2f}"
            if rr >= SURGE_RATIO:
                is_surge = 1
                wm, bs, tr, ag = agentic_signal(repo)
        return repo, created, stars, b, w, ratio, wm, bs, tr, ag, is_surge

    n = surge = 0
    BATCH = 25  # между батчами сверяемся с бюджетом; вежливо к C++ скрину
    for start in range(0, len(todo), BATCH):
        wait_for_budget()
        chunk = todo[start:start + BATCH]
        with ThreadPoolExecutor(max_workers=PAR) as ex:
            futs = {ex.submit(window, rec): rec for rec in chunk}
            for fut in as_completed(futs):
                repo, created, stars, b, w, ratio, wm, bs, tr, ag, is_surge = fut.result()
                surge += is_surge
                f.write(f"{repo}\t{created}\t{stars}\t{b}\t{w}\t{ratio}\t{wm}\t{bs}\t{tr}\t{ag}\n")
                f.flush()
                n += 1
        print(f"  [{lang}] {n}/{len(todo)}, ростов>=1.5: {surge}", flush=True)
    f.close()
    print(f"[{lang}] ГОТОВО: проверено {n}, ростов>=1.5 (в этом запуске): {surge} -> {op}",
          flush=True)


def main():
    if len(sys.argv) < 2:
        print("usage: surge_scan.py <lang> [discover|all]")
        return
    lang = sys.argv[1].lower()
    if lang not in LANG_SEARCH:
        print(f"неизвестный язык: {lang}; доступны {list(LANG_SEARCH)}")
        return
    phase = sys.argv[2] if len(sys.argv) > 2 else "all"
    if phase == "discover":
        if os.path.exists(pool_path(lang)):
            print(f"[{lang}] pool уже есть, пропуск discovery")
        else:
            discover(lang)
        return
    process(lang)


if __name__ == "__main__":
    main()
