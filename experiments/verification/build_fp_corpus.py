#!/usr/bin/env python3
"""Build the #067 regression corpus (§4 of backlog task #070) from round-2
verification results.

Ground truth = round-2 verdicts (`round2_verdicts/`), chosen 2026-06-02 because it
is the only machine-readable verdict set (round 1 lives only in the markdown report
`../ai_repo_run/VERIFICATION_REPORT.md`). These verdicts are LLM-eyeball labels —
NOT reproducible output; treat them as source data, keep them in git.

Input : round2_verdicts/<repo>.json  — one file per repo, each with
        `confirmed` (TP) and `false_positives` (FP) arrays.
Output (deterministic, sorted; regenerate, do not hand-edit):
  fp_corpus_r2.tsv          one row per finding: the labeled measurement corpus
  fp_corpus_r2_summary.json aggregate counts (precision, per-class FP breakdown)

Each new FP filter is measured against this corpus: precision_before/after,
TP_kept, FP_removed_by_class (see task §4 acceptance criteria).
"""
import json
import glob
import os
import collections

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.join(HERE, "round2_verdicts")
TSV = os.path.join(HERE, "fp_corpus_r2.tsv")
SUMMARY = os.path.join(HERE, "fp_corpus_r2_summary.json")

COLUMNS = ["repo", "sha", "label", "fp_class", "klass", "added_loc", "base_loc", "note"]


def split_where(where):
    """`where` is either 'addedpath:span' (FP) or 'added <- base' (TP)."""
    where = (where or "").strip()
    if "<-" in where:
        added, base = where.split("<-", 1)
        return added.strip(), base.strip()
    return where, ""


def clean(text):
    """One-line, TSV-safe."""
    return " ".join((text or "").split()).replace("\t", " ")


def main():
    rows = []
    fp_class = collections.Counter()
    tp = fp = cycles = 0

    for path in sorted(glob.glob(os.path.join(SRC, "*.json"))):
        with open(path, encoding="utf-8") as f:
            d = json.load(f)
        repo = d.get("repo", os.path.basename(path)[:-5])

        for c in d.get("confirmed") or []:
            if not isinstance(c, dict):
                continue
            added, base = split_where(c.get("where"))
            rows.append({
                "repo": repo, "sha": c.get("sha", ""), "label": "TP",
                "fp_class": "", "klass": c.get("klass", ""),
                "added_loc": added, "base_loc": base, "note": clean(c.get("note")),
            })
            tp += 1

        for x in d.get("false_positives") or []:
            if not isinstance(x, dict):
                continue
            added, base = split_where(x.get("where"))
            cls = x.get("fp_class", "?")
            fp_class[cls] += 1
            rows.append({
                "repo": repo, "sha": x.get("sha", ""), "label": "FP",
                "fp_class": cls, "klass": "",
                "added_loc": added, "base_loc": base,
                "note": clean(x.get("what_happened")),
            })
            fp += 1

        if isinstance(d.get("cycle"), dict) and d["cycle"].get("present"):
            cycles += 1

    # Deterministic order: repo, label, location.
    rows.sort(key=lambda r: (r["repo"], r["label"], r["added_loc"], r["sha"]))

    with open(TSV, "w", encoding="utf-8") as out:
        out.write("\t".join(COLUMNS) + "\n")
        for r in rows:
            out.write("\t".join(r[c] for c in COLUMNS) + "\n")

    precision = round(tp / (tp + fp) * 100, 1) if (tp + fp) else 0.0
    summary = {
        "ground_truth": "round2 (round2_verdicts/)",
        "repos": len(glob.glob(os.path.join(SRC, "*.json"))),
        "tp": tp, "fp": fp, "total": tp + fp,
        "precision_pct": precision,
        "cycles_present_repos": cycles,
        "fp_class": dict(sorted(fp_class.items(), key=lambda kv: (-kv[1], kv[0]))),
    }
    with open(SUMMARY, "w", encoding="utf-8") as out:
        json.dump(summary, out, ensure_ascii=False, indent=2)
        out.write("\n")

    print(f"wrote {TSV} ({len(rows)} rows)")
    print(f"wrote {SUMMARY}")
    print(json.dumps(summary, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
