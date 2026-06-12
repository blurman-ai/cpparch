#!/usr/bin/env python3
"""Сводка кросс-языкового surge-скана. Читает <lang>_surge.tsv, печатает:
  - знаменатель (валидно проверено), категории (мёртвые/реактивация/затухание/активны оба)
  - настоящих ростов (ratio>=1.5, активны ОБА), сколько из них agentic
  - МЕДИАНА кросс-год ratio по «активны оба» (сравнение с C++ baseline 1.06x)
  - топ-10 агентских-выросших реп на язык
Сравнивает с changepoint-смещением НЕ полагаясь на него: всё на фиксированных окнах.
"""
import os
import statistics

HERE = os.path.dirname(os.path.abspath(__file__))
LANGS = ["python", "typescript", "javascript", "rust", "go", "java", "csharp", "kotlin", "swift"]
SURGE = 1.5


def load(lang):
    p = os.path.join(HERE, f"{lang}_surge.tsv")
    if not os.path.exists(p):
        return None
    rows = []
    with open(p, encoding="utf-8") as f:
        next(f, None)  # header
        for ln in f:
            c = ln.rstrip("\n").split("\t")
            if len(c) < 10 or "/" not in c[0]:
                continue
            repo, created, stars, b, w, ratio, wm, bs, tr, ag = c[:10]
            try:
                b = int(b); w = int(w)
            except ValueError:
                continue
            rows.append({
                "repo": repo, "created": created, "stars": int(stars or 0),
                "b": b, "w": w,
                "ratio": float(ratio) if ratio else None,
                "wm": int(wm) if wm else 0, "bs": float(bs) if bs else 0.0,
                "tr": int(tr) if tr else 0, "ag": int(ag) if ag else 0,
            })
    return rows


def summarize(lang, rows):
    valid = [r for r in rows if r["b"] >= 0 and r["w"] >= 0]
    dead = [r for r in valid if r["b"] == 0 and r["w"] == 0]
    react = [r for r in valid if r["b"] == 0 and r["w"] > 0]
    fade = [r for r in valid if r["b"] > 0 and r["w"] == 0]
    both = [r for r in valid if r["b"] > 0 and r["w"] > 0]
    ratios = sorted(r["ratio"] for r in both if r["ratio"] is not None)
    med = statistics.median(ratios) if ratios else 0.0
    surges = [r for r in both if r["ratio"] and r["ratio"] >= SURGE]
    agentic = [r for r in surges if r["ag"] == 1]
    return {
        "lang": lang, "checked": len(rows), "valid": len(valid),
        "dead": len(dead), "react": len(react), "fade": len(fade), "both": len(both),
        "med_ratio": med, "surges": surges, "agentic": agentic,
    }


def main():
    summaries = []
    for lang in LANGS:
        rows = load(lang)
        if rows is None:
            continue
        summaries.append(summarize(lang, rows))

    print("=" * 96)
    print("КРОСС-ЯЗЫКОВОЙ SURGE-СКАН — сводка (фикс. окна H2-2024 vs H2-2025, page-count, без клонов)")
    print("C++ baseline (случайные старые репы): медиана кросс-год плотности = 1.06x (плоско)")
    print("=" * 96)
    hdr = (f"{'lang':<11}{'checked':>8}{'valid':>7}{'dead':>6}{'react':>6}{'fade':>6}"
           f"{'both':>6}{'med_x':>7}{'surge>=1.5':>11}{'agentic':>9}")
    print(hdr)
    print("-" * 96)
    tot = {k: 0 for k in ("checked", "valid", "dead", "react", "fade", "both", "surge", "agentic")}
    all_med = []
    for s in summaries:
        print(f"{s['lang']:<11}{s['checked']:>8}{s['valid']:>7}{s['dead']:>6}{s['react']:>6}"
              f"{s['fade']:>6}{s['both']:>6}{s['med_ratio']:>7.2f}"
              f"{len(s['surges']):>11}{len(s['agentic']):>9}")
        tot["checked"] += s["checked"]; tot["valid"] += s["valid"]
        tot["dead"] += s["dead"]; tot["react"] += s["react"]; tot["fade"] += s["fade"]
        tot["both"] += s["both"]; tot["surge"] += len(s["surges"]); tot["agentic"] += len(s["agentic"])
        all_med.append(s["med_ratio"])
    print("-" * 96)
    overall_med = statistics.median([m for m in all_med if m]) if all_med else 0
    print(f"{'ИТОГО':<11}{tot['checked']:>8}{tot['valid']:>7}{tot['dead']:>6}{tot['react']:>6}"
          f"{tot['fade']:>6}{tot['both']:>6}{overall_med:>7.2f}{tot['surge']:>11}{tot['agentic']:>9}")
    print("  med_x в строке ИТОГО = медиана из per-language медиан (для сравнения с 1.06x C++)")

    print("\n" + "=" * 96)
    print("ТОП-10 агентских-выросших реп на язык (repo | created | stars | base→win | ratio | msglen | bot | trail)")
    print("=" * 96)
    for s in summaries:
        ag = sorted(s["agentic"], key=lambda r: r["ratio"], reverse=True)[:10]
        if not ag:
            print(f"\n[{s['lang']}] агентских-выросших: 0")
            continue
        print(f"\n[{s['lang']}] агентских-выросших: {len(s['agentic'])} (показаны топ-{len(ag)} по ratio)")
        for r in ag:
            print(f"  {r['repo']:<42} {r['created']} ⭐{r['stars']:<6} "
                  f"{r['b']:>4}→{r['w']:<4} x{r['ratio']:<5.1f} "
                  f"msglen={r['wm']:<4} bot={r['bs']:<4} trail={r['tr']}")


if __name__ == "__main__":
    main()
